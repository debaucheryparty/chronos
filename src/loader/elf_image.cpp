#include "runtime/loader/elf_image.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "runtime/memory/memory_types.h"

namespace chronos {

Result<ElfImage> ElfImage::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Error(ErrorCode::IoError, "failed to open ELF file");
    }

    const auto file_size = file.tellg();
    if (file_size <= 0) {
        return Error(ErrorCode::ElfLoadFailed, "empty ELF file");
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(file_size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        return Error(ErrorCode::IoError, "failed to read ELF file");
    }

    return Parse(buffer);
}

Result<ElfImage> ElfImage::Parse(std::span<const uint8_t> data) {
    if (data.size() < 16) {
        return Error(ErrorCode::ElfLoadFailed, "file smaller than ELF identifier");
    }

    if (data[0] != elf::kElfMag0 || data[1] != elf::kElfMag1 ||
        data[2] != elf::kElfMag2 || data[3] != elf::kElfMag3) {
        return Error(ErrorCode::ElfLoadFailed, "invalid ELF magic");
    }

    ElfImage image;
    image.data_.assign(data.begin(), data.end());

    const uint8_t elf_class = data[4];
    if (elf_class == elf::kElfClass32) {
        auto res = image.Parse32(data);
        if (!res) {
            return res.error();
        }
    } else if (elf_class == elf::kElfClass64) {
        auto res = image.Parse64(data);
        if (!res) {
            return res.error();
        }
    } else {
        return Error(ErrorCode::UnsupportedAbi, "unsupported ELF class");
    }

    return image;
}

Result<void> ElfImage::Parse32(std::span<const uint8_t> data) {
    if (data.size() < sizeof(elf::Elf32_Ehdr)) {
        return Error(ErrorCode::ElfLoadFailed, "truncated 32-bit ELF header");
    }

    const auto* ehdr = reinterpret_cast<const elf::Elf32_Ehdr*>(data.data());
    if (ehdr->e_machine != elf::kEmArm) {
        return Error(ErrorCode::UnsupportedAbi, "expected ARM32 architecture");
    }

    arch_ = Arch::Arm32;
    entry_point_ = ehdr->e_entry;
    is_pie_ = (ehdr->e_type == elf::kEtDyn);

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(elf::Elf32_Phdr) > data.size()) {
        return Error(ErrorCode::ElfLoadFailed, "program header table out of bounds");
    }

    const auto* phdrs = reinterpret_cast<const elf::Elf32_Phdr*>(data.data() + ehdr->e_phoff);

    bool first_load = true;
    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const auto& ph = phdrs[i];
        if (ph.p_type == elf::kPtLoad && ph.p_memsz > 0) {
            const GuestAddress seg_start = AlignDownToPage(ph.p_vaddr);
            const GuestAddress seg_end = AlignUpToPage(ph.p_vaddr + ph.p_memsz);

            if (first_load) {
                min_vaddr_ = seg_start;
                max_vaddr_ = seg_end;
                first_load = false;
            } else {
                min_vaddr_ = std::min(min_vaddr_, seg_start);
                max_vaddr_ = std::max(max_vaddr_, seg_end);
            }

            segments_.push_back(LoadSegment{
                .vaddr = ph.p_vaddr,
                .mem_size = ph.p_memsz,
                .file_size = ph.p_filesz,
                .file_offset = ph.p_offset,
                .flags = ph.p_flags,
                .align = ph.p_align,
            });
        } else if (ph.p_type == elf::kPtDynamic) {
            dynamic_vaddr_ = ph.p_vaddr;
            dynamic_size_ = ph.p_memsz;
        }
    }

    if (dynamic_vaddr_ != 0) {
        uint64_t dyn_file_offset = 0;
        for (const auto& seg : segments_) {
            if (dynamic_vaddr_ >= seg.vaddr && dynamic_vaddr_ < seg.vaddr + seg.mem_size) {
                dyn_file_offset = seg.file_offset + (dynamic_vaddr_ - seg.vaddr);
                break;
            }
        }

        if (dyn_file_offset != 0 && dyn_file_offset + dynamic_size_ <= data.size()) {
            const auto* dyn = reinterpret_cast<const elf::Elf32_Dyn*>(data.data() + dyn_file_offset);
            const size_t dyn_count = static_cast<size_t>(dynamic_size_ / sizeof(elf::Elf32_Dyn));

            uint32_t strtab_vaddr = 0;
            size_t strsz = 0;

            for (size_t i = 0; i < dyn_count; ++i) {
                if (dyn[i].d_tag == elf::kDtNull) break;
                if (dyn[i].d_tag == elf::kDtStrTab) strtab_vaddr = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtStrSz) strsz = dyn[i].d_un.d_val;
                if (dyn[i].d_tag == elf::kDtInit) init_func_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtInitArray) init_array_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtInitArraySz) init_array_count_ = dyn[i].d_un.d_val / sizeof(uint32_t);
                if (dyn[i].d_tag == elf::kDtFini) fini_func_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtFiniArray) fini_array_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtFiniArraySz) fini_array_count_ = dyn[i].d_un.d_val / sizeof(uint32_t);
            }

            uint64_t strtab_offset = 0;
            for (const auto& seg : segments_) {
                if (strtab_vaddr >= seg.vaddr && strtab_vaddr < seg.vaddr + seg.mem_size) {
                    strtab_offset = seg.file_offset + (strtab_vaddr - seg.vaddr);
                    break;
                }
            }

            if (strtab_offset != 0 && strtab_offset + strsz <= data.size()) {
                const char* strtab = reinterpret_cast<const char*>(data.data() + strtab_offset);
                for (size_t i = 0; i < dyn_count; ++i) {
                    if (dyn[i].d_tag == elf::kDtNull) break;
                    if (dyn[i].d_tag == elf::kDtSoName && dyn[i].d_un.d_val < strsz) {
                        soname_ = strtab + dyn[i].d_un.d_val;
                    } else if (dyn[i].d_tag == elf::kDtNeeded && dyn[i].d_un.d_val < strsz) {
                        needed_.emplace_back(strtab + dyn[i].d_un.d_val);
                    }
                }
            }
        }
    }

    return {};
}

Result<void> ElfImage::Parse64(std::span<const uint8_t> data) {
    if (data.size() < sizeof(elf::Elf64_Ehdr)) {
        return Error(ErrorCode::ElfLoadFailed, "truncated 64-bit ELF header");
    }

    const auto* ehdr = reinterpret_cast<const elf::Elf64_Ehdr*>(data.data());
    if (ehdr->e_machine != elf::kEmAarch64) {
        return Error(ErrorCode::UnsupportedAbi, "expected AArch64 architecture");
    }

    arch_ = Arch::Arm64;
    entry_point_ = ehdr->e_entry;
    is_pie_ = (ehdr->e_type == elf::kEtDyn);

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(elf::Elf64_Phdr) > data.size()) {
        return Error(ErrorCode::ElfLoadFailed, "program header table out of bounds");
    }

    const auto* phdrs = reinterpret_cast<const elf::Elf64_Phdr*>(data.data() + ehdr->e_phoff);

    bool first_load = true;
    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const auto& ph = phdrs[i];
        if (ph.p_type == elf::kPtLoad && ph.p_memsz > 0) {
            const GuestAddress seg_start = AlignDownToPage(ph.p_vaddr);
            const GuestAddress seg_end = AlignUpToPage(ph.p_vaddr + ph.p_memsz);

            if (first_load) {
                min_vaddr_ = seg_start;
                max_vaddr_ = seg_end;
                first_load = false;
            } else {
                min_vaddr_ = std::min(min_vaddr_, seg_start);
                max_vaddr_ = std::max(max_vaddr_, seg_end);
            }

            segments_.push_back(LoadSegment{
                .vaddr = ph.p_vaddr,
                .mem_size = ph.p_memsz,
                .file_size = ph.p_filesz,
                .file_offset = ph.p_offset,
                .flags = ph.p_flags,
                .align = ph.p_align,
            });
        } else if (ph.p_type == elf::kPtDynamic) {
            dynamic_vaddr_ = ph.p_vaddr;
            dynamic_size_ = ph.p_memsz;
        }
    }

    if (dynamic_vaddr_ != 0) {
        uint64_t dyn_file_offset = 0;
        for (const auto& seg : segments_) {
            if (dynamic_vaddr_ >= seg.vaddr && dynamic_vaddr_ < seg.vaddr + seg.mem_size) {
                dyn_file_offset = seg.file_offset + (dynamic_vaddr_ - seg.vaddr);
                break;
            }
        }

        if (dyn_file_offset != 0 && dyn_file_offset + dynamic_size_ <= data.size()) {
            const auto* dyn = reinterpret_cast<const elf::Elf64_Dyn*>(data.data() + dyn_file_offset);
            const size_t dyn_count = static_cast<size_t>(dynamic_size_ / sizeof(elf::Elf64_Dyn));

            uint64_t strtab_vaddr = 0;
            size_t strsz = 0;

            for (size_t i = 0; i < dyn_count; ++i) {
                if (dyn[i].d_tag == elf::kDtNull) break;
                if (dyn[i].d_tag == elf::kDtStrTab) strtab_vaddr = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtStrSz) strsz = dyn[i].d_un.d_val;
                if (dyn[i].d_tag == elf::kDtInit) init_func_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtInitArray) init_array_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtInitArraySz) init_array_count_ = dyn[i].d_un.d_val / sizeof(uint64_t);
                if (dyn[i].d_tag == elf::kDtFini) fini_func_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtFiniArray) fini_array_ = dyn[i].d_un.d_ptr;
                if (dyn[i].d_tag == elf::kDtFiniArraySz) fini_array_count_ = dyn[i].d_un.d_val / sizeof(uint64_t);
            }

            uint64_t strtab_offset = 0;
            for (const auto& seg : segments_) {
                if (strtab_vaddr >= seg.vaddr && strtab_vaddr < seg.vaddr + seg.mem_size) {
                    strtab_offset = seg.file_offset + (strtab_vaddr - seg.vaddr);
                    break;
                }
            }

            if (strtab_offset != 0 && strtab_offset + strsz <= data.size()) {
                const char* strtab = reinterpret_cast<const char*>(data.data() + strtab_offset);
                for (size_t i = 0; i < dyn_count; ++i) {
                    if (dyn[i].d_tag == elf::kDtNull) break;
                    if (dyn[i].d_tag == elf::kDtSoName && dyn[i].d_un.d_val < strsz) {
                        soname_ = strtab + dyn[i].d_un.d_val;
                    } else if (dyn[i].d_tag == elf::kDtNeeded && dyn[i].d_un.d_val < strsz) {
                        needed_.emplace_back(strtab + dyn[i].d_un.d_val);
                    }
                }
            }
        }
    }

    return {};
}

} // namespace chronos
