# Debug Components Library
# This library contains debugging utilities including core dump analysis,
# dynamic library parsing, and ELF parsing.

cmake_minimum_required(VERSION 3.20)

set(DEBUG_SOURCES
    dump.cpp
    dynamic.cpp
)

set(DEBUG_HEADERS
    debug.hpp
    dump.hpp
    dynamic.hpp
)

# Add ELF parser only on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND DEBUG_SOURCES elf.cpp)
    list(APPEND DEBUG_HEADERS elf.hpp)
endif()

# Create the debug library
add_library(lithium_debug STATIC
    ${DEBUG_SOURCES}
    ${DEBUG_HEADERS}
)

# Set target properties
target_include_directories(lithium_debug
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_debug
    PUBLIC
        atom
        loguru
        spdlog::spdlog
)

# Set compiler features
target_compile_features(lithium_debug PUBLIC cxx_std_20)

# Add alias for better namespace support
add_library(lithium::debug ALIAS lithium_debug)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(lithium_debug PRIVATE _WIN32_WINNT=0x0601)
endif()
