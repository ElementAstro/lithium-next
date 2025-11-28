# ============================================================================
# CMakeLists.txt for lithium.client.phd2
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium PHD2 Client
# Description: PHD2 guider client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_phd2)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    client.cpp
    connection.cpp
    phd2_client.cpp
    profile.cpp
)

set(HEADER_FILES
    client.hpp
    connection.hpp
    event_handler.hpp
    exceptions.hpp
    phd2_client.hpp
    profile.hpp
    types.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_client_common
    lithium_tools
    atom-utils
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32)
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
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/Guiders")

message(STATUS "  ├─ Client lib: lithium_client_phd2 configured")
