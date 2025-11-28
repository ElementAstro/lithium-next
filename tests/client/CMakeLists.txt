# CMakeLists.txt for client tests
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_tests)

# Find required packages
find_package(GTest REQUIRED)

# Add test subdirectories
add_subdirectory(common)
add_subdirectory(astap)
add_subdirectory(astrometry)
add_subdirectory(phd2)
add_subdirectory(indi)
add_subdirectory(ascom)

# Create aggregate test target
add_custom_target(client_tests
    DEPENDS client_common_tests
    COMMENT "Building all client tests"
)
