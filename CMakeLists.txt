# CMakeLists.txt for Lithium-Next
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium
# Description: Lithium - Open Astrophotography Terminal
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium-next VERSION 1.0.0 LANGUAGES C CXX)

# Set policies
include(cmake/policies.cmake)

# Set project directories
set(lithium_src_dir ${CMAKE_SOURCE_DIR}/src)
set(lithium_thirdparty_dir ${CMAKE_SOURCE_DIR}/libs/thirdparty)
set(lithium_atom_dir ${CMAKE_SOURCE_DIR}/libs/atom)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

# Include directories
include_directories(${lithium_src_dir})
include_directories(${lithium_thirdparty_dir})
include_directories(${lithium_atom_dir})

include_directories(${lithium_thirdparty_dir}/oatpp)
include_directories(${lithium_thirdparty_dir}/oatpp-zlib)
include_directories(${lithium_thirdparty_dir}/oatpp-openssl)
include_directories(${lithium_thirdparty_dir}/oatpp-websocket)
include_directories(${lithium_thirdparty_dir}/oatpp-swagger)

add_executable(lithium-next ${lithium_src_dir}/app.cpp)

# Include compiler options
include(cmake/compiler_options.cmake)

# Add subdirectories
add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)