# ===========================================================================
# Tests for Target Index Module
# ===========================================================================

# Find dependencies
find_package(GTest REQUIRED)

# Test: TrieIndex
add_executable(test_trie_index
    test_trie_index.cpp
)

target_include_directories(test_trie_index
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_trie_index
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
)

add_test(NAME TrieIndex COMMAND test_trie_index)

# Test: SpatialIndex
add_executable(test_spatial_index
    test_spatial_index.cpp
)

target_include_directories(test_spatial_index
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_spatial_index
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
)

add_test(NAME SpatialIndex COMMAND test_spatial_index)

# Test: FuzzyMatcher
add_executable(test_fuzzy_matcher
    test_fuzzy_matcher.cpp
)

target_include_directories(test_fuzzy_matcher
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(test_fuzzy_matcher
    PRIVATE
    GTest::GTest
    GTest::Main
    spdlog::spdlog
)

add_test(NAME FuzzyMatcher COMMAND test_fuzzy_matcher)

message(STATUS "Configured Target Index tests: TrieIndex, SpatialIndex, FuzzyMatcher")
