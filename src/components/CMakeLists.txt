# ============================================================================
# CMakeLists.txt for Lithium-Components
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Components
# Description: Component management and dependency tracking
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_components 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    dependency.cpp
    loader.cpp
    tracker.cpp
    version.cpp
    debug/dump.cpp
    debug/dynamic.cpp
    debug/elf.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# ============================================================================
# Add subdirectories
# ============================================================================

add_subdirectory(manager)
add_subdirectory(system)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES ${PROJECT_LIBRARIES} lithium_components_manager lithium_system_dependency
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Component management and dependency tracking"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Components")

lithium_component_end(${PROJECT_NAME})
