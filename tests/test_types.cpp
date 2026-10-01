#include "runtime/error.h"
#include "runtime/types.h"
#include "test_framework.h"

namespace chronos {

TEST_CASE(ArchStringRepresentation) {
    ASSERT_EQ(ArchToString(Arch::Arm32), "armeabi-v7a");
    ASSERT_EQ(ArchToString(Arch::Arm64), "arm64-v8a");
}

TEST_CASE(ErrorCodeStringRepresentation) {
    ASSERT_EQ(ErrorCodeToString(ErrorCode::Success), "Success");
    ASSERT_EQ(ErrorCodeToString(ErrorCode::InvalidPackage), "InvalidPackage");
    ASSERT_EQ(ErrorCodeToString(ErrorCode::UnsupportedAbi), "UnsupportedAbi");
    ASSERT_EQ(ErrorCodeToString(ErrorCode::ElfLoadFailed), "ElfLoadFailed");
}

TEST_CASE(GuestAddressBasic) {
    GuestAddress addr = 0x1000'0000ULL;
    GuestSize size = 0x1000;
    ASSERT_EQ(addr + size, 0x1000'1000ULL);
}

} // namespace chronos
