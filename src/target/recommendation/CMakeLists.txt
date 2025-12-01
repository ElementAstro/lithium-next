# ===========================================================================
# CMakeLists.txt for Lithium-Target Recommendation
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target Recommendation
# Description: Modular recommendation system with multiple algorithms
# ===========================================================================

set(RECOMMENDATION_SOURCES
    recommendation_engine.cpp
    matrix_factorization.cpp
    collaborative_filter.cpp
    content_filter.cpp
    hybrid_recommender.cpp
    preference.cpp
)

set(RECOMMENDATION_HEADERS
    recommendation_engine.hpp
    matrix_factorization.hpp
    collaborative_filter.hpp
    content_filter.hpp
    hybrid_recommender.hpp
    recommendation.hpp
    preference.hpp
)

# Create object library for recommendation module
add_library(lithium_target_recommendation_obj OBJECT ${RECOMMENDATION_SOURCES})

target_include_directories(lithium_target_recommendation_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Recommendation depends on core libraries and Eigen3
target_link_libraries(lithium_target_recommendation_obj
    PUBLIC
        atom
        atom-utils
        lithium_config
        yaml-cpp
        Eigen3::Eigen
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_recommendation_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_recommendation_obj "Core/Target/Recommendation")

# Expose object library for parent
set(LITHIUM_TARGET_RECOMMENDATION_OBJECTS $<TARGET_OBJECTS:lithium_target_recommendation_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_RECOMMENDATION_HEADERS ${RECOMMENDATION_HEADERS} PARENT_SCOPE)
