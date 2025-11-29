# ============================================================================
# CMakeLists.txt for Venv Tests
# ============================================================================

set(VENV_TEST_SOURCES
    test_venv_manager.cpp
)

add_library(lithium_script_venv_tests OBJECT ${VENV_TEST_SOURCES})

target_include_directories(lithium_script_venv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_venv_tests PRIVATE
    GTest::gtest
    lithium_script
)
