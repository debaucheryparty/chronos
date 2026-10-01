#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <span>
#include <vector>

#include "runtime/error.h"
#include "runtime/memory/memory_types.h"
#include "runtime/result.h"
#include "runtime/types.h"

namespace chronos {

struct MemoryRegion {
    GuestAddress base = 0;
    GuestSize size = 0;
    MemoryPermission permissions = MemoryPermission::None;
    void* host_ptr = nullptr;
};

class GuestMemory {
public:
    explicit GuestMemory(Arch arch);
    ~GuestMemory();

    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    GuestMemory(GuestMemory&&) noexcept;
    GuestMemory& operator=(GuestMemory&&) noexcept;

    [[nodiscard]] Arch arch() const noexcept { return arch_; }

    Result<GuestAddress> Map(GuestAddress addr, GuestSize size, MemoryPermission perms);
    Result<GuestAddress> Allocate(GuestSize size, MemoryPermission perms);
    Result<void> Unmap(GuestAddress addr, GuestSize size);
    Result<void> Protect(GuestAddress addr, GuestSize size, MemoryPermission perms);

    Result<void> Read(GuestAddress addr, void* out_buffer, size_t size) const;
    Result<void> Write(GuestAddress addr, const void* in_buffer, size_t size);

    [[nodiscard]] void* GetHostPointer(GuestAddress addr) const noexcept;
    [[nodiscard]] bool IsMapped(GuestAddress addr, GuestSize size) const noexcept;

    [[nodiscard]] GuestAddress min_address() const noexcept { return min_address_; }
    [[nodiscard]] GuestAddress max_address() const noexcept { return max_address_; }

private:
    [[nodiscard]] const MemoryRegion* FindRegion(GuestAddress addr) const noexcept;
    [[nodiscard]] MemoryRegion* FindRegion(GuestAddress addr) noexcept;
    [[nodiscard]] bool HasOverlap(GuestAddress addr, GuestSize size) const noexcept;

    Arch arch_;
    GuestAddress min_address_ = 0x0001'0000;
    GuestAddress max_address_ = 0xFFFF'FFFF;
    GuestAddress next_alloc_addr_ = 0x4000'0000;

    std::map<GuestAddress, MemoryRegion> regions_;
};

} // namespace chronos
