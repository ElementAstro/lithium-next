cmake_minimum_required(VERSION 3.20)

# ASI Camera Controller Library
set(ASI_CONTROLLER_SOURCES
    asi_camera_controller.cpp
)

set(ASI_CONTROLLER_HEADERS
    asi_camera_controller.hpp
)

create_asi_camera_module(lithium_device_asi_camera_controller 
    "${ASI_CONTROLLER_SOURCES}"
)

target_include_directories(lithium_device_asi_camera_controller
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Install headers
install(FILES ${ASI_CONTROLLER_HEADERS}
    DESTINATION include/lithium/device/asi/camera/controller
    COMPONENT Development
)

# Install library
install(TARGETS lithium_device_asi_camera_controller
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)
