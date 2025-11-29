# ============================================================================
# CMakeLists.txt for Isolated Tests
# ============================================================================

set(ISOLATED_TEST_SOURCES
    test_runner.cpp
    test_execution_engine.cpp
    test_lifecycle.cpp
    test_resource_monitor.cpp
)

add_library(lithium_script_isolated_tests OBJECT ${ISOLATED_TEST_SOURCES})

target_include_directories(lithium_script_isolated_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_isolated_tests PRIVATE
    GTest::gtest
    lithium_script
)
