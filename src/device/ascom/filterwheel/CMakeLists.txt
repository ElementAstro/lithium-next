cmake_minimum_required(VERSION 3.20)

# ASCOM Filter Wheel Module
project(ascom_filterwheel_module)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Component sources
set(COMPONENT_SOURCES
    components/hardware_interface.cpp
    components/position_manager.cpp
    components/configuration_manager.cpp
    components/monitoring_system.cpp
    components/calibration_system.cpp
)

# Add COM helper for Windows
if(WIN32)
    list(APPEND COMPONENT_SOURCES components/com_helper.cpp)
endif()

# Controller sources
set(CONTROLLER_SOURCES
    controller.cpp
    main.cpp
)

# Create component library
add_library(ascom_filterwheel_components STATIC ${COMPONENT_SOURCES})

target_include_directories(ascom_filterwheel_components PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../template
    ${CMAKE_SOURCE_DIR}/libs/atom
)

target_link_libraries(ascom_filterwheel_components PUBLIC
    spdlog::spdlog
    atom
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ascom_filterwheel_components PRIVATE
        ole32
        oleaut32
        uuid
    )
else()
    find_package(CURL REQUIRED)
    target_link_libraries(ascom_filterwheel_components PRIVATE
        CURL::libcurl
    )
endif()

# Create main controller library
add_library(ascom_filterwheel_controller STATIC ${CONTROLLER_SOURCES})

target_include_directories(ascom_filterwheel_controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../template
    ${CMAKE_SOURCE_DIR}/libs/atom
)

target_link_libraries(ascom_filterwheel_controller PUBLIC
    ascom_filterwheel_components
    spdlog::spdlog
    atom
)

# Create example executable
add_executable(ascom_filterwheel_example main.cpp)

target_link_libraries(ascom_filterwheel_example PRIVATE
    ascom_filterwheel_controller
    ascom_filterwheel_components
)

# Install targets
install(TARGETS ascom_filterwheel_components ascom_filterwheel_controller
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY components/
    DESTINATION include/ascom/filterwheel/components
    FILES_MATCHING PATTERN "*.hpp"
)

install(FILES controller.hpp
    DESTINATION include/ascom/filterwheel
)
