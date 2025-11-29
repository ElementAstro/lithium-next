# SPDX-License-Identifier: GPL-3.0-or-later
# CMakeLists.txt for database module tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_database_tests)

# Find required packages
find_package(GTest REQUIRED)
find_package(SQLite3 REQUIRED)

# Define helper macro for adding database tests
macro(add_database_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME}
        PRIVATE
            lithium_database
            GTest::gtest
            GTest::gtest_main
            SQLite3::SQLite3
    )

    target_include_directories(${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/libs
    )

    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
    )
endmacro()

# Add individual test files
add_database_test(test_database_core test_database_core.cpp)
add_database_test(test_statement test_statement.cpp)
add_database_test(test_transaction test_transaction.cpp)
add_database_test(test_query_builder test_query_builder.cpp)
add_database_test(test_cache_manager test_cache_manager.cpp)

# Create aggregate test target
add_custom_target(database_tests
    DEPENDS
        test_database_core
        test_statement
        test_transaction
        test_query_builder
        test_cache_manager
    COMMENT "Building all database module tests"
)

# Optional: Create a target to run all database tests
add_custom_target(run_database_tests
    COMMAND ctest -V --output-on-failure -R "test_database|test_statement|test_transaction|test_query_builder|test_cache_manager"
    DEPENDS database_tests
    COMMENT "Running all database module tests"
)
