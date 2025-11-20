# ============================================================================
# CMakeLists.txt for Lithium-Task
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Task
# Description: Task sequencing and generation module
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_task 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    generator.cpp
    sequencer.cpp
    target.cpp
    task.cpp
    task_camera.cpp
)

set(PROJECT_HEADERS
    generator.hpp
    sequencer.hpp
    target.hpp
    task.hpp
    task_camera.hpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    lithium_database
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Task sequencing and generation"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Task")

lithium_component_end(${PROJECT_NAME})
