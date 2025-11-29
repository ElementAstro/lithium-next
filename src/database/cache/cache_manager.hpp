// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LITHIUM_DATABASE_CACHE_CACHE_MANAGER_HPP
#define LITHIUM_DATABASE_CACHE_CACHE_MANAGER_HPP

#include "cache_entry.hpp"

#include <atomic>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace lithium::database::cache {

/**
 * @brief Thread-safe cache manager with TTL support and automatic purging.
 *
 * The CacheManager provides a singleton instance that manages a cache of
 * string key-value pairs with time-to-live (TTL) support. Expired entries
 * are automatically purged by a background thread.
 */
class CacheManager {
public:
    /**
     * @brief Gets the singleton instance of the CacheManager.
     *
     * @return A reference to the CacheManager instance.
     */
    static CacheManager& getInstance();

    /**
     * @brief Destructor that cleans up resources properly.
     */
    ~CacheManager();

    /**
     * @brief Puts a value in the cache.
     *
     * @param key The key for the value.
     * @param value The value to cache.
     * @param ttlSeconds Time-to-live in seconds (default: 300).
     */
    void put(const std::string& key, const std::string& value,
             int ttlSeconds = 300);

    /**
     * @brief Gets a value from the cache.
     *
     * @param key The key for the value.
     * @return An optional containing the value if found, empty if not found or
     * expired.
     */
    std::optional<std::string> get(const std::string& key);

    /**
     * @brief Removes a value from the cache.
     *
     * @param key The key for the value to remove.
     * @return True if the value was removed, false if not found.
     */
    bool remove(const std::string& key);

    /**
     * @brief Clears the entire cache.
     */
    void clear();

    /**
     * @brief Clears expired entries from the cache.
     *
     * @return Number of entries removed.
     */
    size_t purgeExpired();

    /**
     * @brief Sets the default TTL for cache entries.
     *
     * @param seconds TTL in seconds.
     */
    void setDefaultTTL(int seconds);

    /**
     * @brief Gets the current size of the cache.
     *
     * @return The number of entries in the cache.
     */
    size_t size() const;

private:
    CacheManager();  // Private constructor for singleton

    mutable std::shared_mutex
        cacheMutex;  ///< Mutex for synchronizing access to the cache.
    std::unordered_map<std::string, CacheEntry> cache;  ///< The cache storage.
    int defaultTTL = 300;  ///< Default time-to-live in seconds.

    // Automatically purge expired entries periodically
    std::thread purgeThread;
    std::atomic<bool> stopPurgeThread{false};

    void purgePeriodically();
};

}  // namespace lithium::database::cache

#endif  // LITHIUM_DATABASE_CACHE_CACHE_MANAGER_HPP
