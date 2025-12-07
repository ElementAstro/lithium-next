# ============================================================================
# CMakeLists.txt for Lithium-INDIGO-Plugin
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-INDIGO-Plugin
# Description: INDIGO device driver plugin for Lithium
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_indigo_plugin 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    indigo_device_plugin.cpp
)

set(PROJECT_HEADERS
    indigo_device_plugin.hpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_client_indigo
    lithium_device_plugin
    lithium_device_service
    lithium_device_common
    loguru
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    HEADERS ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE MODULE
    DESCRIPTION "INDIGO device driver plugin for Lithium"
)

# Set output name without prefix
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "indigo_device_plugin"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Plugins/Devices")

# Install to plugins directory
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION plugins/devices
    RUNTIME DESTINATION plugins/devices
)

lithium_component_end(${PROJECT_NAME})
