# CMakeLists.txt for PHD2 client tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_phd2_tests)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/client/phd2
)

# Test executables
add_executable(test_phd2_profile test_profile.cpp)
add_executable(test_phd2_client test_phd2_client.cpp)

# test_phd2_profile
target_link_libraries(test_phd2_profile
    GTest::gtest
    GTest::gtest_main
    spdlog::spdlog
)

target_compile_features(test_phd2_profile PRIVATE cxx_std_20)

add_test(NAME test_phd2_profile COMMAND test_phd2_profile)

set_tests_properties(test_phd2_profile PROPERTIES
    TIMEOUT 60
    LABELS "client;phd2;guider"
)

# test_phd2_client
target_link_libraries(test_phd2_client
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    lithium_client_common
    lithium_phd2_client
    spdlog::spdlog
)

target_compile_features(test_phd2_client PRIVATE cxx_std_20)

add_test(NAME test_phd2_client COMMAND test_phd2_client)

set_tests_properties(test_phd2_client PROPERTIES
    TIMEOUT 120
    LABELS "client;phd2;guider"
)
