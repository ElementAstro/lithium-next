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

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_INTERFACE_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_INTERFACE_HPP

#include <algorithm>
#include <chrono>
#include <concepts>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../celestial_model.hpp"
#include "atom/type/expected.hpp"

namespace lithium::target::online {

// Forward declarations
class IOnlineProvider;

/**
 * @brief Types of queries supported by online providers
 *
 * Represents different query modes available across various
 * online celestial databases and services.
 */
enum class QueryType {
    ByName,           ///< Search by object name/identifier
    ByCoordinates,    ///< Cone search by RA/Dec coordinates
    ByConstellation,  ///< Search within constellation boundaries
    Ephemeris,        ///< Solar system object ephemeris (JPL only)
    Catalog           ///< Catalog-specific queries
};

/**
 * @brief Parameters for online database queries
 *
 * Flexible query structure supporting multiple search modes.
 * Not all fields are required depending on the QueryType used.
 */
struct OnlineQueryParams {
    QueryType type = QueryType::ByName;
    std::string query;                      ///< Search term or object name
    std::optional<double> ra;               ///< RA in degrees (0-360)
    std::optional<double> dec;              ///< Dec in degrees (-90 to +90)
    std::optional<double> radius;           ///< Search radius in degrees
    std::optional<std::string> catalog;     ///< Specific catalog to query
    std::optional<double> minMagnitude;     ///< Minimum visual magnitude
    std::optional<double> maxMagnitude;     ///< Maximum visual magnitude
    std::optional<std::string> objectType;  ///< Filter by object type
    int limit = 100;                        ///< Maximum results to return
    std::chrono::system_clock::time_point epoch =
        std::chrono::system_clock::now();  ///< Epoch for ephemeris queries

    /**
     * @brief Observer location for ephemeris calculations
     *
     * Used by JPL Horizons and similar ephemeris services
     * to compute apparent coordinates from a specific location.
     */
    struct ObserverLocation {
        double latitude = 0.0;   ///< Latitude in degrees (-90 to +90)
        double longitude = 0.0;  ///< Longitude in degrees (-180 to +180)
        double elevation = 0.0;  ///< Elevation in meters above sea level
    };
    std::optional<ObserverLocation> observer;
};

/**
 * @brief Ephemeris data point for solar system objects
 *
 * Represents a single ephemeris position calculated for a specific
 * time, typically from services like JPL Horizons. Includes both
 * equatorial and horizontal coordinates if observer location provided.
 */
struct EphemerisPoint {
    std::chrono::system_clock::time_point time;
    double ra = 0.0;          ///< Right ascension (degrees)
    double dec = 0.0;         ///< Declination (degrees)
    double distance = 0.0;    ///< Distance (AU)
    double magnitude = 0.0;   ///< Visual magnitude
    double elongation = 0.0;  ///< Solar elongation (degrees)
    double phaseAngle = 0.0;  ///< Phase angle (degrees)
    double azimuth = 0.0;     ///< Azimuth (degrees, if observer set)
    double altitude = 0.0;    ///< Altitude (degrees, if observer set)
};

/**
 * @brief Error information for failed queries
 *
 * Provides detailed error context including error code, message,
 * provider information, and retry guidance for transient failures.
 */
struct OnlineQueryError {
    /**
     * @brief Enumeration of possible error codes
     */
    enum class Code {
        NetworkError,          ///< Network connectivity issue
        Timeout,               ///< Request timed out
        RateLimited,           ///< API rate limit exceeded
        ParseError,            ///< Failed to parse response
        InvalidQuery,          ///< Invalid query parameters
        ServiceUnavailable,    ///< Service temporarily unavailable
        AuthenticationFailed,  ///< Authentication/API key error
        NotFound,              ///< Object not found
        Unknown                ///< Unknown error
    };

    Code code = Code::Unknown;
    std::string message;
    std::string provider;
    std::optional<std::chrono::seconds> retryAfter;
    std::optional<std::string> rawResponse;

    /**
     * @brief Determine if the error is transient and retryable
     *
     * Returns true for network, timeout, rate limit, and service
     * unavailable errors. Returns false for permanent failures.
     *
     * @return True if the query should be retried
     */
    [[nodiscard]] auto isRetryable() const noexcept -> bool {
        return code == Code::NetworkError || code == Code::Timeout ||
               code == Code::RateLimited || code == Code::ServiceUnavailable;
    }
};

/**
 * @brief Result of an online query
 *
 * Contains query results including celestial object data,
 * ephemeris data (for JPL queries), provider metadata,
 * and pagination information.
 */
struct OnlineQueryResult {
    std::vector<CelestialObjectModel> objects;
    std::vector<EphemerisPoint> ephemerisData;  ///< For ephemeris queries
    std::string provider;
    std::chrono::milliseconds queryTime{0};
    bool fromCache = false;
    std::optional<std::string> continuationToken;  ///< For pagination
    size_t totalAvailable = 0;  ///< Total results available (may exceed
                                ///< returned)
};

/**
 * @brief C++20 Concept for online data providers
 *
 * Defines the minimal interface required for a class to be
 * considered an online provider. All online database adapters
 * (SIMBAD, Vizier, JPL Horizons, etc.) must satisfy this concept.
 */
template <typename T>
concept OnlineProvider = requires(T provider, const OnlineQueryParams& params) {
    {
        provider.query(params)
    }
    -> std::same_as<atom::type::Expected<OnlineQueryResult, OnlineQueryError>>;
    { provider.name() } -> std::convertible_to<std::string_view>;
    { provider.isAvailable() } -> std::same_as<bool>;
    { provider.supportedQueryTypes() } -> std::same_as<std::vector<QueryType>>;
};

/**
 * @brief Abstract base interface for all online providers
 *
 * Provides the contract for implementing online celestial database
 * adapters. Implementations should be thread-safe for concurrent
 * queries. Uses the Template Method pattern for query execution
 * with optional caching and rate limiting.
 *
 * @note All implementations MUST be thread-safe for concurrent access
 * @note Derived classes should implement proper error handling and
 *       timeout management
 */
class IOnlineProvider {
public:
    virtual ~IOnlineProvider() = default;

    // Non-copyable
    IOnlineProvider(const IOnlineProvider&) = delete;
    IOnlineProvider& operator=(const IOnlineProvider&) = delete;

    // Movable
    IOnlineProvider(IOnlineProvider&&) noexcept = default;
    IOnlineProvider& operator=(IOnlineProvider&&) noexcept = default;

    /**
     * @brief Execute a synchronous query
     *
     * Performs a blocking query to the online database. The caller
     * is responsible for managing timeouts at the application level
     * if needed.
     *
     * @param params Query parameters
     * @return Expected containing result on success or error on failure
     *
     * @note This method must be thread-safe
     * @note Implementations may apply caching and rate limiting
     */
    [[nodiscard]] virtual auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> = 0;

    /**
     * @brief Execute an asynchronous query
     *
     * Performs a non-blocking query to the online database, returning
     * immediately with a future that will be resolved when the query
     * completes.
     *
     * @param params Query parameters
     * @return Future that will be resolved with result or error
     *
     * @note This method must be thread-safe
     * @note The future's shared state is thread-safe
     */
    [[nodiscard]] virtual auto queryAsync(const OnlineQueryParams& params)
        -> std::future<
            atom::type::Expected<OnlineQueryResult, OnlineQueryError>> = 0;

    /**
     * @brief Get the provider's name
     *
     * Returns a stable, unique identifier for this provider
     * (e.g., "SIMBAD", "Vizier", "JPL_Horizons").
     *
     * @return Provider name/identifier
     */
    [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

    /**
     * @brief Check if the provider is currently available
     *
     * Performs a health check to determine if the service is
     * operational. This may involve a lightweight ping request.
     *
     * @return True if service is available, false otherwise
     *
     * @note This may block briefly while checking connectivity
     */
    [[nodiscard]] virtual auto isAvailable() const -> bool = 0;

    /**
     * @brief Get supported query types
     *
     * Returns the set of QueryType values that this provider
     * supports. Clients should check this before attempting
     * certain query types.
     *
     * @return Vector of supported QueryType values
     */
    [[nodiscard]] virtual auto supportedQueryTypes() const
        -> std::vector<QueryType> = 0;

    /**
     * @brief Get provider base URL
     *
     * Returns the base URL for the online service, useful for
     * documentation and debugging.
     *
     * @return Base URL of the service
     */
    [[nodiscard]] virtual auto baseUrl() const noexcept -> std::string_view = 0;

    /**
     * @brief Check if a specific query type is supported
     *
     * Convenience method for checking single query type support.
     *
     * @param type Query type to check
     * @return True if the query type is supported
     */
    [[nodiscard]] auto supportsQueryType(QueryType type) const -> bool {
        auto types = supportedQueryTypes();
        return std::find(types.begin(), types.end(), type) != types.end();
    }

protected:
    /// Protected default constructor for derived classes
    IOnlineProvider() = default;
};

/**
 * @brief Shared pointer type for providers
 *
 * Used throughout the provider system for polymorphic
 * access to online database implementations.
 */
using OnlineProviderPtr = std::shared_ptr<IOnlineProvider>;

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_INTERFACE_HPP
