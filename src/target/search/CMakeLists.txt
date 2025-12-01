# ===========================================================================
# CMakeLists.txt for Lithium-Target Search
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Search
# Description: Search engine and query mechanisms for celestial targets
# ===========================================================================

set(SEARCH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../reader.cpp
    search_engine.cpp
    coordinate_searcher.cpp
    filter_evaluator.cpp
)

set(SEARCH_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../reader.hpp
    search_engine.hpp
    coordinate_searcher.hpp
    filter_evaluator.hpp
)

# Create object library for search module
add_library(lithium_target_search_obj OBJECT ${SEARCH_SOURCES})

target_include_directories(lithium_target_search_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Search depends on model, index, and repository
target_link_libraries(lithium_target_search_obj
    PUBLIC
        atom
        atom-utils
        lithium_config
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_search_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_search_obj "Core/Target/Search")

# Expose object library for parent
set(LITHIUM_TARGET_SEARCH_OBJECTS $<TARGET_OBJECTS:lithium_target_search_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_SEARCH_HEADERS ${SEARCH_HEADERS} PARENT_SCOPE)
