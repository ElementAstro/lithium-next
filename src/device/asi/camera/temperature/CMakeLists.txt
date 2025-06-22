cmake_minimum_required(VERSION 3.20)

# ASI Camera Temperature module
set(ASI_CAMERA_TEMPERATURE_SOURCES
    temperature_controller.hpp
    temperature_controller.cpp
)

add_library(asi_camera_temperature SHARED ${ASI_CAMERA_TEMPERATURE_SOURCES})
set_property(TARGET asi_camera_temperature PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(asi_camera_temperature PUBLIC ${COMMON_LIBS} asi_camera_core)

# Installation
install(TARGETS asi_camera_temperature
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
