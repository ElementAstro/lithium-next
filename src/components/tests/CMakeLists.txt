# CMakeLists.txt for Component Tests

find_package(GTest QUIET)
if(GTest_FOUND)
    enable_testing()

    # Component Manager Tests
    add_executable(test_component_manager
        test_component_manager.cpp
    )

    target_link_libraries(test_component_manager PRIVATE
        lithium::components::manager
        lithium::components
        GTest::gtest
        GTest::gtest_main
        spdlog::spdlog
    )

    target_compile_features(test_component_manager PRIVATE cxx_std_20)

    set_target_properties(test_component_manager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Component Loader Tests
    add_executable(test_component_loader
        test_component_loader.cpp
    )

    target_link_libraries(test_component_loader PRIVATE
        lithium::components
        GTest::gtest
        GTest::gtest_main
        spdlog::spdlog
    )

    target_compile_features(test_component_loader PRIVATE cxx_std_20)

    set_target_properties(test_component_loader PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Component Dependency Tests
    add_executable(test_component_dependency
        test_component_dependency.cpp
    )

    target_link_libraries(test_component_dependency PRIVATE
        lithium::components
        GTest::gtest
        GTest::gtest_main
        spdlog::spdlog
    )

    target_compile_features(test_component_dependency PRIVATE cxx_std_20)

    set_target_properties(test_component_dependency PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Integration Tests
    add_executable(test_component_integration
        test_component_integration.cpp
    )

    target_link_libraries(test_component_integration PRIVATE
        lithium::components
        lithium::debug
        GTest::gtest
        GTest::gtest_main
        spdlog::spdlog
    )

    target_compile_features(test_component_integration PRIVATE cxx_std_20)

    set_target_properties(test_component_integration PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    # Discover and register tests
    include(GoogleTest)
    gtest_discover_tests(test_component_manager)
    gtest_discover_tests(test_component_loader)
    gtest_discover_tests(test_component_dependency)
    gtest_discover_tests(test_component_integration)

    # Add custom test targets
    add_custom_target(run_component_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS
            test_component_manager
            test_component_loader
            test_component_dependency
            test_component_integration
        COMMENT "Running all component tests"
    )

else()
    message(WARNING "GTest not found. Component tests will not be built.")
endif()
