# ============================================================================
# CMakeLists.txt for ASCOM Alpaca Client
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: lithium.client.ascom
# Description: ASCOM Alpaca REST API client implementation
# Author: Max Qian
# License: GPL3
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(lithium.client.ascom LANGUAGES CXX)

# Source files
set(ASCOM_SOURCES
    alpaca_client.cpp
    ascom_client.cpp
    ascom_device_base.cpp
    ascom_camera.cpp
    ascom_focuser.cpp
    ascom_filterwheel.cpp
    ascom_telescope.cpp
    ascom_rotator.cpp
    ascom_dome.cpp
    ascom_observingconditions.cpp
    ascom_device_factory.cpp
)

# Header files
set(ASCOM_HEADERS
    alpaca_client.hpp
    ascom_client.hpp
    ascom_types.hpp
    ascom_device_base.hpp
    ascom_camera.hpp
    ascom_focuser.hpp
    ascom_filterwheel.hpp
    ascom_telescope.hpp
    ascom_rotator.hpp
    ascom_dome.hpp
    ascom_observingconditions.hpp
    ascom_device_factory.hpp
)

# Create library
add_library(${PROJECT_NAME} STATIC ${ASCOM_SOURCES} ${ASCOM_HEADERS})
add_library(lithium::client::ascom ALIAS ${PROJECT_NAME})

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        lithium.client.common
        atom
    PRIVATE
        spdlog::spdlog
)

# Set properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Install
install(TARGETS ${PROJECT_NAME}
    EXPORT lithiumTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${ASCOM_HEADERS}
    DESTINATION include/lithium/client/ascom
)

lithium_message(SUCCESS "ASCOM client library configured")

# ============================================================================
# ASCOM Device Plugin Subdirectory
# ============================================================================

add_subdirectory(plugin)
