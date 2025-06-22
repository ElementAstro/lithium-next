cmake_minimum_required(VERSION 3.20)
project(lithium_device_asi_camera)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_SOURCE_DIR}/cmake/ScanModule.cmake)

# Common libraries
set(COMMON_LIBS
    loguru atom-system atom-io atom-utils atom-component atom-error)

# ASI SDK detection
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
    add_compile_definitions(LITHIUM_ASI_CAMERA_ENABLED)
else()
    set(ASI_FOUND FALSE)
    message(STATUS "ASI SDK not found, using stub implementation")
endif()

# Create shared library target with PIC
function(create_asi_camera_module NAME SOURCES)
    add_library(${NAME} SHARED ${SOURCES})
    set_property(TARGET ${NAME} PROPERTY POSITION_INDEPENDENT_CODE 1)
    target_link_libraries(${NAME} PUBLIC ${COMMON_LIBS})
    
    if(ASI_FOUND)
        target_include_directories(${NAME} PRIVATE ${ASI_INCLUDE_DIR})
        target_link_libraries(${NAME} PRIVATE ${ASI_LIBRARY})
    endif()
endfunction()

# Component modules
add_subdirectory(components)
add_subdirectory(controller)
add_subdirectory(core)
add_subdirectory(exposure)
add_subdirectory(temperature)
add_subdirectory(hardware)
add_subdirectory(video)
add_subdirectory(sequence)
add_subdirectory(image)
add_subdirectory(properties)

# Main ASI camera module
set(ASI_CAMERA_SOURCES
    asi_camera.cpp
    module.cpp
)

create_asi_camera_module(lithium_device_asi_camera "${ASI_CAMERA_SOURCES}")

# Link component modules
target_link_libraries(lithium_device_asi_camera PUBLIC
    asi_camera_components
    lithium_device_asi_camera_controller
    asi_camera_core
    asi_camera_exposure
    asi_camera_temperature
    asi_camera_hardware
    asi_camera_video
    asi_camera_sequence
    asi_camera_image
    asi_camera_properties
)

# Installation
install(TARGETS lithium_device_asi_camera
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/asi/camera
    FILES_MATCHING PATTERN "*.hpp"
)
