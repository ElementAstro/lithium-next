# ============================================================================
# CMakeLists.txt for Lithium-Config
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Config
# Description: Configuration management module
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_config 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    configor.cpp
    config_cache.cpp
    config_validator.cpp
    config_serializer.cpp
    config_watcher.cpp
)

set(PROJECT_HEADERS
    configor.hpp
    config_cache.hpp
    config_validator.hpp
    config_serializer.hpp
    config_watcher.hpp
    configor_macro.hpp
    json5.hpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    atom
)

# Platform-specific libraries
if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32)
endif()

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Configuration management module"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Config")

lithium_component_end(${PROJECT_NAME})
