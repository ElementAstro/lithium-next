cmake_minimum_required(VERSION 3.20)

# ASI Filter Wheel module
project(lithium_asi_filterwheel LANGUAGES CXX)

# Add components subdirectory
add_subdirectory(components)

set(ASI_FILTERWHEEL_SOURCES
    asi_filterwheel.hpp
    asi_filterwheel.cpp
    controller/asi_filterwheel_controller.hpp
    controller/asi_filterwheel_controller.cpp
    controller/asi_filterwheel_controller_v2.hpp
    controller/asi_filterwheel_controller_v2.cpp
)

# Create shared library
add_library(asi_filterwheel SHARED ${ASI_FILTERWHEEL_SOURCES})
set_property(TARGET asi_filterwheel PROPERTY POSITION_INDEPENDENT_CODE 1)

# Target properties
target_compile_features(asi_filterwheel PRIVATE cxx_std_20)
target_compile_options(asi_filterwheel PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Find and link ASI EFW SDK if available
find_library(ASI_EFW_LIBRARY 
    NAMES EFW_filter libEFW_filter
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI EFW SDK library"
)

if(ASI_EFW_LIBRARY)
    message(STATUS "Found ASI EFW SDK: ${ASI_EFW_LIBRARY}")
    add_compile_definitions(LITHIUM_ASI_EFW_ENABLED)
    target_link_libraries(asi_filterwheel PRIVATE ${ASI_EFW_LIBRARY})
    
    # Find EFW headers
    find_path(ASI_EFW_INCLUDE_DIR
        NAMES EFW_filter.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
        DOC "ASI EFW SDK headers"
    )
    
    if(ASI_EFW_INCLUDE_DIR)
        target_include_directories(asi_filterwheel PRIVATE ${ASI_EFW_INCLUDE_DIR})
    endif()
else()
    message(STATUS "ASI EFW SDK not found, using stub implementation")
endif()

# Link common libraries
target_link_libraries(asi_filterwheel PUBLIC
    atom
    atom
    pthread
    asi_filterwheel_components  # Link the modular components
)

# Include directories
target_include_directories(asi_filterwheel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS asi_filterwheel
    EXPORT asi_filterwheel_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/filterwheel
)

install(FILES asi_filterwheel.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/filterwheel
)

install(EXPORT asi_filterwheel_targets
    FILE asi_filterwheel_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
