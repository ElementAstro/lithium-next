# Focuser Task Module CMakeList

find_package(spdlog REQUIRED)

# Add focuser task sources
set(FOCUSER_TASK_SOURCES
    base.cpp
    position.cpp
    autofocus.cpp
    temperature.cpp
    validation.cpp
    backlash.cpp
    calibration.cpp
    star_analysis.cpp
    factory.cpp
    focus_tasks.cpp
    focus_workflow_example.cpp
)

# Add focuser task headers
set(FOCUSER_TASK_HEADERS
    base.hpp
    position.hpp
    autofocus.hpp
    temperature.hpp
    validation.hpp
    backlash.hpp
    calibration.hpp
    star_analysis.hpp
    factory.hpp
    focus_tasks.hpp
    focus_workflow_example.hpp
)

# Create focuser task library
add_library(lithium_task_focuser STATIC ${FOCUSER_TASK_SOURCES})

# Set target properties
set_target_properties(lithium_task_focuser PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_task_focuser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_task_focuser PRIVATE
    lithium_task_base
    lithium_atom_log
    lithium_atom_error
    spdlog::spdlog
)

# Add to parent target if it exists
if(TARGET lithium_task_custom)
    target_link_libraries(lithium_task_custom PUBLIC lithium_task_focuser)
endif()

# Install headers
install(FILES ${FOCUSER_TASK_HEADERS}
    DESTINATION include/lithium/task/custom/focuser
)

# Install library
install(TARGETS lithium_task_focuser
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Documentation
install(FILES FOCUS_TASK_DOCUMENTATION.md
    DESTINATION share/doc/lithium/task/focuser
)
