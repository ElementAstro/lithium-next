# Custom Task Module CMakeList

# Find required packages
find_package(spdlog REQUIRED)

# Add custom task sources
set(CUSTOM_TASK_SOURCES
    config_task.cpp
    device_task.cpp
    factory.cpp
    script_task.cpp
    search_task.cpp
)

# Add custom task headers
set(CUSTOM_TASK_HEADERS
    config_task.hpp
    device_task.hpp
    factory.hpp
    script_task.hpp
    search_task.hpp
)

# Create custom task base library
add_library(lithium_task_custom STATIC ${CUSTOM_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_custom PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_custom PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_custom PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    spdlog::spdlog
)

# Add subdirectories
add_subdirectory(camera)
add_subdirectory(platesolve)
add_subdirectory(guide)
add_subdirectory(filter)
add_subdirectory(focuser)
add_subdirectory(script)
add_subdirectory(advanced)

# Install headers
install(FILES ${CUSTOM_TASK_HEADERS}
    DESTINATION include/lithium/task/custom
)

# Install library
install(TARGETS lithium_task_custom
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
