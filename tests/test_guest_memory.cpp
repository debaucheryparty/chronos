#include "runtime/memory/guest_memory.h"
#include "test_framework.h"

#include <array>
#include <cstring>

namespace chronos {

TEST_CASE(GuestMemoryMapAndAccess) {
    GuestMemory mem(Arch::Arm64);
    const GuestAddress base = 0x1000'0000;
    const GuestSize size = 0x2000;

    auto res = mem.Map(base, size, MemoryPermission::ReadWrite);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(mem.IsMapped(base, size));

    const std::array<uint8_t, 4> write_data = {0x11, 0x22, 0x33, 0x44};
    auto write_res = mem.Write(base + 0x100, write_data.data(), write_data.size());
    ASSERT_TRUE(write_res.has_value());

    std::array<uint8_t, 4> read_data = {};
    auto read_res = mem.Read(base + 0x100, read_data.data(), read_data.size());
    ASSERT_TRUE(read_res.has_value());
    ASSERT_EQ(read_data[0], 0x11);
    ASSERT_EQ(read_data[1], 0x22);
    ASSERT_EQ(read_data[2], 0x33);
    ASSERT_EQ(read_data[3], 0x44);
}

TEST_CASE(GuestMemoryPermissionEnforcement) {
    GuestMemory mem(Arch::Arm64);
    const GuestAddress base = 0x2000'0000;
    const GuestSize size = 0x1000;

    auto res = mem.Map(base, size, MemoryPermission::Read);
    ASSERT_TRUE(res.has_value());

    const uint32_t val = 0x12345678;
    auto write_res = mem.Write(base, &val, sizeof(val));
    ASSERT_FALSE(write_res.has_value());
    ASSERT_EQ(write_res.error().code, ErrorCode::PermissionDenied);

    auto protect_res = mem.Protect(base, size, MemoryPermission::ReadWrite);
    ASSERT_TRUE(protect_res.has_value());

    write_res = mem.Write(base, &val, sizeof(val));
    ASSERT_TRUE(write_res.has_value());
}

TEST_CASE(GuestMemoryDynamicAllocation) {
    GuestMemory mem(Arch::Arm64);
    const GuestSize size = 0x4000;

    auto res = mem.Allocate(size, MemoryPermission::ReadWrite);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(mem.IsMapped(res.value(), size));
    ASSERT_TRUE(res.value() >= mem.min_address());
    ASSERT_TRUE(res.value() + size <= mem.max_address());
}

TEST_CASE(GuestMemoryUnmap) {
    GuestMemory mem(Arch::Arm32);
    const GuestAddress base = 0x3000'0000;
    const GuestSize size = 0x1000;

    auto map_res = mem.Map(base, size, MemoryPermission::ReadWrite);
    ASSERT_TRUE(map_res.has_value());
    ASSERT_TRUE(mem.IsMapped(base, size));

    auto unmap_res = mem.Unmap(base, size);
    ASSERT_TRUE(unmap_res.has_value());
    ASSERT_FALSE(mem.IsMapped(base, size));
}

} // namespace chronos
