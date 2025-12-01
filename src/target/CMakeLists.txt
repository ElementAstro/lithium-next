# ===========================================================================
# CMakeLists.txt for Lithium-Target
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Target
# Description: Target calculation, celestial search engine, and preference management module
# Author: Max Qian
# License: GPL3
#
# Architecture:
#   - model/        : Core data models for celestial targets
#   - index/        : Indexing and caching mechanisms
#   - repository/   : Data access and persistence layer
#   - search/       : Search engine and query mechanisms
#   - recommendation/: Preference management and recommendations
#   - observability/: Metrics, monitoring, and logging
#   - io/           : Input/Output operations and file handling
#   - service/      : High-level service layer
# ===========================================================================

lithium_component_begin(lithium_target 1.0.0)

# ===========================================================================
# Dependencies
# ===========================================================================

lithium_find_package(Eigen3 REQUIRED)
if(Eigen3_FOUND)
    lithium_print_config("Eigen3 Include Dir" "${EIGEN3_INCLUDE_DIR}")
endif()

# ===========================================================================
# Add Subdirectory Modules
# ===========================================================================

# Model module - core data structures
add_subdirectory(model)

# Index module - caching and indexing
add_subdirectory(index)

# Repository module - data persistence layer
add_subdirectory(repository)

# Search module - search engine implementation
add_subdirectory(search)

# Recommendation module - preferences and recommendations
add_subdirectory(recommendation)

# Observability module - metrics and monitoring
add_subdirectory(observability)

# IO module - file I/O operations
add_subdirectory(io)

# Service module - high-level service layer
add_subdirectory(service)

# Online module - online celestial database queries
add_subdirectory(online)

# ===========================================================================
# Collect Object Libraries from Submodules
# ===========================================================================

# Collect all object libraries from submodules
set(LITHIUM_TARGET_ALL_OBJECTS
    ${LITHIUM_TARGET_MODEL_OBJECTS}
    ${LITHIUM_TARGET_INDEX_OBJECTS}
    ${LITHIUM_TARGET_REPOSITORY_OBJECTS}
    ${LITHIUM_TARGET_SEARCH_OBJECTS}
    ${LITHIUM_TARGET_RECOMMENDATION_OBJECTS}
    ${LITHIUM_TARGET_OBSERVABILITY_OBJECTS}
    ${LITHIUM_TARGET_IO_OBJECTS}
    ${LITHIUM_TARGET_SERVICE_OBJECTS}
    ${LITHIUM_TARGET_ONLINE_OBJECTS}
)

# Collect all headers from submodules
set(LITHIUM_TARGET_ALL_HEADERS
    ${LITHIUM_TARGET_MODEL_HEADERS}
    ${LITHIUM_TARGET_INDEX_HEADERS}
    ${LITHIUM_TARGET_REPOSITORY_HEADERS}
    ${LITHIUM_TARGET_SEARCH_HEADERS}
    ${LITHIUM_TARGET_RECOMMENDATION_HEADERS}
    ${LITHIUM_TARGET_OBSERVABILITY_HEADERS}
    ${LITHIUM_TARGET_IO_HEADERS}
    ${LITHIUM_TARGET_SERVICE_HEADERS}
    ${LITHIUM_TARGET_ONLINE_HEADERS}
)

# ===========================================================================
# Library Dependencies
# ===========================================================================

set(PROJECT_LIBRARIES
    atom
    atom-utils
    lithium_config
    lithium_database
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
    Eigen3::Eigen
)

# ===========================================================================
# Build Aggregated Library
# ===========================================================================

# Build main static library from all submodule object files
lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${LITHIUM_TARGET_ALL_OBJECTS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Target calculation, celestial search engine, and preference management"
)

# ===========================================================================
# Additional Configuration
# ===========================================================================

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Target")

# ===========================================================================
# Public Interface
# ===========================================================================

# Ensure include paths are available to downstream targets
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/index>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/repository>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/search>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/recommendation>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/observability>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/io>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/online>
)

lithium_component_end(${PROJECT_NAME})
