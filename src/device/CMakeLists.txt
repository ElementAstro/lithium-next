# ============================================================================
# CMakeLists.txt for Lithium-Device
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device
# Description: Device management and abstraction layer
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_device 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    manager.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    loguru
    yaml-cpp
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
    DESCRIPTION "Device management and abstraction layer"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Device")

lithium_component_end(${PROJECT_NAME})

# ============================================================================
# Device Subdirectories (drivers and implementations)
# ============================================================================

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Note: This includes atom library paths for device implementations
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

# Add service layer subdirectory
add_subdirectory(service)

# Recursively discover and add device implementation subdirectories
# Note: service is excluded as it was already added above
lithium_message(INFO "Discovering device implementations...")
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Device impl"
    EXCLUDE_DIRS service
)
