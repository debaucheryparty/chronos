#pragma once

#include <cassert>
#include <optional>
#include <utility>
#include <variant>

#include "runtime/error.h"

namespace chronos {

template <typename T>
class Result {
public:
    Result(const T& val)
        : data_(val) {}

    Result(T&& val)
        : data_(std::move(val)) {}

    Result(Error err)
        : data_(std::move(err)) {}

    Result(ErrorCode code)
        : data_(Error(code)) {}

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] T& value() & {
        assert(has_value());
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const& {
        assert(has_value());
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        assert(has_value());
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] T& operator*() & {
        return value();
    }

    [[nodiscard]] const T& operator*() const& {
        return value();
    }

    [[nodiscard]] T* operator->() {
        return &value();
    }

    [[nodiscard]] const T* operator->() const {
        return &value();
    }

    [[nodiscard]] const Error& error() const& {
        assert(!has_value());
        return std::get<Error>(data_);
    }

    [[nodiscard]] Error&& error() && {
        assert(!has_value());
        return std::get<Error>(std::move(data_));
    }

private:
    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    Result() noexcept = default;

    Result(Error err)
        : error_(std::move(err)) {}

    Result(ErrorCode code)
        : error_(Error(code)) {}

    [[nodiscard]] bool has_value() const noexcept {
        return !error_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] const Error& error() const& {
        assert(!has_value());
        return *error_;
    }

    [[nodiscard]] Error&& error() && {
        assert(!has_value());
        return *std::move(error_);
    }

private:
    std::optional<Error> error_;
};

} // namespace chronos
