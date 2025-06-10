# CMakeLists.txt for Lithium-Task-Simple
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Task-Simple
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_task VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(PROJECT_FILES
    generator.cpp
    sequencer.cpp
    target.cpp
    task.cpp
    task_camera.cpp
    custom/camera/basic_exposure.cpp
    custom/camera/calibration_tasks.cpp
    custom/camera/sequence_tasks.cpp
    custom/camera/focus_tasks.cpp
    custom/camera/filter_tasks.cpp
    custom/camera/guide_tasks.cpp
    custom/camera/safety_tasks.cpp
    custom/camera/platesolve_tasks.cpp
    custom/task_manager.cpp
    custom/enhanced_sequencer.cpp
    custom/task_templates.cpp
    custom/script_task.cpp
    custom/device_task.cpp
    custom/config_task.cpp
    generator.hpp
    sequencer.hpp
    target.hpp
    task.hpp
    task_camera.hpp
    custom/camera/basic_exposure.hpp
    custom/camera/calibration_tasks.hpp
    custom/camera/sequence_tasks.hpp
    custom/camera/focus_tasks.hpp
    custom/camera/filter_tasks.hpp
    custom/camera/guide_tasks.hpp
    custom/camera/safety_tasks.hpp
    custom/camera/platesolve_tasks.hpp
    custom/camera/common.hpp
    custom/camera/camera_tasks.hpp
    custom/task_manager.hpp
    custom/enhanced_sequencer.hpp
    custom/task_templates.hpp
    custom/task_common.hpp
    custom/factory.hpp
    custom/script_task.hpp
    custom/device_task.hpp
    custom/config_task.hpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    lithium_config
    lithium_database
    loguru
    yaml-cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})

# Set version properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install target
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
