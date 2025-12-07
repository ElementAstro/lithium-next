# ===========================================================================
# CMakeLists.txt for Lithium-Target Model
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Model
# Description: Core data models for celestial targets and related structures
# ===========================================================================

set(MODEL_SOURCES
    celestial_model.cpp
)

set(MODEL_HEADERS
    # Core data models
    celestial_model.hpp
    celestial_object.hpp
    star_object.hpp

    # Search and filtering
    search_filter.hpp
    search_result.hpp

    # Database models
    database_models.hpp

    # Module aggregator
    model.hpp
)

# Create object library for model module
add_library(lithium_target_model_obj OBJECT ${MODEL_SOURCES})

target_include_directories(lithium_target_model_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_model_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_model_obj "Core/Target/Model")

# Expose object library for parent
set(LITHIUM_TARGET_MODEL_OBJECTS $<TARGET_OBJECTS:lithium_target_model_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_MODEL_HEADERS ${MODEL_HEADERS} PARENT_SCOPE)
