# ============================================================================
# CMakeLists.txt for Lithium-Database
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Database
# Description: Database ORM and management module
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_database 1.0.0)

# ============================================================================
# Dependencies
# ============================================================================

lithium_find_package(SQLite3 REQUIRED)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    orm.cpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    SQLite::SQLite3
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Database ORM and management"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Database")

lithium_component_end(${PROJECT_NAME})
