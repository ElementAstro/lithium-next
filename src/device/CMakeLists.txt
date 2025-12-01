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
# Note: This is a header-only compatibility layer
# The actual implementation is in the manager/ subdirectory
# ============================================================================

# Create interface library for header-only compatibility
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Core/Device")

lithium_component_end(${PROJECT_NAME})

# ============================================================================
# Device Subdirectories (drivers and implementations)
# ============================================================================

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Note: This includes atom library paths for device implementations
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

# Add service layer subdirectory
add_subdirectory(service)

# Add manager subdirectory (modular device manager)
add_subdirectory(manager)

# Recursively discover and add device implementation subdirectories
# Note: service and manager are excluded as they were already added above
lithium_message(INFO "Discovering device implementations...")
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Device impl"
    EXCLUDE_DIRS service manager
)
