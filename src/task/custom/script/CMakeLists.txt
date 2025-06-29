# Script Task Module CMakeList

find_package(spdlog REQUIRED)

# Add script task sources
set(SCRIPT_TASK_SOURCES
    base.cpp
    monitor.cpp
    pipeline.cpp
    python.cpp
    shell.cpp
    workflow.cpp
)

# Add script task headers
set(SCRIPT_TASK_HEADERS
    base.hpp
    monitor.hpp
    pipeline.hpp
    python.hpp
    shell.hpp
    workflow.hpp
)

# Create script task library
add_library(lithium_task_script STATIC ${SCRIPT_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_script PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_script PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_script PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    spdlog::spdlog
)

# Add to parent target if it exists
if(TARGET lithium_task_custom)
    target_link_libraries(lithium_task_custom PUBLIC lithium_task_script)
endif()

# Install headers
install(FILES ${SCRIPT_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/script
)

# Install library
install(TARGETS lithium_task_script
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
