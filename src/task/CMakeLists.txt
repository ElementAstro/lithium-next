# ============================================================================
# CMakeLists.txt for Lithium-Task
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Task
# Description: Task sequencing and generation module with split architecture
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_task 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

# Gather all component task sources
file(GLOB_RECURSE COMPONENTS_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)

set(PROJECT_SOURCES
    # Core module
    core/task.cpp
    core/target.cpp
    core/sequencer.cpp
    core/generator.cpp
    core/factory.cpp
    core/registration.cpp
    # Utils module
    utils/imagepath.cpp
    utils/integration_utils.cpp
    # Adapters module
    adapters/api_adapter.cpp
)

set(PROJECT_HEADERS
    # Main entry point
    task.hpp
    # Core module
    core/task.hpp
    core/target.hpp
    core/sequencer.hpp
    core/generator.hpp
    core/factory.hpp
    core/registration.hpp
    core/exception.hpp
    core/types.hpp
    # Components module
    components/components.hpp
    components/common/task_base.hpp
    components/common/types.hpp
    components/common/validation.hpp
    # Utils module
    utils/imagepath.hpp
    utils/integration_utils.hpp
    utils/utils.hpp
    # Adapters module
    adapters/api_adapter.hpp
    adapters/adapters.hpp
    # All component sources
    ${COMPONENTS_SRC}
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    lithium_database
    lithium_tools
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Task sequencing and generation module"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Task")

lithium_component_end(${PROJECT_NAME})
