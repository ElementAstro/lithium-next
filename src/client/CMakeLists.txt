# CMakeLists.txt for Lithium-libs
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-libs
# Description: All of the built-in libs libraries
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium.libs LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

function(add_subdirectories_recursively start_dir)
  file(GLOB entries "${start_dir}/*")
  foreach(entry ${entries})
    if(IS_DIRECTORY ${entry} AND EXISTS "${entry}/CMakeLists.txt")
      message(STATUS "Adding client library subdirectory: ${entry}")
      add_subdirectory(${entry})
      include_directories(${entry})
    endif()
  endforeach()
endfunction()
add_subdirectories_recursively(${CMAKE_CURRENT_SOURCE_DIR})
