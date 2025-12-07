# CMakeLists.txt for solver plugin system tests
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

cmake_minimum_required(VERSION 3.20)

# Find GTest
find_package(GTest REQUIRED)

# Test sources
set(SOLVER_TEST_SOURCES
    test_solver_type_registry.cpp
    test_solver_factory.cpp
    test_solver_plugin_interface.cpp
)

# Create test executable
add_executable(test_solver_plugin_system ${SOLVER_TEST_SOURCES})

# Include directories
target_include_directories(test_solver_plugin_system
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(test_solver_plugin_system
    PRIVATE
        lithium_solver_core
        lithium_client_common
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# Enable C++20
target_compile_features(test_solver_plugin_system PRIVATE cxx_std_20)

# Add to CTest
include(GoogleTest)
gtest_discover_tests(test_solver_plugin_system
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "solver"
)

# Install test executable
install(TARGETS test_solver_plugin_system
    RUNTIME DESTINATION bin/tests
)
