# CMakeLists.txt for Task Tests

# Find GTest package
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add test executables
add_executable(test_sequence_manager
    test_sequence_manager.cpp
)

# Link against project libraries and testing frameworks
target_link_libraries(test_sequence_manager
    PRIVATE
    lithium_task
    atom
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    ${GMOCK_LIBRARIES}
    spdlog::spdlog
)

# Add tests to ctest
add_test(NAME test_sequence_manager COMMAND test_sequence_manager)

# Include directories
target_include_directories(test_sequence_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
