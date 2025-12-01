// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_VIZIER_PROVIDER_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_VIZIER_PROVIDER_HPP

#include "provider_interface.hpp"
#include "../client/http_client.hpp"
#include "../cache/query_cache.hpp"
#include "../rate_limiter/api_rate_limiter.hpp"

namespace lithium::target::online {

/**
 * @brief Common VizieR catalog identifiers
 */
namespace VizierCatalog {
    constexpr const char* NGC2000 = "VII/118";      // NGC 2000.0
    constexpr const char* MESSIER = "VII/1B";       // Messier catalog
    constexpr const char* IC2000 = "VII/118A";      // IC 2000.0
    constexpr const char* SAC = "VII/118B";         // SAC DSO catalog
    constexpr const char* HIPPARCOS = "I/239";      // Hipparcos main
    constexpr const char* GCVS = "B/gcvs";          // General Catalog of Variable Stars
    constexpr const char* TWOMASS = "II/246";       // 2MASS Point Source
    constexpr const char* UCAC4 = "I/322A";         // UCAC4
    constexpr const char* APASS = "II/336";         // APASS DR9
}  // namespace VizierCatalog

/**
 * @brief VizieR configuration
 */
struct VizierProviderConfig {
    std::string baseUrl = "https://vizier.u-strasbg.fr/viz-bin/votable";
    std::chrono::milliseconds timeout{30000};
    int maxRetries = 3;
    bool useCache = true;
    std::chrono::minutes cacheTTL{120};

    // Default catalogs to query
    std::vector<std::string> defaultCatalogs = {
        VizierCatalog::NGC2000,
        VizierCatalog::MESSIER
    };

    // Output options
    int maxRows = 500;
    bool includeCoordinates = true;
    bool includeMagnitudes = true;
};

/**
 * @brief VizieR astronomical catalog provider
 *
 * Provides access to VizieR catalog service at CDS.
 * VizieR offers access to thousands of astronomical catalogs
 * including NGC, Messier, Hipparcos, 2MASS, and many others.
 *
 * API endpoint: https://vizier.u-strasbg.fr/viz-bin/votable
 * Protocol: HTTP GET with query parameters
 * Response format: VOTable XML
 *
 * Supports:
 * - Cone searches by coordinates
 * - Named object searches (via default catalogs)
 * - Catalog-specific queries
 * - Magnitude filtering
 * - Output column selection
 */
class VizierProvider : public IOnlineProvider {
public:
    static constexpr std::string_view PROVIDER_NAME = "VizieR";
    static constexpr std::string_view BASE_URL =
        "https://vizier.u-strasbg.fr/viz-bin/votable";

    /**
     * @brief Construct VizieR provider with dependencies
     *
     * @param httpClient HTTP client for making requests
     * @param cache Optional query cache
     * @param rateLimiter Optional rate limiter
     * @param config VizieR-specific configuration
     */
    explicit VizierProvider(
        std::shared_ptr<AsyncHttpClient> httpClient,
        std::shared_ptr<QueryCache> cache = nullptr,
        std::shared_ptr<ApiRateLimiter> rateLimiter = nullptr,
        const VizierProviderConfig& config = {});

    ~VizierProvider() override;

    // Non-copyable, movable
    VizierProvider(const VizierProvider&) = delete;
    VizierProvider& operator=(const VizierProvider&) = delete;
    VizierProvider(VizierProvider&&) noexcept;
    VizierProvider& operator=(VizierProvider&&) noexcept;

    /**
     * @brief Execute a synchronous query to VizieR
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> override;

    /**
     * @brief Execute an asynchronous query to VizieR
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     */
    [[nodiscard]] auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> override;

    /**
     * @brief Get the provider name
     *
     * @return "VizieR"
     */
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return PROVIDER_NAME;
    }

    /**
     * @brief Check if VizieR service is available
     *
     * Performs a lightweight health check to verify connectivity
     *
     * @return True if service is available
     */
    [[nodiscard]] auto isAvailable() const -> bool override;

    /**
     * @brief Get supported query types
     *
     * VizieR supports ByCoordinates and Catalog queries
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] auto supportedQueryTypes() const -> std::vector<QueryType> override {
        return {QueryType::ByCoordinates, QueryType::Catalog};
    }

    /**
     * @brief Get provider base URL
     *
     * @return VizieR VOTable endpoint
     */
    [[nodiscard]] auto baseUrl() const noexcept -> std::string_view override {
        return BASE_URL;
    }

    /**
     * @brief Build VizieR query URL
     *
     * Constructs a complete HTTP GET URL with all necessary parameters
     * for the VizieR API.
     *
     * @param params Query parameters
     * @return Complete query URL
     * @throws std::invalid_argument if parameters are invalid
     */
    [[nodiscard]] auto buildQueryUrl(const OnlineQueryParams& params) const
        -> std::string;

    /**
     * @brief Query specific catalog
     *
     * Executes a query against a specific VizieR catalog.
     *
     * @param catalog Catalog identifier (e.g., "VII/118" for NGC2000)
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto queryCatalog(const std::string& catalog,
                                    const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError>;

    /**
     * @brief Set configuration
     *
     * @param config New configuration
     */
    void setConfig(const VizierProviderConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return Reference to current configuration
     */
    [[nodiscard]] auto getConfig() const -> const VizierProviderConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_VIZIER_PROVIDER_HPP
