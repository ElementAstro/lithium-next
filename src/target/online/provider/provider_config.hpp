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

#ifndef LITHIUM_TARGET_ONLINE_PROVIDER_CONFIG_HPP
#define LITHIUM_TARGET_ONLINE_PROVIDER_CONFIG_HPP

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lithium::target::online {

/**
 * @brief Base configuration for all online providers
 *
 * Contains common settings applicable to any online database service.
 */
struct BaseProviderConfig {
    std::string name;                    ///< Provider identifier
    std::string baseUrl;                 ///< Base API URL
    std::chrono::seconds timeout{30};    ///< Request timeout
    int maxRetries = 3;                  ///< Maximum retry attempts
    std::chrono::milliseconds retryDelay{1000};  ///< Delay between retries
    bool useCache = true;                ///< Enable caching
    std::chrono::hours cacheExpiry{24};  ///< Cache expiry time
    int maxCacheSize = 10000;            ///< Maximum cached items
    bool enabled = true;                 ///< Enable/disable provider
    std::optional<std::string> apiKey;   ///< Optional API key/token
    std::optional<std::string> apiSecret;  ///< Optional API secret
};

/**
 * @brief Configuration for SIMBAD provider
 *
 * SIMBAD (Set of Identifications, Measurements and Bibliography
 * for Astronomical Data) - the astronomical database at CDS.
 *
 * https://simbad.u-strasbg.fr/simbad/
 */
struct SimbadConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default SIMBAD settings
     */
    SimbadConfig() {
        name = "SIMBAD";
        baseUrl = "https://simbad.u-strasbg.fr/simbad/sim-script";
        timeout = std::chrono::seconds{15};
        maxRetries = 2;
    }

    std::vector<std::string> outputFormat;  ///< Output format specifiers
    bool mainId = true;                     ///< Include main identifier
    bool allIds = false;                    ///< Include all identifiers
    bool coordinates = true;                ///< Include coordinates
    bool objectType = true;                 ///< Include object type
    bool magnitude = true;                  ///< Include magnitude
    bool spectrum = false;                  ///< Include spectrum
    bool distance = false;                  ///< Include distance
    bool redshift = false;                  ///< Include redshift
};

/**
 * @brief Configuration for Vizier provider
 *
 * Vizier - the astronomical catalog access service at CDS.
 *
 * https://vizier.u-strasbg.fr/
 */
struct VizierConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default Vizier settings
     */
    VizierConfig() {
        name = "Vizier";
        baseUrl = "https://vizier.u-strasbg.fr/viz-bin/votable";
        timeout = std::chrono::seconds{20};
        maxRetries = 3;
    }

    std::vector<std::string> catalogs;      ///< Catalog identifiers to query
    int pageSize = 10000;                   ///< Items per page
    std::optional<double> distanceMatch;    ///< Distance matching tolerance
    bool includeErrors = false;             ///< Include measurement errors
    bool includeQualityFlags = false;       ///< Include data quality flags
};

/**
 * @brief Configuration for JPL Horizons provider
 *
 * NASA JPL Horizons - solar system object ephemeris service.
 *
 * https://ssd.jpl.nasa.gov/horizons/
 */
struct JplHorizonsConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default JPL Horizons settings
     */
    JplHorizonsConfig() {
        name = "JPL_Horizons";
        baseUrl = "https://ssd.jpl.nasa.gov/api/horizons_api.py";
        timeout = std::chrono::seconds{30};
        maxRetries = 2;
    }

    enum class EphemerisFormat {
        HTML,    ///< HTML format
        TEXT,    ///< Plain text format
        JSON,    ///< JSON format
        CSV      ///< CSV format
    };

    EphemerisFormat format = EphemerisFormat::JSON;
    bool includeUncertainty = true;    ///< Include position uncertainty
    bool includeMagnitude = true;      ///< Include visual magnitude
    bool includeAirmass = false;       ///< Include airmass (requires observer)
    bool includePhaseAngle = true;     ///< Include phase angle
    bool includeElongation = true;     ///< Include solar elongation
    int timeSteps = 1;                 ///< Number of ephemeris steps
    std::string smallBodyId;           ///< Target small body ID
    std::string majorBodyId;           ///< Target major body ID
};

/**
 * @brief Configuration for NED provider
 *
 * NED (NASA Extragalactic Database) - extragalactic data service.
 *
 * https://ned.ipac.caltech.edu/
 */
struct NedConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default NED settings
     */
    NedConfig() {
        name = "NED";
        baseUrl = "https://ned.ipac.caltech.edu/cgi-bin/objsearch";
        timeout = std::chrono::seconds{20};
        maxRetries = 2;
    }

    enum class PhotometryInclude {
        None,          ///< No photometry
        Optical,       ///< Optical only
        Infrared,      ///< Infrared only
        Radio,         ///< Radio only
        All            ///< All wavelengths
    };

    PhotometryInclude photometry = PhotometryInclude::Optical;
    bool includeRedshift = true;       ///< Include redshift data
    bool includeDistances = true;      ///< Include distance measurements
    bool includeMorphology = true;     ///< Include morphological data
    bool includeReferences = false;    ///< Include reference counts
    int maxResults = 500;              ///< Maximum results per query
};

/**
 * @brief Configuration for MPC provider
 *
 * MPC (Minor Planet Center) - asteroid and comet data service.
 *
 * https://www.minorplanetcenter.net/
 */
struct MpcConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default MPC settings
     */
    MpcConfig() {
        name = "MPC";
        baseUrl = "https://www.minorplanetcenter.net/iau/services/";
        timeout = std::chrono::seconds{15};
        maxRetries = 3;
    }

    bool includeNearEarthObjects = true;   ///< Filter for NEOs
    bool includeComets = true;             ///< Include comets
    bool includeAsteroids = true;          ///< Include asteroids
    bool includeNewDiscoveries = false;    ///< Include recent discoveries
    int lookbackDays = 30;                 ///< Days for discovery searches
};

/**
 * @brief Configuration for OpenALT provider
 *
 * OpenALT - open astronomy lookup tool for object discovery.
 */
struct OpenAltConfig : BaseProviderConfig {
    /**
     * @brief Constructor with default OpenALT settings
     */
    OpenAltConfig() {
        name = "OpenALT";
        baseUrl = "https://api.openalt.com/api/v1";
        timeout = std::chrono::seconds{10};
        maxRetries = 2;
    }

    enum class DataQuality {
        Basic,      ///< Basic data only
        Standard,   ///< Standard quality
        Extended,   ///< Extended data
        Premium     ///< Premium data
    };

    DataQuality quality = DataQuality::Standard;
    bool includeImages = false;            ///< Include image links
    bool includeSpectrum = false;          ///< Include spectral data
    std::optional<int> minMagnitude;       ///< Minimum magnitude filter
    std::optional<int> maxMagnitude;       ///< Maximum magnitude filter
};

/**
 * @brief Configuration for provider caching system
 *
 * Controls how provider results are cached and reused.
 */
struct CacheConfig {
    bool enabled = true;                          ///< Enable caching
    std::chrono::hours defaultExpiry{24};         ///< Default cache TTL
    int maxEntries = 50000;                       ///< Maximum cache entries
    std::chrono::minutes cleanupInterval{60};     ///< Cleanup task interval
    double compressionRatio = 0.8;                ///< Expected compression ratio

    enum class EvictionPolicy {
        LRU,       ///< Least Recently Used
        LFU,       ///< Least Frequently Used
        FIFO       ///< First In First Out
    };

    EvictionPolicy policy = EvictionPolicy::LRU;
};

/**
 * @brief Configuration for provider rate limiting
 *
 * Controls request rate limiting to respect API quotas.
 */
struct RateLimitConfig {
    bool enabled = true;                          ///< Enable rate limiting
    int requestsPerSecond = 10;                   ///< Max requests/sec
    int requestsPerHour = 10000;                  ///< Max requests/hour
    int requestsPerDay = 100000;                  ///< Max requests/day
    std::chrono::seconds burstWindow{1};          ///< Burst sampling window
    int maxBurstRequests = 50;                    ///< Max burst requests

    /**
     * @brief Check if request would exceed rate limit
     *
     * @param currentSecond Current second count
     * @param currentHour Current hour count
     * @param currentDay Current day count
     * @return True if request is allowed
     */
    [[nodiscard]] auto isAllowed(int currentSecond, int currentHour,
                                 int currentDay) const -> bool {
        return currentSecond < requestsPerSecond &&
               currentHour < requestsPerHour &&
               currentDay < requestsPerDay;
    }
};

/**
 * @brief Aggregated configuration for all providers
 *
 * Top-level configuration container for the entire provider system.
 */
struct ProvidersConfig {
    SimbadConfig simbad;
    VizierConfig vizier;
    JplHorizonsConfig jplHorizons;
    NedConfig ned;
    MpcConfig mpc;
    OpenAltConfig openAlt;

    CacheConfig cache;
    RateLimitConfig rateLimit;

    std::vector<std::string> enabledProviders;  ///< Names of active providers
    std::chrono::seconds healthCheckInterval{300};  ///< Provider health check
    bool failover = true;                       ///< Use backup providers
    bool parallelQueries = true;                ///< Enable parallel queries
    int maxParallelProviders = 3;               ///< Max concurrent providers
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PROVIDER_CONFIG_HPP
