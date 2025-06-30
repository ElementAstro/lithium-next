# ===================================================================================================
# CMakeLists.txt for Lithium-Next - Unified Build System
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium
# Description: Lithium - Open Astrophotography Terminal  
# Author: Max Qian
# License: GPL3
# ===================================================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium-next VERSION 1.0.0 LANGUAGES C CXX)

# ===================================================================================================
# BUILD SYSTEM CONFIGURATION
# ===================================================================================================

# Set modern CMake policies
cmake_policy(SET CMP0069 NEW) # Enable LTO policy
cmake_policy(SET CMP0083 NEW) # Enable PIE policy

# Set build options and policies
include(cmake/policies.cmake)

# Build configuration options
option(ENABLE_BENCHMARKS "Enable performance benchmarks" OFF)
option(ENABLE_PROFILING "Enable performance profiling support" OFF)
option(ENABLE_MEMORY_PROFILING "Enable memory profiling support" OFF)
option(USE_PRECOMPILED_HEADERS "Use precompiled headers for faster builds" ON)
option(ENABLE_UNITY_BUILD "Enable unity builds for faster compilation" OFF)
option(ENABLE_CCACHE "Enable ccache for faster rebuilds" ON)

# Set project directories
set(lithium_src_dir ${CMAKE_SOURCE_DIR}/src)
set(lithium_thirdparty_dir ${CMAKE_SOURCE_DIR}/libs/thirdparty)
set(lithium_atom_dir ${CMAKE_SOURCE_DIR}/libs/atom)

# Module paths
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# ===================================================================================================
# COMPILER AND LANGUAGE CONFIGURATION
# ===================================================================================================

# Include optimized build system
include(cmake/LithiumOptimizations.cmake)

# Configure build system settings
lithium_configure_build_system()

# Check compiler version and set standards
lithium_check_compiler_version()

# Setup dependencies with optimization
lithium_setup_dependencies()

# Configure profiling and benchmarking
lithium_setup_profiling_and_benchmarks()

# ===================================================================================================
# BUILD SYSTEM OPTIMIZATIONS
# ===================================================================================================

# Note: Build system optimizations are now handled by lithium_configure_build_system()
# function from LithiumOptimizations.cmake

# ===================================================================================================
# COMPILER OPTIMIZATIONS
# ===================================================================================================

# Note: Compiler optimizations are now handled by lithium_setup_compiler_optimizations()
# function from LithiumOptimizations.cmake when applied to targets

# ===================================================================================================
# PROFILING AND BENCHMARKING CONFIGURATION
# ===================================================================================================

# Note: Profiling and benchmarking configuration is now handled by 
# lithium_setup_profiling_and_benchmarks() from LithiumOptimizations.cmake

# ===================================================================================================
# DEPENDENCY MANAGEMENT
# ===================================================================================================

# Include directories
include_directories(${lithium_src_dir})
include_directories(${lithium_thirdparty_dir})
include_directories(${lithium_thirdparty_dir}/crow)
include_directories(${lithium_atom_dir})

# Crow configuration
set(CROW_ENABLE_COMPRESSION ON)
set(CROW_ENABLE_SSL ON)

# Core dependencies - using lithium_find_package for consistency
lithium_find_package(NAME Python REQUIRED VERSION 3.7 COMPONENTS Interpreter Development)
lithium_find_package(NAME pybind11 REQUIRED)
lithium_find_package(NAME Readline REQUIRED)
lithium_find_package(NAME Curses REQUIRED)

# Platform-specific libraries
find_library(LIBZ_LIBRARY NAMES z PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)
find_library(LIBBZ2_LIBRARY NAMES bz2 PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)

# ===================================================================================================
# TARGET CONFIGURATION
# ===================================================================================================

# Create main executable
add_executable(lithium-next ${lithium_src_dir}/app.cpp)

# Apply optimized target setup
lithium_setup_target(lithium-next)

# Add precompiled headers if enabled
lithium_add_pch(lithium-next)

# Link libraries
target_link_libraries(lithium-next PRIVATE
    pybind11::module
    pybind11::lto
    lithium_components
    lithium_config
    lithium_database
    lithium_debug
    lithium_device
    lithium_script
    lithium_server
    lithium_target
    lithium_task
    lithium_tools
    atom
    spdlog::spdlog
    Threads::Threads
    ${Readline_LIBRARIES}
    ${CURSES_LIBRARIES}
)

# Include directories for main target
target_include_directories(lithium-next PRIVATE ${Python_INCLUDE_DIRS})

# Add precompiled headers if enabled
if(USE_PRECOMPILED_HEADERS)
    target_precompile_headers(lithium-next PRIVATE
        # Standard library headers
        <algorithm>
        <array>
        <chrono>
        <filesystem>
        <fstream>
        <functional>
        <iostream>
        <memory>
        <string>
        <string_view>
        <thread>
        <unordered_map>
        <unordered_set>
        <vector>
        
        # Third-party headers
        <spdlog/spdlog.h>
    )
    message(STATUS "Precompiled headers enabled for lithium-next")
endif()

# Platform-specific configurations
if(WIN32)
    target_compile_definitions(lithium-next PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(lithium-next PRIVATE
        _GNU_SOURCE
        _DEFAULT_SOURCE
    )
endif()

# Optional library linking
if(TBB_FOUND)
    target_link_libraries(lithium-next PRIVATE TBB::tbb)
endif()

if(OpenMP_FOUND)
    target_link_libraries(lithium-next PRIVATE OpenMP::OpenMP_CXX)
endif()

if(JEMALLOC_LIBRARY)
    target_link_libraries(lithium-next PRIVATE ${JEMALLOC_LIBRARY})
endif()

# ===================================================================================================
# SUBDIRECTORIES
# ===================================================================================================

# Add project subdirectories
add_subdirectory(libs)
add_subdirectory(modules)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)

# ===================================================================================================
# UTILITY FUNCTIONS
# ===================================================================================================

# Function to create performance test
function(lithium_add_performance_test test_name)
    if(ENABLE_BENCHMARKS AND benchmark_FOUND)
        add_executable(${test_name} ${ARGN})
        target_link_libraries(${test_name} benchmark::benchmark)
        
        # Apply performance optimizations
        target_compile_options(${test_name} PRIVATE
            -O3 -DNDEBUG -march=native -ffast-math
        )
        
        # Add to test suite
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endfunction()

# Function to setup target with common properties
function(lithium_setup_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
    
    if(IPO_SUPPORTED AND CMAKE_BUILD_TYPE MATCHES "Release")
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()

# ===================================================================================================
# BUILD SUMMARY
# ===================================================================================================

# Print optimization summary using consolidated function
lithium_print_optimization_summary()

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
