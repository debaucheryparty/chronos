cmake_minimum_required(VERSION 3.20)
project(chronos
    VERSION 0.1.0
    DESCRIPTION "Independent Linux Android Compatibility Runtime"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Core runtime library
add_library(chronos_core
    src/core/logging.cpp
    src/memory/guest_memory.cpp
)

target_include_directories(chronos_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI executable
add_executable(chronos
    src/main.cpp
)

target_link_libraries(chronos
    PRIVATE
        chronos_core
)

# Testing
enable_testing()
add_subdirectory(tests)
