# ===========================================================================
# CMakeLists.txt for Lithium-Target Service
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Service
# Description: High-level service layer for target management and operations
# ===========================================================================

set(SERVICE_SOURCES
    celestial_service.cpp
)

set(SERVICE_HEADERS
    celestial_service.hpp
    service.hpp
)

# Create object library for service module
add_library(lithium_target_service_obj OBJECT ${SERVICE_SOURCES})

target_include_directories(lithium_target_service_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Service depends on all lower-level modules
target_link_libraries(lithium_target_service_obj
    PUBLIC
        atom
        atom-utils
        lithium_config
        lithium_database
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_service_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_service_obj "Core/Target/Service")

# Expose object library for parent
set(LITHIUM_TARGET_SERVICE_OBJECTS $<TARGET_OBJECTS:lithium_target_service_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_SERVICE_HEADERS ${SERVICE_HEADERS} PARENT_SCOPE)
