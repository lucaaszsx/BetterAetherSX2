cmake_minimum_required(VERSION 3.22)

project(bax_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BAX_BUILD_TESTS "Build the host unit tests" ON)

configure_file(src/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp @ONLY)

add_library(bax_core ${CMAKE_CURRENT_BINARY_DIR}/version.cpp src/touch_layout.cpp src/jit/code_buffer.cpp src/jit/a64_branch.cpp)
target_include_directories(bax_core PUBLIC include)
target_compile_options(bax_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BAX_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(bax_core_tests tests/version_test.cpp tests/touch_layout_test.cpp tests/code_buffer_test.cpp tests/a64_branch_test.cpp)
    target_link_libraries(bax_core_tests PRIVATE bax_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(bax_core_tests)
endif()
