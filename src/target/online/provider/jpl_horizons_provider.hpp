// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_JPL_HORIZONS_PROVIDER_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_JPL_HORIZONS_PROVIDER_HPP

#include "provider_interface.hpp"
#include "../client/http_client.hpp"
#include "../cache/query_cache.hpp"
#include "../rate_limiter/api_rate_limiter.hpp"

namespace lithium::target::online {

/**
 * @brief Common solar system object codes for JPL Horizons
 *
 * Standard target identifiers used by JPL Horizons API
 */
namespace JplTarget {
constexpr const char* SUN = "10";
constexpr const char* MERCURY = "199";
constexpr const char* VENUS = "299";
constexpr const char* MOON = "301";
constexpr const char* MARS = "499";
constexpr const char* JUPITER = "599";
constexpr const char* SATURN = "699";
constexpr const char* URANUS = "799";
constexpr const char* NEPTUNE = "899";
constexpr const char* PLUTO = "999";
}  // namespace JplTarget

/**
 * @brief JPL Horizons configuration
 *
 * Configuration for the JPL Horizons provider which provides
 * ephemeris data for solar system objects.
 */
struct JplHorizonsProviderConfig {
    std::string baseUrl = "https://ssd.jpl.nasa.gov/api/horizons.api";
    std::chrono::milliseconds timeout{30000};
    int maxRetries = 3;
    bool useCache = true;
    std::chrono::minutes cacheTTL{5};  // Short TTL for moving objects

    // Ephemeris options
    std::string outputFormat = "json";
    bool includeAirmass = false;
    bool includePhaseAngle = true;
    bool includeMagnitude = true;
    bool includeElongation = true;
};

/**
 * @brief JPL Horizons ephemeris data provider
 *
 * Provides access to JPL Horizons solar system object ephemeris data.
 * Horizons calculates positions of solar system bodies for any date,
 * including planets, moons, asteroids, and comets.
 *
 * JPL Horizons is a system developed by NASA's Jet Propulsion Laboratory
 * to provide highly accurate ephemeris data for solar system bodies.
 * It includes positions, velocities, magnitudes, and other properties.
 *
 * API endpoint: https://ssd.jpl.nasa.gov/api/horizons.api
 * Protocol: HTTP GET with JSON response
 * Response format: JSON
 */
class JplHorizonsProvider : public IOnlineProvider {
public:
    static constexpr std::string_view PROVIDER_NAME = "JPL_Horizons";
    static constexpr std::string_view BASE_URL =
        "https://ssd.jpl.nasa.gov/api/horizons.api";

    /**
     * @brief Construct JPL Horizons provider with dependencies
     *
     * @param httpClient HTTP client for making requests
     * @param cache Optional query cache
     * @param rateLimiter Optional rate limiter
     * @param config JPL Horizons-specific configuration
     */
    explicit JplHorizonsProvider(
        std::shared_ptr<AsyncHttpClient> httpClient,
        std::shared_ptr<QueryCache> cache = nullptr,
        std::shared_ptr<ApiRateLimiter> rateLimiter = nullptr,
        const JplHorizonsProviderConfig& config = {});

    ~JplHorizonsProvider() override;

    // Non-copyable, movable
    JplHorizonsProvider(const JplHorizonsProvider&) = delete;
    JplHorizonsProvider& operator=(const JplHorizonsProvider&) = delete;
    JplHorizonsProvider(JplHorizonsProvider&&) noexcept;
    JplHorizonsProvider& operator=(JplHorizonsProvider&&) noexcept;

    /**
     * @brief Execute a synchronous query to JPL Horizons
     *
     * For ephemeris queries, parameters should include start/stop times.
     * For object search queries, uses the query parameter to look up objects.
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     */
    [[nodiscard]] auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> override;

    /**
     * @brief Execute an asynchronous query to JPL Horizons
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     */
    [[nodiscard]] auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> override;

    /**
     * @brief Get the provider name
     *
     * @return "JPL_Horizons"
     */
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return PROVIDER_NAME;
    }

    /**
     * @brief Check if JPL Horizons service is available
     *
     * Performs a lightweight health check to verify connectivity
     *
     * @return True if service is available
     */
    [[nodiscard]] auto isAvailable() const -> bool override;

    /**
     * @brief Get supported query types
     *
     * JPL Horizons supports ByName and Ephemeris queries
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] auto supportedQueryTypes() const -> std::vector<QueryType> override {
        return {QueryType::ByName, QueryType::Ephemeris};
    }

    /**
     * @brief Get provider base URL
     *
     * @return JPL Horizons API endpoint
     */
    [[nodiscard]] auto baseUrl() const noexcept -> std::string_view override {
        return BASE_URL;
    }

    /**
     * @brief Get ephemeris for a solar system object
     *
     * Queries JPL Horizons for ephemeris data of a solar system object
     * over a specified time range with configurable step size.
     *
     * @param target Target identifier or name
     * @param startTime Ephemeris start time
     * @param endTime Ephemeris end time
     * @param stepSize Step interval for ephemeris points
     * @param observer Optional observer location for apparent coordinates
     * @return Expected containing vector of ephemeris points
     */
    [[nodiscard]] auto getEphemeris(
        const std::string& target,
        std::chrono::system_clock::time_point startTime,
        std::chrono::system_clock::time_point endTime,
        std::chrono::minutes stepSize,
        const std::optional<OnlineQueryParams::ObserverLocation>& observer = std::nullopt)
        -> atom::type::Expected<std::vector<EphemerisPoint>, OnlineQueryError>;

    /**
     * @brief Set configuration
     *
     * @param config New configuration
     */
    void setConfig(const JplHorizonsProviderConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return Reference to current configuration
     */
    [[nodiscard]] auto getConfig() const -> const JplHorizonsProviderConfig&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_JPL_HORIZONS_PROVIDER_HPP
