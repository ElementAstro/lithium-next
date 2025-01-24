# CMakeLists.txt for Lithium-Database
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Database
# Description: The official config module for lithium server
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium_database VERSION 1.0.0 LANGUAGES C CXX)

find_package(SQLite3 REQUIRED)

# Sources and Headers
set(PROJECT_FILES
    orm.cpp
)

# Required libraries
set(PROJECT_LIBS
    atom
    SQLite::SQLite3
)

# Create Static Library
add_library(${PROJECT_NAME} STATIC ${PROJECT_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})

# Set version properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install target
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
