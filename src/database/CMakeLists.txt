# ============================================================================
# CMakeLists.txt for Lithium-Database
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Database
# Description: Modular database ORM and management module
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
    # Core module
    core/database.cpp
    core/transaction.cpp
    core/statement.cpp
    # Query module
    query/query_builder.cpp
    # Cache module
    cache/cache_manager.cpp
)

set(PROJECT_HEADERS
    # Facade
    database.hpp
    # Core module
    core/types.hpp
    core/database.hpp
    core/transaction.hpp
    core/statement.hpp
    # ORM module (header-only)
    orm/column_base.hpp
    orm/column_value.hpp
    orm/column.hpp
    orm/table.hpp
    # Query module
    query/query_builder.hpp
    # Cache module
    cache/cache_entry.hpp
    cache/cache_manager.hpp
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
    HEADERS ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Modular database ORM and management"
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Database")

lithium_component_end(${PROJECT_NAME})
