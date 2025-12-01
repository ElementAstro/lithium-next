// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_OPEN_NGC_PROVIDER_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_OPEN_NGC_PROVIDER_HPP

#include "provider_interface.hpp"
#include "../client/http_client.hpp"
#include "../cache/query_cache.hpp"

namespace lithium::target::online {

struct OpenNgcProviderConfig {
    std::string dataUrl =
        "https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/NGC.csv";
    std::chrono::milliseconds timeout{60000};
    bool useCache = true;
    std::chrono::hours cacheTTL{24};  // Long cache for static data

    bool autoRefresh = true;
    std::chrono::hours refreshInterval{24};
};

/**
 * @brief OpenNGC catalog provider
 *
 * Provides access to the OpenNGC catalog of NGC/IC/Messier objects.
 * Downloads and caches the CSV database locally for fast queries.
 *
 * CSV Source: https://github.com/mattiaverga/OpenNGC
 * Provides NGC (New General Catalogue), IC (Index Catalogue), and
 * Messier catalog entries with astrometric and physical data.
 *
 * Supports:
 * - Name search (NGC, IC, Messier identifiers)
 * - Coordinate searches (cone search by RA/Dec)
 * - Catalog-specific filtering
 *
 * The CSV data includes:
 * - Object identifiers (NGC, IC, Messier)
 * - RA/Dec coordinates (J2000)
 * - Object type (Gx=Galaxy, Pn=Planetary Nebula, etc.)
 * - Constellation
 * - Visual magnitude (V and B magnitudes)
 * - Physical dimensions (major/minor axis)
 * - Position angle
 * - Surface brightness
 * - Hubble type (for galaxies)
 */
class OpenNgcProvider : public IOnlineProvider {
public:
    static constexpr std::string_view PROVIDER_NAME = "OpenNGC";
    static constexpr std::string_view BASE_URL =
        "https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/NGC.csv";

    /**
     * @brief Construct OpenNGC provider with dependencies
     *
     * @param httpClient HTTP client for downloading catalog
     * @param cache Optional query cache
     * @param config OpenNGC-specific configuration
     */
    explicit OpenNgcProvider(
        std::shared_ptr<AsyncHttpClient> httpClient,
        std::shared_ptr<QueryCache> cache = nullptr,
        const OpenNgcProviderConfig& config = {});

    ~OpenNgcProvider() override;

    // Non-copyable, movable
    OpenNgcProvider(const OpenNgcProvider&) = delete;
    OpenNgcProvider& operator=(const OpenNgcProvider&) = delete;
    OpenNgcProvider(OpenNgcProvider&&) noexcept;
    OpenNgcProvider& operator=(OpenNgcProvider&&) noexcept;

    /**
     * @brief Execute a synchronous query
     *
     * Performs a query against the locally cached OpenNGC catalog.
     * Supports name queries, coordinate searches, and catalog filtering.
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> override;

    /**
     * @brief Execute an asynchronous query
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     */
    [[nodiscard]] auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult,
                                            OnlineQueryError>> override;

    /**
     * @brief Get the provider name
     *
     * @return "OpenNGC"
     */
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return PROVIDER_NAME;
    }

    /**
     * @brief Check if catalog data is loaded
     *
     * @return True if catalog data is available for queries
     */
    [[nodiscard]] auto isAvailable() const -> bool override;

    /**
     * @brief Get supported query types
     *
     * OpenNGC supports ByName, ByCoordinates, and Catalog queries
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] auto supportedQueryTypes() const -> std::vector<QueryType> override {
        return {QueryType::ByName, QueryType::ByCoordinates, QueryType::Catalog};
    }

    /**
     * @brief Get provider base URL
     *
     * @return GitHub raw URL for OpenNGC CSV data
     */
    [[nodiscard]] auto baseUrl() const noexcept -> std::string_view override {
        return BASE_URL;
    }

    /**
     * @brief Force refresh of cached catalog data
     *
     * Downloads fresh catalog data from GitHub and rebuilds indexes.
     *
     * @return Expected containing void on success or error on failure
     */
    auto refreshCatalog() -> atom::type::Expected<void, OnlineQueryError>;

    /**
     * @brief Get catalog statistics
     *
     * @return Pair of (number of objects, last update timestamp)
     */
    [[nodiscard]] auto getCatalogStats() const
        -> std::pair<size_t, std::chrono::system_clock::time_point>;

    /**
     * @brief Set configuration
     *
     * @param config New configuration
     */
    void setConfig(const OpenNgcProviderConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return Reference to current configuration
     */
    [[nodiscard]] auto getConfig() const -> const OpenNgcProviderConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_OPEN_NGC_PROVIDER_HPP
