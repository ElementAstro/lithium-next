# ============================================================================
# CMakeLists.txt for Script Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Script-Tests
# Description: Unit tests for Script system components
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_script_tests LANGUAGES CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# ============================================================================
# Add Subdirectories (Modular Test Architecture)
# ============================================================================

# IPC tests
add_subdirectory(ipc)

# Isolated execution tests
add_subdirectory(isolated)

# Shell script tests
add_subdirectory(shell)

# Virtual environment tests
add_subdirectory(venv)

# NumPy/Pandas integration tests
add_subdirectory(numpy)

# Tool registry tests
add_subdirectory(tools)

# ============================================================================
# Core Test Sources
# ============================================================================

set(SCRIPT_TEST_SOURCES
    test_sheller.cpp
    test_sheller_enhanced.cpp
    test_check.cpp
    test_check_enhanced.cpp
    test_tool_registry.cpp
    test_interpreter_pool.cpp
    test_script_export.cpp
    test_script_service.cpp
)

# Create test executable
add_executable(lithium_script_tests
    ${SCRIPT_TEST_SOURCES}
    $<TARGET_OBJECTS:lithium_script_ipc_tests>
    $<TARGET_OBJECTS:lithium_script_isolated_tests>
    $<TARGET_OBJECTS:lithium_script_shell_tests>
    $<TARGET_OBJECTS:lithium_script_venv_tests>
    $<TARGET_OBJECTS:lithium_script_numpy_tests>
    $<TARGET_OBJECTS:lithium_script_tools_tests>
)

target_include_directories(lithium_script_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Python_INCLUDE_DIRS}
)

target_link_libraries(lithium_script_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    lithium_script
    pybind11::embed
    ${Python_LIBRARIES}
)

target_compile_features(lithium_script_tests PRIVATE cxx_std_20)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(lithium_script_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        LABELS "script"
)

# Optional: Create separate test for Python integration
if(Python_FOUND AND pybind11_FOUND)
    add_executable(lithium_python_integration_tests
        test_python_caller.hpp
    )

    target_include_directories(lithium_python_integration_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${Python_INCLUDE_DIRS}
    )

    target_link_libraries(lithium_python_integration_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_script
        pybind11::embed
        ${Python_LIBRARIES}
    )

    target_compile_features(lithium_python_integration_tests PRIVATE cxx_std_20)

    gtest_discover_tests(lithium_python_integration_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            LABELS "python;integration"
    )
endif()
