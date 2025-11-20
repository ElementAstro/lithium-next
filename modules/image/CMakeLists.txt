# ============================================================================
# CMakeLists.txt for Lithium Image Module
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: lithium_image
# Description: Image processing and manipulation module
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_image 1.0.0)

# ============================================================================
# Dependencies
# ============================================================================

lithium_find_package(OpenCV VERSION 4 REQUIRED)
lithium_find_package(CFITSIO REQUIRED)
lithium_find_package(pybind11 REQUIRED)

# ============================================================================
# Compiler-specific Settings
# ============================================================================

# Suppress deprecated enum conversion warnings for this module
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-deprecated-enum-enum-conversion)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    src/thumbhash.cpp
)

include_directories(src)

# ============================================================================
# Build Shared Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES atom ${CMAKE_THREAD_LIBS_INIT} ${OpenCV_LIBS} ${CFITSIO_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE SHARED
    DESCRIPTION "Image processing and manipulation"
)

# Additional include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

# Override output name
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME image
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Apply compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Modules/Image")

lithium_component_end(${PROJECT_NAME})

# ============================================================================
# Subdirectories
# ============================================================================

add_subdirectory(examples)
