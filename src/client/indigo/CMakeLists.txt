# ============================================================================
# CMakeLists.txt for lithium.client.indigo
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium INDIGO Device Client
# Description: INDIGO device implementations (Camera, Focuser, FilterWheel, etc.)
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indigo LANGUAGES CXX)

# ============================================================================
# Platform Check - INDIGO is primarily Linux/macOS
# ============================================================================

if(UNIX)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(INDIGO QUIET indigo)
    endif()

    # Also try manual find
    if(NOT INDIGO_FOUND)
        find_path(INDIGO_INCLUDE_DIR
            NAMES indigo/indigo_bus.h
            PATHS /usr/include /usr/local/include
        )
        find_library(INDIGO_LIBRARY
            NAMES indigo
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        if(INDIGO_INCLUDE_DIR AND INDIGO_LIBRARY)
            set(INDIGO_FOUND TRUE)
            set(INDIGO_INCLUDE_DIRS ${INDIGO_INCLUDE_DIR})
            set(INDIGO_LIBRARIES ${INDIGO_LIBRARY})
        endif()
    endif()
endif()

if(INDIGO_FOUND)
    message(STATUS "  ├─ INDIGO found: ${INDIGO_LIBRARIES}")
    set(INDIGO_AVAILABLE TRUE)
else()
    message(STATUS "  ├─ INDIGO not found - building stub implementation")
    set(INDIGO_AVAILABLE FALSE)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    indigo_client.cpp
    indigo_device_base.cpp
    indigo_camera.cpp
    indigo_focuser.cpp
    indigo_filterwheel.cpp
    indigo_telescope.cpp
    indigo_rotator.cpp
    indigo_dome.cpp
    indigo_weather.cpp
    indigo_gps.cpp
    indigo_device_factory.cpp
)

set(HEADER_FILES
    indigo_client.hpp
    indigo_device_base.hpp
    indigo_camera.hpp
    indigo_focuser.hpp
    indigo_filterwheel.hpp
    indigo_telescope.hpp
    indigo_rotator.hpp
    indigo_dome.hpp
    indigo_weather.hpp
    indigo_gps.hpp
    indigo_device_factory.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_client_common
    atom-component
    atom-error
    atom-utils
    atom-function
    loguru
)

if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32 version iphlpapi)
endif()

if(INDIGO_AVAILABLE)
    list(APPEND PROJECT_LIBRARIES ${INDIGO_LIBRARIES})
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

if(INDIGO_AVAILABLE)
    target_include_directories(${PROJECT_NAME} PRIVATE ${INDIGO_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE INDIGO_AVAILABLE=1)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/INDIGO")

message(STATUS "  ├─ Client lib: lithium_client_indigo configured")

# ============================================================================
# INDIGO Device Plugin Subdirectory
# ============================================================================

add_subdirectory(plugin)
