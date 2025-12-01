// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_REPOSITORY_CACHED_REPOSITORY_HPP
#define LITHIUM_TARGET_REPOSITORY_CACHED_REPOSITORY_HPP

#include <atom/search/lru.hpp>

#include <memory>
#include <shared_mutex>

#include "repository_interface.hpp"

namespace lithium::target::repository {

/**
 * @brief Cached repository using decorator pattern
 *
 * Wraps any ICelestialRepository implementation and adds LRU caching layer.
 * Uses the atom::search::ThreadSafeLRUCache for efficient caching with
 * automatic eviction of least-recently-used items.
 *
 * Features:
 * - Transparent caching of repository operations
 * - Configurable cache size and TTL
 * - Cache invalidation strategies
 * - Statistics tracking (hits, misses, etc.)
 *
 * @note Cache is transparent to caller - no API changes
 * @note All caching is thread-safe
 */
class CachedRepository : public ICelestialRepository {
public:
    /**
     * @brief Construct cached repository
     * @param inner Inner repository to wrap
     * @param cacheSize Maximum number of items in cache (default: 1000)
     * @throws std::invalid_argument if inner is nullptr
     */
    explicit CachedRepository(std::unique_ptr<ICelestialRepository> inner,
                              size_t cacheSize = 1000);

    ~CachedRepository() override = default;

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

    // ==================== Cache Management ====================

    /**
     * @brief Invalidate cache entry for specific ID
     * @param id Object ID
     */
    void invalidateById(int64_t id);

    /**
     * @brief Invalidate cache entry for specific identifier
     * @param identifier Object identifier
     */
    void invalidateByIdentifier(const std::string& identifier);

    /**
     * @brief Clear all cache entries
     */
    void clearCache();

    /**
     * @brief Get cache performance statistics
     * @return Cache statistics (hits, misses, size, etc.)
     */
    [[nodiscard]] auto getCacheStats() const -> CacheStats;

    /**
     * @brief Resize the cache to a new capacity
     * @param newSize New cache capacity
     */
    void resizeCache(size_t newSize);

    /**
     * @brief Set TTL for cache entries
     * @param ttlSeconds Time-to-live in seconds
     */
    void setCacheTTL(std::optional<std::chrono::seconds> ttlSeconds);

private:
    std::unique_ptr<ICelestialRepository> inner_;

    // LRU caches for different access patterns
    atom::search::ThreadSafeLRUCache<int64_t, CelestialObjectModel> idCache_;
    atom::search::ThreadSafeLRUCache<std::string, CelestialObjectModel>
        identifierCache_;

    // TTL for cache entries
    std::optional<std::chrono::seconds> cacheTTL_;
    mutable std::shared_mutex ttlMutex_;

    // Statistics
    mutable std::atomic<size_t> cacheHits_{0};
    mutable std::atomic<size_t> cacheMisses_{0};

    // Helper methods
    void updateCacheEntry(const CelestialObjectModel& obj);
    void removeFromCache(int64_t id, const std::string& identifier);
};

}  // namespace lithium::target::repository

#endif  // LITHIUM_TARGET_REPOSITORY_CACHED_REPOSITORY_HPP
