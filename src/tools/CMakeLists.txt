# ============================================================================
# CMakeLists.txt for Lithium-Tools
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Tools
# Description: Astronomy calculation and utility tools
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_tools 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    calculate.cpp
    convert.cpp
    croods.cpp
    libastro.cpp
    solverutils.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
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
    DESCRIPTION "Astronomy calculation and utility tools"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Tools")

lithium_component_end(${PROJECT_NAME})
