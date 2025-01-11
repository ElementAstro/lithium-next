# CMakeLists.txt for lithium.indiserver
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi)

# Add source and header files in one place for better management
set(SOURCE_FILES
    async_system_command.cpp
    driverlist.cpp
    database.cpp
    indiserver.cpp
    collection.cpp
    iconnector.cpp
    indihub_agent.cpp
)

set(HEADER_FILES
    async_system_command.hpp
    collection.hpp
    driverlist.hpp
    database.hpp
    indiserver.hpp
    iconnector.hpp
    indihub_agent.hpp
)

set(TEST_FILES
    tests/test_collection.cpp
    tests/test_connector.cpp
    tests/test_driverlist.cpp
)

# Specify the external libraries
set(LIBS
    loguru
    atom-system
    atom-io
    tinyxml2
    atom-component
    atom-error
    atom-utils
    atom-function
)

if (WIN32)
    list(APPEND LIBS ws2_32 version iphlpapi)
endif()

# Create the module library
add_library(lithium_client_indi SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(lithium_client_indi PUBLIC ${LIBS})