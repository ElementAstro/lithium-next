# CMakeLists.txt for Lithium-Script
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Script
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_script VERSION 1.0.0 LANGUAGES C CXX)

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)

# Sources and Headers
set(PROJECT_FILES
    check.cpp
    python_caller.cpp
    sheller.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    loguru
    yaml-cpp
    pybind11::embed
    ${CMAKE_THREAD_LIBS_INIT}
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${Python3_INCLUDE_DIRS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})

include_directories(${Python3_INCLUDE_DIRS})

# Set version properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install target
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
