# ============================================================================
# CMakeLists.txt for Shell Tests
# ============================================================================

set(SHELL_TEST_SOURCES
    test_hooks.cpp
    test_versioning.cpp
    test_metadata.cpp
    test_script_manager.cpp
)

add_library(lithium_script_shell_tests OBJECT ${SHELL_TEST_SOURCES})

target_include_directories(lithium_script_shell_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_shell_tests PRIVATE
    GTest::gtest
    lithium_script
)
