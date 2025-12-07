# Core Components Library
# This library contains the core component management functionality
# including module loading, dependency management, version handling, and file tracking.

cmake_minimum_required(VERSION 3.20)

set(CORE_SOURCES
    dependency.cpp
    loader.cpp
    tracker.cpp
    version.cpp
)

set(CORE_HEADERS
    core.hpp
    dependency.hpp
    loader.hpp
    module.hpp
    tracker.hpp
    types.hpp
    version.hpp
)

# Create the core library
add_library(lithium_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Set target properties
target_include_directories(lithium_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_core
    PUBLIC
        atom
        loguru
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Set compiler features
target_compile_features(lithium_core PUBLIC cxx_std_20)

# Add alias for better namespace support
add_library(lithium::core ALIAS lithium_core)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(lithium_core PRIVATE _WIN32_WINNT=0x0601)
endif()
