# ============================================================================
# CMakeLists.txt for lithium.client.astrometry.remote
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium Astrometry Remote Client
# Description: Astrometry.net remote API client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_astrometry_remote VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    client.cpp
    utils.cpp
)

set(HEADER_FILES
    client.hpp
    utils.hpp
)

# ============================================================================
# Build Library
# ============================================================================

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/tools
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    lithium_client_common
    lithium_tools
    CURL::libcurl
    spdlog::spdlog
    Threads::Threads
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/Solvers/Remote")

message(STATUS "    └─ Remote client: lithium_client_astrometry_remote configured")
