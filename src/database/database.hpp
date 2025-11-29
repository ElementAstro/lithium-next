#ifndef LITHIUM_DATABASE_HPP
#define LITHIUM_DATABASE_HPP

/**
 * @file database.hpp
 * @brief Unified facade header for the lithium database module.
 *
 * This header provides access to all database module components:
 * - Core: Database, Transaction, Statement, exception types
 * - ORM: Column, ColumnValue, Table templates
 * - Query: QueryBuilder for fluent SQL construction
 * - Cache: CacheManager for query result caching
 */

// Core components
#include "core/types.hpp"
#include "core/database.hpp"
#include "core/transaction.hpp"
#include "core/statement.hpp"

// ORM components
#include "orm/column_base.hpp"
#include "orm/column_value.hpp"
#include "orm/column.hpp"
#include "orm/table.hpp"

// Query building
#include "query/query_builder.hpp"

// Caching
#include "cache/cache_manager.hpp"

#endif // LITHIUM_DATABASE_HPP
