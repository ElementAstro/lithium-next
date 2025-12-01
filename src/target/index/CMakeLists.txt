# ===========================================================================
# CMakeLists.txt for Lithium-Target Index
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Index
# Description: Indexing and caching mechanisms for target data
# ===========================================================================

set(INDEX_SOURCES
    # Trie-based autocomplete index
    trie_index.cpp

    # R-tree spatial indexing for celestial coordinates
    spatial_index.cpp

    # BK-tree fuzzy matching
    fuzzy_matcher.cpp
)

set(INDEX_HEADERS
    # Trie index header
    trie_index.hpp

    # Spatial index header
    spatial_index.hpp

    # Fuzzy matcher header
    fuzzy_matcher.hpp

    # Module index header
    index.hpp
)

# Create object library for index module
add_library(lithium_target_index_obj OBJECT ${INDEX_SOURCES})

target_include_directories(lithium_target_index_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
)

# Link dependencies
target_link_libraries(lithium_target_index_obj
    PUBLIC
    spdlog::spdlog
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_index_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_index_obj "Core/Target/Index")

# Expose object library for parent
set(LITHIUM_TARGET_INDEX_OBJECTS $<TARGET_OBJECTS:lithium_target_index_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_INDEX_HEADERS ${INDEX_HEADERS} PARENT_SCOPE)

message(STATUS "Configured Target Index module with Trie, Spatial, and Fuzzy indices")
