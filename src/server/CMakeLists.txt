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
    # Core server components
    command.cpp
    eventloop.cpp
    rate_limiter.cpp
    task_manager.cpp
    websocket.cpp
    websocket/log_stream.cpp

    # Device command handlers
    command/camera.cpp
    command/mount.cpp
    command/focuser.cpp
    command/filterwheel.cpp
    command/device.cpp
    command/device_manager.cpp
    command/dome.cpp
    command/guider.cpp
    command/solver.cpp

    # Server management commands
    command/server.cpp
    command/task.cpp

    # Configuration and scripting
    command/config_ws.cpp
    command/gpio.cpp
    command/script.cpp

    # Controllers
    controller/database.cpp

    # Plugin system
    plugin/plugin_interface.cpp
    plugin/plugin_loader.cpp
    plugin/plugin_manager.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_components
    lithium_config
    lithium_database
    lithium_logging
    lithium_script
    spdlog::spdlog
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
