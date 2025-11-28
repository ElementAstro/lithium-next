# Module Loader Tests
cmake_minimum_required(VERSION 3.20)

project(lithium_loader_tests LANGUAGES CXX)

# Add test module subdirectory
add_subdirectory(test_module)

# Find GTest
find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_module_loader.cpp
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

# Add dependency on test module for runtime
add_dependencies(${PROJECT_NAME} test_module)

# Copy test module to test output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:test_module>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
