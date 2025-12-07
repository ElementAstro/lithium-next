# Device Common Module CMakeLists.txt
# Error handling, exceptions, and result types

cmake_minimum_required(VERSION 3.20)

# Define the common module as header-only
add_library(lithium_device_common INTERFACE)

target_include_directories(lithium_device_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(lithium_device_common INTERFACE
    nlohmann_json::nlohmann_json
)

# Alias for use in other modules
add_library(lithium::device_common ALIAS lithium_device_common)
