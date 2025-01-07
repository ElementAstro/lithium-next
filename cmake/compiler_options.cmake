# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug'.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Check and set C++ standard
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)
check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)

if(HAS_CXX23_FLAG)
    set(CMAKE_CXX_STANDARD 23)
elseif(HAS_CXX20_FLAG)
    set(CMAKE_CXX_STANDARD 20)
else()
    message(FATAL_ERROR "C++20 standard is required!")
endif()

# Check and set compiler version
function(check_compiler_version)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(GCC_VERSION VERSION_LESS "13.0")
            message(WARNING "g++ version ${GCC_VERSION} is too old. Trying to find a higher version.")
            find_program(GCC_COMPILER NAMES g++-13 g++-14 g++-15)
            if(GCC_COMPILER)
                set(CMAKE_CXX_COMPILER ${GCC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                message(STATUS "Using found g++ compiler: ${GCC_COMPILER}")
            else()
                message(FATAL_ERROR "g++ version 13.0 or higher is required")
            endif()
        else()
            message(STATUS "Using g++ version ${GCC_VERSION}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} --version
            OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT CLANG_VERSION_OUTPUT MATCHES "clang version ([0-9]+\\.[0-9]+)")
            message(FATAL_ERROR "Unable to determine Clang version.")
        endif()
        set(CLANG_VERSION "${CMAKE_MATCH_1}")
        if(CLANG_VERSION VERSION_LESS "16.0")
            message(WARNING "Clang version ${CLANG_VERSION} is too old. Trying to find a higher version.")
            find_program(CLANG_COMPILER NAMES clang-17 clang-18 clang-19)
            if(CLANG_COMPILER)
                set(CMAKE_CXX_COMPILER ${CLANG_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                message(STATUS "Using found Clang compiler: ${CLANG_COMPILER}")
            else()
                message(FATAL_ERROR "Clang version 16.0 or higher is required")
            endif()
        else()
            message(STATUS "Using Clang version ${CLANG_VERSION}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.28)
            message(WARNING "MSVC version ${CMAKE_CXX_COMPILER_VERSION} is too old. Trying to find a newer version.")
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
                        message(FATAL_ERROR "MSVC version 19.28 (Visual Studio 2019 16.10) or higher is required")
                    else()
                        set(CMAKE_CXX_COMPILER ${MSVC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                        message(STATUS "Using MSVC compiler: ${MSVC_COMPILER}")
                    endif()
                else()
                    message(FATAL_ERROR "Unable to determine MSVC version.")
                endif()
            else()
                message(FATAL_ERROR "MSVC version 19.28 (Visual Studio 2019 16.10) or higher is required")
            endif()
        else()
            message(STATUS "Using MSVC version ${CMAKE_CXX_COMPILER_VERSION}")
        endif()
    endif()
endfunction()

check_compiler_version()

# Set C standard
set(CMAKE_C_STANDARD 17)

# Set compiler flags for Apple platform
if(APPLE)
    check_cxx_compiler_flag(-stdlib=libc++ HAS_LIBCXX_FLAG)
    if(HAS_LIBCXX_FLAG)
        target_compile_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
    endif()
endif()

# Set build architecture for non-Apple platforms
if(NOT APPLE)
    set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "Build architecture for non-Apple platforms" FORCE)
endif()

# Additional compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

# Enable ASan for Debug builds
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MINGW)
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    endif()
endif()

# Enable LTO for Release builds
if(CMAKE_BUILD_TYPE MATCHES "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -flto)
        target_link_options(${PROJECT_NAME} PRIVATE -flto)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /GL)
        target_link_options(${PROJECT_NAME} PRIVATE /LTCG)
    endif()
endif()