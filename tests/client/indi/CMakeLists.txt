# ============================================================================
# CMakeLists.txt for lithium.client.indi tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium INDI Device Client Tests
# Description: Tests for INDI device implementations
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi_tests)

# ============================================================================
# Test Source Files
# ============================================================================

set(TEST_SOURCES
    test_indi_device_base.cpp
    test_indi_camera.cpp
    test_indi_focuser.cpp
    test_indi_filterwheel.cpp
    test_indi_telescope.cpp
    test_indi_rotator.cpp
    test_indi_dome.cpp
    test_indi_weather.cpp
    test_indi_gps.cpp
    test_indi_device_factory.cpp
)

# ============================================================================
# Dependencies
# ============================================================================

find_package(GTest REQUIRED)

set(TEST_LIBRARIES
    lithium_client_indi
    GTest::gtest
    GTest::gtest_main
)

# ============================================================================
# Build Test Executable
# ============================================================================

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${TEST_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Tests/Client")

# ============================================================================
# Register Tests with CTest
# ============================================================================

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "client;indi"
)

message(STATUS "  ├─ Test: lithium_client_indi_tests configured")
