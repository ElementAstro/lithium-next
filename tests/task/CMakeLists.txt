# ============================================================================
# CMakeLists.txt for Task Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Task-Tests
# Description: Unit tests for Task system components
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_task_tests LANGUAGES CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Test sources
set(TASK_TEST_SOURCES
    test_task.cpp
    test_script_task.cpp
    test_enhanced_system.cpp
    test_generator.cpp
    test_imagepath.cpp
    test_target.cpp
)

# Create test executable
add_executable(lithium_task_tests ${TASK_TEST_SOURCES})

target_include_directories(lithium_task_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Python_INCLUDE_DIRS}
)

target_link_libraries(lithium_task_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    lithium_task
    lithium_script
    pybind11::embed
    ${Python_LIBRARIES}
)

target_compile_features(lithium_task_tests PRIVATE cxx_std_20)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(lithium_task_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        LABELS "task"
)
