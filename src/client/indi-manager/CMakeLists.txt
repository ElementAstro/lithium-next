# ============================================================================
# CMakeLists.txt for lithium.client.indi-manager
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium INDI Manager Client
# Description: INDI server and driver management client (renamed to avoid
#              conflicts with the real INDI client library)
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi_manager)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    async_system_command.cpp
    database.cpp
    driver_collection.cpp
    fifo_channel.cpp
    indihub_agent.cpp
    manager_client.cpp
    manager_connector.cpp
    property_utils.cpp
    server_manager.cpp
)

set(HEADER_FILES
    async_system_command.hpp
    connector_interface.hpp
    database.hpp
    device_container.hpp
    driver_collection.hpp
    fifo_channel.hpp
    indihub_agent.hpp
    manager_client.hpp
    manager_connector.hpp
    property_utils.hpp
    server_config.hpp
    server_manager.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_client_common
    lithium_tools
    loguru
    atom-system
    atom-io
    atom-component
    atom-error
    atom-utils
    atom-function
    spdlog::spdlog
)

if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32 version iphlpapi)
endif()

# ============================================================================
# Build Library
# ============================================================================

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/tools
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/INDI-Manager")

message(STATUS "  ├─ Client lib: lithium_client_indi_manager configured")
