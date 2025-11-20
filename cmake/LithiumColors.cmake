# LithiumColors.cmake
# Provides colored and formatted output for CMake messages
#
# This project is licensed under the terms of the GPL3 license.
# Author: Max Qian
# License: GPL3

# Check if terminal supports colors
if(NOT WIN32)
    string(ASCII 27 Esc)
    set(ColorReset "${Esc}[m")
    set(ColorBold "${Esc}[1m")
    set(Red "${Esc}[31m")
    set(Green "${Esc}[32m")
    set(Yellow "${Esc}[33m")
    set(Blue "${Esc}[34m")
    set(Magenta "${Esc}[35m")
    set(Cyan "${Esc}[36m")
    set(White "${Esc}[37m")
    set(BoldRed "${Esc}[1;31m")
    set(BoldGreen "${Esc}[1;32m")
    set(BoldYellow "${Esc}[1;33m")
    set(BoldBlue "${Esc}[1;34m")
    set(BoldMagenta "${Esc}[1;35m")
    set(BoldCyan "${Esc}[1;36m")
    set(BoldWhite "${Esc}[1;37m")
else()
    # Windows doesn't support ANSI colors in older CMake versions
    # But we'll still define them for consistency
    set(ColorReset "")
    set(ColorBold "")
    set(Red "")
    set(Green "")
    set(Yellow "")
    set(Blue "")
    set(Magenta "")
    set(Cyan "")
    set(White "")
    set(BoldRed "")
    set(BoldGreen "")
    set(BoldYellow "")
    set(BoldBlue "")
    set(BoldMagenta "")
    set(BoldCyan "")
    set(BoldWhite "")
endif()

# Function to print a colored message
function(lithium_message TYPE MESSAGE)
    if(TYPE STREQUAL "STATUS")
        message(STATUS "${BoldGreen}[Lithium]${ColorReset} ${MESSAGE}")
    elseif(TYPE STREQUAL "WARNING")
        message(WARNING "${BoldYellow}[Lithium Warning]${ColorReset} ${MESSAGE}")
    elseif(TYPE STREQUAL "ERROR")
        message(SEND_ERROR "${BoldRed}[Lithium Error]${ColorReset} ${MESSAGE}")
    elseif(TYPE STREQUAL "FATAL_ERROR")
        message(FATAL_ERROR "${BoldRed}[Lithium Fatal Error]${ColorReset} ${MESSAGE}")
    elseif(TYPE STREQUAL "INFO")
        message(STATUS "${BoldCyan}[Lithium Info]${ColorReset} ${MESSAGE}")
    elseif(TYPE STREQUAL "SUCCESS")
        message(STATUS "${BoldGreen}[Lithium Success]${ColorReset} ${MESSAGE}")
    else()
        message("${MESSAGE}")
    endif()
endfunction()

# Function to print a section header
function(lithium_print_header HEADER_TEXT)
    set(SEPARATOR "================================================================================")
    message(STATUS "${BoldCyan}${SEPARATOR}${ColorReset}")
    message(STATUS "${BoldCyan}  ${HEADER_TEXT}${ColorReset}")
    message(STATUS "${BoldCyan}${SEPARATOR}${ColorReset}")
endfunction()

# Function to print a subsection header
function(lithium_print_subsection SUBSECTION_TEXT)
    message(STATUS "${BoldBlue}--- ${SUBSECTION_TEXT} ---${ColorReset}")
endfunction()

# Function to print a key-value pair
function(lithium_print_config KEY VALUE)
    message(STATUS "${BoldWhite}  ${KEY}:${ColorReset} ${Green}${VALUE}${ColorReset}")
endfunction()

# Function to print a build configuration summary
function(lithium_print_build_config)
    lithium_print_header("Build Configuration")
    lithium_print_config("Project Name" "${PROJECT_NAME}")
    lithium_print_config("Version" "${PROJECT_VERSION}")
    lithium_print_config("Build Type" "${CMAKE_BUILD_TYPE}")
    lithium_print_config("C++ Standard" "${CMAKE_CXX_STANDARD}")
    lithium_print_config("C Standard" "${CMAKE_C_STANDARD}")
    lithium_print_config("Compiler" "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    lithium_print_config("Source Directory" "${CMAKE_SOURCE_DIR}")
    lithium_print_config("Binary Directory" "${CMAKE_BINARY_DIR}")
    lithium_print_config("Install Prefix" "${CMAKE_INSTALL_PREFIX}")
    message(STATUS "")
endfunction()
