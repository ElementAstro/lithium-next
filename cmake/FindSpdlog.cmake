# ============================================================================
# FindSpdlog.cmake - spdlog and cpptrace dependency management
# ============================================================================
# This module provides:
#   - spdlog::spdlog          - spdlog logging library
#   - cpptrace::cpptrace      - Stack trace library (optional)
#
# Cache Variables:
#   LITHIUM_ENABLE_CPPTRACE   - Enable cpptrace for stack traces (default: ON)
#
# Compile Definitions:
#   LITHIUM_ENABLE_CPPTRACE   - Set to 1 if cpptrace is available
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/LithiumColors.cmake)

# ============================================================================
# User Configuration Options
# ============================================================================

option(LITHIUM_ENABLE_CPPTRACE "Enable cpptrace for stack traces" ON)

# ============================================================================
# spdlog Detection and Configuration
# ============================================================================

set(SPDLOG_FOUND FALSE)
set(SPDLOG_FROM_SUBMODULE FALSE)

lithium_message(INFO "Searching for spdlog...")

# Step 1: Try find_package (system/vcpkg)
find_package(spdlog CONFIG QUIET)

if(spdlog_FOUND)
    set(SPDLOG_FOUND TRUE)
    lithium_message(SUCCESS "Found spdlog via find_package (CONFIG mode)")
    if(spdlog_VERSION)
        lithium_message(STATUS "  Version: ${spdlog_VERSION}")
    endif()
else()
    # Step 2: Fallback to submodule
    set(SPDLOG_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/libs/spdlog")

    if(EXISTS "${SPDLOG_SUBMODULE_DIR}/CMakeLists.txt")
        lithium_message(INFO "Using bundled spdlog from submodule")

        # Configure spdlog build options
        set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(SPDLOG_ENABLE_PCH ON CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
        set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)

        # Add subdirectory
        add_subdirectory(${SPDLOG_SUBMODULE_DIR} ${CMAKE_BINARY_DIR}/spdlog EXCLUDE_FROM_ALL)

        set(SPDLOG_FOUND TRUE)
        set(SPDLOG_FROM_SUBMODULE TRUE)
        lithium_message(SUCCESS "spdlog configured from submodule")
    else()
        message(FATAL_ERROR
            "spdlog not found!\n"
            "Options:\n"
            "  1. Install via package manager: vcpkg install spdlog / apt install libspdlog-dev\n"
            "  2. Initialize submodule: git submodule update --init libs/spdlog")
    endif()
endif()

# ============================================================================
# cpptrace Detection and Configuration
# ============================================================================

set(CPPTRACE_FOUND FALSE)

if(LITHIUM_ENABLE_CPPTRACE)
    lithium_message(INFO "Searching for cpptrace...")

    # Step 1: Try find_package
    find_package(cpptrace CONFIG QUIET)

    if(cpptrace_FOUND)
        set(CPPTRACE_FOUND TRUE)
        lithium_message(SUCCESS "Found cpptrace via find_package")
        if(cpptrace_VERSION)
            lithium_message(STATUS "  Version: ${cpptrace_VERSION}")
        endif()
    else()
        # Step 2: Fallback to submodule
        set(CPPTRACE_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/libs/cpptrace")

        if(EXISTS "${CPPTRACE_SUBMODULE_DIR}/CMakeLists.txt")
            lithium_message(INFO "Using bundled cpptrace from submodule")

            # Configure cpptrace build options
            set(CPPTRACE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
            set(CPPTRACE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
            set(CPPTRACE_STATIC ON CACHE BOOL "" FORCE)
            # Disable C++20 modules scanning for MinGW Makefiles compatibility
            set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "" FORCE)

            # Add subdirectory
            add_subdirectory(${CPPTRACE_SUBMODULE_DIR} ${CMAKE_BINARY_DIR}/cpptrace EXCLUDE_FROM_ALL)

            set(CPPTRACE_FOUND TRUE)
            lithium_message(SUCCESS "cpptrace configured from submodule")
        else()
            lithium_message(WARNING
                "cpptrace not found. Stack traces will be disabled.\n"
                "To enable: git submodule add https://github.com/jeremy-rifkin/cpptrace libs/cpptrace")
        endif()
    endif()
else()
    lithium_message(INFO "cpptrace disabled by LITHIUM_ENABLE_CPPTRACE=OFF")
endif()

# ============================================================================
# Export Compile Definitions
# ============================================================================

if(CPPTRACE_FOUND)
    add_compile_definitions(LITHIUM_ENABLE_CPPTRACE=1)
else()
    add_compile_definitions(LITHIUM_ENABLE_CPPTRACE=0)
endif()

# ============================================================================
# Summary Report
# ============================================================================

lithium_print_subsection("Logging System Configuration")
lithium_print_config("spdlog" "${SPDLOG_FOUND} (submodule: ${SPDLOG_FROM_SUBMODULE})")
lithium_print_config("cpptrace" "${CPPTRACE_FOUND}")
