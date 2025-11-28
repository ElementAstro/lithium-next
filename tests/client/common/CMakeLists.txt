# CMakeLists.txt for client common tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_common_tests)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
)

# Test executables
set(TEST_SOURCES
    test_client_base.cpp
    test_solver_client.cpp
    test_guider_client.cpp
)

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME}
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        lithium_client_common
        spdlog::spdlog
        atom-utils
    )

    target_compile_features(${TEST_NAME} PRIVATE cxx_std_20)

    # Add to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        LABELS "client;common"
    )
endforeach()

# Group tests
add_custom_target(client_common_tests
    DEPENDS test_client_base test_solver_client test_guider_client
    COMMENT "Building client common tests"
)
