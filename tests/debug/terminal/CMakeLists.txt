# ============================================================================
# CMakeLists.txt for Lithium Debug Terminal Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Debug-Terminal-Tests
# Description: Unit tests for terminal components
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_debug_terminal_component_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

# ============================================================================
# Test Sources
# ============================================================================

set(TERMINAL_TEST_SOURCES
    test_types.cpp
    test_renderer.cpp
    test_command_executor.cpp
    test_history_manager.cpp
    test_input_controller.cpp
    test_tui_manager.cpp
)

# ============================================================================
# Create Test Executable
# ============================================================================

add_executable(${PROJECT_NAME} ${TERMINAL_TEST_SOURCES})

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
        LABELS "debug;terminal;components"
        TIMEOUT 60
)

# ============================================================================
# Individual Test Targets
# ============================================================================

# Types tests
add_executable(test_terminal_types test_types.cpp)
target_include_directories(test_terminal_types PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_types PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_types PRIVATE cxx_std_20)
add_test(NAME test_terminal_types COMMAND test_terminal_types)
set_tests_properties(test_terminal_types PROPERTIES LABELS "debug;terminal;types")

# Renderer tests
add_executable(test_terminal_renderer test_renderer.cpp)
target_include_directories(test_terminal_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_renderer PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_renderer PRIVATE cxx_std_20)
add_test(NAME test_terminal_renderer COMMAND test_terminal_renderer)
set_tests_properties(test_terminal_renderer PROPERTIES LABELS "debug;terminal;renderer")

# Command executor tests
add_executable(test_terminal_command_executor test_command_executor.cpp)
target_include_directories(test_terminal_command_executor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_command_executor PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_command_executor PRIVATE cxx_std_20)
add_test(NAME test_terminal_command_executor COMMAND test_terminal_command_executor)
set_tests_properties(test_terminal_command_executor PROPERTIES LABELS "debug;terminal;executor")

# History manager tests
add_executable(test_terminal_history_manager test_history_manager.cpp)
target_include_directories(test_terminal_history_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_history_manager PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_history_manager PRIVATE cxx_std_20)
add_test(NAME test_terminal_history_manager COMMAND test_terminal_history_manager)
set_tests_properties(test_terminal_history_manager PROPERTIES LABELS "debug;terminal;history")

# Input controller tests
add_executable(test_terminal_input_controller test_input_controller.cpp)
target_include_directories(test_terminal_input_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_input_controller PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_input_controller PRIVATE cxx_std_20)
add_test(NAME test_terminal_input_controller COMMAND test_terminal_input_controller)
set_tests_properties(test_terminal_input_controller PROPERTIES LABELS "debug;terminal;input")

# TUI manager tests
add_executable(test_terminal_tui_manager test_tui_manager.cpp)
target_include_directories(test_terminal_tui_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty
)
target_link_libraries(test_terminal_tui_manager PRIVATE
    GTest::gtest GTest::gtest_main lithium_debug atom loguru spdlog::spdlog
)
target_compile_features(test_terminal_tui_manager PRIVATE cxx_std_20)
add_test(NAME test_terminal_tui_manager COMMAND test_terminal_tui_manager)
set_tests_properties(test_terminal_tui_manager PROPERTIES LABELS "debug;terminal;tui")
