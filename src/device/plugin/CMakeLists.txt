# ============================================================================
# CMakeLists.txt for Lithium-Device-Plugin
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device-Plugin
# Description: Device plugin interface and loader with hot-plug support
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_device_plugin 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    device_plugin_interface.cpp
    device_plugin_loader.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_device_common
    lithium_device_service
    loguru
)

# Platform-specific libraries
if(WIN32)
    # Windows doesn't need additional libraries for LoadLibrary
elseif(UNIX)
    # Unix/Linux needs libdl for dlopen
    list(APPEND PROJECT_LIBRARIES ${CMAKE_DL_LIBS})
endif()

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Device plugin interface and loader with hot-plug support"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Device/Plugin")

lithium_component_end(${PROJECT_NAME})
