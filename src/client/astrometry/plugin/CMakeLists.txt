# CMakeLists.txt for Astrometry.net solver plugin
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

cmake_minimum_required(VERSION 3.20)

project(lithium_astrometry_solver_plugin LANGUAGES CXX)

# Plugin source files
set(ASTROMETRY_PLUGIN_SOURCES
    astrometry_solver_plugin.cpp
)

set(ASTROMETRY_PLUGIN_HEADERS
    astrometry_solver_plugin.hpp
)

# Build as shared library for dynamic loading
if(LITHIUM_SOLVER_PLUGINS_SHARED)
    add_library(lithium_astrometry_solver_plugin SHARED
        ${ASTROMETRY_PLUGIN_SOURCES}
        ${ASTROMETRY_PLUGIN_HEADERS}
    )

    # Export symbols
    target_compile_definitions(lithium_astrometry_solver_plugin PRIVATE
        LITHIUM_BUILDING_PLUGIN
    )

    set_target_properties(lithium_astrometry_solver_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "astrometry_solver_plugin"
    )
else()
    # Build as static library for linking
    add_library(lithium_astrometry_solver_plugin STATIC
        ${ASTROMETRY_PLUGIN_SOURCES}
        ${ASTROMETRY_PLUGIN_HEADERS}
    )
endif()

# Alias target
add_library(lithium::astrometry_solver_plugin ALIAS lithium_astrometry_solver_plugin)

# Include directories
target_include_directories(lithium_astrometry_solver_plugin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lithium_astrometry_solver_plugin
    PUBLIC
        lithium_solver_core
        lithium_astrometry_client
    PRIVATE
        atom-system
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Find CURL for remote API
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(lithium_astrometry_solver_plugin PRIVATE CURL::libcurl)
    target_compile_definitions(lithium_astrometry_solver_plugin PRIVATE LITHIUM_HAS_CURL)
endif()

# Compiler features
target_compile_features(lithium_astrometry_solver_plugin PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_astrometry_solver_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Install rules
install(TARGETS lithium_astrometry_solver_plugin
    EXPORT lithium_astrometry_solver_plugin_targets
    LIBRARY DESTINATION lib/lithium/plugins
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin/lithium/plugins
)

install(FILES ${ASTROMETRY_PLUGIN_HEADERS}
    DESTINATION include/lithium/client/astrometry/plugin
)
