# ============================================================================
# CMakeLists.txt for Lithium Client Libraries
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium Client Libraries
# Description: Client-side protocol implementations (INDI, StellarSolver, etc.)
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium.client LANGUAGES CXX)

lithium_print_subsection("Configuring Client Libraries")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Note: This includes atom library paths for client implementations
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

# Add common base library first
add_subdirectory(common)

# Recursively discover and add client library subdirectories
lithium_message(INFO "Discovering client implementations...")
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Client lib"
    EXCLUDE_DIRS "common"
)

lithium_message(SUCCESS "Client libraries configured")
message(STATUS "")
