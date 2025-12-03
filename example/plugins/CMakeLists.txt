# CMakeLists.txt for example plugins
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

# Example plugin demonstrating the plugin system
add_library(example_plugin SHARED
    example_plugin.cpp
)

target_include_directories(example_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty/nlohmann
    ${CMAKE_SOURCE_DIR}/libs/thirdparty/crow/include
)

target_link_libraries(example_plugin PRIVATE
    lithium_server
    atom_log
    spdlog::spdlog
)

# Set C++23 standard
target_compile_features(example_plugin PUBLIC cxx_std_23)

# Set output directory to plugins folder
set_target_properties(example_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/server
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/server
)

# Install to plugins directory
install(TARGETS example_plugin
    LIBRARY DESTINATION plugins/server
    RUNTIME DESTINATION plugins/server
)
