// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_NED_PROVIDER_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_NED_PROVIDER_HPP

#include "provider_interface.hpp"
#include "../client/http_client.hpp"
#include "../cache/query_cache.hpp"
#include "../rate_limiter/api_rate_limiter.hpp"

namespace lithium::target::online {

/**
 * @brief NED (NASA/IPAC Extragalactic Database) configuration
 *
 * Configuration for the NED provider which specializes in extragalactic
 * object data including galaxies, quasars, and active galactic nuclei.
 */
struct NedProviderConfig {
    std::string baseUrl = "https://ned.ipac.caltech.edu/tap/sync";
    std::chrono::milliseconds timeout{45000};
    int maxRetries = 3;
    bool useCache = true;
    std::chrono::minutes cacheTTL{60};

    // Query options
    bool includePhotometry = true;
    bool includeRedshift = true;
    bool includeDistances = true;
    bool includeMorphology = true;
};

/**
 * @brief NED astronomical database provider
 *
 * Provides access to the NASA/IPAC Extragalactic Database (NED)
 * using TAP protocol. NED specializes in extragalactic objects
 * including galaxies, quasars, AGN, and related data.
 *
 * NED (NASA/IPAC Extragalactic Database) is a comprehensive
 * online database of extragalactic objects. It includes extensive
 * data on galaxies, quasars, supernovae, and AGN.
 *
 * API endpoint: https://ned.ipac.caltech.edu/tap/sync
 * Protocol: TAP (Table Access Protocol) with ADQL (Astronomical Data Query Language)
 * Response format: VOTable XML or JSON
 */
class NedProvider : public IOnlineProvider {
public:
    static constexpr std::string_view PROVIDER_NAME = "NED";
    static constexpr std::string_view BASE_URL =
        "https://ned.ipac.caltech.edu/tap/sync";

    /**
     * @brief Construct NED provider with dependencies
     *
     * @param httpClient HTTP client for making requests
     * @param cache Optional query cache
     * @param rateLimiter Optional rate limiter
     * @param config NED-specific configuration
     */
    explicit NedProvider(
        std::shared_ptr<AsyncHttpClient> httpClient,
        std::shared_ptr<QueryCache> cache = nullptr,
        std::shared_ptr<ApiRateLimiter> rateLimiter = nullptr,
        const NedProviderConfig& config = {});

    ~NedProvider() override;

    // Non-copyable, movable
    NedProvider(const NedProvider&) = delete;
    NedProvider& operator=(const NedProvider&) = delete;
    NedProvider(NedProvider&&) noexcept;
    NedProvider& operator=(NedProvider&&) noexcept;

    /**
     * @brief Execute a synchronous query to NED
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> override;

    /**
     * @brief Execute an asynchronous query to NED
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     */
    [[nodiscard]] auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> override;

    /**
     * @brief Get the provider name
     *
     * @return "NED"
     */
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return PROVIDER_NAME;
    }

    /**
     * @brief Check if NED service is available
     *
     * Performs a lightweight health check to verify connectivity
     *
     * @return True if service is available
     */
    [[nodiscard]] auto isAvailable() const -> bool override;

    /**
     * @brief Get supported query types
     *
     * NED supports ByName and ByCoordinates queries
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] auto supportedQueryTypes() const -> std::vector<QueryType> override {
        return {QueryType::ByName, QueryType::ByCoordinates};
    }

    /**
     * @brief Get provider base URL
     *
     * @return NED TAP sync endpoint
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
    void setConfig(const NedProviderConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return Reference to current configuration
     */
    [[nodiscard]] auto getConfig() const -> const NedProviderConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_NED_PROVIDER_HPP
