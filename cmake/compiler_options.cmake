# ============================================================================
# Compiler Options Configuration
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/LithiumColors.cmake)

# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    lithium_message(INFO "Setting build type to 'Debug' (default)")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

lithium_message(STATUS "Build Type: ${BoldYellow}${CMAKE_BUILD_TYPE}${ColorReset}")

# Check and set C++ standard
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)
check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)

if(HAS_CXX23_FLAG)
    set(CMAKE_CXX_STANDARD 23)
    lithium_message(SUCCESS "Using C++23 standard")
elseif(HAS_CXX20_FLAG)
    set(CMAKE_CXX_STANDARD 20)
    lithium_message(SUCCESS "Using C++20 standard")
else()
    lithium_message(FATAL_ERROR "C++20 standard or higher is required!")
endif()

# Check and set compiler version
function(lithium_check_compiler_version)
    lithium_print_subsection("Compiler Detection")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(GCC_VERSION VERSION_LESS "13.0")
            lithium_message(WARNING "g++ version ${GCC_VERSION} is too old. Trying to find a higher version.")
            find_program(GCC_COMPILER NAMES g++-13 g++-14 g++-15)
            if(GCC_COMPILER)
                set(CMAKE_CXX_COMPILER ${GCC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                lithium_message(SUCCESS "Using found g++ compiler: ${GCC_COMPILER}")
            else()
                lithium_message(FATAL_ERROR "g++ version 13.0 or higher is required")
            endif()
        else()
            lithium_message(SUCCESS "Using g++ version ${BoldGreen}${GCC_VERSION}${ColorReset}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} --version
            OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT CLANG_VERSION_OUTPUT MATCHES "clang version ([0-9]+\\.[0-9]+)")
            lithium_message(FATAL_ERROR "Unable to determine Clang version.")
        endif()
        set(CLANG_VERSION "${CMAKE_MATCH_1}")
        if(CLANG_VERSION VERSION_LESS "16.0")
            lithium_message(WARNING "Clang version ${CLANG_VERSION} is too old. Trying to find a higher version.")
            find_program(CLANG_COMPILER NAMES clang-17 clang-18 clang-19)
            if(CLANG_COMPILER)
                set(CMAKE_CXX_COMPILER ${CLANG_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                lithium_message(SUCCESS "Using found Clang compiler: ${CLANG_COMPILER}")
            else()
                lithium_message(FATAL_ERROR "Clang version 16.0 or higher is required")
            endif()
        else()
            lithium_message(SUCCESS "Using Clang version ${BoldGreen}${CLANG_VERSION}${ColorReset}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.28)
            lithium_message(WARNING "MSVC version ${CMAKE_CXX_COMPILER_VERSION} is too old. Trying to find a newer version.")
            find_program(MSVC_COMPILER NAMES cl)
            if(MSVC_COMPILER)
                execute_process(
                    COMMAND ${MSVC_COMPILER} /?
                    OUTPUT_VARIABLE MSVC_VERSION_OUTPUT
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                if(MSVC_VERSION_OUTPUT MATCHES "Version ([0-9]+\\.[0-9]+)")
                    set(MSVC_VERSION "${CMAKE_MATCH_1}")
                    if(MSVC_VERSION VERSION_LESS "19.28")
                        lithium_message(FATAL_ERROR "MSVC version 19.28 (Visual Studio 2019 16.10) or higher is required")
                    else()
                        set(CMAKE_CXX_COMPILER ${MSVC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                        lithium_message(SUCCESS "Using MSVC compiler: ${MSVC_COMPILER}")
                    endif()
                else()
                    lithium_message(FATAL_ERROR "Unable to determine MSVC version.")
                endif()
            else()
                lithium_message(FATAL_ERROR "MSVC version 19.28 (Visual Studio 2019 16.10) or higher is required")
            endif()
        else()
            lithium_message(SUCCESS "Using MSVC version ${BoldGreen}${CMAKE_CXX_COMPILER_VERSION}${ColorReset}")
        endif()
    endif()
endfunction()

lithium_check_compiler_version()

# ============================================================================
# Global Compiler Settings
# ============================================================================

# Set C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
lithium_message(INFO "Using C17 standard")

# Set build architecture for non-Apple platforms
if(NOT APPLE)
    set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "Build architecture for non-Apple platforms" FORCE)
endif()

# ============================================================================
# Compiler Flag Functions
# ============================================================================

# Function to apply standard compiler flags to a target
function(lithium_apply_compiler_flags TARGET_NAME)
    # Apple-specific flags
    if(APPLE)
        check_cxx_compiler_flag(-stdlib=libc++ HAS_LIBCXX_FLAG)
        if(HAS_LIBCXX_FLAG)
            target_compile_options(${TARGET_NAME} PRIVATE -stdlib=libc++)
        endif()
    endif()

    # Warning flags
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET_NAME} PRIVATE /W4)
    endif()

    # LTO for Release builds
    if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET_NAME} PRIVATE -flto)
            target_link_options(${TARGET_NAME} PRIVATE -flto)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            target_compile_options(${TARGET_NAME} PRIVATE /GL)
            target_link_options(${TARGET_NAME} PRIVATE /LTCG)
        endif()
    endif()
endfunction()

# Function to apply strict compiler flags (for new/clean code)
function(lithium_apply_strict_flags TARGET_NAME)
    lithium_apply_compiler_flags(${TARGET_NAME})

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Werror
            -Wconversion
            -Wsign-conversion
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET_NAME} PRIVATE /WX)
    endif()
endfunction()

lithium_message(STATUS "Compiler options configured successfully")
message(STATUS "")
