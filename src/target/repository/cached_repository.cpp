// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "cached_repository.hpp"

#include <spdlog/spdlog.h>

namespace lithium::target::repository {

CachedRepository::CachedRepository(std::unique_ptr<ICelestialRepository> inner,
                                   size_t cacheSize)
    : inner_(std::move(inner)),
      idCache_(cacheSize),
      identifierCache_(cacheSize) {
    if (!inner_) {
        throw std::invalid_argument("Inner repository cannot be null");
    }
    SPDLOG_INFO("CachedRepository initialized with cache size: {}", cacheSize);
}

// ==================== Helper Methods ====================

void CachedRepository::updateCacheEntry(const CelestialObjectModel& obj) {
    try {
        std::shared_lock lock(ttlMutex_);
        idCache_.put(obj.id, obj, cacheTTL_);
        identifierCache_.put(obj.identifier, obj, cacheTTL_);
    } catch (const std::exception& e) {
        SPDLOG_WARN("Failed to update cache entry: {}", e.what());
    }
}

void CachedRepository::removeFromCache(int64_t id,
                                       const std::string& identifier) {
    try {
        idCache_.erase(id);
        identifierCache_.erase(identifier);
    } catch (const std::exception& e) {
        SPDLOG_WARN("Failed to remove cache entry: {}", e.what());
    }
}

// ==================== CRUD Operations ====================

auto CachedRepository::insert(const CelestialObjectModel& obj)
    -> std::expected<int64_t, std::string> {
    auto result = inner_->insert(obj);
    if (result) {
        CelestialObjectModel newObj = obj;
        newObj.id = result.value();
        updateCacheEntry(newObj);
    }
    return result;
}

auto CachedRepository::update(const CelestialObjectModel& obj)
    -> std::expected<void, std::string> {
    auto result = inner_->update(obj);
    if (result) {
        updateCacheEntry(obj);
    } else {
        removeFromCache(obj.id, obj.identifier);
    }
    return result;
}

auto CachedRepository::remove(int64_t id) -> bool {
    bool result = inner_->remove(id);
    if (result) {
        // Try to get identifier from cache for cleanup
        if (auto cached = idCache_.get(id)) {
            removeFromCache(id, cached->identifier);
        } else {
            idCache_.erase(id);
        }
    }
    return result;
}

auto CachedRepository::findById(int64_t id)
    -> std::optional<CelestialObjectModel> {
    // Try cache first
    if (auto cached = idCache_.get(id)) {
        cacheHits_++;
        SPDLOG_DEBUG("Cache hit for id={}", id);
        return cached;
    }

    cacheMisses_++;
    auto result = inner_->findById(id);
    if (result) {
        updateCacheEntry(result.value());
    }
    return result;
}

auto CachedRepository::findByIdentifier(const std::string& identifier)
    -> std::optional<CelestialObjectModel> {
    // Try cache first
    if (auto cached = identifierCache_.get(identifier)) {
        cacheHits_++;
        SPDLOG_DEBUG("Cache hit for identifier={}", identifier);
        return cached;
    }

    cacheMisses_++;
    auto result = inner_->findByIdentifier(identifier);
    if (result) {
        updateCacheEntry(result.value());
    }
    return result;
}

// ==================== Batch Operations ====================

auto CachedRepository::batchInsert(
    std::span<const CelestialObjectModel> objects, size_t chunkSize) -> int {
    int count = inner_->batchInsert(objects, chunkSize);

    // Invalidate caches after batch operation
    // Note: We don't cache individual insertions to avoid inconsistency
    clearCache();

    return count;
}

auto CachedRepository::batchUpdate(
    std::span<const CelestialObjectModel> objects, size_t chunkSize) -> int {
    int count = inner_->batchUpdate(objects, chunkSize);

    // Update cache with modified objects
    for (const auto& obj : objects) {
        updateCacheEntry(obj);
    }

    return count;
}

auto CachedRepository::upsert(
    std::span<const CelestialObjectModel> objects) -> int {
    int count = inner_->upsert(objects);

    // Update cache with upserted objects
    for (const auto& obj : objects) {
        updateCacheEntry(obj);
    }

    return count;
}

// ==================== Search Operations ====================

auto CachedRepository::searchByName(const std::string& pattern, int limit)
    -> std::vector<CelestialObjectModel> {
    // Search results typically not cached due to pattern variability
    cacheMisses_++;
    return inner_->searchByName(pattern, limit);
}

auto CachedRepository::fuzzySearch(const std::string& name, int tolerance,
                                   int limit)
    -> std::vector<std::pair<CelestialObjectModel, int>> {
    // Fuzzy search results not cached
    cacheMisses_++;
    return inner_->fuzzySearch(name, tolerance, limit);
}

auto CachedRepository::search(const CelestialSearchFilter& filter)
    -> std::vector<CelestialObjectModel> {
    // Complex searches not cached due to filter variability
    cacheMisses_++;
    return inner_->search(filter);
}

auto CachedRepository::autocomplete(const std::string& prefix, int limit)
    -> std::vector<std::string> {
    // Autocomplete results not cached
    cacheMisses_++;
    return inner_->autocomplete(prefix, limit);
}

auto CachedRepository::searchByCoordinates(double ra, double dec,
                                           double radius, int limit)
    -> std::vector<CelestialObjectModel> {
    // Coordinate searches not cached
    cacheMisses_++;
    return inner_->searchByCoordinates(ra, dec, radius, limit);
}

auto CachedRepository::getByType(const std::string& type, int limit)
    -> std::vector<CelestialObjectModel> {
    // Type-based searches not cached (would require key per type)
    cacheMisses_++;
    return inner_->getByType(type, limit);
}

auto CachedRepository::getByMagnitudeRange(double minMag, double maxMag,
                                           int limit)
    -> std::vector<CelestialObjectModel> {
    // Range searches not cached
    cacheMisses_++;
    return inner_->getByMagnitudeRange(minMag, maxMag, limit);
}

// ==================== Statistics ====================

auto CachedRepository::count() -> size_t {
    return inner_->count();
}

auto CachedRepository::countByType()
    -> std::unordered_map<std::string, int64_t> {
    return inner_->countByType();
}

// ==================== Cache Management ====================

void CachedRepository::invalidateById(int64_t id) {
    idCache_.erase(id);
    SPDLOG_DEBUG("Cache invalidated for id={}", id);
}

void CachedRepository::invalidateByIdentifier(const std::string& identifier) {
    identifierCache_.erase(identifier);
    SPDLOG_DEBUG("Cache invalidated for identifier={}", identifier);
}

void CachedRepository::clearCache() {
    try {
        idCache_.clear();
        identifierCache_.clear();
        SPDLOG_INFO("Cache cleared");
    } catch (const std::exception& e) {
        SPDLOG_WARN("Failed to clear cache: {}", e.what());
    }
}

auto CachedRepository::getCacheStats() const -> CacheStats {
    CacheStats stats;
    stats.hitCount = cacheHits_.load();
    stats.missCount = cacheMisses_.load();
    stats.currentSize = idCache_.size();  // Both caches should have same size
    // maxSize would need to be tracked separately
    stats.hitRate = stats.getHitRate();

    return stats;
}

void CachedRepository::resizeCache(size_t newSize) {
    try {
        idCache_.resize(newSize);
        identifierCache_.resize(newSize);
        SPDLOG_INFO("Cache resized to {}", newSize);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to resize cache: {}", e.what());
    }
}

void CachedRepository::setCacheTTL(
    std::optional<std::chrono::seconds> ttlSeconds) {
    std::unique_lock lock(ttlMutex_);
    cacheTTL_ = ttlSeconds;

    if (ttlSeconds) {
        SPDLOG_INFO("Cache TTL set to {} seconds", ttlSeconds->count());
    } else {
        SPDLOG_INFO("Cache TTL disabled");
    }
}

}  // namespace lithium::target::repository
