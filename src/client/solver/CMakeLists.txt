# CMakeLists.txt for solver plugin system
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

cmake_minimum_required(VERSION 3.20)

project(lithium_solver_core LANGUAGES CXX)

# Option for shared plugin libraries
option(LITHIUM_SOLVER_PLUGINS_SHARED "Build solver plugins as shared libraries" OFF)

# Collect source files
set(SOLVER_CORE_SOURCES
    # Plugin system
    plugin/solver_plugin_interface.cpp
    plugin/solver_plugin_loader.cpp

    # Services
    service/solver_type_registry.cpp
    service/solver_factory.cpp
    service/solver_manager.cpp
)

set(SOLVER_CORE_HEADERS
    # Common types
    common/solver_types.hpp

    # Plugin system
    plugin/solver_plugin_interface.hpp
    plugin/solver_plugin_loader.hpp

    # Services
    service/solver_type_registry.hpp
    service/solver_factory.hpp
    service/solver_manager.hpp
)

# Create static library
add_library(lithium_solver_core STATIC
    ${SOLVER_CORE_SOURCES}
    ${SOLVER_CORE_HEADERS}
)

# Alias target
add_library(lithium::solver_core ALIAS lithium_solver_core)

# Include directories
target_include_directories(lithium_solver_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lithium_solver_core
    PUBLIC
        lithium_client_common
        atom-component
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Platform-specific libraries for dynamic loading
if(WIN32)
    # Windows uses LoadLibrary/FreeLibrary from kernel32
elseif(UNIX)
    target_link_libraries(lithium_solver_core PRIVATE ${CMAKE_DL_LIBS})
endif()

# Compiler features
target_compile_features(lithium_solver_core PUBLIC cxx_std_20)

# Compile definitions
target_compile_definitions(lithium_solver_core
    PRIVATE
        $<$<BOOL:${LITHIUM_SOLVER_PLUGINS_SHARED}>:LITHIUM_SOLVER_PLUGINS_SHARED>
)

# Set properties
set_target_properties(lithium_solver_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Install rules
install(TARGETS lithium_solver_core
    EXPORT lithium_solver_core_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY common plugin service
    DESTINATION include/lithium/solver
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT lithium_solver_core_targets
    FILE lithium_solver_core-targets.cmake
    NAMESPACE lithium::
    DESTINATION lib/cmake/lithium_solver_core
)
