#pragma once

#include <cstdint>

#include "runtime/types.h"

namespace chronos {

// Android targets use 4 KB virtual pages by default.
constexpr GuestSize kPageSize = 4096;
constexpr GuestSize kPageMask = kPageSize - 1;

enum class MemoryPermission : uint8_t {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Execute = 1 << 2,
    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    ReadWriteExecute = Read | Write | Execute,
};

constexpr MemoryPermission operator|(MemoryPermission lhs, MemoryPermission rhs) noexcept {
    return static_cast<MemoryPermission>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr MemoryPermission operator&(MemoryPermission lhs, MemoryPermission rhs) noexcept {
    return static_cast<MemoryPermission>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr bool HasPermission(MemoryPermission perms, MemoryPermission required) noexcept {
    return (perms & required) == required;
}

constexpr GuestAddress AlignDownToPage(GuestAddress addr) noexcept {
    return addr & ~kPageMask;
}

constexpr GuestAddress AlignUpToPage(GuestAddress addr) noexcept {
    return (addr + kPageMask) & ~kPageMask;
}

} // namespace chronos
