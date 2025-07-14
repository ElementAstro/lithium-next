# ==================== Plate Solve Tasks Module ====================
# This module contains all plate solving related tasks including:
# - PlateSolveExposureTask: Basic plate solving with exposures
# - CenteringTask: Automatic centering using plate solving
# - MosaicTask: Automated mosaic imaging with plate solving

# Find required packages
find_package(spdlog REQUIRED)

# Define platesolve task sources
set(PLATESOLVE_TASK_SOURCES
    common.cpp
    exposure.cpp
    centering.cpp
    mosaic.cpp
    platesolve_tasks.cpp
    task_registration.cpp
)

# Define platesolve task headers
set(PLATESOLVE_TASK_HEADERS
    common.hpp
    exposure.hpp
    centering.hpp
    mosaic.hpp
    platesolve_tasks.hpp
)

# Create platesolve task library
add_library(lithium_task_platesolve STATIC ${PLATESOLVE_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_platesolve PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_platesolve PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_platesolve PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    lithium_client_astrometry
    lithium_client_astap
    lithium_device_template
    spdlog::spdlog
)

# Install headers
install(FILES ${PLATESOLVE_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/platesolve
    COMPONENT Development
)

# Install library
install(TARGETS lithium_task_platesolve
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
