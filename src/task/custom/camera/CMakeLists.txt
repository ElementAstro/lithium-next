# Camera Task Module CMakeList

# Find required packages
find_package(spdlog REQUIRED)

# Add camera task sources
set(CAMERA_TASK_SOURCES
    basic_exposure.cpp
    calibration_tasks.cpp
    video_tasks.cpp
    temperature_tasks.cpp
    frame_tasks.cpp
    parameter_tasks.cpp
    telescope_tasks.cpp
    device_coordination_tasks.cpp
    sequence_analysis_tasks.cpp
)

# Add camera task headers
set(CAMERA_TASK_HEADERS
    basic_exposure.hpp
    calibration_tasks.hpp
    video_tasks.hpp
    temperature_tasks.hpp
    frame_tasks.hpp
    parameter_tasks.hpp
    telescope_tasks.hpp
    device_coordination_tasks.hpp
    sequence_analysis_tasks.hpp
    camera_tasks.hpp
    common.hpp
    examples.hpp
)

# Create camera task library
add_library(lithium_task_camera STATIC ${CAMERA_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_camera PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_camera PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    spdlog::spdlog
)

# Add to parent target if it exists
if(TARGET lithium_task_custom)
    target_link_libraries(lithium_task_custom PUBLIC lithium_task_camera)
endif()

# Install headers
install(FILES ${CAMERA_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/camera
)

# Install library
install(TARGETS lithium_task_camera
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
