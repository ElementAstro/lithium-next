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
    # Core module
    core/dependency.cpp
    core/loader.cpp
    core/tracker.cpp
    core/version.cpp
    # Debug module
    debug/dump.cpp
    debug/dynamic.cpp
    debug/elf.cpp
)

set(PROJECT_HEADERS
    # Main entry point
    components.hpp
    # Core module
    core/core.hpp
    core/types.hpp
    core/dependency.hpp
    core/loader.hpp
    core/module.hpp
    core/tracker.hpp
    core/version.hpp
    # Manager module (aggregated header)
    manager/manager_module.hpp
    # Debug module
    debug/debug.hpp
    debug/dump.hpp
    debug/dynamic.hpp
    debug/elf.hpp
    # System module
    system/system.hpp
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

add_subdirectory(core)
add_subdirectory(debug)
add_subdirectory(manager)
add_subdirectory(system)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES} lithium_core lithium_debug lithium_components_manager lithium_system_dependency
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Component management and dependency tracking"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Components")

lithium_component_end(${PROJECT_NAME})
