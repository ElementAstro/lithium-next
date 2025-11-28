/*
 * cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: High-performance Configuration Cache Component

**************************************************/

#ifndef LITHIUM_CONFIG_COMPONENTS_CACHE_HPP
#define LITHIUM_CONFIG_COMPONENTS_CACHE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium::config {

/**
 * @brief High-performance configuration cache with LRU eviction and TTL support
 *
 * This class provides thread-safe caching for configuration values with:
 * - LRU (Least Recently Used) eviction policy
 * - TTL (Time To Live) support for cache entries
 * - Lock-free read operations where possible
 * - Memory-efficient storage with move semantics
 */
class ConfigCache {
public:
    /**
     * @brief Cache entry structure with metadata
     */
    struct CacheEntry {
        json value;  ///< Cached JSON value
        std::chrono::steady_clock::time_point
            lastAccess;  ///< Last access time for LRU
        std::chrono::steady_clock::time_point expiry;  ///< Expiry time for TTL
        std::atomic<uint64_t> accessCount{0};  ///< Access frequency counter

        CacheEntry(json val, std::chrono::milliseconds ttl =
                                 std::chrono::milliseconds::zero())
            : value(std::move(val)),
              lastAccess(std::chrono::steady_clock::now()),
              expiry(ttl.count() > 0
                         ? lastAccess + ttl
                         : std::chrono::steady_clock::time_point::max()) {}
    };

    /**
     * @brief Configuration for cache behavior
     */
    struct Config {
        size_t maxSize{1000};  ///< Maximum number of cached entries
        std::chrono::milliseconds defaultTtl{
            30000};  ///< Default TTL (30 seconds)
        std::chrono::milliseconds cleanupInterval{
            60000};              ///< Cleanup interval (1 minute)
        bool enableStats{true};  ///< Enable cache statistics
    };

    /**
     * @brief Cache statistics for monitoring
     */
    struct Statistics {
        std::atomic<uint64_t> hits{0};         ///< Cache hits
        std::atomic<uint64_t> misses{0};       ///< Cache misses
        std::atomic<uint64_t> evictions{0};    ///< Number of evictions
        std::atomic<uint64_t> expirations{0};  ///< Number of expirations
        std::atomic<uint64_t> currentSize{0};  ///< Current cache size

        // Explicit copy constructor for atomics
        Statistics(const Statistics& other)
            : hits(other.hits.load()),
              misses(other.misses.load()),
              evictions(other.evictions.load()),
              expirations(other.expirations.load()),
              currentSize(other.currentSize.load()) {}

        Statistics() = default;

        /**
         * @brief Calculate cache hit ratio
         * @return Hit ratio as percentage (0.0 - 100.0)
         */
        [[nodiscard]] double getHitRatio() const noexcept {
            const auto totalAccess = hits.load() + misses.load();
            return totalAccess > 0
                       ? (static_cast<double>(hits.load()) / totalAccess) *
                             100.0
                       : 0.0;
        }
    };

    /**
     * @brief Constructor with configuration
     * @param config Cache configuration parameters
     */
    explicit ConfigCache(const Config& config);

    /**
     * @brief Default constructor with default configuration
     */
    ConfigCache();

    /**
     * @brief Destructor
     */
    ~ConfigCache();

    ConfigCache(const ConfigCache&) = delete;
    ConfigCache& operator=(const ConfigCache&) = delete;
    ConfigCache(ConfigCache&&) noexcept = default;
    ConfigCache& operator=(ConfigCache&&) noexcept = default;

    /**
     * @brief Get cached value for key
     * @param key Cache key
     * @return Optional cached value, nullopt if not found or expired
     */
    [[nodiscard]] std::optional<json> get(std::string_view key);

    /**
     * @brief Store value in cache
     * @param key Cache key
     * @param value JSON value to cache
     * @param ttl Time to live for this entry (uses default if not specified)
     */
    void put(std::string_view key, json value,
             std::chrono::milliseconds ttl = std::chrono::milliseconds::zero());

    /**
     * @brief Remove entry from cache
     * @param key Cache key to remove
     * @return True if entry was removed, false if not found
     */
    bool remove(std::string_view key);

    /**
     * @brief Check if key exists and is not expired
     * @param key Cache key to check
     * @return True if key exists and is valid
     */
    [[nodiscard]] bool contains(std::string_view key) const;

    /**
     * @brief Clear all cache entries
     */
    void clear();

    /**
     * @brief Get current cache statistics
     * @return Cache statistics structure
     */
    [[nodiscard]] Statistics getStatistics() const;

    /**
     * @brief Manually trigger cleanup of expired entries
     * @return Number of entries cleaned up
     */
    size_t cleanup();

    /**
     * @brief Get current cache size
     * @return Number of entries in cache
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Check if cache is empty
     * @return True if cache is empty
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Set new maximum cache size
     * @param newMaxSize New maximum size (triggers eviction if needed)
     */
    void setMaxSize(size_t newMaxSize);

    /**
     * @brief Set new default TTL
     * @param newTtl New default time to live
     */
    void setDefaultTtl(std::chrono::milliseconds newTtl);

    /**
     * @brief Get current configuration
     * @return Current cache configuration
     */
    [[nodiscard]] const Config& getConfig() const;

    /**
     * @brief Update cache configuration
     * @param newConfig New configuration to apply
     */
    void setConfig(const Config& newConfig);

    /**
     * @brief Get all cached keys
     * @return Vector of all keys currently in cache
     */
    [[nodiscard]] std::vector<std::string> getKeys() const;

    /**
     * @brief Get or compute value with factory function
     * @param key Cache key
     * @param factory Function to compute value if not cached
     * @param ttl Optional TTL for the entry
     * @return Cached or computed value
     */
    [[nodiscard]] json getOrCompute(
        std::string_view key,
        std::function<json()> factory,
        std::chrono::milliseconds ttl = std::chrono::milliseconds::zero());

    /**
     * @brief Batch get multiple values
     * @param keys Vector of keys to retrieve
     * @return Map of key to optional value
     */
    [[nodiscard]] std::unordered_map<std::string, std::optional<json>> 
        getBatch(const std::vector<std::string>& keys);

    /**
     * @brief Batch put multiple values
     * @param entries Map of key-value pairs to cache
     * @param ttl Optional TTL for all entries
     */
    void putBatch(const std::unordered_map<std::string, json>& entries,
                  std::chrono::milliseconds ttl = std::chrono::milliseconds::zero());

    // ========================================================================
    // Hook Support
    // ========================================================================

    /**
     * @brief Cache event types for hooks
     */
    enum class CacheEvent {
        PUT,        ///< Entry was added/updated
        GET,        ///< Entry was accessed
        REMOVE,     ///< Entry was removed
        EVICT,      ///< Entry was evicted (LRU)
        EXPIRE,     ///< Entry expired (TTL)
        CLEAR       ///< Cache was cleared
    };

    /**
     * @brief Cache hook callback signature
     * @param event Type of cache event
     * @param key The cache key involved
     * @param value Optional value (for PUT/GET events)
     */
    using CacheHook = std::function<void(CacheEvent event, 
                                         std::string_view key, 
                                         const std::optional<json>& value)>;

    /**
     * @brief Register a cache event hook
     * @param hook Callback function for cache events
     * @return Hook ID for later removal
     */
    size_t addHook(CacheHook hook);

    /**
     * @brief Remove a registered hook
     * @param hookId ID of the hook to remove
     * @return True if hook was removed
     */
    bool removeHook(size_t hookId);

    /**
     * @brief Clear all registered hooks
     */
    void clearHooks();

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::config

// Backward compatibility alias
namespace lithium {
using ConfigCache = lithium::config::ConfigCache;
}

#endif  // LITHIUM_CONFIG_COMPONENTS_CACHE_HPP
