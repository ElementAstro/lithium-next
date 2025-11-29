# ============================================================================
# CMakeLists.txt for NumPy Tests
# ============================================================================

set(NUMPY_TEST_SOURCES
    test_array_ops.cpp
)

add_library(lithium_script_numpy_tests OBJECT ${NUMPY_TEST_SOURCES})

target_include_directories(lithium_script_numpy_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Python_INCLUDE_DIRS}
)

target_link_libraries(lithium_script_numpy_tests PRIVATE
    GTest::gtest
    lithium_script
    pybind11::embed
    ${Python_LIBRARIES}
)
