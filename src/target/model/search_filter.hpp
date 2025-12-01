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

#ifndef LITHIUM_TARGET_MODEL_SEARCH_FILTER_HPP
#define LITHIUM_TARGET_MODEL_SEARCH_FILTER_HPP

#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace lithium::target::model {

/**
 * @brief Search filter criteria for celestial objects
 *
 * Comprehensive filtering options for searching celestial objects in the
 * database. Supports name patterns, physical properties, coordinates, and
 * observability constraints.
 */
struct CelestialSearchFilter {
    // ==================== Name and Identification ====================

    /// Name pattern (supports wildcards %, _)
    std::string namePattern;

    /// Exact identifier match (e.g., "M31", "NGC 224")
    std::string identifier;

    /// Messier catalog identifier
    std::string messierIdentifier;

    /// Chinese name pattern
    std::string chineseName;

    // ==================== Classification ====================

    /// Object type filter (e.g., "Galaxy", "Nebula", "Star Cluster")
    std::string type;

    /// Morphological classification (e.g., "Spiral", "Elliptical")
    std::string morphology;

    /// Constellation name in English
    std::string constellationEn;

    /// Constellation name in Chinese
    std::string constellationZh;

    /// Amateur observing difficulty rank (e.g., "Easy", "Medium", "Hard")
    std::string amateurRank;

    // ==================== Magnitude Constraints ====================

    /// Minimum visual magnitude (V band)
    double minMagnitude = -std::numeric_limits<double>::infinity();

    /// Maximum visual magnitude (V band)
    double maxMagnitude = std::numeric_limits<double>::infinity();

    /// Minimum surface brightness (mag/arcmin²)
    double minSurfaceBrightness = -std::numeric_limits<double>::infinity();

    /// Maximum surface brightness (mag/arcmin²)
    double maxSurfaceBrightness = std::numeric_limits<double>::infinity();

    // ==================== Size Constraints ====================

    /// Minimum major axis size (arcmin)
    double minMajorAxis = 0.0;

    /// Maximum major axis size (arcmin)
    double maxMajorAxis = std::numeric_limits<double>::infinity();

    /// Minimum minor axis size (arcmin)
    double minMinorAxis = 0.0;

    /// Maximum minor axis size (arcmin)
    double maxMinorAxis = std::numeric_limits<double>::infinity();

    // ==================== Coordinate Constraints ====================

    /// Minimum right ascension (degrees, 0-360)
    double minRA = 0.0;

    /// Maximum right ascension (degrees, 0-360)
    double maxRA = 360.0;

    /// Minimum declination (degrees, -90 to 90)
    double minDec = -90.0;

    /// Maximum declination (degrees, -90 to 90)
    double maxDec = 90.0;

    // ==================== Observability Constraints ====================

    /// Observer latitude for visibility calculation (degrees, -90 to 90)
    /// If set, only objects visible from this latitude are returned
    std::optional<double> observerLatitude;

    /// Minimum altitude above horizon for observability (degrees)
    double minAltitude = 0.0;

    // ==================== Pagination and Sorting ====================

    /// Maximum number of results to return
    int limit = 100;

    /// Number of results to skip (for pagination)
    int offset = 0;

    /// Field to order results by (e.g., "identifier", "magnitude", "RA")
    std::string orderBy = "identifier";

    /// Sort in ascending order if true, descending if false
    bool ascending = true;

    // ==================== Advanced Options ====================

    /// Include objects with null/missing values in results
    bool includeIncomplete = false;

    /// Search in aliases and extended names
    bool searchAliases = true;

    /// Fuzzy search tolerance (maximum edit distance)
    int fuzzyTolerance = 0;

    /**
     * @brief Check if filter is valid
     *
     * @return true if all constraints are self-consistent
     */
    [[nodiscard]] auto isValid() const -> bool {
        // Check magnitude constraints
        if (minMagnitude > maxMagnitude) {
            return false;
        }

        // Check RA constraints
        if (minRA < 0.0 || minRA >= 360.0 || maxRA < 0.0 ||
            maxRA >= 360.0 || minRA > maxRA) {
            return false;
        }

        // Check Dec constraints
        if (minDec < -90.0 || minDec > 90.0 || maxDec < -90.0 ||
            maxDec > 90.0 || minDec > maxDec) {
            return false;
        }

        // Check size constraints
        if (minMajorAxis < 0.0 || minMinorAxis < 0.0 ||
            minMajorAxis > maxMajorAxis || minMinorAxis > maxMinorAxis) {
            return false;
        }

        // Check observer latitude
        if (observerLatitude.has_value()) {
            if (observerLatitude.value() < -90.0 ||
                observerLatitude.value() > 90.0) {
                return false;
            }
        }

        // Check altitude
        if (minAltitude < -90.0 || minAltitude > 90.0) {
            return false;
        }

        // Check pagination
        if (limit <= 0 || offset < 0) {
            return false;
        }

        // Check fuzzy tolerance
        if (fuzzyTolerance < 0 || fuzzyTolerance > 5) {
            return false;
        }

        return true;
    }

    /**
     * @brief Reset filter to default values
     */
    void reset() {
        namePattern.clear();
        identifier.clear();
        messierIdentifier.clear();
        chineseName.clear();
        type.clear();
        morphology.clear();
        constellationEn.clear();
        constellationZh.clear();
        amateurRank.clear();
        minMagnitude = -std::numeric_limits<double>::infinity();
        maxMagnitude = std::numeric_limits<double>::infinity();
        minSurfaceBrightness = -std::numeric_limits<double>::infinity();
        maxSurfaceBrightness = std::numeric_limits<double>::infinity();
        minMajorAxis = 0.0;
        maxMajorAxis = std::numeric_limits<double>::infinity();
        minMinorAxis = 0.0;
        maxMinorAxis = std::numeric_limits<double>::infinity();
        minRA = 0.0;
        maxRA = 360.0;
        minDec = -90.0;
        maxDec = 90.0;
        observerLatitude.reset();
        minAltitude = 0.0;
        limit = 100;
        offset = 0;
        orderBy = "identifier";
        ascending = true;
        includeIncomplete = false;
        searchAliases = true;
        fuzzyTolerance = 0;
    }

    /**
     * @brief Create a filter for objects visible from given location
     *
     * @param latitude Observer latitude in degrees
     * @param minAlt Minimum altitude above horizon
     * @return Configured filter
     */
    static auto forVisibleObjects(double latitude, double minAlt = 10.0)
        -> CelestialSearchFilter {
        CelestialSearchFilter filter;
        filter.observerLatitude = latitude;
        filter.minAltitude = minAlt;
        return filter;
    }

    /**
     * @brief Create a filter for objects of a specific type
     *
     * @param objectType Type to filter by
     * @return Configured filter
     */
    static auto forType(const std::string& objectType)
        -> CelestialSearchFilter {
        CelestialSearchFilter filter;
        filter.type = objectType;
        return filter;
    }

    /**
     * @brief Create a filter for bright objects
     *
     * @param maxMag Maximum magnitude (brighter = lower numbers)
     * @return Configured filter
     */
    static auto forBrightObjects(double maxMag = 6.0)
        -> CelestialSearchFilter {
        CelestialSearchFilter filter;
        filter.maxMagnitude = maxMag;
        return filter;
    }

    /**
     * @brief Create a filter for objects in a specific constellation
     *
     * @param constellation Constellation name (English)
     * @return Configured filter
     */
    static auto forConstellation(const std::string& constellation)
        -> CelestialSearchFilter {
        CelestialSearchFilter filter;
        filter.constellationEn = constellation;
        return filter;
    }

    /**
     * @brief Create a filter for extended objects
     *
     * @param minSize Minimum size in arcminutes
     * @return Configured filter
     */
    static auto forExtendedObjects(double minSize = 1.0)
        -> CelestialSearchFilter {
        CelestialSearchFilter filter;
        filter.minMajorAxis = minSize;
        return filter;
    }
};

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_SEARCH_FILTER_HPP
