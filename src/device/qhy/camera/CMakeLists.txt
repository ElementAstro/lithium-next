cmake_minimum_required(VERSION 3.20)
project(lithium_device_qhy_camera)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_SOURCE_DIR}/cmake/ScanModule.cmake)

# Common libraries
set(COMMON_LIBS
    loguru atom-system atom-io atom-utils atom-component atom-error)

# QHY SDK detection
find_path(QHY_INCLUDE_DIR qhyccd.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES qhy libqhy
    DOC "QHY SDK include directory"
)

find_library(QHY_LIBRARY
    NAMES qhyccd libqhyccd
    PATHS /usr/lib /usr/local/lib
    PATH_SUFFIXES qhy
    DOC "QHY SDK library"
)

if(QHY_INCLUDE_DIR AND QHY_LIBRARY)
    set(QHY_FOUND TRUE)
    message(STATUS "Found QHY SDK: ${QHY_LIBRARY}")
    add_compile_definitions(LITHIUM_QHY_CAMERA_ENABLED)
else()
    set(QHY_FOUND FALSE)
    message(STATUS "QHY SDK not found, using stub implementation")
endif()

# Create shared library target with PIC
function(create_qhy_camera_module NAME SOURCES)
    add_library(${NAME} SHARED ${SOURCES})
    set_property(TARGET ${NAME} PROPERTY POSITION_INDEPENDENT_CODE 1)
    target_link_libraries(${NAME} PUBLIC ${COMMON_LIBS})
    
    if(QHY_FOUND)
        target_include_directories(${NAME} PRIVATE ${QHY_INCLUDE_DIR})
        target_link_libraries(${NAME} PRIVATE ${QHY_LIBRARY})
    endif()
endfunction()

# Component modules
add_subdirectory(core)
add_subdirectory(exposure)
add_subdirectory(temperature)
add_subdirectory(video)
add_subdirectory(sequence)
add_subdirectory(image)
add_subdirectory(properties)
add_subdirectory(hardware)

# Filter wheel module (separate from camera hardware)
add_subdirectory(../filterwheel filterwheel)

# Main QHY camera module
set(QHY_CAMERA_SOURCES
    qhy_camera.cpp
    module.cpp
)

create_qhy_camera_module(lithium_device_qhy_camera "${QHY_CAMERA_SOURCES}")

# Link component modules
target_link_libraries(lithium_device_qhy_camera PUBLIC
    qhy_camera_core
    qhy_camera_exposure
    qhy_camera_temperature
    qhy_camera_video
    qhy_camera_sequence
    qhy_camera_image
    qhy_camera_properties
    qhy_camera_hardware
    qhy_filterwheel
)

# Installation
install(TARGETS lithium_device_qhy_camera
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/qhy/camera
    FILES_MATCHING PATTERN "*.hpp"
)
