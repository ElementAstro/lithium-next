# CMakeLists.txt for Lithium Components
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Components
# Description: Core component system for Lithium astrophotography software
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_components 
    VERSION 1.0.0 
    DESCRIPTION "Lithium core component management system"
    LANGUAGES C CXX
)

# =============================================================================
# Build Options
# =============================================================================
option(BUILD_COMPONENTS_SHARED "Build components as shared libraries" OFF)
option(BUILD_COMPONENTS_TESTS "Build component tests" OFF)
option(BUILD_COMPONENTS_EXAMPLES "Build component examples" OFF)
option(ENABLE_COMPONENT_PROFILING "Enable component profiling" OFF)

# =============================================================================
# Component Core Sources
# =============================================================================
set(COMPONENT_CORE_SOURCES
    # Core component system
    dependency.cpp
    loader.cpp
    tracker.cpp
    version.cpp
    manager.cpp
    
    # Component system headers
    dependency.hpp
    loader.hpp
    tracker.hpp
    version.hpp
    manager.hpp
    module.hpp
    system_dependency.hpp
)

# =============================================================================
# Required Dependencies
# =============================================================================
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

set(COMPONENT_REQUIRED_LIBS
    atom::atom
    yaml-cpp
    Threads::Threads
    loguru
)

# =============================================================================
# Add Subdirectories (Component Modules)
# =============================================================================
# Manager subsystem
add_subdirectory(manager)

# Debug subsystem (comprehensive debugging tools)
add_subdirectory(debug)

# System subsystem (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/system/CMakeLists.txt)
    add_subdirectory(system)
endif()

# =============================================================================
# Main Components Library
# =============================================================================
if(BUILD_COMPONENTS_SHARED)
    add_library(${PROJECT_NAME} SHARED ${COMPONENT_CORE_SOURCES})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
else()
    add_library(${PROJECT_NAME} STATIC ${COMPONENT_CORE_SOURCES})
endif()

# Create alias for consistent naming
add_library(lithium::components ALIAS ${PROJECT_NAME})

# =============================================================================
# Target Configuration
# =============================================================================
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/lithium/components>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${COMPONENT_REQUIRED_LIBS}
        lithium::components::manager
        lithium::debug
    PRIVATE
        $<$<BOOL:${BUILD_COMPONENTS_SHARED}>:${CMAKE_DL_LIBS}>
)

# =============================================================================
# Compiler Features and Definitions
# =============================================================================
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        LITHIUM_COMPONENTS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        LITHIUM_COMPONENTS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        LITHIUM_COMPONENTS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        $<$<CONFIG:Debug>:LITHIUM_COMPONENTS_DEBUG>
        $<$<BOOL:${ENABLE_COMPONENT_PROFILING}>:LITHIUM_COMPONENTS_PROFILING>
)

# =============================================================================
# Position Independent Code
# =============================================================================
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# Compiler Options
# =============================================================================
target_compile_options(${PROJECT_NAME} PRIVATE
    # GCC/Clang optimizations
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wconversion>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wsign-conversion>
    
    # MSVC optimizations
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
)

# =============================================================================
# Platform-specific Configuration
# =============================================================================
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITHIUM_PLATFORM_WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITHIUM_PLATFORM_LINUX
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITHIUM_PLATFORM_MACOS
    )
endif()

# =============================================================================
# Examples
# =============================================================================
if(BUILD_COMPONENTS_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Testing
# =============================================================================
if(BUILD_COMPONENTS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Installation
# =============================================================================
include(GNUInstallDirs)

# Install the library
install(TARGETS ${PROJECT_NAME}
    EXPORT lithium_components_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES
    dependency.hpp
    loader.hpp
    tracker.hpp
    version.hpp
    manager.hpp
    module.hpp
    system_dependency.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/components
)

# Install export targets
install(EXPORT lithium_components_targets
    FILE lithium_components_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium_components
)

# =============================================================================
# Status Messages
# =============================================================================
message(STATUS "Lithium Components Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${BUILD_COMPONENTS_SHARED}")
message(STATUS "  Examples: ${BUILD_COMPONENTS_EXAMPLES}")
message(STATUS "  Tests: ${BUILD_COMPONENTS_TESTS}")
message(STATUS "  Profiling: ${ENABLE_COMPONENT_PROFILING}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
