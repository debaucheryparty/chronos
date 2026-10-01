#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "runtime/error.h"
#include "runtime/loader/elf_types.h"
#include "runtime/result.h"
#include "runtime/types.h"

namespace chronos {

struct LoadSegment {
    GuestAddress vaddr = 0;
    GuestSize mem_size = 0;
    GuestSize file_size = 0;
    uint64_t file_offset = 0;
    uint32_t flags = 0;
    uint64_t align = 0;
};

class ElfImage {
public:
    static Result<ElfImage> Parse(std::span<const uint8_t> data);
    static Result<ElfImage> LoadFromFile(const std::filesystem::path& path);

    [[nodiscard]] Arch arch() const noexcept { return arch_; }
    [[nodiscard]] GuestAddress entry_point() const noexcept { return entry_point_; }
    [[nodiscard]] GuestAddress min_vaddr() const noexcept { return min_vaddr_; }
    [[nodiscard]] GuestAddress max_vaddr() const noexcept { return max_vaddr_; }
    [[nodiscard]] GuestSize memory_size() const noexcept { return max_vaddr_ - min_vaddr_; }
    [[nodiscard]] bool is_pie() const noexcept { return is_pie_; }

    [[nodiscard]] const std::vector<LoadSegment>& segments() const noexcept { return segments_; }
    [[nodiscard]] std::span<const uint8_t> raw_data() const noexcept { return data_; }

    [[nodiscard]] const std::string& soname() const noexcept { return soname_; }
    [[nodiscard]] const std::vector<std::string>& needed_libraries() const noexcept { return needed_; }

    [[nodiscard]] GuestAddress dynamic_vaddr() const noexcept { return dynamic_vaddr_; }
    [[nodiscard]] GuestSize dynamic_size() const noexcept { return dynamic_size_; }

    [[nodiscard]] GuestAddress init_func() const noexcept { return init_func_; }
    [[nodiscard]] GuestAddress init_array() const noexcept { return init_array_; }
    [[nodiscard]] size_t init_array_count() const noexcept { return init_array_count_; }

    [[nodiscard]] GuestAddress fini_func() const noexcept { return fini_func_; }
    [[nodiscard]] GuestAddress fini_array() const noexcept { return fini_array_; }
    [[nodiscard]] size_t fini_array_count() const noexcept { return fini_array_count_; }

private:
    Result<void> Parse32(std::span<const uint8_t> data);
    Result<void> Parse64(std::span<const uint8_t> data);

    Arch arch_ = Arch::Arm64;
    GuestAddress entry_point_ = 0;
    GuestAddress min_vaddr_ = 0;
    GuestAddress max_vaddr_ = 0;
    bool is_pie_ = false;

    std::vector<LoadSegment> segments_;
    std::vector<uint8_t> data_;

    std::string soname_;
    std::vector<std::string> needed_;

    GuestAddress dynamic_vaddr_ = 0;
    GuestSize dynamic_size_ = 0;

    GuestAddress init_func_ = 0;
    GuestAddress init_array_ = 0;
    size_t init_array_count_ = 0;

    GuestAddress fini_func_ = 0;
    GuestAddress fini_array_ = 0;
    size_t fini_array_count_ = 0;
};

} // namespace chronos
