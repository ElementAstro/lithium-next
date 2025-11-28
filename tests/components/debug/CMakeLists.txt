# Tests for Debug Components (ELF Parser, Core Dump Analyzer, Dynamic Library Parser)
cmake_minimum_required(VERSION 3.20)

project(lithium_debug_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

# Test sources - platform conditional
set(TEST_SOURCES
    test_dump.cpp
    test_dynamic.cpp
)

# ELF parser tests only available on Linux
if(UNIX AND NOT APPLE)
    list(APPEND TEST_SOURCES test_elf.cpp)
endif()

# Create test executable
add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_components
        atom
        loguru
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ELF_TESTS)
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
