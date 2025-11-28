# CMakeLists.txt for ASTAP client tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_astap_tests)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
)

# Test executable
add_executable(test_astap_client test_astap_client.cpp)

target_link_libraries(test_astap_client
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    lithium_client_common
    spdlog::spdlog
    atom-utils
    atom-io
    atom-system
)

target_compile_features(test_astap_client PRIVATE cxx_std_20)

# Add to CTest
add_test(NAME test_astap_client COMMAND test_astap_client)

set_tests_properties(test_astap_client PROPERTIES
    TIMEOUT 120
    LABELS "client;astap;solver"
)
