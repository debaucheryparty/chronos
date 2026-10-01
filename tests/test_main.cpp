#include "test_framework.h"

#include <iostream>

int main() {
    auto& tests = chronos::testing::GetTestRegistry();
    std::cout << "[----------] Running " << tests.size() << " tests.\n";

    size_t passed = 0;
    for (const auto& test : tests) {
        std::cout << "[ RUN      ] " << test.name << '\n';
        test.func();
        std::cout << "[       OK ] " << test.name << '\n';
        ++passed;
    }

    std::cout << "[----------] " << passed << " tests passed.\n";
    std::cout << "[  PASSED  ] " << passed << " tests.\n";
    return 0;
}
