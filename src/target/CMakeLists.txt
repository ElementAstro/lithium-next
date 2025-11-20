# ============================================================================
# CMakeLists.txt for Lithium-Target
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Target
# Description: Target calculation and preference management module
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_target 1.0.0)

# ============================================================================
# Dependencies
# ============================================================================

lithium_find_package(Eigen3 REQUIRED)
if(Eigen3_FOUND)
    lithium_print_config("Eigen3 Include Dir" "${EIGEN3_INCLUDE_DIR}")
endif()

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    engine.cpp
    preference.cpp
    reader.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    atom-utils
    lithium_config
    loguru
    yaml-cpp
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
    DESCRIPTION "Target calculation and preference management"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Target")

lithium_component_end(${PROJECT_NAME})
