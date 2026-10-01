add_executable(chronos_tests
    test_main.cpp
    test_guest_cpu.cpp
    test_guest_memory.cpp
    test_result.cpp
    test_types.cpp
)

target_link_libraries(chronos_tests
    PRIVATE
        chronos_core
)

target_include_directories(chronos_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME chronos_unit_tests COMMAND chronos_tests)
