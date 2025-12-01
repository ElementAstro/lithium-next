# ===========================================================================
# CMakeLists.txt for Lithium-Target Model Tests
# ===========================================================================

set(MODEL_TEST_SOURCES
    test_celestial_object.cpp
    test_star_object.cpp
    test_search_filter.cpp
)

foreach(test_source ${MODEL_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
            lithium_target
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    lithium_set_target_folder(${test_name} "Tests/Target/Model")
endforeach()
