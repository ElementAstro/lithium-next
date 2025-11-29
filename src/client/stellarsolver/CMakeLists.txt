# ============================================================================
# CMakeLists.txt for lithium.client.stellarsolver
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium StellarSolver Client
# Description: StellarSolver plate solver client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_client_stellarsolver)

# ============================================================================
# Dependencies Check
# ============================================================================

# Check for StellarSolver
find_package(StellarSolver QUIET)
if(NOT STELLARSOLVER_FOUND)
    message(STATUS "StellarSolver not found - lithium_client_stellarsolver will be skipped")
    return()
endif()

# Check for Qt
find_package(Qt6 COMPONENTS Core QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core QUIET)
    if(NOT Qt5_FOUND)
        message(STATUS "Qt not found - lithium_client_stellarsolver will be skipped")
        return()
    endif()
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(SOURCE_FILES
    statistic.cpp
    stellarsolver.cpp
    stellarsolver_client.cpp
)

set(HEADER_FILES
    config.h
    statistic.hpp
    stellarsolver.hpp
    stellarsolver_client.hpp
)

# ============================================================================
# Build Library
# ============================================================================

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/tools
    ${STELLARSOLVER_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    lithium_client_common
    lithium_tools
    Qt${QT_VERSION_MAJOR}::Core
    stellarsolver
    spdlog::spdlog
)

# Enable Qt MOC
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
    FOLDER "Client/Solvers"
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

message(STATUS "  ├─ Client lib: lithium_client_stellarsolver configured")

# ============================================================================
# Python Bindings (Optional)
# ============================================================================

find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(ssbindings MODULE binding.cpp)

    target_include_directories(ssbindings PRIVATE
        ${pybind11_INCLUDE_DIRS}
        ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
        ${STELLARSOLVER_INCLUDE_DIRS}
    )

    target_link_libraries(ssbindings PRIVATE
        ${PROJECT_NAME}
        Qt${QT_VERSION_MAJOR}::Core
        stellarsolver
    )

    set_target_properties(ssbindings PROPERTIES
        AUTOMOC ON
        FOLDER "Client/Solvers/Bindings"
    )

    message(STATUS "    └─ Python bindings: ssbindings configured")
endif()
