# ===========================================================================
# CMakeLists.txt for Lithium-Target Recommendation Tests
# ===========================================================================

set(RECOMMENDATION_TEST_SOURCES
    test_collaborative_filter.cpp
    test_content_filter.cpp
    test_hybrid_recommender.cpp
    test_recommendation_engine.cpp
)

foreach(test_source ${RECOMMENDATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
            lithium_target
            Eigen3::Eigen
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    lithium_set_target_folder(${test_name} "Tests/Target/Recommendation")
endforeach()
