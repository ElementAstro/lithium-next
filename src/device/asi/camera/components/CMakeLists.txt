cmake_minimum_required(VERSION 3.20)

# ASI Camera Components
project(lithium_asi_camera_components LANGUAGES CXX)

# Component source files
set(COMPONENT_SOURCES
    hardware_interface.cpp
    exposure_manager.cpp
    video_manager.cpp
    temperature_controller.cpp
    property_manager.cpp
    sequence_manager.cpp      # Using existing header but needs implementation
    image_processor.cpp
)

# Component header files
set(COMPONENT_HEADERS
    hardware_interface.hpp
    exposure_manager.hpp
    video_manager.hpp
    temperature_controller.hpp
    property_manager.hpp
    sequence_manager.hpp
    image_processor.hpp
)

# Create shared library for ASI camera components
add_library(asi_camera_components SHARED ${COMPONENT_SOURCES})
set_property(TARGET asi_camera_components PROPERTY POSITION_INDEPENDENT_CODE 1)

# Target properties
target_compile_features(asi_camera_components PRIVATE cxx_std_20)
target_compile_options(asi_camera_components PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Link libraries
target_link_libraries(asi_camera_components PUBLIC
    loguru
    atom-system
    atom-io
    atom-utils
    atom-component
    atom-error
    atom-type
)

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(asi_camera_components PRIVATE Threads::Threads)

# Include directories
target_include_directories(asi_camera_components PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Installation
install(TARGETS asi_camera_components
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${COMPONENT_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/asi/camera/components
)

# Set library properties
set_target_properties(asi_camera_components PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(asi_camera_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(asi_camera_components
    PUBLIC
        loguru
        atom-system
        atom-io
        atom-utils
        atom-component
        atom-error
)

# ASI SDK detection and linking
find_path(ASI_INCLUDE_DIR ASICamera2.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES asi libasi
    DOC "ASI SDK include directory"
)

find_library(ASI_LIBRARY
    NAMES ASICamera2 libasicamera
    PATHS /usr/lib /usr/local/lib
    PATH_SUFFIXES asi
    DOC "ASI SDK library"
)

if(ASI_INCLUDE_DIR AND ASI_LIBRARY)
    set(ASI_FOUND TRUE)
    message(STATUS "Found ASI SDK: ${ASI_LIBRARY}")
    target_compile_definitions(asi_camera_components PUBLIC LITHIUM_ASI_CAMERA_ENABLED)
    target_include_directories(asi_camera_components PRIVATE ${ASI_INCLUDE_DIR})
    target_link_libraries(asi_camera_components PRIVATE ${ASI_LIBRARY})
else()
    set(ASI_FOUND FALSE)
    message(STATUS "ASI SDK not found, using stub implementation")
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(asi_camera_components PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# Installation
install(TARGETS asi_camera_components
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(FILES ${COMPONENT_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/asi/camera/components
)

# Export targets
install(EXPORT asi_camera_components_targets
    FILE asi_camera_components_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
