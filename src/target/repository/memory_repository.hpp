// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_REPOSITORY_MEMORY_REPOSITORY_HPP
#define LITHIUM_TARGET_REPOSITORY_MEMORY_REPOSITORY_HPP

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "repository_interface.hpp"

namespace lithium::target::repository {

/**
 * @brief In-memory implementation of celestial repository
 *
 * Provides fast, volatile storage suitable for testing, caching, and
 * development scenarios. All operations are thread-safe using shared_mutex.
 *
 * Features:
 * - Fast lookups with hash-based storage
 * - Thread-safe concurrent access
 * - No persistent storage
 * - Ideal for testing and in-memory caching
 *
 * @note All data is lost when the application terminates
 * @note Thread-safe for concurrent readers and writers
 */
class MemoryRepository : public ICelestialRepository {
public:
    /**
     * @brief Construct empty memory repository
     */
    MemoryRepository() = default;

    ~MemoryRepository() override = default;

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
     * @brief Clear all data from repository
     */
    void clear();

    /**
     * @brief Get current size of repository
     * @return Number of stored objects
     */
    [[nodiscard]] auto size() const -> size_t;

private:
    // Storage using both ID and identifier indexes
    std::unordered_map<int64_t, CelestialObjectModel> byId_;
    std::unordered_map<std::string, CelestialObjectModel> byIdentifier_;
    int64_t nextId_ = 1;
    mutable std::shared_mutex mutex_;

    // Helper methods
    [[nodiscard]] auto generateId() -> int64_t;

    [[nodiscard]] static auto matchesPattern(const std::string& str,
                                             const std::string& pattern)
        -> bool;

    [[nodiscard]] static auto levenshteinDistance(const std::string& s1,
                                                  const std::string& s2)
        -> int;

    [[nodiscard]] static auto haversineDistance(double ra1, double dec1,
                                                double ra2, double dec2)
        -> double;

    [[nodiscard]] static auto toLower(const std::string& str) -> std::string;
};

}  // namespace lithium::target::repository

#endif  // LITHIUM_TARGET_REPOSITORY_MEMORY_REPOSITORY_HPP
