#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace chronos {

using GuestAddress = uint64_t;
using GuestAddress32 = uint32_t;
using GuestAddress64 = uint64_t;
using GuestSize = uint64_t;

enum class Arch {
    Arm32,
    Arm64,
};

constexpr std::string_view ArchToString(Arch arch) noexcept {
    switch (arch) {
    case Arch::Arm32:
        return "armeabi-v7a";
    case Arch::Arm64:
        return "arm64-v8a";
    }
    return "unknown";
}

} // namespace chronos
