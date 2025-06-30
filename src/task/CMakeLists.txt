# CMakeLists.txt for Lithium-Task-Simple
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Task-Simple
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_task VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
file(GLOB_RECURSE CUSTOM_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/custom/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/custom/*.hpp"
)

set(PROJECT_FILES
    generator.cpp
    sequencer.cpp
    target.cpp
    task.cpp
    generator.hpp
    sequencer.hpp
    target.hpp
    task.hpp
    ${CUSTOM_SRC}
)

# Add subdirectories for organized build
add_subdirectory(custom)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    lithium_database
    spdlog::spdlog
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${Python3_INCLUDE_DIRS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})

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
