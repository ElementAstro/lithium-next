cmake_minimum_required(VERSION 3.20)

project(lithium_config_test LANGUAGES CXX)

# Collect all test source files recursively
file(GLOB_RECURSE TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/test_*.cpp
    ${PROJECT_SOURCE_DIR}/core/test_*.cpp
    ${PROJECT_SOURCE_DIR}/components/test_*.cpp
    ${PROJECT_SOURCE_DIR}/utils/test_*.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

target_link_libraries(${PROJECT_NAME}
    gtest
    gtest_main
    lithium_config
    loguru
    spdlog::spdlog
)
