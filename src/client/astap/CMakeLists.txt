# ============================================================================
# CMakeLists.txt for lithium.client.astap
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium ASTAP Client
# Description: ASTAP plate solver client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_astap)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    astap.cpp
    astap_client.cpp
    options.cpp
)

set(HEADER_FILES
    astap.hpp
    astap_client.hpp
    options.hpp
)

# ============================================================================
# Dependencies
# ============================================================================

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CFITSIO cfitsio)
endif()

if(NOT CFITSIO_FOUND)
    find_package(CFITSIO QUIET)
endif()

if(NOT CFITSIO_FOUND)
    message(STATUS "CFITSIO not found - lithium_client_astap will be skipped")
    return()
endif()

set(PROJECT_LIBRARIES
    lithium_client_common
    lithium_tools
    atom-component
    atom-io
    atom-system
    atom-utils
    spdlog::spdlog
    ${CFITSIO_LIBRARIES}
)

# ============================================================================
# Build Library
# ============================================================================

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/tools
    ${CFITSIO_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARIES})

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Client/Solvers")

# Add alias for solver plugin linking
add_library(lithium::astap_client ALIAS ${PROJECT_NAME})

# Add plugin subdirectory if solver plugins are enabled
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugin AND LITHIUM_BUILD_SOLVER_PLUGINS)
    add_subdirectory(plugin)
endif()

message(STATUS "  ├─ Client lib: lithium_client_astap configured")
