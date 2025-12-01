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
project(lithium_debug_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

# ============================================================================
# Add subdirectory for terminal component tests
# ============================================================================

add_subdirectory(terminal)

# ============================================================================
# Test Sources - Core Debug Module Tests
# ============================================================================

set(TEST_SOURCES
    test_check.cpp
    test_check_comprehensive.cpp
    test_suggestion.cpp
    test_suggestion_comprehensive.cpp
    test_terminal.cpp
    test_terminal_comprehensive.cpp
    test_terminal_components.cpp
    test_runner.cpp
    unified_tests.cpp
)

# ============================================================================
# Create Main Test Executable
# ============================================================================

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
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
        LABELS "debug"
        TIMEOUT 120
)

# ============================================================================
# Individual Test Targets
# ============================================================================

# CommandChecker comprehensive tests
add_executable(test_check_comprehensive test_check_comprehensive.cpp)
target_include_directories(test_check_comprehensive PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_check_comprehensive PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_check_comprehensive PRIVATE cxx_std_20)
add_test(NAME test_check_comprehensive COMMAND test_check_comprehensive)
set_tests_properties(test_check_comprehensive PROPERTIES
    LABELS "debug;check;comprehensive"
    TIMEOUT 60
)

# SuggestionEngine comprehensive tests
add_executable(test_suggestion_comprehensive test_suggestion_comprehensive.cpp)
target_include_directories(test_suggestion_comprehensive PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_suggestion_comprehensive PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_suggestion_comprehensive PRIVATE cxx_std_20)
add_test(NAME test_suggestion_comprehensive COMMAND test_suggestion_comprehensive)
set_tests_properties(test_suggestion_comprehensive PROPERTIES
    LABELS "debug;suggestion;comprehensive"
    TIMEOUT 60
)

# ConsoleTerminal comprehensive tests
add_executable(test_terminal_comprehensive_standalone test_terminal_comprehensive.cpp)
target_include_directories(test_terminal_comprehensive_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_comprehensive_standalone PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_comprehensive_standalone PRIVATE cxx_std_20)
add_test(NAME test_terminal_comprehensive_standalone COMMAND test_terminal_comprehensive_standalone)
set_tests_properties(test_terminal_comprehensive_standalone PROPERTIES
    LABELS "debug;terminal;comprehensive"
    TIMEOUT 60
)

# Terminal component tests (standalone)
add_executable(test_terminal_standalone test_terminal_components.cpp)
target_include_directories(test_terminal_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_standalone PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_standalone PRIVATE cxx_std_20)
add_test(NAME test_terminal_standalone COMMAND test_terminal_standalone)
set_tests_properties(test_terminal_standalone PROPERTIES
    LABELS "debug;terminal;standalone"
    TIMEOUT 60
)

# Unified tests (integration and performance)
add_executable(test_unified unified_tests.cpp test_runner.cpp)
target_include_directories(test_unified PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_unified PRIVATE
    GTest::gtest GTest::gtest_main GTest::gmock lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_unified PRIVATE cxx_std_20)
add_test(NAME test_unified COMMAND test_unified)
set_tests_properties(test_unified PROPERTIES
    LABELS "debug;unified;integration"
    TIMEOUT 120
)
