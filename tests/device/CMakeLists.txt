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

# Create aggregate test target
add_custom_target(device_tests
    DEPENDS test_indi_adapter test_ascom_adapter test_phd2_client test_guider_service
    COMMENT "Building all device tests"
)
