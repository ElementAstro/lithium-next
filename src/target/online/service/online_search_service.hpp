// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_SERVICE_ONLINE_SEARCH_SERVICE_HPP
#define LITHIUM_TARGET_ONLINE_SERVICE_ONLINE_SEARCH_SERVICE_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"
#include "../provider/provider_interface.hpp"
#include "../cache/query_cache.hpp"
#include "../rate_limiter/api_rate_limiter.hpp"

namespace lithium::target::online {

/**
 * @brief Merge strategy for combining results
 */
enum class MergeStrategy {
    PreferLocal,      ///< Local data takes precedence
    PreferOnline,     ///< Online data takes precedence
    MostRecent,       ///< Use most recently updated
    MostComplete,     ///< Use entry with most fields populated
    Union             ///< Include all unique results
};

/**
 * @brief Service configuration
 */
struct OnlineSearchConfig {
    // Provider enablement
    bool enableSimbad = true;
    bool enableVizier = true;
    bool enableNed = true;
    bool enableJplHorizons = true;
    bool enableOpenNgc = true;

    // Caching
    CacheConfig cacheConfig;

    // Rate limiting
    std::unordered_map<std::string, RateLimitRule> rateLimits;

    // Query behavior
    bool enableFallback = true;
    int maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
    bool enableParallelQueries = true;
    size_t maxConcurrentProviders = 3;

    // Timeouts
    std::chrono::milliseconds queryTimeout{30000};
    std::chrono::milliseconds totalTimeout{60000};

    // Result handling
    MergeStrategy defaultMergeStrategy = MergeStrategy::PreferLocal;
    int defaultLimit = 100;

    // Provider priority for fallback
    std::vector<std::string> providerPriority = {
        "SIMBAD", "VizieR", "NED", "OpenNGC", "JPL_Horizons"
    };
};

/**
 * @brief Service statistics
 */
struct OnlineSearchStats {
    size_t totalQueries = 0;
    size_t successfulQueries = 0;
    size_t cachedQueries = 0;
    size_t failedQueries = 0;
    std::chrono::milliseconds avgQueryTime{0};
    std::chrono::milliseconds totalQueryTime{0};
    std::unordered_map<std::string, size_t> queriesPerProvider;
    std::chrono::system_clock::time_point lastQuery;
};

/**
 * @brief Main service facade for online celestial searches
 *
 * Coordinates multiple providers, handles caching, rate limiting,
 * fallback logic, and result merging.
 */
class OnlineSearchService {
public:
    explicit OnlineSearchService(const OnlineSearchConfig& config = {});
    ~OnlineSearchService();

    // Non-copyable, movable
    OnlineSearchService(const OnlineSearchService&) = delete;
    OnlineSearchService& operator=(const OnlineSearchService&) = delete;
    OnlineSearchService(OnlineSearchService&&) noexcept;
    OnlineSearchService& operator=(OnlineSearchService&&) noexcept;

    /**
     * @brief Initialize service and all providers
     */
    [[nodiscard]] auto initialize() -> atom::type::Expected<void, std::string>;

    /**
     * @brief Check if service is initialized
     */
    [[nodiscard]] bool isInitialized() const;

    // ========== Single Provider Queries ==========

    /**
     * @brief Query a specific provider
     */
    [[nodiscard]] auto queryProvider(const std::string& providerName,
                                     const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError>;

    // ========== Multi-Provider Queries ==========

    /**
     * @brief Query all enabled providers in parallel
     */
    [[nodiscard]] auto queryAll(const OnlineQueryParams& params)
        -> std::vector<atom::type::Expected<OnlineQueryResult, OnlineQueryError>>;

    /**
     * @brief Auto-select best provider based on query type
     */
    [[nodiscard]] auto queryAuto(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError>;

    /**
     * @brief Query with fallback chain
     */
    [[nodiscard]] auto queryWithFallback(
        const OnlineQueryParams& params,
        const std::vector<std::string>& providerPriority = {})
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError>;

    // ========== Convenience Methods ==========

    /**
     * @brief Search by object name
     */
    [[nodiscard]] auto searchByName(const std::string& name, int limit = 50)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Search by coordinates (cone search)
     */
    [[nodiscard]] auto searchByCoordinates(double ra, double dec,
                                           double radiusDeg, int limit = 100)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Get ephemeris for solar system object
     */
    [[nodiscard]] auto getEphemeris(
        const std::string& target,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
        -> atom::type::Expected<EphemerisPoint, OnlineQueryError>;

    // ========== Provider Management ==========

    /**
     * @brief Get list of available providers
     */
    [[nodiscard]] auto getAvailableProviders() const -> std::vector<std::string>;

    /**
     * @brief Check if a provider is available
     */
    [[nodiscard]] auto isProviderAvailable(const std::string& name) const -> bool;

    /**
     * @brief Enable/disable a provider
     */
    void setProviderEnabled(const std::string& name, bool enabled);

    /**
     * @brief Get a provider by name
     */
    [[nodiscard]] auto getProvider(const std::string& name) const
        -> OnlineProviderPtr;

    // ========== Cache Management ==========

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] auto getCacheStats() const -> CacheStats;

    /**
     * @brief Clear all caches
     */
    void clearCache();

    /**
     * @brief Clear cache for specific provider
     */
    void clearProviderCache(const std::string& provider);

    // ========== Service Management ==========

    /**
     * @brief Get service statistics
     */
    [[nodiscard]] auto getStats() const -> OnlineSearchStats;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    /**
     * @brief Get service configuration
     */
    [[nodiscard]] auto getConfig() const -> const OnlineSearchConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_SERVICE_ONLINE_SEARCH_SERVICE_HPP
