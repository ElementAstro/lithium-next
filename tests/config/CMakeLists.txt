cmake_minimum_required(VERSION 3.20)

project(lithium_config_test LANGUAGES CXX)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_*.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main lithium_config spdlog::spdlog)
