# ===========================================================================
# CMakeLists.txt for Lithium-Target Search Tests
# ===========================================================================

set(SEARCH_TEST_SOURCES
    test_search_engine.cpp
    test_coordinate_searcher.cpp
    test_filter_evaluator.cpp
)

foreach(test_source ${SEARCH_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
            lithium_target
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    lithium_set_target_folder(${test_name} "Tests/Target/Search")
endforeach()
