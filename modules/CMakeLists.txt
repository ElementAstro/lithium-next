# ============================================================================
# CMakeLists.txt for Lithium Builtin Modules
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium Builtin Modules
# Description: A collection of useful system function modules
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium_modules C CXX)

lithium_print_subsection("Discovering Lithium Modules")
lithium_message(INFO "Scanning for modules with package descriptors...")

# Custom function to add module subdirectories
# Only adds directories that have both CMakeLists.txt and a package descriptor
function(lithium_add_module_subdirectories start_dir)
    file(GLOB entries "${start_dir}/*")
    set(module_count 0)
    
    foreach(entry ${entries})
        if(IS_DIRECTORY ${entry})
            get_filename_component(module_name ${entry} NAME)
            set(has_cmake FALSE)
            set(has_package FALSE)
            
            if(EXISTS "${entry}/CMakeLists.txt")
                set(has_cmake TRUE)
            endif()
            
            if(EXISTS "${entry}/package.json" OR EXISTS "${entry}/package.yaml")
                set(has_package TRUE)
            endif()
            
            if(has_cmake AND has_package)
                lithium_message(STATUS "  ├─ Module: ${BoldCyan}${module_name}${ColorReset}")
                add_subdirectory(${entry})
                math(EXPR module_count "${module_count} + 1")
            elseif(has_cmake)
                lithium_message(WARNING "Skipping ${module_name}: missing package descriptor")
            endif()
        endif()
    endforeach()
    
    if(module_count EQUAL 0)
        lithium_message(WARNING "No valid modules found")
    else()
        lithium_message(SUCCESS "Found ${module_count} module(s)")
    endif()
endfunction()

lithium_add_module_subdirectories(${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "")
