cmake_minimum_required(VERSION 3.20)

# ASI Camera Core module
set(ASI_CAMERA_CORE_SOURCES
    asi_camera_core.hpp
    asi_camera_core.cpp
)

add_library(asi_camera_core SHARED ${ASI_CAMERA_CORE_SOURCES})
set_property(TARGET asi_camera_core PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(asi_camera_core PUBLIC ${COMMON_LIBS})

if(ASI_FOUND)
    target_include_directories(asi_camera_core PRIVATE ${ASI_INCLUDE_DIR})
    target_link_libraries(asi_camera_core PRIVATE ${ASI_LIBRARY})
endif()

# Installation
install(TARGETS asi_camera_core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
