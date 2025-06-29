# Filter Task Module CMakeList

find_package(spdlog REQUIRED)

# Add filter task sources
set(FILTER_TASK_SOURCES
    base.cpp
    calibration.cpp
    change.cpp
    filter_tasks_factory.cpp
    lrgb_sequence.cpp
    narrowband_sequence.cpp
)

# Add filter task headers
set(FILTER_TASK_HEADERS
    base.hpp
    calibration.hpp
    change.hpp
    lrgb_sequence.hpp
    narrowband_sequence.hpp
)

# Create filter task library
add_library(lithium_task_filter STATIC ${FILTER_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_filter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_filter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_filter PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    spdlog::spdlog
)

# Add to parent target if it exists
if(TARGET lithium_task_custom)
    target_link_libraries(lithium_task_custom PUBLIC lithium_task_filter)
endif()

# Install headers
install(FILES ${FILTER_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/filter
)

# Install library
install(TARGETS lithium_task_filter
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
