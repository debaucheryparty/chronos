#include "runtime/error.h"
#include "runtime/result.h"
#include "test_framework.h"

namespace chronos {

namespace {

Result<int> Divide(int a, int b) {
    if (b == 0) {
        return Error(ErrorCode::InvalidArgument, "division by zero");
    }
    return a / b;
}

Result<void> CheckPositive(int val) {
    if (val <= 0) {
        return Error(ErrorCode::InvalidArgument, "value must be positive");
    }
    return {};
}

} // namespace

TEST_CASE(ResultSuccessWithValue) {
    auto res = Divide(10, 2);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(static_cast<bool>(res));
    ASSERT_EQ(res.value(), 5);
    ASSERT_EQ(*res, 5);
}

TEST_CASE(ResultFailureWithError) {
    auto res = Divide(10, 0);
    ASSERT_FALSE(res.has_value());
    ASSERT_FALSE(static_cast<bool>(res));
    ASSERT_EQ(res.error().code, ErrorCode::InvalidArgument);
    ASSERT_EQ(res.error().message, "division by zero");
}

TEST_CASE(VoidResultSuccess) {
    auto res = CheckPositive(42);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(static_cast<bool>(res));
}

TEST_CASE(VoidResultFailure) {
    auto res = CheckPositive(-5);
    ASSERT_FALSE(res.has_value());
    ASSERT_FALSE(static_cast<bool>(res));
    ASSERT_EQ(res.error().code, ErrorCode::InvalidArgument);
}

} // namespace chronos
