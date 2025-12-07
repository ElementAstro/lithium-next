# ============================================================================
# CMakeLists.txt for Device Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_device_tests)

# Find required packages
find_package(GTest REQUIRED)

# Add test subdirectories
add_subdirectory(service)
add_subdirectory(phd2)

# Device Manager Tests
add_executable(test_device_manager
    test_device_manager.cpp
)

target_link_libraries(test_device_manager
    PRIVATE
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        lithium_device
        lithium_device_service
        lithium.client.indi
        lithium.client.ascom
        atom-error
        atom-utils
        spdlog::spdlog
)

target_include_directories(test_device_manager
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

add_test(NAME test_device_manager COMMAND test_device_manager)

# Create aggregate test target
add_custom_target(device_tests
    DEPENDS test_indi_adapter test_ascom_adapter test_phd2_client test_guider_service test_device_manager test_device_backend test_base_service test_device_types
    COMMENT "Building all device tests"
)
