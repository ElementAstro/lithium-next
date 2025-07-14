# CMakeLists.txt for Component Manager
# Core component management functionality for Lithium

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Component Manager Library
# =============================================================================
set(COMPONENT_MANAGER_SOURCES
    manager.cpp
    manager_impl.cpp
)

set(COMPONENT_MANAGER_HEADERS
    manager.hpp
    manager_impl.hpp
    types.hpp
    exceptions.hpp
)

# Create the component manager library
add_library(lithium_components_manager STATIC
    ${COMPONENT_MANAGER_SOURCES}
    ${COMPONENT_MANAGER_HEADERS}
)

# Create alias for consistent naming
add_library(lithium::components::manager ALIAS lithium_components_manager)

# =============================================================================
# Target Configuration
# =============================================================================
target_include_directories(lithium_components_manager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/lithium/components/manager>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# =============================================================================
# Dependencies
# =============================================================================
find_package(Threads REQUIRED)

target_link_libraries(lithium_components_manager
    PUBLIC
        atom
        Threads::Threads
        spdlog::spdlog
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# =============================================================================
# Compiler Features
# =============================================================================
target_compile_features(lithium_components_manager PUBLIC cxx_std_20)

target_compile_definitions(lithium_components_manager
    PRIVATE
        LITHIUM_COMPONENT_MANAGER_VERSION_MAJOR=1
        LITHIUM_COMPONENT_MANAGER_VERSION_MINOR=0
        LITHIUM_COMPONENT_MANAGER_VERSION_PATCH=0
        $<$<CONFIG:Debug>:LITHIUM_COMPONENT_MANAGER_DEBUG>
)

# =============================================================================
# Position Independent Code
# =============================================================================
set_property(TARGET lithium_components_manager PROPERTY POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# Compiler Options
# =============================================================================
target_compile_options(lithium_components_manager PRIVATE
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
    target_compile_definitions(lithium_components_manager PRIVATE
        LITHIUM_PLATFORM_WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(lithium_components_manager PRIVATE
        LITHIUM_PLATFORM_LINUX
    )
elseif(APPLE)
    target_compile_definitions(lithium_components_manager PRIVATE
        LITHIUM_PLATFORM_MACOS
    )
endif()

# =============================================================================
# Installation
# =============================================================================
include(GNUInstallDirs)

# Install the library
install(TARGETS lithium_components_manager
    EXPORT lithium_components_manager_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${COMPONENT_MANAGER_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/components/manager
)

# Install export targets (temporarily disabled due to dependency issues)
# install(EXPORT lithium_components_manager_targets
#     FILE lithium_components_manager_targets.cmake
#     NAMESPACE lithium::components::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium_components_manager
# )
