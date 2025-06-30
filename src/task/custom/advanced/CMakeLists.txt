# Advanced astrophotography tasks
# This module contains sophisticated automated imaging tasks

set(ADVANCED_TASK_SOURCES
    smart_exposure_task.cpp
    deep_sky_sequence_task.cpp
    planetary_imaging_task.cpp
    timelapse_task.cpp
    meridian_flip_task.cpp
    intelligent_sequence_task.cpp
    auto_calibration_task.cpp
    weather_monitor_task.cpp
    observatory_automation_task.cpp
    mosaic_imaging_task.cpp
    focus_optimization_task.cpp
    advanced_tasks.cpp
    task_registration.cpp
    advanced_task_registration.cpp
)

set(ADVANCED_TASK_HEADERS
    smart_exposure_task.hpp
    deep_sky_sequence_task.hpp
    planetary_imaging_task.hpp
    timelapse_task.hpp
    meridian_flip_task.hpp
    intelligent_sequence_task.hpp
    auto_calibration_task.hpp
    weather_monitor_task.hpp
    observatory_automation_task.hpp
    mosaic_imaging_task.hpp
    focus_optimization_task.hpp
    advanced_tasks.hpp
)

# Create advanced tasks library
add_library(lithium_advanced_tasks STATIC
    ${ADVANCED_TASK_SOURCES}
    ${ADVANCED_TASK_HEADERS}
)

target_link_libraries(lithium_advanced_tasks
    PRIVATE
        lithium_task_base
        lithium_camera_tasks
        atom
        atom
        atom
)

target_include_directories(lithium_advanced_tasks
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Set target properties
set_target_properties(lithium_advanced_tasks PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Add compiler warnings
target_compile_options(lithium_advanced_tasks PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Export library for parent CMakeLists.txt
set(LITHIUM_ADVANCED_TASKS_LIBRARY lithium_advanced_tasks PARENT_SCOPE)
