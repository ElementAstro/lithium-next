# CMakeLists.txt for Debug Component
# Core debugging and analysis functionality for Lithium

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Debug Component Library
# =============================================================================
set(DEBUG_COMPONENT_SOURCES
    dump.cpp
    dynamic.cpp
    elf.cpp
)

set(DEBUG_COMPONENT_HEADERS
    dump.hpp
    dynamic.hpp
    elf.hpp
)

# Create the debug component library
add_library(lithium_components_debug STATIC
    ${DEBUG_COMPONENT_SOURCES}
    ${DEBUG_COMPONENT_HEADERS}
)

# Create alias for consistent naming
add_library(lithium::components::debug ALIAS lithium_components_debug)

# =============================================================================
# Target Configuration
# =============================================================================
target_include_directories(lithium_components_debug
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/lithium/components/debug>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# =============================================================================
# Dependencies
# =============================================================================
find_package(Threads REQUIRED)

target_link_libraries(lithium_components_debug
    PUBLIC
        atom::atom
        Threads::Threads
        loguru
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# =============================================================================
# Compiler Features
# =============================================================================
target_compile_features(lithium_components_debug PUBLIC cxx_std_20)

target_compile_definitions(lithium_components_debug
    PRIVATE
        LITHIUM_DEBUG_COMPONENT_VERSION_MAJOR=1
        LITHIUM_DEBUG_COMPONENT_VERSION_MINOR=0
        LITHIUM_DEBUG_COMPONENT_VERSION_PATCH=0
        $<$<CONFIG:Debug>:LITHIUM_DEBUG_COMPONENT_DEBUG>
)

# =============================================================================
# Position Independent Code
# =============================================================================
set_property(TARGET lithium_components_debug PROPERTY POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# Compiler Options
# =============================================================================
target_compile_options(lithium_components_debug PRIVATE
    # GCC/Clang warnings
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wconversion>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wsign-conversion>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wcast-align>
    
    # MSVC warnings
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
)

# =============================================================================
# Platform-specific Configuration
# =============================================================================
if(WIN32)
    target_compile_definitions(lithium_components_debug PRIVATE
        LITHIUM_PLATFORM_WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(lithium_components_debug PRIVATE
        LITHIUM_PLATFORM_LINUX
    )
elseif(APPLE)
    target_compile_definitions(lithium_components_debug PRIVATE
        LITHIUM_PLATFORM_MACOS
    )
endif()

# =============================================================================
# Installation
# =============================================================================
include(GNUInstallDirs)

# Install the library
install(TARGETS lithium_components_debug
    EXPORT lithium_components_debug_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${DEBUG_COMPONENT_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/components/debug
)

# Install export targets
install(EXPORT lithium_components_debug_targets
    FILE lithium_components_debug_targets.cmake
    NAMESPACE lithium::components::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium_components_debug
)
