# ============================================================================
# CMakeLists.txt for Lithium-Server
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Server
# Description: Web server, WebSocket, and command handling
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_server 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    command.cpp
    eventloop.cpp
    rate_limiter.cpp
    websocket.cpp
    command/camera.cpp
    command/mount.cpp
    command/focuser.cpp
    command/filterwheel.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
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
    DESCRIPTION "Web server, WebSocket, and command handling"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Server")

lithium_component_end(${PROJECT_NAME})
