cmake_minimum_required(VERSION 3.20)

# ASI Camera Exposure module
set(ASI_CAMERA_EXPOSURE_SOURCES
    exposure_controller.hpp
    exposure_controller.cpp
)

add_library(asi_camera_exposure SHARED ${ASI_CAMERA_EXPOSURE_SOURCES})
set_property(TARGET asi_camera_exposure PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(asi_camera_exposure PUBLIC ${COMMON_LIBS} asi_camera_core)

# Installation
install(TARGETS asi_camera_exposure
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
