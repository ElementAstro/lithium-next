cmake_minimum_required(VERSION 3.20)

# ASI Camera Hardware module
set(ASI_CAMERA_HARDWARE_SOURCES
    hardware_controller.hpp
    hardware_controller.cpp
)

add_library(asi_camera_hardware SHARED ${ASI_CAMERA_HARDWARE_SOURCES})
set_property(TARGET asi_camera_hardware PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(asi_camera_hardware PUBLIC ${COMMON_LIBS} asi_camera_core)

# Include EAF and EFW SDK stubs
target_include_directories(asi_camera_hardware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Installation
install(TARGETS asi_camera_hardware
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
