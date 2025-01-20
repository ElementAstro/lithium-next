# CMakeLists.txt for Lithium-Device
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device
# Description: The official device module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_device VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(PROJECT_FILES
    manager.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    loguru
    yaml-cpp
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

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

function(add_subdirectories_recursively start_dir)
  file(GLOB entries "${start_dir}/*")
  foreach(entry ${entries})
    if(IS_DIRECTORY ${entry} AND EXISTS "${entry}/CMakeLists.txt")
      message(STATUS "Adding device library subdirectory: ${entry}")
      add_subdirectory(${entry})
      include_directories(${entry})
    endif()
  endforeach()
endfunction()
add_subdirectories_recursively(${CMAKE_CURRENT_SOURCE_DIR})
