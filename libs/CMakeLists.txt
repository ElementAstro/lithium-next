# ============================================================================
# CMakeLists.txt for Lithium Libraries
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-libs
# Description: All of the built-in libraries including submodules
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium.libs LANGUAGES CXX)

lithium_print_subsection("Configuring Lithium Libraries")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add atom library include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)
lithium_message(INFO "Added Atom library includes")

# Recursively discover and add library subdirectories
# Note: This includes submodules (atom, thirdparty) which should not be modified
# Note: spdlog and cpptrace are excluded as they are configured via FindSpdlog.cmake
lithium_message(STATUS "Discovering library subdirectories...")
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Library"
    EXCLUDE_DIRS spdlog cpptrace
)

lithium_message(SUCCESS "Library configuration complete")
message(STATUS "")
