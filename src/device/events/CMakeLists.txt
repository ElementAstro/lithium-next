# ============================================================================
# CMakeLists.txt for Lithium-Device-Events
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device-Events
# Description: Device event bus for inter-component communication
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_device_events 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    device_event_bus.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    loguru
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
    DESCRIPTION "Device event bus for inter-component communication"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Device/Events")

lithium_component_end(${PROJECT_NAME})
