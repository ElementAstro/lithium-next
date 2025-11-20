# ============================================================================
# CMakeLists.txt for Lithium-Next
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium
# Description: Lithium - Open Astrophotography Terminal
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium-next VERSION 1.0.0 LANGUAGES C CXX)

# ============================================================================
# CMake Module Path and Core Includes
# ============================================================================

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# Include core CMake modules
include(cmake/policies.cmake)
include(cmake/LithiumColors.cmake)
include(cmake/LithiumUtils.cmake)
include(GNUInstallDirs)

# Print project header
lithium_print_header("Lithium-Next: Open Astrophotography Terminal")
lithium_message(STATUS "Version: ${BoldYellow}${PROJECT_VERSION}${ColorReset}")
lithium_message(STATUS "Author: Max Qian")
lithium_message(STATUS "License: GPL3")
message(STATUS "")

# ============================================================================
# Project Directory Structure
# ============================================================================

set(lithium_src_dir ${CMAKE_SOURCE_DIR}/src)
set(lithium_thirdparty_dir ${CMAKE_SOURCE_DIR}/libs/thirdparty)
set(lithium_atom_dir ${CMAKE_SOURCE_DIR}/libs/atom)

lithium_print_subsection("Project Directories")
lithium_print_config("Source Directory" "${lithium_src_dir}")
lithium_print_config("Third-party Libraries" "${lithium_thirdparty_dir}")
lithium_print_config("Atom Library" "${lithium_atom_dir}")
message(STATUS "")

# ============================================================================
# Build Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Crow web framework options
set(CROW_ENABLE_COMPRESSION ON)
set(CROW_ENABLE_SSL ON)

# ============================================================================
# Include Directories (Legacy - consider migrating to target-based)
# ============================================================================

include_directories(${lithium_src_dir})
include_directories(${lithium_thirdparty_dir})
include_directories(${lithium_thirdparty_dir}/crow)
include_directories(${lithium_atom_dir})

# ============================================================================
# External Library Detection
# ============================================================================

lithium_print_subsection("Detecting External Libraries")

# Core compression libraries
find_library(LIBZ_LIBRARY NAMES z PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)
find_library(LIBBZ2_LIBRARY NAMES bz2 PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)

if(LIBZ_LIBRARY)
    lithium_message(SUCCESS "Found libz: ${LIBZ_LIBRARY}")
endif()

if(LIBBZ2_LIBRARY)
    lithium_message(SUCCESS "Found libbz2: ${LIBBZ2_LIBRARY}")
endif()

# Python and bindings
# Force pybind11 to use the modern FindPython module
set(PYBIND11_FINDPYTHON ON)
lithium_find_package(Python VERSION 3.7 COMPONENTS Interpreter Development REQUIRED)
lithium_find_package(pybind11 REQUIRED)

# Terminal libraries
lithium_find_package(Readline REQUIRED)
lithium_find_package(Curses REQUIRED)

message(STATUS "")

# ============================================================================
# Compiler Options
# ============================================================================

include(cmake/compiler_options.cmake)

# ============================================================================
# Subdirectories
# ============================================================================

lithium_print_subsection("Adding Project Subdirectories")

add_subdirectory(libs)
add_subdirectory(modules)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)

message(STATUS "")

# ============================================================================
# Main Executable
# ============================================================================

lithium_print_subsection("Configuring Main Executable")

add_executable(lithium-next ${lithium_src_dir}/app.cpp)

# Link all required libraries
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
    loguru
    ${Readline_LIBRARIES}
    ${CURSES_LIBRARIES}
)

target_include_directories(lithium-next PRIVATE ${Python_INCLUDE_DIRS})

# Apply compiler flags
lithium_apply_compiler_flags(lithium-next)

lithium_message(SUCCESS "Main executable 'lithium-next' configured")
message(STATUS "")

# ============================================================================
# IDE Configuration
# ============================================================================

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ============================================================================
# Installation Rules
# ============================================================================

install(TARGETS lithium-next
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================================
# Build Summary
# ============================================================================

lithium_print_header("Configuration Complete")
lithium_message(SUCCESS "Lithium-Next is ready to build!")
lithium_print_config("Build Type" "${CMAKE_BUILD_TYPE}")
lithium_print_config("C++ Compiler" "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
lithium_print_config("Install Prefix" "${CMAKE_INSTALL_PREFIX}")
lithium_message(INFO "Run 'cmake --build .' to build the project")
message(STATUS "")
