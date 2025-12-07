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

#include "cache_manager.hpp"

#include <spdlog/spdlog.h>

namespace lithium::database::cache {

//------------------------------------------------------------------------------
// CacheManager Implementation
//------------------------------------------------------------------------------

CacheManager::CacheManager() : defaultTTL(300), stopPurgeThread(false) {
    // Start purge thread
    purgeThread = std::thread(&CacheManager::purgePeriodically, this);
}

CacheManager::~CacheManager() {
    // Signal the purge thread to stop
    {
        std::lock_guard<std::mutex> lock(stopMutex);
        stopPurgeThread.store(true);
    }
    stopCond.notify_one();

    // Wait for purge thread to finish
    if (purgeThread.joinable()) {
        purgeThread.join();
    }
}

CacheManager& CacheManager::getInstance() {
    static CacheManager instance;
    return instance;
}

void CacheManager::put(const std::string& key, const std::string& value,
                       int ttlSeconds) {
    if (key.empty()) {
        spdlog::warn("Attempted to cache with empty key");
        return;
    }

    if (ttlSeconds <= 0) {
        ttlSeconds = defaultTTL;
    }

    auto expiry =
        std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    cache[key] = CacheEntry{value, expiry};
}

std::optional<std::string> CacheManager::get(const std::string& key) {
    if (key.empty()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = cache.find(key);
    if (it != cache.end()) {
        // Check if entry has expired
        if (std::chrono::steady_clock::now() < it->second.expiry) {
            return it->second.value;
        }
    }
    return std::nullopt;
}

bool CacheManager::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    return cache.erase(key) > 0;
}

void CacheManager::clear() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    cache.clear();
}

size_t CacheManager::purgeExpired() {
    const auto now = std::chrono::steady_clock::now();
    size_t removedCount = 0;

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    for (auto it = cache.begin(); it != cache.end();) {
        if (now >= it->second.expiry) {
            it = cache.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    return removedCount;
}

void CacheManager::setDefaultTTL(int seconds) {
    if (seconds > 0) {
        defaultTTL = seconds;
    }
}

size_t CacheManager::size() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    return cache.size();
}

void CacheManager::purgePeriodically() {
    using namespace std::chrono_literals;

    try {
        while (true) {
            // Wait for stop signal or timeout (60 seconds)
            std::unique_lock<std::mutex> lock(stopMutex);
            if (stopCond.wait_for(lock, 60s,
                                  [this] { return stopPurgeThread.load(); })) {
                // Stop signal received
                break;
            }

            // Timeout - perform purge
            lock.unlock();
            size_t removed = purgeExpired();
            if (removed > 0) {
                spdlog::info("Cache purge: removed {} expired entries",
                             removed);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in cache purge thread: {}", e.what());
    }
}

}  // namespace lithium::database::cache
