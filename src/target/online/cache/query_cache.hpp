// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_CACHE_QUERY_CACHE_HPP
#define LITHIUM_TARGET_ONLINE_CACHE_QUERY_CACHE_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "../provider/provider_interface.hpp"

namespace lithium::target::online {

/**
 * @brief Cache configuration
 */
struct CacheConfig {
    size_t maxEntries = 1000;
    std::chrono::minutes defaultTTL{60};
    bool persistToDisk = false;
    std::string persistPath = "data/cache/online_queries.cache";

    // Per-provider TTL overrides
    std::chrono::minutes simbadTTL{120};
    std::chrono::minutes vizierTTL{120};
    std::chrono::minutes nedTTL{60};
    std::chrono::minutes jplHorizonsTTL{5};
    std::chrono::minutes openNgcTTL{1440};  // 24 hours
};

/**
 * @brief Cache statistics
 */
struct CacheStats {
    size_t entries = 0;
    size_t hits = 0;
    size_t misses = 0;
    double hitRate = 0.0;
    std::chrono::system_clock::time_point lastCleanup;
};

/**
 * @brief Query result cache with TTL support
 *
 * Thread-safe cache for online query results.
 * Uses LRU eviction when capacity is reached.
 */
class QueryCache {
public:
    explicit QueryCache(const CacheConfig& config = {});
    ~QueryCache();

    // Non-copyable, movable
    QueryCache(const QueryCache&) = delete;
    QueryCache& operator=(const QueryCache&) = delete;
    QueryCache(QueryCache&&) noexcept;
    QueryCache& operator=(QueryCache&&) noexcept;

    /**
     * @brief Get cached result
     */
    [[nodiscard]] auto get(const std::string& cacheKey)
        -> std::optional<OnlineQueryResult>;

    /**
     * @brief Store result in cache
     */
    void put(const std::string& cacheKey, const OnlineQueryResult& result,
             std::optional<std::chrono::minutes> ttl = std::nullopt);

    /**
     * @brief Generate cache key from provider and params
     */
    [[nodiscard]] static auto generateKey(const std::string& provider,
                                          const OnlineQueryParams& params)
        -> std::string;

    /**
     * @brief Clear all cached entries
     */
    void clear();

    /**
     * @brief Clear entries for specific provider
     */
    void clearProvider(const std::string& provider);

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] auto getStats() const -> CacheStats;

    /**
     * @brief Get TTL for provider
     */
    [[nodiscard]] auto getTTLForProvider(const std::string& provider) const
        -> std::chrono::minutes;

    /**
     * @brief Check if key exists and is valid
     */
    [[nodiscard]] auto contains(const std::string& cacheKey) const -> bool;

    /**
     * @brief Remove specific entry
     */
    void remove(const std::string& cacheKey);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_CACHE_QUERY_CACHE_HPP
