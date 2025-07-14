# FindDependencies.cmake - Optimized dependency management for Lithium

# Initialize found packages cache variable (only once)
if(NOT DEFINED LITHIUM_FOUND_PACKAGES)
    set(LITHIUM_FOUND_PACKAGES "" CACHE INTERNAL "Found packages")
endif()

# Function to find and configure packages with optimization
function(lithium_find_package)
    set(options REQUIRED QUIET)
    set(oneValueArgs NAME VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(LITHIUM_PKG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(LITHIUM_PKG_REQUIRED)
        set(REQUIRED_FLAG REQUIRED)
    else()
        set(REQUIRED_FLAG "")
    endif()

    if(LITHIUM_PKG_QUIET)
        set(QUIET_FLAG QUIET)
    else()
        set(QUIET_FLAG "")
    endif()

    # Try to find the package
    if(LITHIUM_PKG_VERSION)
        find_package(${LITHIUM_PKG_NAME} ${LITHIUM_PKG_VERSION} ${REQUIRED_FLAG} ${QUIET_FLAG} COMPONENTS ${LITHIUM_PKG_COMPONENTS})
    else()
        find_package(${LITHIUM_PKG_NAME} ${REQUIRED_FLAG} ${QUIET_FLAG} COMPONENTS ${LITHIUM_PKG_COMPONENTS})
    endif()

    # Store package info for optimization
    if(${LITHIUM_PKG_NAME}_FOUND)
        message(STATUS "Found ${LITHIUM_PKG_NAME}: ${${LITHIUM_PKG_NAME}_VERSION}")

        # Get current list of found packages
        get_property(CURRENT_PACKAGES CACHE LITHIUM_FOUND_PACKAGES PROPERTY VALUE)
        if(NOT CURRENT_PACKAGES)
            set(CURRENT_PACKAGES "")
        endif()

        # Check if package is already in the list to avoid duplicates
        list(FIND CURRENT_PACKAGES ${LITHIUM_PKG_NAME} PACKAGE_INDEX)
        if(PACKAGE_INDEX EQUAL -1)
            list(APPEND CURRENT_PACKAGES ${LITHIUM_PKG_NAME})
            set(LITHIUM_FOUND_PACKAGES "${CURRENT_PACKAGES}" CACHE INTERNAL "Found packages")
        endif()
    endif()
endfunction()

# Function to setup compiler-specific optimizations
function(lithium_setup_compiler_optimizations target)
    # Enable modern C++ features
    target_compile_features(${target} PRIVATE cxx_std_23)

    # Compiler-specific optimizations
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            # Build type specific optimizations
            $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native -mtune=native -flto=auto -fno-fat-lto-objects>
            $<$<CONFIG:Debug>:-Og -g3 -fno-inline>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
            $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>

            # Warning flags
            -Wall -Wextra -Wpedantic

            # Performance optimizations
            -fno-omit-frame-pointer
            -ffast-math
            -funroll-loops
            -fprefetch-loop-arrays
            -fthread-jumps
            -fgcse-after-reload
            -fipa-cp-clone
            -floop-nest-optimize
            -ftree-loop-distribution
            -ftree-vectorize

            # Architecture-specific optimizations
            -msse4.2
            -mavx
            -mavx2

            # Modern C++23 features
            -fcoroutines
            -fconcepts

            # Security hardening
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
            -fPIE
        )

        # Clang-specific optimizations
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                $<$<CONFIG:MinSizeRel>:-Oz>  # Clang's better size optimization
                -fvectorize
            )
        endif()

        # Link-time optimizations
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-flto=auto -fuse-linker-plugin -Wl,--gc-sections -Wl,--as-needed>
            -pie
            -Wl,-z,relro
            -Wl,-z,now
            -Wl,-z,noexecstack
        )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:/O2 /DNDEBUG /GL /arch:AVX2>
            $<$<CONFIG:Debug>:/Od /Zi>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /DNDEBUG>
            $<$<CONFIG:MinSizeRel>:/O1 /DNDEBUG>
            /W4
            /favor:INTEL64
            /Oi
            /std:c++latest
        )

        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
        )
    endif()

    # Enable IPO/LTO for release builds
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()

# Function to setup target with common properties
function(lithium_setup_target target)
    lithium_setup_compiler_optimizations(${target})

    # Common properties
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        VISIBILITY_INLINES_HIDDEN ON
        CXX_VISIBILITY_PRESET hidden
    )

    # Platform-specific settings
    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
        )
    endif()

    if(UNIX AND NOT APPLE)
        target_compile_definitions(${target} PRIVATE
            _GNU_SOURCE
            _DEFAULT_SOURCE
        )
    endif()
endfunction()

# Function to add precompiled headers efficiently
function(lithium_add_pch target)
    if(USE_PRECOMPILED_HEADERS)
        target_precompile_headers(${target} PRIVATE
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
            <nlohmann/json.hpp>
        )
        message(STATUS "Precompiled headers enabled for ${target}")
    endif()
endfunction()

# Optimized package discovery with caching
macro(lithium_setup_dependencies)
    # Use pkg-config for Linux packages when available
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig QUIET)
    endif()

    # Core dependencies
    lithium_find_package(NAME Threads REQUIRED)
    lithium_find_package(NAME spdlog REQUIRED)

    # Optional performance libraries
    lithium_find_package(NAME TBB QUIET)
    if(TBB_FOUND)
        message(STATUS "Intel TBB found - enabling parallel algorithms")
        add_compile_definitions(LITHIUM_USE_TBB)
    endif()

    # OpenMP for parallel computing
    lithium_find_package(NAME OpenMP QUIET)
    if(OpenMP_FOUND AND OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found - enabling parallel computing")
        add_compile_definitions(LITHIUM_USE_OPENMP)
    endif()

    # Memory allocator optimization
    lithium_find_package(NAME jemalloc QUIET)
    if(jemalloc_FOUND)
        message(STATUS "jemalloc found - using optimized memory allocator")
        add_compile_definitions(LITHIUM_USE_JEMALLOC)
    endif()
endmacro()

# Function to print optimization summary
function(lithium_print_optimization_summary)
    message(STATUS "=== Lithium Build Optimization Summary ===")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

    if(CMAKE_BUILD_TYPE MATCHES "Release")
        message(STATUS "IPO/LTO: ${LITHIUM_IPO_ENABLED}")
    endif()

    if(USE_PRECOMPILED_HEADERS)
        message(STATUS "Precompiled Headers: Enabled")
    endif()

    if(CMAKE_UNITY_BUILD)
        message(STATUS "Unity Builds: Enabled (batch size: ${CMAKE_UNITY_BUILD_BATCH_SIZE})")
    endif()

    if(CCACHE_PROGRAM)
        message(STATUS "ccache: ${CCACHE_PROGRAM}")
    endif()

    # Clean up and display found packages
    get_property(FOUND_PACKAGES CACHE LITHIUM_FOUND_PACKAGES PROPERTY VALUE)
    if(FOUND_PACKAGES)
        # Remove any empty elements and duplicates
        list(REMOVE_DUPLICATES FOUND_PACKAGES)
        list(REMOVE_ITEM FOUND_PACKAGES "")
        string(REPLACE ";" ", " PACKAGES_STRING "${FOUND_PACKAGES}")
        message(STATUS "Found Packages: ${PACKAGES_STRING}")
    else()
        message(STATUS "Found Packages: None")
    endif()

    message(STATUS "==========================================")
endfunction()

# Function to configure profiling and benchmarking
function(lithium_setup_profiling_and_benchmarks)
    # Configure benchmarking
    if(ENABLE_BENCHMARKS)
        find_package(benchmark QUIET)
        if(benchmark_FOUND)
            message(STATUS "Google Benchmark found - enabling performance benchmarks")
            add_compile_definitions(LITHIUM_BENCHMARKS_ENABLED)

            # Benchmark-specific optimizations for release builds
            if(CMAKE_BUILD_TYPE MATCHES "Release")
                if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                    add_compile_options(
                        -O3 -DNDEBUG -march=native -mtune=native
                        -ffast-math -funroll-loops -flto=auto
                        -fno-plt -fno-semantic-interposition
                    )
                    add_link_options(
                        -flto=auto -Wl,--as-needed -Wl,--gc-sections -Wl,--strip-all
                    )
                endif()
            endif()
        else()
            message(WARNING "Google Benchmark not found - benchmarks disabled")
        endif()
    endif()

    # Configure profiling
    if(ENABLE_PROFILING)
        # Enable profiling symbols even in release builds
        add_compile_options(-g -fno-omit-frame-pointer)
        add_compile_definitions(LITHIUM_PROFILING_ENABLED)

        # Find profiling tools
        find_program(PERF_EXECUTABLE NAMES perf)
        if(PERF_EXECUTABLE)
            message(STATUS "perf found: ${PERF_EXECUTABLE}")
        endif()

        find_program(VALGRIND_EXECUTABLE NAMES valgrind)
        if(VALGRIND_EXECUTABLE)
            message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
        endif()
    endif()

    # Configure memory profiling
    if(ENABLE_MEMORY_PROFILING)
        add_compile_options(
            -fno-builtin-malloc -fno-builtin-calloc
            -fno-builtin-realloc -fno-builtin-free
        )
        add_compile_definitions(LITHIUM_MEMORY_PROFILING_ENABLED)
    endif()
endfunction()

# Function to create performance test with optimizations
function(lithium_add_performance_test test_name)
    if(ENABLE_BENCHMARKS AND benchmark_FOUND)
        add_executable(${test_name} ${ARGN})
        target_link_libraries(${test_name} benchmark::benchmark)

        # Apply performance optimizations
        lithium_setup_compiler_optimizations(${test_name})

        # Add to test suite
        add_test(NAME ${test_name} COMMAND ${test_name})

        message(STATUS "Added performance test: ${test_name}")
    else()
        message(STATUS "Skipping performance test ${test_name} - benchmarks not enabled")
    endif()
endfunction()

# Function to check and set compiler version requirements
function(lithium_check_compiler_version)
    include(CheckCXXCompilerFlag)

    # Check C++ standard support
    check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)
    check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)

    if(HAS_CXX23_FLAG)
        set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
        message(STATUS "Using C++23")
    elseif(HAS_CXX20_FLAG)
        set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
        message(STATUS "Using C++20")
    else()
        message(FATAL_ERROR "C++20 standard is required!")
    endif()

    # Check GCC version
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(GCC_VERSION VERSION_LESS "13.0")
            message(WARNING "g++ version ${GCC_VERSION} is too old. Trying to find a higher version.")
            find_program(GCC_COMPILER NAMES g++-13 g++-14 g++-15)
            if(GCC_COMPILER)
                set(CMAKE_CXX_COMPILER ${GCC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                message(STATUS "Using found g++ compiler: ${GCC_COMPILER}")
            else()
                message(FATAL_ERROR "g++ version 13.0 or higher is required")
            endif()
        else()
            message(STATUS "Using g++ version ${GCC_VERSION}")
        endif()

    # Check Clang version
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} --version
            OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT CLANG_VERSION_OUTPUT MATCHES "clang version ([0-9]+\\.[0-9]+)")
            message(FATAL_ERROR "Unable to determine Clang version.")
        endif()
        set(CLANG_VERSION "${CMAKE_MATCH_1}")
        if(CLANG_VERSION VERSION_LESS "16.0")
            message(WARNING "Clang version ${CLANG_VERSION} is too old. Trying to find a higher version.")
            find_program(CLANG_COMPILER NAMES clang++-17 clang++-18 clang++-19)
            if(CLANG_COMPILER)
                set(CMAKE_CXX_COMPILER ${CLANG_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                message(STATUS "Using found Clang compiler: ${CLANG_COMPILER}")
            else()
                message(FATAL_ERROR "Clang version 16.0 or higher is required")
            endif()
        else()
            message(STATUS "Using Clang version ${CLANG_VERSION}")
        endif()

    # Check MSVC version
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.28)
            message(FATAL_ERROR "MSVC version 19.28 (Visual Studio 2019 16.10) or higher is required")
        else()
            message(STATUS "Using MSVC version ${CMAKE_CXX_COMPILER_VERSION}")
        endif()
    endif()

    # Set C standard
    set(CMAKE_C_STANDARD 17 PARENT_SCOPE)

    # Apple-specific settings
    if(APPLE)
        check_cxx_compiler_flag(-stdlib=libc++ HAS_LIBCXX_FLAG)
        if(HAS_LIBCXX_FLAG)
            add_compile_options(-stdlib=libc++)
        endif()
    endif()
endfunction()

# Function to configure build system settings
function(lithium_configure_build_system)
    # Set default build type
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to 'Debug'.")
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type." FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    endif()

    # Set global properties
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)

    # Enable ccache if available
    if(ENABLE_CCACHE)
        find_program(CCACHE_PROGRAM ccache)
        if(CCACHE_PROGRAM)
            message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
            set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
            set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
        endif()
    endif()

    # Parallel build optimization
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N} PARENT_SCOPE)
        message(STATUS "Parallel build with ${N} cores")
    endif()

    # Unity builds
    if(ENABLE_UNITY_BUILD)
        set(CMAKE_UNITY_BUILD ON PARENT_SCOPE)
        set(CMAKE_UNITY_BUILD_BATCH_SIZE 8 PARENT_SCOPE)
        message(STATUS "Unity builds enabled")
    endif()

    # Ninja generator optimizations
    if(CMAKE_GENERATOR STREQUAL "Ninja")
        set(CMAKE_JOB_POOLS compile=8 link=2 PARENT_SCOPE)
        set(CMAKE_JOB_POOL_COMPILE compile PARENT_SCOPE)
        set(CMAKE_JOB_POOL_LINK link PARENT_SCOPE)
    endif()

    # IPO/LTO configuration
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    if(IPO_SUPPORTED)
        message(STATUS "IPO/LTO supported")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON PARENT_SCOPE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON PARENT_SCOPE)
    else()
        message(STATUS "IPO/LTO not supported: ${IPO_ERROR}")
    endif()
endfunction()
