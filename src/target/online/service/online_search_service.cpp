// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "online_search_service.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include "../client/http_client.hpp"
#include "../provider/jpl_horizons_provider.hpp"
#include "../provider/ned_provider.hpp"
#include "../provider/open_ngc_provider.hpp"
#include "../provider/simbad_provider.hpp"
#include "../provider/vizier_provider.hpp"

namespace lithium::target::online {

/**
 * @brief Implementation of OnlineSearchService (PIMPL pattern)
 *
 * Handles all internal state and logic for the online search service.
 * Uses thread-safe synchronization primitives for concurrent access.
 */
class OnlineSearchService::Impl {
public:
    explicit Impl(const OnlineSearchConfig& config)
        : config_(config),
          initialized_(false),
          httpClient_(std::make_shared<AsyncHttpClient>()),
          cache_(std::make_shared<QueryCache>(config.cacheConfig)),
          rateLimiter_(std::make_shared<ApiRateLimiter>()),
          stats_{} {
        stats_.lastQuery = std::chrono::system_clock::now();

        // Initialize provider enabled map
        enabled_providers_["SIMBAD"] = config.enableSimbad;
        enabled_providers_["VizieR"] = config.enableVizier;
        enabled_providers_["NED"] = config.enableNed;
        enabled_providers_["JPL_Horizons"] = config.enableJplHorizons;
        enabled_providers_["OpenNGC"] = config.enableOpenNgc;
    }

    ~Impl() = default;

    // Non-copyable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Thread-safe lock for read operations
    mutable std::shared_mutex state_mutex_;

    OnlineSearchConfig config_;
    bool initialized_;

    // Shared infrastructure
    std::shared_ptr<AsyncHttpClient> httpClient_;
    std::shared_ptr<QueryCache> cache_;
    std::shared_ptr<ApiRateLimiter> rateLimiter_;

    // Provider registry
    std::unordered_map<std::string, OnlineProviderPtr> providers_;
    std::unordered_map<std::string, bool> enabled_providers_;

    // Statistics
    OnlineSearchStats stats_;

    /**
     * @brief Initialize rate limiting for a provider
     */
    void initializeRateLimiter(const std::string& provider) {
        if (config_.rateLimits.find(provider) != config_.rateLimits.end()) {
            rateLimiter_->setProviderLimit(provider,
                                           config_.rateLimits.at(provider));
        }
    }

    /**
     * @brief Apply rate limiting before query
     */
    auto applyRateLimit(const std::string& provider)
        -> std::optional<std::chrono::milliseconds> {
        auto waitTime = rateLimiter_->tryAcquire(provider);
        if (waitTime) {
            SPDLOG_DEBUG("Rate limit applied to {}: wait {}ms", provider,
                         waitTime->count());
            std::this_thread::sleep_for(*waitTime);
        }
        return waitTime;
    }

    /**
     * @brief Record query execution statistics
     */
    void recordQueryExecution(const std::string& provider,
                              std::chrono::milliseconds duration, bool success,
                              bool fromCache) {
        std::unique_lock<std::shared_mutex> lock(state_mutex_);

        stats_.totalQueries++;
        stats_.totalQueryTime += duration;
        if (fromCache) {
            stats_.cachedQueries++;
        }
        if (success) {
            stats_.successfulQueries++;
        } else {
            stats_.failedQueries++;
        }

        if (stats_.queriesPerProvider.find(provider) ==
            stats_.queriesPerProvider.end()) {
            stats_.queriesPerProvider[provider] = 0;
        }
        stats_.queriesPerProvider[provider]++;

        // Update average query time
        if (stats_.totalQueries > 0) {
            stats_.avgQueryTime =
                stats_.totalQueryTime / static_cast<int>(stats_.totalQueries);
        }

        stats_.lastQuery = std::chrono::system_clock::now();
    }
};

// ============================================================================
// Public API Implementation
// ============================================================================

OnlineSearchService::OnlineSearchService(const OnlineSearchConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

OnlineSearchService::~OnlineSearchService() = default;

OnlineSearchService::OnlineSearchService(OnlineSearchService&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {}

OnlineSearchService& OnlineSearchService::operator=(
    OnlineSearchService&& other) noexcept {
    pImpl_ = std::move(other.pImpl_);
    return *this;
}

auto OnlineSearchService::initialize()
    -> atom::type::Expected<void, std::string> {
    std::unique_lock<std::shared_mutex> lock(pImpl_->state_mutex_);

    if (pImpl_->initialized_) {
        return {};  // Already initialized
    }

    try {
        // Create HTTP client
        HttpClientConfig httpConfig;
        httpConfig.defaultTimeout = pImpl_->config_.queryTimeout;
        pImpl_->httpClient_ = std::make_shared<AsyncHttpClient>(httpConfig);

        // Initialize cache
        pImpl_->cache_ =
            std::make_shared<QueryCache>(pImpl_->config_.cacheConfig);

        // Initialize rate limiter with provider-specific rules
        for (const auto& [provider, rule] : pImpl_->config_.rateLimits) {
            pImpl_->rateLimiter_->setProviderLimit(provider, rule);
        }

        // Create SIMBAD provider
        if (pImpl_->config_.enableSimbad) {
            try {
                auto simbadProvider = std::make_shared<SimbadProvider>(
                    pImpl_->httpClient_, pImpl_->cache_, pImpl_->rateLimiter_);
                pImpl_->providers_["SIMBAD"] = simbadProvider;
                pImpl_->initializeRateLimiter("SIMBAD");
                SPDLOG_INFO("SIMBAD provider initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize SIMBAD provider: {}",
                            e.what());
            }
        }

        // Create VizieR provider
        if (pImpl_->config_.enableVizier) {
            try {
                auto vizierProvider = std::make_shared<VizierProvider>(
                    pImpl_->httpClient_, pImpl_->cache_, pImpl_->rateLimiter_);
                pImpl_->providers_["VizieR"] = vizierProvider;
                pImpl_->initializeRateLimiter("VizieR");
                SPDLOG_INFO("VizieR provider initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize VizieR provider: {}",
                            e.what());
            }
        }

        // Create NED provider
        if (pImpl_->config_.enableNed) {
            try {
                auto nedProvider = std::make_shared<NedProvider>(
                    pImpl_->httpClient_, pImpl_->cache_, pImpl_->rateLimiter_);
                pImpl_->providers_["NED"] = nedProvider;
                pImpl_->initializeRateLimiter("NED");
                SPDLOG_INFO("NED provider initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize NED provider: {}", e.what());
            }
        }

        // Create JPL Horizons provider
        if (pImpl_->config_.enableJplHorizons) {
            try {
                auto jplProvider = std::make_shared<JplHorizonsProvider>(
                    pImpl_->httpClient_, pImpl_->cache_, pImpl_->rateLimiter_);
                pImpl_->providers_["JPL_Horizons"] = jplProvider;
                pImpl_->initializeRateLimiter("JPL_Horizons");
                SPDLOG_INFO("JPL Horizons provider initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize JPL Horizons provider: {}",
                            e.what());
            }
        }

        // Create OpenNGC provider
        if (pImpl_->config_.enableOpenNgc) {
            try {
                auto openNgcProvider = std::make_shared<OpenNgcProvider>(
                    pImpl_->httpClient_, pImpl_->cache_, pImpl_->rateLimiter_);
                pImpl_->providers_["OpenNGC"] = openNgcProvider;
                pImpl_->initializeRateLimiter("OpenNGC");
                SPDLOG_INFO("OpenNGC provider initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize OpenNGC provider: {}",
                            e.what());
            }
        }

        if (pImpl_->providers_.empty()) {
            return atom::type::unexpected("No providers could be initialized");
        }

        pImpl_->initialized_ = true;
        SPDLOG_INFO("OnlineSearchService initialized with {} providers",
                    pImpl_->providers_.size());

        return {};

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to initialize OnlineSearchService: {}", e.what());
        return atom::type::unexpected(std::string(e.what()));
    }
}

bool OnlineSearchService::isInitialized() const {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    return pImpl_->initialized_;
}

auto OnlineSearchService::queryProvider(const std::string& providerName,
                                        const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);

    auto it = pImpl_->providers_.find(providerName);
    if (it == pImpl_->providers_.end()) {
        OnlineQueryError err;
        err.code = OnlineQueryError::Code::Unknown;
        err.message = "Provider not found: " + providerName;
        err.provider = providerName;
        return atom::type::unexpected(err);
    }

    auto provider = it->second;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Apply rate limiting
    pImpl_->applyRateLimit(providerName);

    // Check cache
    auto cacheKey = QueryCache::generateKey(providerName, params);
    if (auto cached = pImpl_->cache_->get(cacheKey)) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        pImpl_->recordQueryExecution(providerName, duration, true, true);
        SPDLOG_DEBUG("Cache hit for {} query", providerName);
        return *cached;
    }

    // Execute query with retries
    OnlineQueryError lastError;
    for (int attempt = 0; attempt < pImpl_->config_.maxRetries; ++attempt) {
        if (attempt > 0) {
            SPDLOG_DEBUG("Retrying {} query (attempt {}/{})", providerName,
                         attempt + 1, pImpl_->config_.maxRetries);
            std::this_thread::sleep_for(pImpl_->config_.retryDelay);
        }

        auto result = provider->query(params);
        if (result) {
            // Cache successful result
            pImpl_->cache_->put(cacheKey, *result);

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);
            pImpl_->recordQueryExecution(providerName, duration, true, false);
            pImpl_->rateLimiter_->recordRequestComplete(providerName, true);

            SPDLOG_DEBUG("{} query successful", providerName);
            return *result;
        }

        lastError = result.error();

        // Handle rate limiting with Retry-After
        if (lastError.code == OnlineQueryError::Code::RateLimited &&
            lastError.retryAfter) {
            pImpl_->rateLimiter_->recordRateLimitResponse(
                providerName, *lastError.retryAfter);
        }

        // Don't retry on non-retryable errors
        if (!lastError.isRetryable()) {
            break;
        }

        pImpl_->rateLimiter_->recordRequestComplete(providerName, false);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    pImpl_->recordQueryExecution(providerName, duration, false, false);
    pImpl_->rateLimiter_->recordRequestComplete(providerName, false);

    SPDLOG_WARN("{} query failed: {}", providerName, lastError.message);
    return atom::type::unexpected(lastError);
}

auto OnlineSearchService::queryAll(const OnlineQueryParams& params)
    -> std::vector<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);

    std::vector<
        std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>>>
        futures;
    futures.reserve(pImpl_->providers_.size());

    SPDLOG_DEBUG("Executing parallel queries to all {} providers",
                 pImpl_->providers_.size());

    // Launch queries in parallel with concurrency limiting
    size_t activeQueries = 0;
    for (const auto& [name, provider] : pImpl_->providers_) {
        if (!pImpl_->enabled_providers_[name]) {
            continue;
        }

        // Limit concurrent providers
        if (activeQueries >= pImpl_->config_.maxConcurrentProviders) {
            // Wait for a query to complete
            for (auto& f : futures) {
                if (f.valid()) {
                    f.wait();
                }
            }
            activeQueries = 0;
        }

        // Launch async query
        auto future = std::async(std::launch::async, [this, name, &params]() {
            return queryProvider(name, params);
        });
        futures.push_back(std::move(future));
        activeQueries++;
    }

    // Collect results
    std::vector<atom::type::Expected<OnlineQueryResult, OnlineQueryError>>
        results;
    for (auto& f : futures) {
        if (f.valid()) {
            results.push_back(f.get());
        }
    }

    SPDLOG_DEBUG("Parallel queries completed: {} results", results.size());
    return results;
}

auto OnlineSearchService::queryAuto(const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);

    // Select best provider based on query type
    std::string selectedProvider;
    switch (params.type) {
        case QueryType::ByName:
            selectedProvider = "SIMBAD";
            break;
        case QueryType::ByCoordinates:
            selectedProvider = "SIMBAD";
            break;
        case QueryType::Ephemeris:
            selectedProvider = "JPL_Horizons";
            break;
        case QueryType::ByConstellation:
            selectedProvider = "OpenNGC";
            break;
        case QueryType::Catalog:
            selectedProvider = "VizieR";
            break;
        default:
            selectedProvider = "SIMBAD";
    }

    // Verify provider is available
    if (pImpl_->providers_.find(selectedProvider) == pImpl_->providers_.end() ||
        !pImpl_->enabled_providers_[selectedProvider]) {
        // Fallback to first available provider
        if (!pImpl_->providers_.empty()) {
            selectedProvider = pImpl_->providers_.begin()->first;
        } else {
            OnlineQueryError err;
            err.code = OnlineQueryError::Code::ServiceUnavailable;
            err.message = "No providers available";
            return atom::type::unexpected(err);
        }
    }

    SPDLOG_DEBUG("Auto-selected provider: {} for query type: {}",
                 selectedProvider, static_cast<int>(params.type));

    return queryProvider(selectedProvider, params);
}

auto OnlineSearchService::queryWithFallback(
    const OnlineQueryParams& params,
    const std::vector<std::string>& providerPriority)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);

    // Determine priority list
    std::vector<std::string> priority = providerPriority.empty()
                                            ? pImpl_->config_.providerPriority
                                            : providerPriority;

    OnlineQueryError lastError;
    for (const auto& provider : priority) {
        if (pImpl_->providers_.find(provider) == pImpl_->providers_.end() ||
            !pImpl_->enabled_providers_[provider]) {
            continue;
        }

        SPDLOG_DEBUG("Attempting query with provider: {}", provider);
        auto result = queryProvider(provider, params);
        if (result) {
            SPDLOG_INFO("Query succeeded with provider: {}", provider);
            return result;
        }

        lastError = result.error();
        if (!lastError.isRetryable()) {
            // Don't fallback on permanent errors
            SPDLOG_WARN("Non-retryable error from {}: {}", provider,
                        lastError.message);
            return result;
        }

        SPDLOG_DEBUG("Provider {} failed, trying next in chain", provider);
    }

    lastError.message =
        "All providers in fallback chain failed: " + lastError.message;
    SPDLOG_ERROR("Fallback chain exhausted: {}", lastError.message);
    return atom::type::unexpected(lastError);
}

auto OnlineSearchService::searchByName(const std::string& name, int limit)
    -> std::vector<CelestialObjectModel> {
    OnlineQueryParams params;
    params.type = QueryType::ByName;
    params.query = name;
    params.limit = limit;

    auto result = queryAuto(params);
    if (result) {
        return result->objects;
    }
    return {};
}

auto OnlineSearchService::searchByCoordinates(double ra, double dec,
                                              double radiusDeg, int limit)
    -> std::vector<CelestialObjectModel> {
    OnlineQueryParams params;
    params.type = QueryType::ByCoordinates;
    params.ra = ra;
    params.dec = dec;
    params.radius = radiusDeg;
    params.limit = limit;

    auto result = queryAuto(params);
    if (result) {
        return result->objects;
    }
    return {};
}

auto OnlineSearchService::getEphemeris(
    const std::string& target, std::chrono::system_clock::time_point time)
    -> atom::type::Expected<EphemerisPoint, OnlineQueryError> {
    OnlineQueryParams params;
    params.type = QueryType::Ephemeris;
    params.query = target;
    params.epoch = time;

    auto result = queryProvider("JPL_Horizons", params);
    if (result && !result->ephemerisData.empty()) {
        return result->ephemerisData.front();
    }

    OnlineQueryError err;
    err.code = OnlineQueryError::Code::NotFound;
    err.message = "No ephemeris data available for: " + target;
    err.provider = "JPL_Horizons";
    return atom::type::unexpected(err);
}

auto OnlineSearchService::getAvailableProviders() const
    -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    std::vector<std::string> providers;
    for (const auto& [name, provider] : pImpl_->providers_) {
        providers.push_back(name);
    }
    return providers;
}

auto OnlineSearchService::isProviderAvailable(const std::string& name) const
    -> bool {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    auto it = pImpl_->providers_.find(name);
    if (it == pImpl_->providers_.end()) {
        return false;
    }
    return it->second && it->second->isAvailable();
}

void OnlineSearchService::setProviderEnabled(const std::string& name,
                                             bool enabled) {
    std::unique_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    if (pImpl_->enabled_providers_.find(name) !=
        pImpl_->enabled_providers_.end()) {
        pImpl_->enabled_providers_[name] = enabled;
        SPDLOG_INFO("Provider {} {}", name, enabled ? "enabled" : "disabled");
    }
}

auto OnlineSearchService::getProvider(const std::string& name) const
    -> OnlineProviderPtr {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    auto it = pImpl_->providers_.find(name);
    if (it != pImpl_->providers_.end()) {
        return it->second;
    }
    return nullptr;
}

auto OnlineSearchService::getCacheStats() const -> CacheStats {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    return pImpl_->cache_->getStats();
}

void OnlineSearchService::clearCache() {
    std::unique_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    pImpl_->cache_->clear();
    SPDLOG_INFO("All caches cleared");
}

void OnlineSearchService::clearProviderCache(const std::string& provider) {
    std::unique_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    pImpl_->cache_->clearProvider(provider);
    SPDLOG_INFO("Cache cleared for provider: {}", provider);
}

auto OnlineSearchService::getStats() const -> OnlineSearchStats {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    return pImpl_->stats_;
}

void OnlineSearchService::resetStats() {
    std::unique_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    pImpl_->stats_ = OnlineSearchStats{};
    pImpl_->stats_.lastQuery = std::chrono::system_clock::now();
    SPDLOG_INFO("Service statistics reset");
}

auto OnlineSearchService::getConfig() const -> const OnlineSearchConfig& {
    std::shared_lock<std::shared_mutex> lock(pImpl_->state_mutex_);
    return pImpl_->config_;
}

}  // namespace lithium::target::online
