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

set(CROW_ENABLE_COMPRESSION ON)
set(CROW_ENABLE_SSL ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

# Include directories
include_directories(${lithium_src_dir})
include_directories(${lithium_thirdparty_dir})
include_directories(${lithium_atom_dir})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python 3.7 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Readline REQUIRED)
find_package(Curses REQUIRED)

add_executable(lithium-next ${lithium_src_dir}/app.cpp)

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

# Include compiler options
include(cmake/compiler_options.cmake)

# Add subdirectories
add_subdirectory(libs)
add_subdirectory(modules)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
