#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace chronos::testing {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& GetTestRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline void RegisterTest(std::string name, std::function<void()> func) {
    GetTestRegistry().push_back({std::move(name), std::move(func)});
}

struct TestAutoRegister {
    TestAutoRegister(std::string name, std::function<void()> func) {
        RegisterTest(std::move(name), std::move(func));
    }
};

#define TEST_CASE(name) \
    static void Test_##name(); \
    static const ::chronos::testing::TestAutoRegister reg_##name(#name, Test_##name); \
    static void Test_##name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << '\n'; \
            std::exit(EXIT_FAILURE); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

} // namespace chronos::testing
