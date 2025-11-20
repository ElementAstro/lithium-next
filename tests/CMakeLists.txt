# ============================================================================
# CMakeLists.txt for Lithium Tests
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Tests
# Description: Unit tests for Lithium components
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium.tests LANGUAGES CXX)

lithium_print_subsection("Configuring Unit Tests")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add test-specific CMake module paths
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

# Recursively discover and add test subdirectories
lithium_message(INFO "Discovering test suites...")
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Test suite"
)

lithium_message(SUCCESS "Unit tests configured")
message(STATUS "")
