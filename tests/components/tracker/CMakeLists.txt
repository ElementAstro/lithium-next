# File Tracker Tests
cmake_minimum_required(VERSION 3.20)

project(lithium_tracker_tests LANGUAGES CXX)

# Find GTest
find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_file_tracker.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

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

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
