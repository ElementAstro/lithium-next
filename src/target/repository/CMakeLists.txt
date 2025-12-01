# ===========================================================================
# CMakeLists.txt for Lithium-Target Repository
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Repository
# Description: Data access and persistence layer for celestial targets
# ===========================================================================

set(REPOSITORY_SOURCES
    celestial_repository.cpp
    repository_interface.cpp
    repository_factory.cpp
    sqlite_repository.cpp
    memory_repository.cpp
    cached_repository.cpp
)

set(REPOSITORY_HEADERS
    celestial_repository.hpp
    repository_interface.hpp
    sqlite_repository.hpp
    memory_repository.hpp
    cached_repository.hpp
)

# Create object library for repository module
add_library(lithium_target_repository_obj OBJECT ${REPOSITORY_SOURCES})

target_include_directories(lithium_target_repository_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Repository depends on model and database
target_link_libraries(lithium_target_repository_obj
    PUBLIC
        atom
        atom-utils
        lithium_database
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_repository_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_repository_obj "Core/Target/Repository")

# Expose object library for parent
set(LITHIUM_TARGET_REPOSITORY_OBJECTS $<TARGET_OBJECTS:lithium_target_repository_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_REPOSITORY_HEADERS ${REPOSITORY_HEADERS} PARENT_SCOPE)
