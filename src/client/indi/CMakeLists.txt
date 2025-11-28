# ============================================================================
# CMakeLists.txt for lithium.client.indi
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium INDI Client
# Description: INDI server and driver management client
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    async_system_command.cpp
    driverlist.cpp
    database.cpp
    indiserver.cpp
    collection.cpp
    iconnector.cpp
    indihub_agent.cpp
)

set(HEADER_FILES
    async_system_command.hpp
    collection.hpp
    connector.hpp
    container.hpp
    driverlist.hpp
    database.hpp
    indiserver.hpp
    iconnector.hpp
    indihub_agent.hpp
    indi_client.hpp
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
    tinyxml2
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
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/INDI")

message(STATUS "  ├─ Client lib: lithium_client_indi configured")
