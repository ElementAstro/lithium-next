# ============================================================================
# CMakeLists.txt for Lithium-Debug
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Debug
# Description: Debugging and diagnostic utilities
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_debug 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    check.cpp
    suggestion.cpp
    terminal.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    loguru
    yaml-cpp
    pybind11::embed
    ${CMAKE_THREAD_LIBS_INIT}
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Debugging and diagnostic utilities"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Debug")

lithium_component_end(${PROJECT_NAME})
