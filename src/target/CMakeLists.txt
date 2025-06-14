# CMakeLists.txt for Lithium-Target
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Target
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_target VERSION 1.0.0 LANGUAGES C CXX)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
if(Eigen3_FOUND)
    message(STATUS "Eigen3 found at: ${EIGEN3_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Eigen3 not found!")
endif()

# Sources and Headers
set(PROJECT_FILES
    engine.cpp
    preference.cpp
    reader.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    atom-utils
    lithium_config
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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
