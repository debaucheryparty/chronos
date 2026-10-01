#include "runtime/memory/guest_memory.h"

#include <algorithm>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace chronos {

namespace {

void* AllocateHostMemory(size_t size, MemoryPermission perms) {
#if defined(_WIN32)
    DWORD win_perms = PAGE_NOACCESS;
    if (HasPermission(perms, MemoryPermission::ReadWriteExecute)) {
        win_perms = PAGE_EXECUTE_READWRITE;
    } else if (HasPermission(perms, MemoryPermission::ReadExecute)) {
        win_perms = PAGE_EXECUTE_READ;
    } else if (HasPermission(perms, MemoryPermission::ReadWrite)) {
        win_perms = PAGE_READWRITE;
    } else if (HasPermission(perms, MemoryPermission::Read)) {
        win_perms = PAGE_READONLY;
    }
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, win_perms);
#else
    int prot = PROT_NONE;
    if (HasPermission(perms, MemoryPermission::Read)) {
        prot |= PROT_READ;
    }
    if (HasPermission(perms, MemoryPermission::Write)) {
        prot |= PROT_WRITE;
    }
    if (HasPermission(perms, MemoryPermission::Execute)) {
        prot |= PROT_EXEC;
    }
    void* ptr = mmap(nullptr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
#endif
}

void FreeHostMemory(void* ptr, size_t size) {
    if (!ptr) {
        return;
    }
#if defined(_WIN32)
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

bool ProtectHostMemory(void* ptr, size_t size, MemoryPermission perms) {
    if (!ptr) {
        return false;
    }
#if defined(_WIN32)
    DWORD win_perms = PAGE_NOACCESS;
    if (HasPermission(perms, MemoryPermission::ReadWriteExecute)) {
        win_perms = PAGE_EXECUTE_READWRITE;
    } else if (HasPermission(perms, MemoryPermission::ReadExecute)) {
        win_perms = PAGE_EXECUTE_READ;
    } else if (HasPermission(perms, MemoryPermission::ReadWrite)) {
        win_perms = PAGE_READWRITE;
    } else if (HasPermission(perms, MemoryPermission::Read)) {
        win_perms = PAGE_READONLY;
    }
    DWORD old_protect = 0;
    return VirtualProtect(ptr, size, win_perms, &old_protect) != 0;
#else
    int prot = PROT_NONE;
    if (HasPermission(perms, MemoryPermission::Read)) {
        prot |= PROT_READ;
    }
    if (HasPermission(perms, MemoryPermission::Write)) {
        prot |= PROT_WRITE;
    }
    if (HasPermission(perms, MemoryPermission::Execute)) {
        prot |= PROT_EXEC;
    }
    return mprotect(ptr, size, prot) == 0;
#endif
}

} // namespace

GuestMemory::GuestMemory(Arch arch)
    : arch_(arch) {
    if (arch_ == Arch::Arm64) {
        // AArch64 Android user-space address space (up to 48 bits)
        min_address_ = 0x0001'0000;
        max_address_ = 0x0000'7FFF'FFFF'FFFF;
        next_alloc_addr_ = 0x0000'0060'0000'0000;
    } else {
        // 32-bit ARM Android user-space address space (4 GB max)
        min_address_ = 0x0001'0000;
        max_address_ = 0xFFFF'FFFF;
        next_alloc_addr_ = 0x4000'0000;
    }
}

GuestMemory::~GuestMemory() {
    for (auto& [_, region] : regions_) {
        FreeHostMemory(region.host_ptr, static_cast<size_t>(region.size));
    }
    regions_.clear();
}

GuestMemory::GuestMemory(GuestMemory&& other) noexcept
    : arch_(other.arch_),
      min_address_(other.min_address_),
      max_address_(other.max_address_),
      next_alloc_addr_(other.next_alloc_addr_),
      regions_(std::move(other.regions_)) {}

GuestMemory& GuestMemory::operator=(GuestMemory&& other) noexcept {
    if (this != &other) {
        for (auto& [_, region] : regions_) {
            FreeHostMemory(region.host_ptr, static_cast<size_t>(region.size));
        }
        regions_.clear();

        arch_ = other.arch_;
        min_address_ = other.min_address_;
        max_address_ = other.max_address_;
        next_alloc_addr_ = other.next_alloc_addr_;
        regions_ = std::move(other.regions_);
    }
    return *this;
}

bool GuestMemory::HasOverlap(GuestAddress addr, GuestSize size) const noexcept {
    const GuestAddress end = addr + size;
    for (const auto& [base, region] : regions_) {
        const GuestAddress region_end = base + region.size;
        if (addr < region_end && end > base) {
            return true;
        }
    }
    return false;
}

const MemoryRegion* GuestMemory::FindRegion(GuestAddress addr) const noexcept {
    if (regions_.empty()) {
        return nullptr;
    }
    auto it = regions_.upper_bound(addr);
    if (it != regions_.begin()) {
        --it;
        if (addr >= it->second.base && addr < it->second.base + it->second.size) {
            return &it->second;
        }
    }
    return nullptr;
}

MemoryRegion* GuestMemory::FindRegion(GuestAddress addr) noexcept {
    if (regions_.empty()) {
        return nullptr;
    }
    auto it = regions_.upper_bound(addr);
    if (it != regions_.begin()) {
        --it;
        if (addr >= it->second.base && addr < it->second.base + it->second.size) {
            return &it->second;
        }
    }
    return nullptr;
}

Result<GuestAddress> GuestMemory::Map(GuestAddress addr, GuestSize size, MemoryPermission perms) {
    if (size == 0) {
        return Error(ErrorCode::InvalidArgument, "map size must be non-zero");
    }

    const GuestAddress aligned_addr = AlignDownToPage(addr);
    const GuestSize aligned_size = AlignUpToPage(size + (addr - aligned_addr));

    if (aligned_addr < min_address_ || aligned_addr + aligned_size > max_address_) {
        return Error(ErrorCode::InvalidAddress, "address outside guest bounds");
    }

    if (HasOverlap(aligned_addr, aligned_size)) {
        return Error(ErrorCode::InvalidAddress, "memory region already mapped");
    }

    void* host_ptr = AllocateHostMemory(static_cast<size_t>(aligned_size), perms);
    if (!host_ptr) {
        return Error(ErrorCode::OutOfMemory, "host memory allocation failed");
    }

    MemoryRegion region{
        .base = aligned_addr,
        .size = aligned_size,
        .permissions = perms,
        .host_ptr = host_ptr,
    };

    regions_[aligned_addr] = region;
    return aligned_addr;
}

Result<GuestAddress> GuestMemory::Allocate(GuestSize size, MemoryPermission perms) {
    if (size == 0) {
        return Error(ErrorCode::InvalidArgument, "allocation size must be non-zero");
    }

    const GuestSize aligned_size = AlignUpToPage(size);
    GuestAddress candidate = next_alloc_addr_;

    while (candidate + aligned_size <= max_address_) {
        if (!HasOverlap(candidate, aligned_size)) {
            auto res = Map(candidate, aligned_size, perms);
            if (res) {
                next_alloc_addr_ = candidate + aligned_size;
                return candidate;
            }
        }
        candidate += kPageSize;
    }

    return Error(ErrorCode::OutOfMemory, "unable to find contiguous guest address space");
}

Result<void> GuestMemory::Unmap(GuestAddress addr, GuestSize size) {
    if (size == 0) {
        return Error(ErrorCode::InvalidArgument, "unmap size must be non-zero");
    }

    const GuestAddress aligned_addr = AlignDownToPage(addr);
    auto it = regions_.find(aligned_addr);
    if (it == regions_.end()) {
        return Error(ErrorCode::InvalidAddress, "region not found");
    }

    FreeHostMemory(it->second.host_ptr, static_cast<size_t>(it->second.size));
    regions_.erase(it);
    return {};
}

Result<void> GuestMemory::Protect(GuestAddress addr, GuestSize size, MemoryPermission perms) {
    if (size == 0) {
        return Error(ErrorCode::InvalidArgument, "protect size must be non-zero");
    }

    const GuestAddress aligned_addr = AlignDownToPage(addr);
    auto it = regions_.find(aligned_addr);
    if (it == regions_.end()) {
        return Error(ErrorCode::InvalidAddress, "region not found");
    }

    if (!ProtectHostMemory(it->second.host_ptr, static_cast<size_t>(it->second.size), perms)) {
        return Error(ErrorCode::PermissionDenied, "failed to update host page protection");
    }

    it->second.permissions = perms;
    return {};
}

void* GuestMemory::GetHostPointer(GuestAddress addr) const noexcept {
    const MemoryRegion* region = FindRegion(addr);
    if (!region || !region->host_ptr) {
        return nullptr;
    }

    const auto offset = static_cast<uintptr_t>(addr - region->base);
    return static_cast<uint8_t*>(region->host_ptr) + offset;
}

bool GuestMemory::IsMapped(GuestAddress addr, GuestSize size) const noexcept {
    if (size == 0) {
        return false;
    }

    GuestAddress current = addr;
    const GuestAddress end = addr + size;

    while (current < end) {
        const MemoryRegion* region = FindRegion(current);
        if (!region) {
            return false;
        }
        const GuestAddress region_end = region->base + region->size;
        current = region_end;
    }
    return true;
}

Result<void> GuestMemory::Read(GuestAddress addr, void* out_buffer, size_t size) const {
    if (size == 0) {
        return {};
    }
    if (!out_buffer) {
        return Error(ErrorCode::InvalidArgument, "destination buffer is null");
    }

    const MemoryRegion* region = FindRegion(addr);
    if (!region || !HasPermission(region->permissions, MemoryPermission::Read)) {
        return Error(ErrorCode::PermissionDenied, "address not readable");
    }

    if (addr + size > region->base + region->size) {
        return Error(ErrorCode::InvalidAddress, "read spans unmapped or discontinuous region");
    }

    const void* src = GetHostPointer(addr);
    std::memcpy(out_buffer, src, size);
    return {};
}

Result<void> GuestMemory::Write(GuestAddress addr, const void* in_buffer, size_t size) {
    if (size == 0) {
        return {};
    }
    if (!in_buffer) {
        return Error(ErrorCode::InvalidArgument, "source buffer is null");
    }

    MemoryRegion* region = FindRegion(addr);
    if (!region || !HasPermission(region->permissions, MemoryPermission::Write)) {
        return Error(ErrorCode::PermissionDenied, "address not writable");
    }

    if (addr + size > region->base + region->size) {
        return Error(ErrorCode::InvalidAddress, "write spans unmapped or discontinuous region");
    }

    void* dst = GetHostPointer(addr);
    std::memcpy(dst, in_buffer, size);
    return {};
}

} // namespace chronos
