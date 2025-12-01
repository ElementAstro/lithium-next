# ===========================================================================
# CMakeLists.txt for Lithium-Target Repository Tests
# ===========================================================================

set(REPOSITORY_TEST_SOURCES
    test_celestial_repository.cpp
    test_cached_repository.cpp
    test_memory_repository.cpp
)

foreach(test_source ${REPOSITORY_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
            lithium_target
            lithium_database
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    lithium_set_target_folder(${test_name} "Tests/Target/Repository")
endforeach()
