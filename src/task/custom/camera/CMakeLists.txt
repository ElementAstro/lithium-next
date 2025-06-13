# Camera Task Module CMakeList

# Find required packages
find_package(spdlog REQUIRED)

# Add camera task sources
set(CAMERA_TASK_SOURCES
    basic_exposure.cpp
    calibration_tasks.cpp
    filter_tasks.cpp
    guide_tasks.cpp
    platesolve_tasks.cpp
    safety_tasks.cpp
    sequence_tasks.cpp
)

# Add camera task headers
set(CAMERA_TASK_HEADERS
    basic_exposure.hpp
    calibration_tasks.hpp
    camera_tasks.hpp
    common.hpp
    filter_tasks.hpp
    guide_tasks.hpp
    platesolve_tasks.hpp
    safety_tasks.hpp
    sequence_tasks.hpp
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
