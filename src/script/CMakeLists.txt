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
# Add Subdirectories (Modular Architecture)
# ============================================================================

# Shell script execution module
add_subdirectory(shell)

# IPC protocol module
add_subdirectory(ipc)

# Virtual environment management module
add_subdirectory(venv)

# Python tool registry module
add_subdirectory(tools)

# NumPy/Pandas integration module
add_subdirectory(numpy)

# Isolated Python execution module
add_subdirectory(isolated)

# ============================================================================
# Core Source Files (Top-level)
# ============================================================================

set(PROJECT_SOURCES
    check.cpp
    interpreter_pool.cpp
    python_caller.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    # Submodule libraries
    lithium_script_shell
    lithium_script_ipc
    lithium_script_venv
    lithium_script_tools
    lithium_script_numpy
    lithium_script_isolated
    # External dependencies
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
