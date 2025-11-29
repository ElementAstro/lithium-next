# ============================================================================
# CMakeLists.txt for Tools Tests
# ============================================================================

set(TOOLS_TEST_SOURCES
    test_tool_info.cpp
)

add_library(lithium_script_tools_tests OBJECT ${TOOLS_TEST_SOURCES})

target_include_directories(lithium_script_tools_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_tools_tests PRIVATE
    GTest::gtest
    lithium_script
)
