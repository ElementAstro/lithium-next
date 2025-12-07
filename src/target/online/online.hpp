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

#ifndef LITHIUM_TARGET_ONLINE_ONLINE_HPP
#define LITHIUM_TARGET_ONLINE_ONLINE_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

namespace lithium::target::online {

/**
 * @brief Configuration for online search operations
 */
struct OnlineSearchConfig {
    /// Enable online search
    bool enabled = true;

    /// Connection timeout in milliseconds
    int timeoutMs = 5000;

    /// Maximum retries for failed requests
    int maxRetries = 3;

    /// API key if required
    std::string apiKey;

    /// Base URL for online service
    std::string baseUrl;

    /// Enable caching of online results
    bool enableCache = true;

    /// Cache TTL in seconds
    int cacheTtlSeconds = 3600;
};

/**
 * @brief Ephemeris point for celestial object at specific time
 */
struct EphemerisPoint {
    /// Right ascension in degrees
    double ra = 0.0;

    /// Declination in degrees
    double dec = 0.0;

    /// Magnitude
    std::optional<double> magnitude;

    /// Distance from observer (in AU or kilometers)
    std::optional<double> distance;

    /// Angular velocity in RA (degrees per hour)
    double raVelocity = 0.0;

    /// Angular velocity in Dec (degrees per hour)
    double decVelocity = 0.0;

    /// Observation time
    std::chrono::system_clock::time_point time;

    /// Data source identifier
    std::string source;
};

/**
 * @brief Result merger for combining local and online results
 */
class ResultMerger {
public:
    virtual ~ResultMerger() = default;

    /**
     * @brief Merge two result sets
     *
     * @param localResults Results from local database
     * @param onlineResults Results from online sources
     * @return Merged and deduplicated results
     */
    virtual auto mergeResults(const std::vector<std::string>& localResults,
                              const std::vector<std::string>& onlineResults)
        -> std::vector<std::string> = 0;
};

/**
 * @brief Online search service interface
 */
class OnlineSearchService {
public:
    virtual ~OnlineSearchService() = default;

    /**
     * @brief Initialize the service
     *
     * @param config Configuration for the service
     * @return Success or error message
     */
    virtual auto initialize(const OnlineSearchConfig& config)
        -> atom::type::Expected<void, std::string> = 0;

    /**
     * @brief Search online databases by name
     *
     * @param query Search query
     * @param limit Maximum results
     * @return Vector of object identifiers
     */
    virtual auto searchByName(const std::string& query, int limit = 50)
        -> std::vector<std::string> = 0;

    /**
     * @brief Search online by coordinates
     *
     * @param ra Right ascension in degrees
     * @param dec Declination in degrees
     * @param radiusDeg Search radius in degrees
     * @param limit Maximum results
     * @return Vector of object identifiers
     */
    virtual auto searchByCoordinates(double ra, double dec, double radiusDeg,
                                     int limit = 100)
        -> std::vector<std::string> = 0;

    /**
     * @brief Get ephemeris from online source
     *
     * @param objectName Object name to query
     * @param time Time for ephemeris
     * @return Ephemeris point if available
     */
    virtual auto getEphemeris(const std::string& objectName,
                              std::chrono::system_clock::time_point time)
        -> std::optional<EphemerisPoint> = 0;

    /**
     * @brief Get object details from online source
     *
     * @param identifier Object identifier
     * @return JSON string with object details
     */
    virtual auto getObjectDetails(const std::string& identifier)
        -> std::optional<std::string> = 0;
};

/**
 * @brief Factory for creating online search service
 */
class OnlineSearchServiceFactory {
public:
    /**
     * @brief Create online search service
     *
     * @param serviceType Type of service ("simbad", "vizier", "jpl", etc.)
     * @return Shared pointer to service, or nullptr on error
     */
    static auto createService(const std::string& serviceType = "simbad")
        -> std::shared_ptr<OnlineSearchService>;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_ONLINE_HPP
