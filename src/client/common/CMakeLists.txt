# CMakeLists.txt for lithium.client.common
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_common)

# Source files
set(SOURCE_FILES
    client_base.cpp
    solver_client.cpp
    guider_client.cpp
    server_client.cpp
)

# Header files
set(HEADER_FILES
    client_base.hpp
    solver_client.hpp
    guider_client.hpp
    server_client.hpp
)

# Dependencies
set(LIBS
    spdlog::spdlog
    atom-utils
    lithium_tools
)

# Create the library
add_library(lithium_client_common STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(lithium_client_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/tools
)

target_link_libraries(lithium_client_common PUBLIC ${LIBS})

# Set C++ standard
target_compile_features(lithium_client_common PUBLIC cxx_std_20)
