# CMakeLists.txt for StellarSolver solver plugin
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

cmake_minimum_required(VERSION 3.20)

project(lithium_stellarsolver_plugin LANGUAGES CXX)

# Find Qt (required for StellarSolver)
find_package(Qt6 QUIET COMPONENTS Core)
if(NOT Qt6_FOUND)
    find_package(Qt5 QUIET COMPONENTS Core)
endif()

# Find StellarSolver library
find_package(StellarSolver QUIET)

# Check if dependencies are available
if(NOT (Qt5_FOUND OR Qt6_FOUND))
    message(STATUS "Qt not found - StellarSolver plugin will not be built")
    return()
endif()

if(NOT StellarSolver_FOUND)
    message(STATUS "StellarSolver not found - plugin will be built without library support")
endif()

# Plugin source files
set(STELLARSOLVER_PLUGIN_SOURCES
    stellarsolver_plugin.cpp
)

set(STELLARSOLVER_PLUGIN_HEADERS
    stellarsolver_plugin.hpp
)

# Build as shared library for dynamic loading
if(LITHIUM_SOLVER_PLUGINS_SHARED)
    add_library(lithium_stellarsolver_plugin SHARED
        ${STELLARSOLVER_PLUGIN_SOURCES}
        ${STELLARSOLVER_PLUGIN_HEADERS}
    )

    # Export symbols
    target_compile_definitions(lithium_stellarsolver_plugin PRIVATE
        LITHIUM_BUILDING_PLUGIN
    )

    set_target_properties(lithium_stellarsolver_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "stellarsolver_plugin"
    )
else()
    # Build as static library for linking
    add_library(lithium_stellarsolver_plugin STATIC
        ${STELLARSOLVER_PLUGIN_SOURCES}
        ${STELLARSOLVER_PLUGIN_HEADERS}
    )
endif()

# Alias target
add_library(lithium::stellarsolver_plugin ALIAS lithium_stellarsolver_plugin)

# Include directories
target_include_directories(lithium_stellarsolver_plugin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lithium_stellarsolver_plugin
    PUBLIC
        lithium_solver_core
        lithium_stellarsolver_client
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Link Qt
if(Qt6_FOUND)
    target_link_libraries(lithium_stellarsolver_plugin PRIVATE Qt6::Core)
    target_compile_definitions(lithium_stellarsolver_plugin PRIVATE LITHIUM_HAS_QT6)
elseif(Qt5_FOUND)
    target_link_libraries(lithium_stellarsolver_plugin PRIVATE Qt5::Core)
    target_compile_definitions(lithium_stellarsolver_plugin PRIVATE LITHIUM_HAS_QT5)
endif()

# Link StellarSolver if found
if(StellarSolver_FOUND)
    target_link_libraries(lithium_stellarsolver_plugin PRIVATE StellarSolver::StellarSolver)
    target_compile_definitions(lithium_stellarsolver_plugin PRIVATE LITHIUM_HAS_STELLARSOLVER)
endif()

# Compiler features
target_compile_features(lithium_stellarsolver_plugin PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_stellarsolver_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    AUTOMOC ON
)

# Install rules
install(TARGETS lithium_stellarsolver_plugin
    EXPORT lithium_stellarsolver_plugin_targets
    LIBRARY DESTINATION lib/lithium/plugins
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin/lithium/plugins
)

install(FILES ${STELLARSOLVER_PLUGIN_HEADERS}
    DESTINATION include/lithium/client/stellarsolver/plugin
)
