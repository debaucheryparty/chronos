#include "runtime/loader/elf_loader.h"

#include <cstring>
#include <vector>

namespace chronos {

ElfLoader::ElfLoader(GuestMemory& memory)
    : memory_(memory) {}

Result<LoadedLibrary> ElfLoader::LoadFromFile(const std::filesystem::path& path) {
    auto image_res = ElfImage::LoadFromFile(path);
    if (!image_res) {
        return image_res.error();
    }
    return Load(image_res.value());
}

Result<LoadedLibrary> ElfLoader::Load(const ElfImage& image) {
    if (image.segments().empty()) {
        return Error(ErrorCode::ElfLoadFailed, "no loadable segments");
    }

    GuestAddress load_bias = 0;
    if (image.is_pie()) {
        auto alloc_res = memory_.Allocate(image.memory_size(), MemoryPermission::ReadWriteExecute);
        if (!alloc_res) {
            return alloc_res.error();
        }
        load_bias = alloc_res.value() - image.min_vaddr();
    }

    for (const auto& seg : image.segments()) {
        const GuestAddress seg_vaddr = load_bias + seg.vaddr;
        MemoryPermission perms = MemoryPermission::Read;
        if (seg.flags & elf::kPfW) perms = perms | MemoryPermission::Write;
        if (seg.flags & elf::kPfX) perms = perms | MemoryPermission::Execute;

        if (!memory_.IsMapped(seg_vaddr, seg.mem_size)) {
            auto map_res = memory_.Map(seg_vaddr, seg.mem_size, perms | MemoryPermission::Write);
            if (!map_res) {
                return map_res.error();
            }
        }

        if (seg.file_size > 0) {
            auto write_res = memory_.Write(seg_vaddr, image.raw_data().data() + seg.file_offset, static_cast<size_t>(seg.file_size));
            if (!write_res) {
                return write_res.error();
            }
        }

        if (seg.mem_size > seg.file_size) {
            const size_t bss_size = static_cast<size_t>(seg.mem_size - seg.file_size);
            std::vector<uint8_t> zeros(bss_size, 0);
            auto write_res = memory_.Write(seg_vaddr + seg.file_size, zeros.data(), bss_size);
            if (!write_res) {
                return write_res.error();
            }
        }
    }

    LoadedLibrary lib;
    lib.name = image.soname().empty() ? "anonymous" : image.soname();
    lib.load_bias = load_bias;
    lib.entry_point = load_bias + image.entry_point();
    lib.memory_size = image.memory_size();
    lib.arch = image.arch();
    lib.dynamic_addr = load_bias + image.dynamic_vaddr();

    if (image.init_func() != 0) {
        lib.init_functions.push_back(load_bias + image.init_func());
    }

    if (image.init_array() != 0 && image.init_array_count() > 0) {
        if (image.arch() == Arch::Arm64) {
            std::vector<uint64_t> init_ptrs(image.init_array_count());
            if (memory_.Read(load_bias + image.init_array(), init_ptrs.data(), init_ptrs.size() * sizeof(uint64_t))) {
                for (uint64_t ptr : init_ptrs) {
                    if (ptr != 0 && ptr != static_cast<uint64_t>(-1)) {
                        lib.init_functions.push_back(load_bias + ptr);
                    }
                }
            }
        } else {
            std::vector<uint32_t> init_ptrs(image.init_array_count());
            if (memory_.Read(load_bias + image.init_array(), init_ptrs.data(), init_ptrs.size() * sizeof(uint32_t))) {
                for (uint32_t ptr : init_ptrs) {
                    if (ptr != 0 && ptr != static_cast<uint32_t>(-1)) {
                        lib.init_functions.push_back(load_bias + ptr);
                    }
                }
            }
        }
    }

    if (image.arch() == Arch::Arm64) {
        PopulateSymbols64(lib, image);
        auto rel_res = ApplyRelocations64(lib, image);
        if (!rel_res) {
            return rel_res.error();
        }
    } else {
        PopulateSymbols32(lib, image);
        auto rel_res = ApplyRelocations32(lib, image);
        if (!rel_res) {
            return rel_res.error();
        }
    }

    for (const auto& [name, addr] : lib.exported_symbols) {
        global_symbols_[name] = addr;
    }

    loaded_libraries_.push_back(lib);
    return lib;
}

void ElfLoader::PopulateSymbols32(LoadedLibrary& lib, const ElfImage& image) {
    if (image.dynamic_vaddr() == 0) return;

    uint32_t symtab_vaddr = 0;
    uint32_t strtab_vaddr = 0;
    size_t strsz = 0;

    const GuestAddress dyn_addr = lib.load_bias + image.dynamic_vaddr();
    const size_t dyn_count = static_cast<size_t>(image.dynamic_size() / sizeof(elf::Elf32_Dyn));
    std::vector<elf::Elf32_Dyn> dyns(dyn_count);

    if (!memory_.Read(dyn_addr, dyns.data(), dyn_count * sizeof(elf::Elf32_Dyn))) {
        return;
    }

    for (const auto& d : dyns) {
        if (d.d_tag == elf::kDtNull) break;
        if (d.d_tag == elf::kDtSymTab) symtab_vaddr = d.d_un.d_ptr;
        if (d.d_tag == elf::kDtStrTab) strtab_vaddr = d.d_un.d_ptr;
        if (d.d_tag == elf::kDtStrSz) strsz = d.d_un.d_val;
    }

    if (symtab_vaddr == 0 || strtab_vaddr == 0 || strsz == 0) {
        return;
    }

    std::vector<char> strtab(strsz);
    if (!memory_.Read(lib.load_bias + strtab_vaddr, strtab.data(), strsz)) {
        return;
    }

    GuestAddress curr_sym = lib.load_bias + symtab_vaddr;
    while (true) {
        elf::Elf32_Sym sym{};
        if (!memory_.Read(curr_sym, &sym, sizeof(sym))) {
            break;
        }
        if (sym.st_name == 0 && sym.st_value == 0 && sym.st_info == 0) {
            curr_sym += sizeof(elf::Elf32_Sym);
            continue;
        }
        if (sym.st_name >= strsz) {
            break;
        }

        const char* name = strtab.data() + sym.st_name;
        if (*name != '\0' && sym.st_shndx != 0) {
            lib.exported_symbols[name] = lib.load_bias + sym.st_value;
        }
        curr_sym += sizeof(elf::Elf32_Sym);
    }
}

void ElfLoader::PopulateSymbols64(LoadedLibrary& lib, const ElfImage& image) {
    if (image.dynamic_vaddr() == 0) return;

    uint64_t symtab_vaddr = 0;
    uint64_t strtab_vaddr = 0;
    size_t strsz = 0;

    const GuestAddress dyn_addr = lib.load_bias + image.dynamic_vaddr();
    const size_t dyn_count = static_cast<size_t>(image.dynamic_size() / sizeof(elf::Elf64_Dyn));
    std::vector<elf::Elf64_Dyn> dyns(dyn_count);

    if (!memory_.Read(dyn_addr, dyns.data(), dyn_count * sizeof(elf::Elf64_Dyn))) {
        return;
    }

    for (const auto& d : dyns) {
        if (d.d_tag == elf::kDtNull) break;
        if (d.d_tag == elf::kDtSymTab) symtab_vaddr = d.d_un.d_ptr;
        if (d.d_tag == elf::kDtStrTab) strtab_vaddr = d.d_un.d_ptr;
        if (d.d_tag == elf::kDtStrSz) strsz = d.d_un.d_val;
    }

    if (symtab_vaddr == 0 || strtab_vaddr == 0 || strsz == 0) {
        return;
    }

    std::vector<char> strtab(strsz);
    if (!memory_.Read(lib.load_bias + strtab_vaddr, strtab.data(), strsz)) {
        return;
    }

    GuestAddress curr_sym = lib.load_bias + symtab_vaddr;
    while (true) {
        elf::Elf64_Sym sym{};
        if (!memory_.Read(curr_sym, &sym, sizeof(sym))) {
            break;
        }
        if (sym.st_name == 0 && sym.st_value == 0 && sym.st_info == 0) {
            curr_sym += sizeof(elf::Elf64_Sym);
            continue;
        }
        if (sym.st_name >= strsz) {
            break;
        }

        const char* name = strtab.data() + sym.st_name;
        if (*name != '\0' && sym.st_shndx != 0) {
            lib.exported_symbols[name] = lib.load_bias + sym.st_value;
        }
        curr_sym += sizeof(elf::Elf64_Sym);
    }
}

Result<void> ElfLoader::ApplyRelocations32(LoadedLibrary& lib, const ElfImage& image) {
    if (image.dynamic_vaddr() == 0) return {};

    uint32_t rel_vaddr = 0;
    size_t rel_size = 0;
    size_t rel_ent = sizeof(elf::Elf32_Rel);

    const GuestAddress dyn_addr = lib.load_bias + image.dynamic_vaddr();
    const size_t dyn_count = static_cast<size_t>(image.dynamic_size() / sizeof(elf::Elf32_Dyn));
    std::vector<elf::Elf32_Dyn> dyns(dyn_count);

    if (memory_.Read(dyn_addr, dyns.data(), dyn_count * sizeof(elf::Elf32_Dyn))) {
        for (const auto& d : dyns) {
            if (d.d_tag == elf::kDtNull) break;
            if (d.d_tag == elf::kDtRel) rel_vaddr = d.d_un.d_ptr;
            if (d.d_tag == elf::kDtRelSz) rel_size = d.d_un.d_val;
            if (d.d_tag == elf::kDtRelEnt && d.d_un.d_val > 0) rel_ent = d.d_un.d_val;
        }
    }

    if (rel_vaddr != 0 && rel_size > 0) {
        const size_t count = rel_size / rel_ent;
        for (size_t i = 0; i < count; ++i) {
            elf::Elf32_Rel rel{};
            if (!memory_.Read(lib.load_bias + rel_vaddr + i * rel_ent, &rel, sizeof(rel))) {
                break;
            }

            const uint32_t r_type = elf::Elf32RType(rel.r_info);
            const GuestAddress target_addr = lib.load_bias + rel.r_offset;

            if (r_type == elf::kRArmRelative) {
                uint32_t orig = 0;
                memory_.Read(target_addr, &orig, sizeof(orig));
                orig += static_cast<uint32_t>(lib.load_bias);
                memory_.Write(target_addr, &orig, sizeof(orig));
            }
        }
    }

    return {};
}

Result<void> ElfLoader::ApplyRelocations64(LoadedLibrary& lib, const ElfImage& image) {
    if (image.dynamic_vaddr() == 0) return {};

    uint64_t rela_vaddr = 0;
    size_t rela_size = 0;
    size_t rela_ent = sizeof(elf::Elf64_Rela);

    const GuestAddress dyn_addr = lib.load_bias + image.dynamic_vaddr();
    const size_t dyn_count = static_cast<size_t>(image.dynamic_size() / sizeof(elf::Elf64_Dyn));
    std::vector<elf::Elf64_Dyn> dyns(dyn_count);

    if (memory_.Read(dyn_addr, dyns.data(), dyn_count * sizeof(elf::Elf64_Dyn))) {
        for (const auto& d : dyns) {
            if (d.d_tag == elf::kDtNull) break;
            if (d.d_tag == elf::kDtRela) rela_vaddr = d.d_un.d_ptr;
            if (d.d_tag == elf::kDtRelaSz) rela_size = d.d_un.d_val;
            if (d.d_tag == elf::kDtRelaEnt && d.d_un.d_val > 0) rela_ent = d.d_un.d_val;
        }
    }

    if (rela_vaddr != 0 && rela_size > 0) {
        const size_t count = rela_size / rela_ent;
        for (size_t i = 0; i < count; ++i) {
            elf::Elf64_Rela rela{};
            if (!memory_.Read(lib.load_bias + rela_vaddr + i * rela_ent, &rela, sizeof(rela))) {
                break;
            }

            const uint32_t r_type = elf::Elf64RType(rela.r_info);
            const GuestAddress target_addr = lib.load_bias + rela.r_offset;

            if (r_type == elf::kRAarch64Relative) {
                const uint64_t resolved = lib.load_bias + static_cast<uint64_t>(rela.r_addend);
                memory_.Write(target_addr, &resolved, sizeof(resolved));
            }
        }
    }

    return {};
}

std::optional<GuestAddress> ElfLoader::ResolveSymbol(std::string_view name) const {
    auto it = global_symbols_.find(std::string(name));
    if (it != global_symbols_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ElfLoader::RegisterSymbol(std::string name, GuestAddress addr) {
    global_symbols_[std::move(name)] = addr;
}

} // namespace chronos
