# CMakeLists.txt for server plugin system
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

# Plugin system library
add_library(lithium_server_plugin STATIC
    plugin_interface.cpp
    plugin_loader.cpp
    plugin_manager.cpp
)

target_include_directories(lithium_server_plugin PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/libs/thirdparty/nlohmann
)

target_link_libraries(lithium_server_plugin PUBLIC
    lithium_components
    atom_function
    atom_log
    spdlog::spdlog
)

# Set C++23 standard
target_compile_features(lithium_server_plugin PUBLIC cxx_std_23)

# Install headers
install(FILES
    plugin_interface.hpp
    plugin_loader.hpp
    plugin_manager.hpp
    DESTINATION include/lithium/server/plugin
)
