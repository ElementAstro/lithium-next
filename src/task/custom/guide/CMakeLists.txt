# Guide Tasks Module
# This module contains all guide-related tasks for the Lithium system

# Header files
set(GUIDE_TASK_HEADERS
    connection_tasks.hpp
    calibration_tasks.hpp
    control_tasks.hpp
    dither_tasks.hpp
    exposure_tasks.hpp
    all_tasks.hpp
)

# Source files
set(GUIDE_TASK_SOURCES
    connection_tasks.cpp
    calibration_tasks.cpp
    control_tasks.cpp
    dither_tasks.cpp
    exposure_tasks.cpp
    all_tasks.cpp
)

# Create guide tasks library
add_library(lithium_task_guide_tasks STATIC
    ${GUIDE_TASK_SOURCES}
    ${GUIDE_TASK_HEADERS}
)

# Set target properties
set_target_properties(lithium_task_guide_tasks PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_guide_tasks
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link dependencies
target_link_libraries(lithium_task_guide_tasks
    PUBLIC
        lithium_task_base
        lithium_task_common
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        atom_error
)

# Compiler-specific options
target_compile_options(lithium_task_guide_tasks PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Install rules
install(TARGETS lithium_task_guide_tasks
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${GUIDE_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/guide
)
