# ============================================================================
# CMakeLists.txt for lithium.client.astrometry
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium Astrometry Client
# Description: Astrometry.net plate solver client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_astrometry)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    astrometry.cpp
    astrometry_client.cpp
)

set(HEADER_FILES
    astrometry.hpp
    astrometry_client.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_client_common
    lithium_tools
    atom-component
    atom-io
    atom-system
    atom-utils
    spdlog::spdlog
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

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/Solvers")

message(STATUS "  ├─ Client lib: lithium_client_astrometry configured")

# ============================================================================
# Remote Client Subdirectory
# ============================================================================

# Add remote client if CURL is available
find_package(CURL QUIET)
if(CURL_FOUND)
    add_subdirectory(remote)
else()
    message(STATUS "    └─ CURL not found - astrometry remote client skipped")
endif()
