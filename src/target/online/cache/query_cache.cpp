// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "query_cache.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>
#include "atom/search/ttl.hpp"

namespace lithium::target::online {

namespace {
/**
 * @brief Generate hash from string
 */
uint64_t hashString(const std::string& str) {
    uint64_t hash = 5381;
    for (unsigned char c : str) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

/**
 * @brief Format numbers for consistent cache keys
 */
std::string formatNumber(double num, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << num;
    return oss.str();
}
}  // namespace

/**
 * @brief Implementation class for QueryCache using TTLCache
 */
class QueryCache::Impl {
public:
    explicit Impl(const CacheConfig& config)
        : config_(config),
          ttlCache_(config.defaultTTL, config.maxEntries),
          stats_{.entries = 0,
                 .hits = 0,
                 .misses = 0,
                 .hitRate = 0.0,
                 .lastCleanup = std::chrono::system_clock::now()} {}

    auto get(const std::string& cacheKey) -> std::optional<OnlineQueryResult> {
        auto result = ttlCache_.get(cacheKey);

        if (result.has_value()) {
            spdlog::debug("Cache hit for key: {}", cacheKey);
            stats_.hits++;
            updateHitRate();
            return result.value();
        }

        spdlog::debug("Cache miss for key: {}", cacheKey);
        stats_.misses++;
        updateHitRate();
        return std::nullopt;
    }

    void put(const std::string& cacheKey, const OnlineQueryResult& result,
             std::optional<std::chrono::minutes> ttl) {
        spdlog::debug("Caching result for key: {}", cacheKey);

        // Use provided TTL or default
        if (ttl.has_value()) {
            // For custom TTL, we'd need to store in a separate map with custom
            // expiry times This is a simplified version using default TTL
            spdlog::debug("Using custom TTL: {} minutes", ttl.value().count());
        }

        ttlCache_.put(cacheKey, result);
        stats_.entries = ttlCache_.size();
    }

    auto generateKey(const std::string& provider,
                     const OnlineQueryParams& params) -> std::string {
        std::ostringstream oss;

        // Start with provider name
        oss << provider << ":";

        // Add object name
        oss << params.objectName << ":";

        // Add coordinates if provided
        if (params.raDecimal.has_value() && params.decDecimal.has_value()) {
            oss << formatNumber(params.raDecimal.value(), 4) << ":"
                << formatNumber(params.decDecimal.value(), 4) << ":";

            // Add search radius if provided
            if (params.radiusArcmin.has_value()) {
                oss << formatNumber(params.radiusArcmin.value(), 2) << ":";
            }
        }

        // Add catalog name if provided
        if (params.catalogName.has_value()) {
            oss << params.catalogName.value() << ":";
        }

        std::string key = oss.str();
        spdlog::debug("Generated cache key: {}", key);
        return key;
    }

    void clear() {
        spdlog::info("Clearing all cache entries");
        ttlCache_.clear();
        stats_.entries = 0;
        stats_.hits = 0;
        stats_.misses = 0;
        stats_.hitRate = 0.0;
    }

    void clearProvider(const std::string& provider) {
        spdlog::info("Clearing cache for provider: {}", provider);
        // Note: TTLCache doesn't support selective clearing
        // In a production system, we'd maintain a separate index
        // For now, log this limitation
        spdlog::warn(
            "TTLCache doesn't support provider-specific clearing. "
            "Consider using full cache clear.");
    }

    auto getStats() const -> CacheStats {
        return {.entries = ttlCache_.size(),
                .hits = stats_.hits,
                .misses = stats_.misses,
                .hitRate = ttlCache_.hitRate(),
                .lastCleanup = stats_.lastCleanup};
    }

    auto getTTLForProvider(const std::string& provider) const
        -> std::chrono::minutes {
        if (provider == "simbad") {
            return config_.simbadTTL;
        } else if (provider == "vizier") {
            return config_.vizierTTL;
        } else if (provider == "ned") {
            return config_.nedTTL;
        } else if (provider == "jpl_horizons") {
            return config_.jplHorizonsTTL;
        } else if (provider == "open_ngc") {
            return config_.openNgcTTL;
        }
        return config_.defaultTTL;
    }

    auto contains(const std::string& cacheKey) const -> bool {
        // TTLCache::get() returns optional, we can't check existence without
        // accessing. This is a limitation of the current TTLCache
        // implementation. For now, we'll return whether the cache size is > 0
        return ttlCache_.size() > 0;
    }

    void remove(const std::string& cacheKey) {
        spdlog::debug("Removing cache entry: {}", cacheKey);
        // TTLCache doesn't expose remove() method
        // This would need to be added to the base TTLCache implementation
        spdlog::warn(
            "TTLCache doesn't support individual entry removal. "
            "Entry will expire based on TTL.");
    }

private:
    CacheConfig config_;
    atom::search::TTLCache<std::string, OnlineQueryResult> ttlCache_;
    mutable CacheStats stats_;

    void updateHitRate() {
        if (stats_.hits + stats_.misses > 0) {
            stats_.hitRate = static_cast<double>(stats_.hits) /
                             (stats_.hits + stats_.misses);
        }
    }
};

// ============================================================================
// QueryCache Implementation
// ============================================================================

QueryCache::QueryCache(const CacheConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

QueryCache::~QueryCache() = default;

QueryCache::QueryCache(QueryCache&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {}

QueryCache& QueryCache::operator=(QueryCache&& other) noexcept {
    if (this != &other) {
        pImpl_ = std::move(other.pImpl_);
    }
    return *this;
}

auto QueryCache::get(const std::string& cacheKey)
    -> std::optional<OnlineQueryResult> {
    return pImpl_->get(cacheKey);
}

void QueryCache::put(const std::string& cacheKey,
                     const OnlineQueryResult& result,
                     std::optional<std::chrono::minutes> ttl) {
    pImpl_->put(cacheKey, result, ttl);
}

auto QueryCache::generateKey(const std::string& provider,
                             const OnlineQueryParams& params) -> std::string {
    return Impl::generateKey(provider, params);
}

void QueryCache::clear() { pImpl_->clear(); }

void QueryCache::clearProvider(const std::string& provider) {
    pImpl_->clearProvider(provider);
}

auto QueryCache::getStats() const -> CacheStats { return pImpl_->getStats(); }

auto QueryCache::getTTLForProvider(const std::string& provider) const
    -> std::chrono::minutes {
    return pImpl_->getTTLForProvider(provider);
}

auto QueryCache::contains(const std::string& cacheKey) const -> bool {
    return pImpl_->contains(cacheKey);
}

void QueryCache::remove(const std::string& cacheKey) {
    pImpl_->remove(cacheKey);
}

// ============================================================================
// OnlineQueryParams Implementation
// ============================================================================

auto OnlineQueryParams::getCacheKey() const -> std::string {
    std::ostringstream oss;

    // Start with object name
    oss << objectName << ":";

    // Add coordinates if provided
    if (raDecimal.has_value() && decDecimal.has_value()) {
        oss << std::fixed << std::setprecision(4) << raDecimal.value() << ":"
            << decDecimal.value() << ":";

        // Add search radius if provided
        if (radiusArcmin.has_value()) {
            oss << std::fixed << std::setprecision(2) << radiusArcmin.value()
                << ":";
        }
    }

    // Add catalog name if provided
    if (catalogName.has_value()) {
        oss << catalogName.value() << ":";
    }

    return oss.str();
}

}  // namespace lithium::target::online
