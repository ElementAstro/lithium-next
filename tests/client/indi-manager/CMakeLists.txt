# ============================================================================
# CMakeLists.txt for INDI Manager Client Tests
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi_manager_tests)

# Find GTest
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_manager_client.cpp
)

# Create test executable
add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lithium_client_indi_manager
    lithium_client_common
    GTest::gtest
    GTest::gmock
)

# Register tests
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

message(STATUS "  ├─ Test: lithium_client_indi_manager_tests configured")
