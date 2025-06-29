# INDI Camera Component Library
cmake_minimum_required(VERSION 3.16)

# Component source files
set(INDI_CAMERA_SOURCES
    # Core component
    core/indi_camera_core.cpp

    # Controller components
    exposure/exposure_controller.cpp
    video/video_controller.cpp
    temperature/temperature_controller.cpp
    hardware/hardware_controller.cpp

    # Processing components
    image/image_processor.cpp
    sequence/sequence_manager.cpp
    properties/property_handler.cpp

    # Main camera class
    indi_camera.cpp
)

# Component header files
set(INDI_CAMERA_HEADERS
    component_base.hpp

    # Core component
    core/indi_camera_core.hpp

    # Controller components
    exposure/exposure_controller.hpp
    video/video_controller.hpp
    temperature/temperature_controller.hpp
    hardware/hardware_controller.hpp

    # Processing components
    image/image_processor.hpp
    sequence/sequence_manager.hpp
    properties/property_handler.hpp

    # Main camera class
    indi_camera.hpp
)

# Create the camera component library
add_library(indi_camera_components STATIC ${INDI_CAMERA_SOURCES} ${INDI_CAMERA_HEADERS})

# Include directories
target_include_directories(indi_camera_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/exposure
        ${CMAKE_CURRENT_SOURCE_DIR}/video
        ${CMAKE_CURRENT_SOURCE_DIR}/temperature
        ${CMAKE_CURRENT_SOURCE_DIR}/hardware
        ${CMAKE_CURRENT_SOURCE_DIR}/image
        ${CMAKE_CURRENT_SOURCE_DIR}/sequence
        ${CMAKE_CURRENT_SOURCE_DIR}/properties
)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(INDI REQUIRED libindi)
find_package(spdlog REQUIRED)

# Link libraries
target_link_libraries(indi_camera_components
    PUBLIC
        ${INDI_LIBRARIES}
        spdlog::spdlog
    PRIVATE
        pthread
)

# Compiler features
target_compile_features(indi_camera_components PUBLIC cxx_std_20)

# Compiler options
target_compile_options(indi_camera_components PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# Preprocessor definitions
target_compile_definitions(indi_camera_components PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)

# Install targets
install(TARGETS indi_camera_components
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${INDI_CAMERA_HEADERS}
    DESTINATION include/lithium/device/indi/camera
)

# Create a convenience target for the complete INDI camera module
add_library(lithium::indi_camera ALIAS indi_camera_components)

# Export target
install(EXPORT indi_camera_componentsTargets
    FILE LithiumINDICameraTargets.cmake
    NAMESPACE lithium::
    DESTINATION lib/cmake/lithium
)

# Component summary
message(STATUS "INDI Camera Components:")
message(STATUS "  - Core: Device connection and INDI BaseClient")
message(STATUS "  - Exposure: Exposure control and timing")
message(STATUS "  - Video: Video streaming and recording")
message(STATUS "  - Temperature: Cooling and thermal management")
message(STATUS "  - Hardware: Gain, offset, shutter, fan controls")
message(STATUS "  - Image: Image processing and quality analysis")
message(STATUS "  - Sequence: Automated capture sequences")
message(STATUS "  - Properties: INDI property management")
message(STATUS "  - Main: Unified camera interface")
