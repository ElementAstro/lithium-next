# ============================================================================
# CMakeLists.txt for Lithium-Script
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Script
# Description: Python scripting and shell command execution
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_script 1.0.0)

# ============================================================================
# Dependencies
# ============================================================================

lithium_find_package(Python COMPONENTS Interpreter Development)
lithium_find_package(pybind11)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    check.cpp
    python_caller.cpp
    sheller.cpp
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
    DESCRIPTION "Python scripting and shell command execution"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Script")

lithium_component_end(${PROJECT_NAME})
