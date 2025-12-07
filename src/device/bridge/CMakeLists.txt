# ============================================================================
# CMakeLists.txt for Lithium-Device-Bridge
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device-Bridge
# Description: Bridge between device manager and component manager
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_device_bridge 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    device_component_adapter.cpp
    device_component_bridge.cpp
    device_config_watcher.cpp
    device_metrics.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_device_common
    lithium_device_plugin
    lithium_components
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
    DESCRIPTION "Bridge between device manager and component manager"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Device/Bridge")

lithium_component_end(${PROJECT_NAME})
