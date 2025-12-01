// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_REPOSITORY_SQLITE_REPOSITORY_HPP
#define LITHIUM_TARGET_REPOSITORY_SQLITE_REPOSITORY_HPP

#include <memory>
#include <shared_mutex>
#include <string>

#include "../../database/database.hpp"
#include "repository_interface.hpp"

namespace lithium::target::repository {

/**
 * @brief SQLite implementation of celestial repository
 *
 * Provides persistent storage and advanced search capabilities for celestial
 * objects using SQLite3. All operations are thread-safe using shared_mutex.
 *
 * Features:
 * - Full ACID compliance via transactions
 * - Efficient spatial indexing for coordinate searches
 * - Batch operations with transaction support
 * - Advanced search with multiple filter criteria
 *
 * @note Thread-safe for concurrent readers and writers
 */
class SqliteRepository : public ICelestialRepository {
public:
    /**
     * @brief Construct SQLite repository
     * @param dbPath Path to SQLite database file
     * @throws std::runtime_error if database initialization fails
     */
    explicit SqliteRepository(const std::string& dbPath);

    /**
     * @brief Construct with existing database connection
     * @param db Shared pointer to database instance
     * @throws std::runtime_error if database is invalid
     */
    explicit SqliteRepository(
        std::shared_ptr<database::core::Database> db);

    ~SqliteRepository() override = default;

    // ==================== CRUD Operations ====================

    [[nodiscard]] auto insert(const CelestialObjectModel& obj)
        -> std::expected<int64_t, std::string> override;

    [[nodiscard]] auto update(const CelestialObjectModel& obj)
        -> std::expected<void, std::string> override;

    [[nodiscard]] auto remove(int64_t id) -> bool override;

    [[nodiscard]] auto findById(int64_t id)
        -> std::optional<CelestialObjectModel> override;

    [[nodiscard]] auto findByIdentifier(const std::string& identifier)
        -> std::optional<CelestialObjectModel> override;

    // ==================== Batch Operations ====================

    [[nodiscard]] auto batchInsert(
        std::span<const CelestialObjectModel> objects, size_t chunkSize = 100)
        -> int override;

    [[nodiscard]] auto batchUpdate(
        std::span<const CelestialObjectModel> objects, size_t chunkSize = 100)
        -> int override;

    [[nodiscard]] auto upsert(
        std::span<const CelestialObjectModel> objects) -> int override;

    // ==================== Search Operations ====================

    [[nodiscard]] auto searchByName(const std::string& pattern,
                                    int limit = 100)
        -> std::vector<CelestialObjectModel> override;

    [[nodiscard]] auto fuzzySearch(const std::string& name, int tolerance = 2,
                                   int limit = 50)
        -> std::vector<std::pair<CelestialObjectModel, int>> override;

    [[nodiscard]] auto search(const CelestialSearchFilter& filter)
        -> std::vector<CelestialObjectModel> override;

    [[nodiscard]] auto autocomplete(const std::string& prefix,
                                    int limit = 10)
        -> std::vector<std::string> override;

    [[nodiscard]] auto searchByCoordinates(double ra, double dec,
                                           double radius, int limit = 50)
        -> std::vector<CelestialObjectModel> override;

    [[nodiscard]] auto getByType(const std::string& type, int limit = 100)
        -> std::vector<CelestialObjectModel> override;

    [[nodiscard]] auto getByMagnitudeRange(double minMag, double maxMag,
                                           int limit = 100)
        -> std::vector<CelestialObjectModel> override;

    // ==================== Statistics ====================

    [[nodiscard]] auto count() -> size_t override;

    [[nodiscard]] auto countByType()
        -> std::unordered_map<std::string, int64_t> override;

    // ==================== Maintenance ====================

    /**
     * @brief Initialize database schema
     * @return true if successful
     *
     * Creates all necessary tables and indexes if they don't exist.
     */
    auto initializeSchema() -> bool;

    /**
     * @brief Create database indexes for optimized queries
     */
    void createIndexes();

    /**
     * @brief Optimize database (VACUUM, ANALYZE)
     */
    void optimize();

    /**
     * @brief Clear all data from repository
     * @param includeStatistics Also clear rating and search history
     * @return true if successful
     */
    auto clearAll(bool includeStatistics = false) -> bool;

    /**
     * @brief Get database statistics
     * @return Statistics summary as JSON string
     */
    [[nodiscard]] auto getStatistics() -> std::string;

private:
    std::shared_ptr<database::core::Database> db_;
    mutable std::shared_mutex mutex_;

    // Helper methods
    [[nodiscard]] auto modelToInsertSql(const CelestialObjectModel& obj)
        -> std::string;

    [[nodiscard]] auto modelToUpdateSql(const CelestialObjectModel& obj)
        -> std::string;

    [[nodiscard]] auto executeFind(const std::string& sql)
        -> std::optional<CelestialObjectModel>;

    [[nodiscard]] auto executeSearch(const std::string& sql)
        -> std::vector<CelestialObjectModel>;

    [[nodiscard]] auto rowToCelestialObject(
        const std::vector<std::vector<std::string>>& row)
        -> std::optional<CelestialObjectModel>;
};

}  // namespace lithium::target::repository

#endif  // LITHIUM_TARGET_REPOSITORY_SQLITE_REPOSITORY_HPP
