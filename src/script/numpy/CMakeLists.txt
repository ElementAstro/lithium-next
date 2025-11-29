# CMakeLists.txt for numpy module
# Part of Lithium-Next Script System

set(NUMPY_SOURCES
    array_ops.cpp
    dataframe_ops.cpp
    astro_ops.cpp
)

set(NUMPY_HEADERS
    types.hpp
    array_view.hpp
    array_ops.hpp
    dataframe_ops.hpp
    astro_ops.hpp
    numpy_utils.hpp
)

# Create library
add_library(lithium_script_numpy STATIC ${NUMPY_SOURCES} ${NUMPY_HEADERS})

target_include_directories(lithium_script_numpy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_numpy
    PUBLIC
        pybind11::pybind11
        spdlog::spdlog
)

# C++20 required
target_compile_features(lithium_script_numpy PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_script_numpy PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Export headers for IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${NUMPY_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${NUMPY_SOURCES})
