# ============================================================================
# CMakeLists.txt for Lithium-Config
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Config
# Description: Configuration management module with split architecture
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_config 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    # Core module
    core/manager.cpp
    # Components module
    components/cache.cpp
    components/validator.cpp
    components/serializer.cpp
    components/watcher.cpp
)

set(PROJECT_HEADERS
    # Main entry point
    config.hpp
    # Core module
    core/exception.hpp
    core/manager.hpp
    core/types.hpp
    # Components module
    components/cache.hpp
    components/validator.hpp
    components/serializer.hpp
    components/watcher.hpp
    components/components.hpp
    # Utils module
    utils/json5.hpp
    utils/macro.hpp
    utils/utils.hpp
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
