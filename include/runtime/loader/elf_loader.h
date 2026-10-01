#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "runtime/error.h"
#include "runtime/loader/elf_image.h"
#include "runtime/memory/guest_memory.h"
#include "runtime/result.h"
#include "runtime/types.h"

namespace chronos {

struct LoadedLibrary {
    std::string name;
    GuestAddress load_bias = 0;
    GuestAddress entry_point = 0;
    GuestSize memory_size = 0;
    Arch arch = Arch::Arm64;

    GuestAddress dynamic_addr = 0;
    std::vector<GuestAddress> init_functions;

    std::unordered_map<std::string, GuestAddress> exported_symbols;

    [[nodiscard]] std::optional<GuestAddress> ResolveSymbol(std::string_view sym_name) const {
        auto it = exported_symbols.find(std::string(sym_name));
        if (it != exported_symbols.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

class ElfLoader {
public:
    explicit ElfLoader(GuestMemory& memory);

    Result<LoadedLibrary> Load(const ElfImage& image);
    Result<LoadedLibrary> LoadFromFile(const std::filesystem::path& path);

    [[nodiscard]] const std::vector<LoadedLibrary>& loaded_libraries() const noexcept {
        return loaded_libraries_;
    }

    [[nodiscard]] std::optional<GuestAddress> ResolveSymbol(std::string_view name) const;
    void RegisterSymbol(std::string name, GuestAddress addr);

private:
    Result<void> ApplyRelocations32(LoadedLibrary& lib, const ElfImage& image);
    Result<void> ApplyRelocations64(LoadedLibrary& lib, const ElfImage& image);
    void PopulateSymbols32(LoadedLibrary& lib, const ElfImage& image);
    void PopulateSymbols64(LoadedLibrary& lib, const ElfImage& image);

    GuestMemory& memory_;
    std::vector<LoadedLibrary> loaded_libraries_;
    std::unordered_map<std::string, GuestAddress> global_symbols_;
};

} // namespace chronos
