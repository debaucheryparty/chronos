#pragma once

#include <string>
#include <string_view>

namespace chronos {

enum class ErrorCode {
    Success = 0,
    InvalidPackage,
    UnsupportedAbi,
    ElfLoadFailed,
    SymbolNotFound,
    JniError,
    GraphicsInitFailed,
    PermissionDenied,
    IoError,
    OutOfMemory,
    InvalidAddress,
    NotImplemented,
    InvalidArgument,
};

constexpr std::string_view ErrorCodeToString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Success:
        return "Success";
    case ErrorCode::InvalidPackage:
        return "InvalidPackage";
    case ErrorCode::UnsupportedAbi:
        return "UnsupportedAbi";
    case ErrorCode::ElfLoadFailed:
        return "ElfLoadFailed";
    case ErrorCode::SymbolNotFound:
        return "SymbolNotFound";
    case ErrorCode::JniError:
        return "JniError";
    case ErrorCode::GraphicsInitFailed:
        return "GraphicsInitFailed";
    case ErrorCode::PermissionDenied:
        return "PermissionDenied";
    case ErrorCode::IoError:
        return "IoError";
    case ErrorCode::OutOfMemory:
        return "OutOfMemory";
    case ErrorCode::InvalidAddress:
        return "InvalidAddress";
    case ErrorCode::NotImplemented:
        return "NotImplemented";
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    }
    return "UnknownError";
}

struct Error {
    ErrorCode code = ErrorCode::Success;
    std::string message;

    constexpr Error() noexcept = default;

    constexpr explicit Error(ErrorCode c) noexcept
        : code(c) {}

    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}

    constexpr bool IsSuccess() const noexcept {
        return code == ErrorCode::Success;
    }

    constexpr explicit operator bool() const noexcept {
        return code != ErrorCode::Success;
    }

    bool operator==(const Error& other) const noexcept {
        return code == other.code && message == other.message;
    }

    bool operator==(ErrorCode other_code) const noexcept {
        return code == other_code;
    }
};

} // namespace chronos
