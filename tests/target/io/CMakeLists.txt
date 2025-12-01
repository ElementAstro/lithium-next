# ===========================================================================
# CMakeLists.txt for Lithium Target IO Tests
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Test suite for target IO module
# ===========================================================================

# CSV Handler Tests
add_executable(test_csv_handler test_csv_handler.cpp)
target_link_libraries(test_csv_handler
    PRIVATE
        lithium_target
        GTest::gtest_main
        ${CMAKE_THREAD_LIBS_INIT}
)

# JSON Handler Tests
add_executable(test_json_handler test_json_handler.cpp)
target_link_libraries(test_json_handler
    PRIVATE
        lithium_target
        GTest::gtest_main
        ${CMAKE_THREAD_LIBS_INIT}
)

# Integration Tests
add_executable(test_io_integration test_integration.cpp)
target_link_libraries(test_io_integration
    PRIVATE
        lithium_target
        GTest::gtest_main
        ${CMAKE_THREAD_LIBS_INIT}
)

# Register tests
gtest_discover_tests(test_csv_handler)
gtest_discover_tests(test_json_handler)
gtest_discover_tests(test_io_integration)

# Set IDE folders
lithium_set_target_folder(test_csv_handler "Tests/Target/IO")
lithium_set_target_folder(test_json_handler "Tests/Target/IO")
lithium_set_target_folder(test_io_integration "Tests/Target/IO")
