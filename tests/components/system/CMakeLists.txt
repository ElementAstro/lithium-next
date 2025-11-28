# Tests for System Dependency Management
cmake_minimum_required(VERSION 3.20)

project(lithium_system_dependency_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_dependency_manager.cpp
    test_platform_detector.cpp
    test_package_manager_registry.cpp
)

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
        lithium_system_dependency
        lithium_components
        atom
        loguru
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
