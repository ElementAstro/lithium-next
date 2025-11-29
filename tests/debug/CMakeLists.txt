# ============================================================================
# CMakeLists.txt for Lithium Debug Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Debug-Tests
# Description: Unit tests for debug terminal, command checker, and suggestion engine
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_debug_terminal_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

# ============================================================================
# Test Sources
# ============================================================================

set(TEST_SOURCES
    test_check.cpp
    test_suggestion.cpp
    test_terminal.cpp
    test_terminal_components.cpp
    test_runner.cpp
)

# ============================================================================
# Create Test Executable
# ============================================================================

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_debug
        atom
        loguru
        spdlog::spdlog
)

# ============================================================================
# Compiler Settings
# ============================================================================

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# ============================================================================
# Register Tests with CTest
# ============================================================================

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}
    PROPERTIES
        LABELS "debug;terminal"
        TIMEOUT 60
)

# ============================================================================
# Additional Test Targets (Individual Tests)
# ============================================================================

# Terminal component tests (standalone)
add_executable(test_terminal_standalone
    test_terminal_components.cpp
)

target_include_directories(test_terminal_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)

target_link_libraries(test_terminal_standalone
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_debug
        atom
        loguru
        spdlog::spdlog
)

target_compile_features(test_terminal_standalone PRIVATE cxx_std_20)

add_test(NAME test_terminal_standalone COMMAND test_terminal_standalone)
set_tests_properties(test_terminal_standalone PROPERTIES
    LABELS "debug;terminal;standalone"
    TIMEOUT 60
)
