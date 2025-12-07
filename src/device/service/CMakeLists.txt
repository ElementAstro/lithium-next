# ============================================================================
# CMakeLists.txt for Lithium-Device-Service
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device-Service
# Description: Device service layer providing high-level device operations
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_device_service 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    camera_service.cpp
    mount_service.cpp
    focuser_service.cpp
    filterwheel_service.cpp
    dome_service.cpp
    guider_service.cpp
    indi_adapter.cpp
    ascom_adapter.cpp
    indi_backend.cpp
    ascom_backend.cpp
    device_factory.cpp
    device_type_registry.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    lithium_device
    lithium_device_common
    lithium.client.indi
    lithium.client.ascom
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
    DESCRIPTION "Device service layer providing high-level device operations"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Device/Service")

lithium_component_end(${PROJECT_NAME})
