# LithiumUtils.cmake
# Provides utility functions for the Lithium build system
#
# This project is licensed under the terms of the GPL3 license.
# Author: Max Qian
# License: GPL3

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Include color utilities
include(${CMAKE_CURRENT_LIST_DIR}/LithiumColors.cmake)

# Function to recursively add subdirectories with CMakeLists.txt
# Usage: lithium_add_subdirectories_recursive(START_DIR [OPTIONS])
# Options:
#   PATTERN_FILE - Only add directories containing this file in addition to CMakeLists.txt
#   MESSAGE_PREFIX - Custom prefix for log messages
#   EXCLUDE_DIRS - List of directory names to exclude
function(lithium_add_subdirectories_recursive START_DIR)
    set(options "")
    set(oneValueArgs PATTERN_FILE MESSAGE_PREFIX)
    set(multiValueArgs EXCLUDE_DIRS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_MESSAGE_PREFIX)
        set(ARG_MESSAGE_PREFIX "Adding subdirectory")
    endif()

    file(GLOB entries "${START_DIR}/*")
    foreach(entry ${entries})
        if(IS_DIRECTORY ${entry})
            get_filename_component(dir_name ${entry} NAME)

            # Check if directory is in exclude list
            set(is_excluded FALSE)
            if(ARG_EXCLUDE_DIRS)
                foreach(exclude_dir ${ARG_EXCLUDE_DIRS})
                    if("${dir_name}" STREQUAL "${exclude_dir}")
                        set(is_excluded TRUE)
                        break()
                    endif()
                endforeach()
            endif()

            if(is_excluded)
                continue()
            endif()

            set(should_add FALSE)

            # Check if CMakeLists.txt exists
            if(EXISTS "${entry}/CMakeLists.txt")
                # If pattern file is specified, check if it exists too
                if(ARG_PATTERN_FILE)
                    if(EXISTS "${entry}/${ARG_PATTERN_FILE}")
                        set(should_add TRUE)
                    endif()
                else()
                    set(should_add TRUE)
                endif()
            endif()

            if(should_add)
                lithium_message(STATUS "${ARG_MESSAGE_PREFIX}: ${BoldCyan}${dir_name}${ColorReset}")
                add_subdirectory(${entry})
            endif()
        endif()
    endforeach()
endfunction()

# Function to create a standardized library target
# Usage: lithium_add_library(
#   NAME <target_name>
#   SOURCES <source_files>
#   [HEADERS <header_files>]
#   [LIBRARIES <libraries_to_link>]
#   [INCLUDES <include_directories>]
#   [VERSION <version>]
#   [TYPE STATIC|SHARED|MODULE]
#   [PUBLIC_HEADERS]
# )
function(lithium_add_library)
    set(options PUBLIC_HEADERS)
    set(oneValueArgs NAME VERSION TYPE DESCRIPTION)
    set(multiValueArgs SOURCES HEADERS LIBRARIES INCLUDES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "lithium_add_library: NAME is required")
    endif()
    
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "lithium_add_library: SOURCES is required for ${ARG_NAME}")
    endif()
    
    # Set default values
    if(NOT ARG_VERSION)
        set(ARG_VERSION "${PROJECT_VERSION}")
    endif()
    
    if(NOT ARG_TYPE)
        set(ARG_TYPE STATIC)
    endif()
    
    # Print library creation message
    lithium_message(STATUS "Creating ${ARG_TYPE} library: ${BoldMagenta}${ARG_NAME}${ColorReset}")
    if(ARG_DESCRIPTION)
        message(STATUS "  Description: ${ARG_DESCRIPTION}")
    endif()
    
    # Create the library
    add_library(${ARG_NAME} ${ARG_TYPE} ${ARG_SOURCES} ${ARG_HEADERS})
    
    # Set position independent code
    set_property(TARGET ${ARG_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
    
    # Set target properties
    set_target_properties(${ARG_NAME} PROPERTIES
        VERSION ${ARG_VERSION}
        SOVERSION 1
        OUTPUT_NAME ${ARG_NAME}
    )
    
    # Add include directories
    if(ARG_INCLUDES)
        target_include_directories(${ARG_NAME} PUBLIC ${ARG_INCLUDES})
    else()
        target_include_directories(${ARG_NAME} PUBLIC 
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
    endif()
    
    # Link libraries
    if(ARG_LIBRARIES)
        target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBRARIES})
    endif()
    
    # Install rules
    install(TARGETS ${ARG_NAME}
        EXPORT ${ARG_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Install public headers if specified
    if(ARG_PUBLIC_HEADERS AND ARG_HEADERS)
        install(FILES ${ARG_HEADERS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${ARG_NAME}
        )
    endif()
    
    message(STATUS "  Sources: ${ARG_SOURCES}")
    if(ARG_LIBRARIES)
        message(STATUS "  Links: ${ARG_LIBRARIES}")
    endif()
endfunction()

# Function to find and report a package with formatted output
function(lithium_find_package PACKAGE_NAME)
    set(options REQUIRED QUIET)
    set(oneValueArgs VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    lithium_message(INFO "Searching for package: ${BoldYellow}${PACKAGE_NAME}${ColorReset}")
    
    # Build find_package arguments
    set(find_args ${PACKAGE_NAME})
    
    if(ARG_VERSION)
        list(APPEND find_args ${ARG_VERSION})
    endif()
    
    if(ARG_REQUIRED)
        list(APPEND find_args REQUIRED)
    endif()
    
    if(ARG_QUIET)
        list(APPEND find_args QUIET)
    endif()
    
    if(ARG_COMPONENTS)
        list(APPEND find_args COMPONENTS ${ARG_COMPONENTS})
    endif()
    
    # Find the package
    find_package(${find_args})
    
    # Report result
    if(${PACKAGE_NAME}_FOUND OR ${${PACKAGE_NAME}_FOUND})
        lithium_message(SUCCESS "Found ${PACKAGE_NAME}")
        if(${PACKAGE_NAME}_VERSION)
            message(STATUS "  Version: ${${PACKAGE_NAME}_VERSION}")
        endif()
    else()
        if(ARG_REQUIRED)
            lithium_message(FATAL_ERROR "${PACKAGE_NAME} not found!")
        else()
            lithium_message(WARNING "${PACKAGE_NAME} not found")
        endif()
    endif()
endfunction()

# Function to print a summary of all found/not found packages
function(lithium_print_dependency_summary)
    lithium_print_subsection("Dependency Summary")
    # This would need to track found packages, but CMake doesn't have a built-in way
    # to enumerate all found packages. Projects should call this with their own list.
endfunction()

# Function to add compiler warnings in a standardized way
function(lithium_add_compiler_warnings TARGET_NAME)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET_NAME} PRIVATE
            /W4
            /wd4100  # Unreferenced formal parameter
        )
    endif()
endfunction()

# Function to enable LTO for a target
function(lithium_enable_lto TARGET_NAME)
    if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET_NAME} PRIVATE -flto)
            target_link_options(${TARGET_NAME} PRIVATE -flto)
            lithium_message(INFO "LTO enabled for ${TARGET_NAME}")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            target_compile_options(${TARGET_NAME} PRIVATE /GL)
            target_link_options(${TARGET_NAME} PRIVATE /LTCG)
            lithium_message(INFO "LTCG enabled for ${TARGET_NAME}")
        endif()
    endif()
endfunction()

# Function to configure IDE folder structure
function(lithium_set_target_folder TARGET_NAME FOLDER_NAME)
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${FOLDER_NAME})
endfunction()

# Macro to start a component build
macro(lithium_component_begin COMPONENT_NAME COMPONENT_VERSION)
    lithium_print_subsection("Configuring ${COMPONENT_NAME}")
    project(${COMPONENT_NAME} VERSION ${COMPONENT_VERSION} LANGUAGES C CXX)
endmacro()

# Function to print component completion
function(lithium_component_end COMPONENT_NAME)
    lithium_message(SUCCESS "Configured ${COMPONENT_NAME}")
    message(STATUS "")
endfunction()

# Function to validate source files exist
function(lithium_validate_sources)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    foreach(source ${ARG_SOURCES})
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            lithium_message(WARNING "Source file not found: ${source}")
        endif()
    endforeach()
endfunction()

# Function to add a test with Google Test
# Usage: lithium_add_test(
#   NAME <test_name>
#   SOURCES <source_files>
#   [LIBS <libraries_to_link>]
#   [INCLUDES <include_directories>]
# )
function(lithium_add_test)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LIBS INCLUDES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "lithium_add_test: NAME is required")
    endif()

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "lithium_add_test: SOURCES is required for ${ARG_NAME}")
    endif()

    # Create the test executable
    add_executable(${ARG_NAME} ${ARG_SOURCES})

    # Link libraries
    target_link_libraries(${ARG_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ${ARG_LIBS}
    )

    # Add include directories
    if(ARG_INCLUDES)
        target_include_directories(${ARG_NAME} PRIVATE ${ARG_INCLUDES})
    endif()

    # Register with CTest
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})

    # Set IDE folder
    set_target_properties(${ARG_NAME} PROPERTIES FOLDER "Tests")

    lithium_message(STATUS "  ├─ Test: ${BoldGreen}${ARG_NAME}${ColorReset}")
endfunction()
