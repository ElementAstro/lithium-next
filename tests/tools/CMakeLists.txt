# Tests for lithium_tools module

set(TOOLS_TEST_SOURCES
    test_astronomy_constants.cpp
)

foreach(test_source ${TOOLS_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
            lithium_tools
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
