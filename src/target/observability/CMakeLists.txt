# ===========================================================================
# CMakeLists.txt for Lithium-Target Observability
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Observability
# Description: Observability metrics, monitoring, and logging for targets
# ===========================================================================

set(OBSERVABILITY_SOURCES
    visibility_calculator.cpp
    time_window_filter.cpp
)

set(OBSERVABILITY_HEADERS
    visibility_calculator.hpp
    time_window_filter.hpp
    module.hpp
)

# Create object library for observability module
add_library(lithium_target_observability_obj OBJECT ${OBSERVABILITY_SOURCES})

target_include_directories(lithium_target_observability_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Observability depends on logging
target_link_libraries(lithium_target_observability_obj
    PUBLIC
        atom
        atom-utils
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_observability_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_observability_obj "Core/Target/Observability")

# Expose object library for parent
set(LITHIUM_TARGET_OBSERVABILITY_OBJECTS $<TARGET_OBJECTS:lithium_target_observability_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_OBSERVABILITY_HEADERS ${OBSERVABILITY_HEADERS} PARENT_SCOPE)
