# ============================================================================
# CMakeLists.txt for Lithium-Script-Isolated
# ============================================================================
# Isolated Python subprocess execution module
# ============================================================================

lithium_component_begin(lithium_script_isolated 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    config_discovery.cpp
    execution_engine.cpp
    lifecycle.cpp
    message_handlers.cpp
    resource_monitor.cpp
    runner.cpp
)

# Platform-specific process spawning
if(WIN32)
    list(APPEND PROJECT_SOURCES process_spawning_win32.cpp)
else()
    list(APPEND PROJECT_SOURCES process_spawning_unix.cpp)
endif()

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    lithium_script_ipc
    atom
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
)

# Windows-specific libraries
if(WIN32)
    list(APPEND PROJECT_LIBRARIES psapi)
endif()

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Isolated Python subprocess execution"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Script/Isolated")

lithium_component_end(${PROJECT_NAME})
