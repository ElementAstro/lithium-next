# CMakeLists.txt for Astrometry client tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_astrometry_tests)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
)

# Test executable
add_executable(test_astrometry_client test_astrometry_client.cpp)

target_link_libraries(test_astrometry_client
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    lithium_client_common
    spdlog::spdlog
    atom-utils
    atom-io
    atom-system
)

target_compile_features(test_astrometry_client PRIVATE cxx_std_20)

# Add to CTest
add_test(NAME test_astrometry_client COMMAND test_astrometry_client)

set_tests_properties(test_astrometry_client PROPERTIES
    TIMEOUT 120
    LABELS "client;astrometry;solver"
)
