# ===========================================================================
# Tests for Target Online Module
# ===========================================================================

# Find dependencies
find_package(GTest REQUIRED)

# Test: ApiRateLimiter
add_executable(test_api_rate_limiter
    test_api_rate_limiter.cpp
)

target_include_directories(test_api_rate_limiter
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_api_rate_limiter
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
    lithium_atom
)

add_test(NAME ApiRateLimiter COMMAND test_api_rate_limiter)

# Test: VOTable Parser
add_executable(test_votable_parser
    test_votable_parser.cpp
)

target_include_directories(test_votable_parser
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_votable_parser
    PRIVATE
    GTest::GTest
    GTest::Main
    nlohmann_json::nlohmann_json
)

add_test(NAME VotableParser COMMAND test_votable_parser)

# Test: JSON Response Parser
add_executable(test_json_response_parser
    test_json_response_parser.cpp
)

target_include_directories(test_json_response_parser
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_json_response_parser
    PRIVATE
    GTest::GTest
    GTest::Main
    nlohmann_json::nlohmann_json
)

add_test(NAME JsonResponseParser COMMAND test_json_response_parser)

# Test: Result Merger
add_executable(test_result_merger
    test_result_merger.cpp
)

target_include_directories(test_result_merger
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_result_merger
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

add_test(NAME ResultMerger COMMAND test_result_merger)

message(STATUS "Configured Target Online tests: ApiRateLimiter, VotableParser, JsonResponseParser, ResultMerger")
