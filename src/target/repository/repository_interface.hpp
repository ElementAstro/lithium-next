// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_REPOSITORY_REPOSITORY_INTERFACE_HPP
#define LITHIUM_TARGET_REPOSITORY_REPOSITORY_INTERFACE_HPP

#include <concepts>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "../celestial_model.hpp"

namespace lithium::target::repository {

// Forward declarations
struct CacheStats;

/**
 * @brief Concept for celestial repository implementations
 *
 * Defines the interface that any celestial repository must satisfy.
 * Used for compile-time interface checking and template constraints.
 */
template <typename T>
concept CelestialRepositoryLike = requires(T repo, int64_t id,
                                           const std::string& identifier) {
    { repo.findById(id) }
    ->std::same_as<std::optional<CelestialObjectModel>>;
    { repo.findByIdentifier(identifier) }
    ->std::same_as<std::optional<CelestialObjectModel>>;
    { repo.count() } -> std::convertible_to<size_t>;
    { repo.insert(std::declval<const CelestialObjectModel&>()) }
    ->std::same_as<std::expected<int64_t, std::string>>;
    { repo.update(std::declval<const CelestialObjectModel&>()) }
    ->std::same_as<std::expected<void, std::string>>;
    { repo.remove(id) } -> std::convertible_to<bool>;
};

/**
 * @brief Statistics about cache performance
 */
struct CacheStats {
    size_t hitCount = 0;      ///< Number of cache hits
    size_t missCount = 0;     ///< Number of cache misses
    size_t currentSize = 0;   ///< Current number of items in cache
    size_t maxSize = 0;       ///< Maximum cache capacity
    double hitRate = 0.0;     ///< Hit rate percentage

    [[nodiscard]] double getHitRate() const {
        if (hitCount + missCount == 0) return 0.0;
        return static_cast<double>(hitCount) / (hitCount + missCount) * 100.0;
    }
};

/**
 * @brief Abstract interface for celestial object repository
 *
 * Provides CRUD operations, batch operations, and search capabilities
 * for celestial objects. Implementations must be thread-safe.
 *
 * Error Handling:
 * - Returns std::expected<T, std::string> for fallible operations
 * - Returns std::optional<T> for queries that may not find results
 * - Throws exceptions for critical initialization errors only
 */
class ICelestialRepository {
public:
    virtual ~ICelestialRepository() = default;

    // Prevent copying
    ICelestialRepository(const ICelestialRepository&) = delete;
    ICelestialRepository& operator=(const ICelestialRepository&) = delete;

    // Allow moving
    ICelestialRepository(ICelestialRepository&&) noexcept = default;
    ICelestialRepository& operator=(ICelestialRepository&&) noexcept =
        default;

    // ==================== CRUD Operations ====================

    /**
     * @brief Insert a celestial object into the repository
     * @param obj Object to insert
     * @return Expected<int64_t, string> - ID of inserted object or error
     *
     * Thread-safe. May fail if:
     * - Database connection lost
     * - Duplicate identifier
     * - Invalid data values
     */
    [[nodiscard]] virtual auto insert(const CelestialObjectModel& obj)
        -> std::expected<int64_t, std::string> = 0;

    /**
     * @brief Update a celestial object in the repository
     * @param obj Object to update (must have valid id)
     * @return Expected<void, string> - Success or error message
     *
     * Thread-safe. May fail if:
     * - Object with given ID not found
     * - Database connection lost
     * - Invalid data values
     */
    [[nodiscard]] virtual auto update(const CelestialObjectModel& obj)
        -> std::expected<void, std::string> = 0;

    /**
     * @brief Delete a celestial object from the repository
     * @param id Object ID to delete
     * @return true if deleted, false if not found or error
     *
     * Thread-safe.
     */
    [[nodiscard]] virtual auto remove(int64_t id) -> bool = 0;

    /**
     * @brief Find object by database ID
     * @param id Object ID
     * @return Object if found, std::nullopt otherwise
     *
     * Thread-safe. May use caching depending on implementation.
     */
    [[nodiscard]] virtual auto findById(int64_t id)
        -> std::optional<CelestialObjectModel> = 0;

    /**
     * @brief Find object by identifier (e.g., "M31", "NGC 224")
     * @param identifier Object identifier string
     * @return Object if found, std::nullopt otherwise
     *
     * Thread-safe. May use caching depending on implementation.
     */
    [[nodiscard]] virtual auto findByIdentifier(const std::string& identifier)
        -> std::optional<CelestialObjectModel> = 0;

    // ==================== Batch Operations ====================

    /**
     * @brief Insert multiple objects in batches with transactions
     * @param objects Span of objects to insert
     * @param chunkSize Transaction chunk size (default: 100)
     * @return Number of successfully inserted objects
     *
     * Thread-safe. Uses transactions for each chunk to ensure consistency.
     * Continues on individual object failures.
     */
    [[nodiscard]] virtual auto batchInsert(
        std::span<const CelestialObjectModel> objects, size_t chunkSize = 100)
        -> int = 0;

    /**
     * @brief Update multiple objects in batches
     * @param objects Span of objects to update
     * @param chunkSize Transaction chunk size (default: 100)
     * @return Number of successfully updated objects
     *
     * Thread-safe. Uses transactions for consistency.
     */
    [[nodiscard]] virtual auto batchUpdate(
        std::span<const CelestialObjectModel> objects, size_t chunkSize = 100)
        -> int = 0;

    /**
     * @brief Insert or update objects (upsert operation)
     * @param objects Span of objects to upsert
     * @return Number of affected objects
     *
     * Thread-safe. For each object:
     * - Insert if not found
     * - Update if found by ID
     */
    [[nodiscard]] virtual auto upsert(
        std::span<const CelestialObjectModel> objects) -> int = 0;

    // ==================== Search Operations ====================

    /**
     * @brief Search by name pattern (SQL LIKE pattern)
     * @param pattern Search pattern (e.g., "M*", "%Andromeda%")
     * @param limit Maximum results
     * @return Matching objects
     *
     * Thread-safe. Uses SQL LIKE operator.
     */
    [[nodiscard]] virtual auto searchByName(const std::string& pattern,
                                            int limit = 100)
        -> std::vector<CelestialObjectModel> = 0;

    /**
     * @brief Fuzzy search by name using Levenshtein distance
     * @param name Name to search
     * @param tolerance Maximum edit distance (default: 2)
     * @param limit Maximum results (default: 50)
     * @return Matching objects with edit distance scores
     *
     * Thread-safe. Sorted by distance (closest first).
     */
    [[nodiscard]] virtual auto fuzzySearch(const std::string& name,
                                           int tolerance = 2, int limit = 50)
        -> std::vector<std::pair<CelestialObjectModel, int>> = 0;

    /**
     * @brief Complex search with multiple filter criteria
     * @param filter Search filter with name, type, magnitude, coordinates, etc.
     * @return Matching objects
     *
     * Thread-safe. Supports pagination via filter.limit and filter.offset.
     */
    [[nodiscard]] virtual auto search(const CelestialSearchFilter& filter)
        -> std::vector<CelestialObjectModel> = 0;

    /**
     * @brief Get autocomplete suggestions for prefix
     * @param prefix Name prefix to complete
     * @param limit Maximum suggestions (default: 10)
     * @return List of matching names
     *
     * Thread-safe. Fast prefix-based search, may use specialized index.
     */
    [[nodiscard]] virtual auto autocomplete(const std::string& prefix,
                                            int limit = 10)
        -> std::vector<std::string> = 0;

    /**
     * @brief Search by celestial coordinates (radius search)
     * @param ra Right ascension (degrees)
     * @param dec Declination (degrees)
     * @param radius Search radius (degrees)
     * @param limit Maximum results
     * @return Objects within radius, sorted by distance
     *
     * Thread-safe. Uses spatial index for efficiency.
     */
    [[nodiscard]] virtual auto searchByCoordinates(double ra, double dec,
                                                   double radius,
                                                   int limit = 50)
        -> std::vector<CelestialObjectModel> = 0;

    /**
     * @brief Get objects by type
     * @param type Object type (e.g., "galaxy", "nebula", "cluster")
     * @param limit Maximum results
     * @return Matching objects
     *
     * Thread-safe.
     */
    [[nodiscard]] virtual auto getByType(const std::string& type,
                                         int limit = 100)
        -> std::vector<CelestialObjectModel> = 0;

    /**
     * @brief Get objects by magnitude range
     * @param minMag Minimum magnitude
     * @param maxMag Maximum magnitude
     * @param limit Maximum results
     * @return Matching objects
     *
     * Thread-safe.
     */
    [[nodiscard]] virtual auto getByMagnitudeRange(double minMag,
                                                   double maxMag,
                                                   int limit = 100)
        -> std::vector<CelestialObjectModel> = 0;

    // ==================== Statistics ====================

    /**
     * @brief Get total number of objects
     * @return Total count
     *
     * Thread-safe. May be approximate depending on implementation.
     */
    [[nodiscard]] virtual auto count() -> size_t = 0;

    /**
     * @brief Get count grouped by type
     * @return Map of type to count
     *
     * Thread-safe.
     */
    [[nodiscard]] virtual auto countByType()
        -> std::unordered_map<std::string, int64_t> = 0;

protected:
    ICelestialRepository() = default;
};

/**
 * @brief Factory for creating repository instances
 *
 * Simplifies repository creation and dependency management.
 */
class RepositoryFactory {
public:
    /**
     * @brief Create a SQLite repository
     * @param dbPath Path to SQLite database file
     * @return Unique pointer to repository
     */
    [[nodiscard]] static auto createSqliteRepository(const std::string& dbPath)
        -> std::unique_ptr<ICelestialRepository>;

    /**
     * @brief Create an in-memory repository
     * @return Unique pointer to repository
     */
    [[nodiscard]] static auto createMemoryRepository()
        -> std::unique_ptr<ICelestialRepository>;

    /**
     * @brief Create a cached repository wrapping another repository
     * @param inner Inner repository implementation
     * @param cacheSize Cache capacity (default: 1000)
     * @return Unique pointer to cached repository
     */
    [[nodiscard]] static auto createCachedRepository(
        std::unique_ptr<ICelestialRepository> inner, size_t cacheSize = 1000)
        -> std::unique_ptr<ICelestialRepository>;
};

}  // namespace lithium::target::repository

#endif  // LITHIUM_TARGET_REPOSITORY_REPOSITORY_INTERFACE_HPP
