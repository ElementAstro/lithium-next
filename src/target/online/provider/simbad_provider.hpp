// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_SIMBAD_PROVIDER_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_SIMBAD_PROVIDER_HPP

#include "provider_interface.hpp"
#include "../client/http_client.hpp"
#include "../cache/query_cache.hpp"
#include "../rate_limiter/api_rate_limiter.hpp"

namespace lithium::target::online {

/**
 * @brief SIMBAD configuration
 */
struct SimbadProviderConfig {
    std::string baseUrl = "https://simbad.u-strasbg.fr/simbad/sim-tap/sync";
    std::chrono::milliseconds timeout{30000};
    int maxRetries = 3;
    bool useCache = true;
    std::chrono::minutes cacheTTL{120};

    // Query options
    bool includeSpectrum = false;
    bool includeDimensions = true;
    bool includeMagnitudes = true;
    bool includeProperMotion = false;
    bool includeRedshift = false;
};

/**
 * @brief SIMBAD astronomical database provider
 *
 * Provides access to the SIMBAD database using TAP protocol.
 * Supports object identifier queries and cone searches.
 *
 * SIMBAD (Set of Identifications, Measurements, and Bibliography
 * for Astronomical Data) is the standard astronomical database
 * maintained by CDS (Centre de Données astronomiques de Strasbourg).
 *
 * API endpoint: https://simbad.u-strasbg.fr/simbad/sim-tap/sync
 * Protocol: TAP (Table Access Protocol) with ADQL (Astronomical Data Query Language)
 * Response format: VOTable XML
 */
class SimbadProvider : public IOnlineProvider {
public:
    static constexpr std::string_view PROVIDER_NAME = "SIMBAD";
    static constexpr std::string_view BASE_URL =
        "https://simbad.u-strasbg.fr/simbad/sim-tap/sync";

    /**
     * @brief Construct SIMBAD provider with dependencies
     *
     * @param httpClient HTTP client for making requests
     * @param cache Optional query cache
     * @param rateLimiter Optional rate limiter
     * @param config SIMBAD-specific configuration
     */
    explicit SimbadProvider(
        std::shared_ptr<AsyncHttpClient> httpClient,
        std::shared_ptr<QueryCache> cache = nullptr,
        std::shared_ptr<ApiRateLimiter> rateLimiter = nullptr,
        const SimbadProviderConfig& config = {});

    ~SimbadProvider() override;

    // Non-copyable, movable
    SimbadProvider(const SimbadProvider&) = delete;
    SimbadProvider& operator=(const SimbadProvider&) = delete;
    SimbadProvider(SimbadProvider&&) noexcept;
    SimbadProvider& operator=(SimbadProvider&&) noexcept;

    /**
     * @brief Execute a synchronous query to SIMBAD
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> override;

    /**
     * @brief Execute an asynchronous query to SIMBAD
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     */
    [[nodiscard]] auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> override;

    /**
     * @brief Get the provider name
     *
     * @return "SIMBAD"
     */
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return PROVIDER_NAME;
    }

    /**
     * @brief Check if SIMBAD service is available
     *
     * Performs a lightweight health check to verify connectivity
     *
     * @return True if service is available
     */
    [[nodiscard]] auto isAvailable() const -> bool override;

    /**
     * @brief Get supported query types
     *
     * SIMBAD supports ByName, ByCoordinates, and Catalog queries
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] auto supportedQueryTypes() const -> std::vector<QueryType> override {
        return {QueryType::ByName, QueryType::ByCoordinates, QueryType::Catalog};
    }

    /**
     * @brief Get provider base URL
     *
     * @return SIMBAD TAP sync endpoint
     */
    [[nodiscard]] auto baseUrl() const noexcept -> std::string_view override {
        return BASE_URL;
    }

    /**
     * @brief Build ADQL query from parameters
     *
     * Constructs an ADQL (Astronomical Data Query Language) query
     * appropriate for the given query type and parameters.
     *
     * @param params Query parameters
     * @return ADQL query string
     * @throws std::invalid_argument if parameters are invalid
     */
    [[nodiscard]] auto buildAdqlQuery(const OnlineQueryParams& params) const
        -> std::string;

    /**
     * @brief Set configuration
     *
     * @param config New configuration
     */
    void setConfig(const SimbadProviderConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return Reference to current configuration
     */
    [[nodiscard]] auto getConfig() const -> const SimbadProviderConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_SIMBAD_PROVIDER_HPP
