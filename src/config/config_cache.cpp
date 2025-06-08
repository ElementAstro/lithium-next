/*
 * config_cache.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "config_cache.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>


namespace lithium {

struct ConfigCache::Impl {
    Config config_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    Statistics stats_;
    std::shared_ptr<spdlog::logger> logger_;

    std::thread cleanupThread_;
    std::atomic<bool> running_{true};

    explicit Impl(const Config& config) : config_(config) {
        try {
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    "logs/config_cache.log", 1024 * 1024, 5);
            logger_ =
                std::make_shared<spdlog::logger>("config_cache", file_sink);
            logger_->set_level(spdlog::level::info);
            spdlog::register_logger(logger_);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create config cache logger: {}", e.what());
        }

        if (config_.enableStats && logger_) {
            logger_->info(
                "ConfigCache initialized with max size: {}, default TTL: {}ms",
                config_.maxSize, config_.defaultTtl.count());
        }

        if (config_.cleanupInterval.count() > 0) {
            cleanupThread_ = std::thread(&Impl::cleanupWorker, this);
        }
    }

    ~Impl() {
        running_ = false;
        if (cleanupThread_.joinable()) {
            cleanupThread_.join();
        }

        if (logger_) {
            logger_->info(
                "ConfigCache destroyed. Final stats - Hits: {}, Misses: {}, "
                "Hit ratio: {:.2f}%",
                stats_.hits.load(), stats_.misses.load(), stats_.getHitRatio());
        }
    }

    void cleanupWorker() {
        while (running_) {
            std::this_thread::sleep_for(config_.cleanupInterval);
            if (!running_)
                break;

            const auto cleaned = performCleanup();
            if (cleaned > 0 && logger_) {
                logger_->debug("Cleaned up {} expired cache entries", cleaned);
            }
        }
    }

    size_t performCleanup() {
        std::unique_lock lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        size_t cleaned = 0;

        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second->expiry <= now) {
                it = cache_.erase(it);
                ++cleaned;
                ++stats_.expirations;
            } else {
                ++it;
            }
        }

        stats_.currentSize = cache_.size();
        return cleaned;
    }

    void evictLru() {
        if (cache_.empty())
            return;

        auto lruIt = std::min_element(
            cache_.begin(), cache_.end(), [](const auto& a, const auto& b) {
                return a.second->lastAccess < b.second->lastAccess;
            });

        if (lruIt != cache_.end()) {
            if (logger_) {
                logger_->debug("Evicting LRU cache entry: {}", lruIt->first);
            }
            cache_.erase(lruIt);
            ++stats_.evictions;
        }
    }

    bool isExpired(const CacheEntry& entry) const noexcept {
        return entry.expiry <= std::chrono::steady_clock::now();
    }
};

ConfigCache::ConfigCache(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

ConfigCache::~ConfigCache() = default;

std::optional<json> ConfigCache::get(std::string_view key) {
    std::shared_lock readLock(impl_->mutex_);

    const auto it = impl_->cache_.find(std::string(key));
    if (it == impl_->cache_.end()) {
        ++impl_->stats_.misses;
        return std::nullopt;
    }

    auto& entry = *it->second;

    if (impl_->isExpired(entry)) {
        readLock.unlock();
        std::unique_lock writeLock(impl_->mutex_);

        const auto it2 = impl_->cache_.find(std::string(key));
        if (it2 != impl_->cache_.end() && impl_->isExpired(*it2->second)) {
            impl_->cache_.erase(it2);
            ++impl_->stats_.expirations;
            ++impl_->stats_.misses;
            impl_->stats_.currentSize = impl_->cache_.size();
        }
        return std::nullopt;
    }

    entry.lastAccess = std::chrono::steady_clock::now();
    ++entry.accessCount;
    ++impl_->stats_.hits;

    if (impl_->logger_) {
        impl_->logger_->trace("Cache hit for key: {}", key);
    }

    return entry.value;
}

void ConfigCache::put(std::string_view key, json value,
                      std::chrono::milliseconds ttl) {
    std::unique_lock lock(impl_->mutex_);

    const auto effectiveTtl = ttl.count() > 0 ? ttl : impl_->config_.defaultTtl;
    auto entry = std::make_unique<CacheEntry>(std::move(value), effectiveTtl);

    const std::string keyStr(key);

    if (impl_->cache_.size() >= impl_->config_.maxSize &&
        impl_->cache_.find(keyStr) == impl_->cache_.end()) {
        impl_->evictLru();
    }

    impl_->cache_[keyStr] = std::move(entry);
    impl_->stats_.currentSize = impl_->cache_.size();

    if (impl_->logger_) {
        impl_->logger_->trace("Cached value for key: {}, TTL: {}ms", key,
                              effectiveTtl.count());
    }
}

bool ConfigCache::remove(std::string_view key) {
    std::unique_lock lock(impl_->mutex_);

    const auto removed = impl_->cache_.erase(std::string(key)) > 0;
    if (removed) {
        impl_->stats_.currentSize = impl_->cache_.size();
        if (impl_->logger_) {
            impl_->logger_->trace("Removed cache entry for key: {}", key);
        }
    }

    return removed;
}

bool ConfigCache::contains(std::string_view key) const {
    std::shared_lock lock(impl_->mutex_);

    const auto it = impl_->cache_.find(std::string(key));
    if (it == impl_->cache_.end()) {
        return false;
    }

    return !impl_->isExpired(*it->second);
}

void ConfigCache::clear() {
    std::unique_lock lock(impl_->mutex_);

    const auto oldSize = impl_->cache_.size();
    impl_->cache_.clear();
    impl_->stats_.currentSize = 0;

    if (impl_->logger_ && oldSize > 0) {
        impl_->logger_->info("Cleared {} cache entries", oldSize);
    }
}

ConfigCache::Statistics ConfigCache::getStatistics() const {
    std::shared_lock lock(impl_->mutex_);
    impl_->stats_.currentSize = impl_->cache_.size();
    return impl_->stats_;
}

size_t ConfigCache::cleanup() { return impl_->performCleanup(); }

size_t ConfigCache::size() const {
    std::shared_lock lock(impl_->mutex_);
    return impl_->cache_.size();
}

bool ConfigCache::empty() const {
    std::shared_lock lock(impl_->mutex_);
    return impl_->cache_.empty();
}

void ConfigCache::setMaxSize(size_t newMaxSize) {
    std::unique_lock lock(impl_->mutex_);
    impl_->config_.maxSize = newMaxSize;

    while (impl_->cache_.size() > newMaxSize) {
        impl_->evictLru();
    }

    if (impl_->logger_) {
        impl_->logger_->info("Cache max size changed to: {}", newMaxSize);
    }
}

void ConfigCache::setDefaultTtl(std::chrono::milliseconds newTtl) {
    std::unique_lock lock(impl_->mutex_);
    impl_->config_.defaultTtl = newTtl;

    if (impl_->logger_) {
        impl_->logger_->info("Cache default TTL changed to: {}ms",
                             newTtl.count());
    }
}

}  // namespace lithium
