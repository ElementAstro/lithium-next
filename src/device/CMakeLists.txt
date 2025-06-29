# CMakeLists.txt for Lithium-Device
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Device
# Description: The official device module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_device VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(PROJECT_FILES
    manager.cpp
    device_factory.cpp
)

# Mock device sources
set(MOCK_DEVICE_FILES
    template/mock/mock_camera.cpp
    template/mock/mock_focuser.cpp
    template/mock/mock_rotator.cpp
    template/mock/mock_dome.cpp
    template/mock/mock_filterwheel.cpp
)

# INDI device sources (if available)
set(INDI_DEVICE_FILES
    indi/camera.cpp
    indi/telescope.cpp
    indi/focuser.cpp
    indi/filterwheel.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    loguru
    yaml-cpp
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Create Mock Devices Library
add_library(${PROJECT_NAME}_mock STATIC ${MOCK_DEVICE_FILES})
set_property(TARGET ${PROJECT_NAME}_mock PROPERTY POSITION_INDEPENDENT_CODE ON)

# Create INDI Devices Library (optional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/indi/camera.cpp")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(INDI QUIET indi)

    if(INDI_FOUND)
        add_library(${PROJECT_NAME}_indi STATIC ${INDI_DEVICE_FILES})
        set_property(TARGET ${PROJECT_NAME}_indi PROPERTY POSITION_INDEPENDENT_CODE ON)
        target_include_directories(${PROJECT_NAME}_indi PRIVATE ${INDI_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME}_indi PRIVATE ${INDI_LIBRARIES} ${PROJECT_LIBS})

        # Install INDI library
        install(TARGETS ${PROJECT_NAME}_indi
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME}_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})
target_link_libraries(${PROJECT_NAME}_mock PRIVATE ${PROJECT_LIBS})

# Set version properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME}_mock PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}_mock
)

# Create integration test executable
if(BUILD_TESTING)
    add_executable(device_integration_test device_integration_test.cpp)
    target_link_libraries(device_integration_test PRIVATE
        ${PROJECT_NAME}_mock
        ${PROJECT_LIBS}
    )
    target_include_directories(device_integration_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Add test
    add_test(NAME DeviceIntegrationTest COMMAND device_integration_test)
endif()

# Install targets
install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_mock
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(DIRECTORY template/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/template
    FILES_MATCHING PATTERN "*.hpp"
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/atom/atom)

function(add_subdirectories_recursively start_dir)
  file(GLOB entries "${start_dir}/*")
  foreach(entry ${entries})
    if(IS_DIRECTORY ${entry} AND EXISTS "${entry}/CMakeLists.txt")
      message(STATUS "Adding device library subdirectory: ${entry}")
      add_subdirectory(${entry})
      include_directories(${entry})
    endif()
  endforeach()
endfunction()
add_subdirectories_recursively(${CMAKE_CURRENT_SOURCE_DIR})

# Add subdirectories
add_subdirectory(template)
add_subdirectory(indi)
add_subdirectory(ascom)
# Add camera implementations
add_subdirectory(qhy)
add_subdirectory(asi)
add_subdirectory(atik)
add_subdirectory(sbig)
add_subdirectory(fli)
add_subdirectory(playerone)
