# ===========================================================================
# Tests for Target Observability Module
# ===========================================================================

# Find dependencies
find_package(GTest REQUIRED)

# Test: VisibilityCalculator
add_executable(test_visibility_calculator
    test_visibility_calculator.cpp
)

target_include_directories(test_visibility_calculator
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_visibility_calculator
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_compile_features(test_visibility_calculator PRIVATE cxx_std_23)

add_test(NAME VisibilityCalculator COMMAND test_visibility_calculator)

# Test: TimeWindowFilter
add_executable(test_time_window_filter
    test_time_window_filter.cpp
)

target_include_directories(test_time_window_filter
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_time_window_filter
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_compile_features(test_time_window_filter PRIVATE cxx_std_23)

add_test(NAME TimeWindowFilter COMMAND test_time_window_filter)

message(STATUS "Configured Target Observability tests: VisibilityCalculator, TimeWindowFilter")
