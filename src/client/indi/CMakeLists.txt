# ============================================================================
# CMakeLists.txt for lithium.client.indi
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium INDI Device Client
# Description: INDI device implementations (Camera, Focuser, FilterWheel, Telescope)
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    indi_device_base.cpp
    indi_camera.cpp
    indi_focuser.cpp
    indi_filterwheel.cpp
    indi_telescope.cpp
    indi_rotator.cpp
    indi_dome.cpp
    indi_weather.cpp
    indi_gps.cpp
    indi_device_factory.cpp
)

set(HEADER_FILES
    indi_device_base.hpp
    indi_camera.hpp
    indi_focuser.hpp
    indi_filterwheel.hpp
    indi_telescope.hpp
    indi_rotator.hpp
    indi_dome.hpp
    indi_weather.hpp
    indi_gps.hpp
    indi_device_factory.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_client_common
    lithium_client_indi_manager
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
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/INDI")

message(STATUS "  ├─ Client lib: lithium_client_indi configured")

# ============================================================================
# INDI Device Plugin Subdirectory
# ============================================================================

add_subdirectory(plugin)
