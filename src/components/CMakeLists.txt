# CMakeLists.txt for Lithium-Components
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Components
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_components VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(PROJECT_FILES
    dependency.cpp
    loader.cpp
    tracker.cpp
    version.cpp

    debug/dump.cpp
    debug/dynamic.cpp
    debug/elf.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add manager subdirectory
add_subdirectory(manager)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS} lithium_components_manager)

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
