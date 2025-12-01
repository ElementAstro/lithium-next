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

#ifndef LITHIUM_TARGET_SEARCH_COORDINATE_SEARCHER_HPP
#define LITHIUM_TARGET_SEARCH_COORDINATE_SEARCHER_HPP

#include <memory>
#include <optional>
#include <vector>

#include "atom/type/expected.hpp"

#include "celestial_model.hpp"
#include "index/spatial_index.hpp"

namespace lithium::target::search {

/**
 * @brief Specialized searcher for coordinate-based queries
 *
 * Provides efficient spherical coordinate searches on the celestial sphere
 * using R-tree spatial indexing. Supports both radius and box searches with
 * Haversine distance calculations.
 */
class CoordinateSearcher {
public:
    /**
     * @brief Construct coordinate searcher with spatial index
     *
     * @param spatialIndex Shared pointer to spatial index
     */
    explicit CoordinateSearcher(
        std::shared_ptr<index::SpatialIndex> spatialIndex);

    /**
     * @brief Destructor
     */
    ~CoordinateSearcher();

    // Non-copyable
    CoordinateSearcher(const CoordinateSearcher&) = delete;
    CoordinateSearcher& operator=(const CoordinateSearcher&) = delete;

    // Movable
    CoordinateSearcher(CoordinateSearcher&&) noexcept;
    CoordinateSearcher& operator=(CoordinateSearcher&&) noexcept;

    /**
     * @brief Search for objects within radius from coordinates
     *
     * Finds all celestial objects within specified radius from given
     * RA/Dec position using great circle distance (Haversine formula).
     *
     * Time Complexity: O(log N) for index traversal, O(k) for result gathering
     * where N = total objects, k = result count
     *
     * @param ra Right ascension in degrees (0-360)
     * @param dec Declination in degrees (-90 to +90)
     * @param radius Search radius in degrees (0 to 180)
     * @param limit Maximum results to return
     * @return Vector of results, sorted by distance (nearest first)
     */
    [[nodiscard]] auto searchRadius(
        double ra,
        double dec,
        double radius,
        int limit = 100) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Search in rectangular box around coordinates
     *
     * Finds objects in axis-aligned box in RA/Dec coordinate space.
     * Simpler than radius search but less astronomically accurate.
     *
     * Time Complexity: O(log N + k) where N = total objects, k = results
     *
     * @param centerRa Center right ascension (degrees)
     * @param centerDec Center declination (degrees)
     * @param raWidth Width in RA direction (degrees)
     * @param decHeight Height in Dec direction (degrees)
     * @param limit Maximum results to return
     * @return Vector of results within box
     */
    [[nodiscard]] auto searchBox(
        double centerRa,
        double centerDec,
        double raWidth,
        double decHeight,
        int limit = 100) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Check visibility from observer location
     *
     * Determines if objects at given coordinates are visible from
     * observer location at minimum altitude.
     *
     * @param ra Object right ascension (degrees)
     * @param dec Object declination (degrees)
     * @param observerLatitude Observer latitude (degrees, -90 to 90)
     * @param minAltitude Minimum altitude above horizon (degrees)
     * @return true if object is visible from location
     */
    [[nodiscard]] static auto isVisible(
        double ra,
        double dec,
        double observerLatitude,
        double minAltitude = 0.0) -> bool;

    /**
     * @brief Calculate great circle distance between two coordinates
     *
     * Uses Haversine formula for accurate distance on sphere.
     * More accurate than simple Euclidean distance for celestial coordinates.
     *
     * @param ra1 First point RA (degrees)
     * @param dec1 First point Dec (degrees)
     * @param ra2 Second point RA (degrees)
     * @param dec2 Second point Dec (degrees)
     * @return Angular distance in degrees
     */
    [[nodiscard]] static auto angularDistance(
        double ra1,
        double dec1,
        double ra2,
        double dec2) -> double;

    /**
     * @brief Validate celestial coordinates
     *
     * @param ra Right ascension to check (degrees)
     * @param dec Declination to check (degrees)
     * @return true if coordinates are valid
     */
    [[nodiscard]] static auto isValidCoordinate(double ra, double dec)
        -> bool;

    /**
     * @brief Get statistics about indexed objects
     *
     * @return Number of indexed celestial objects
     */
    [[nodiscard]] auto getObjectCount() const -> size_t;

private:
    /// Implementation
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::search

#endif  // LITHIUM_TARGET_SEARCH_COORDINATE_SEARCHER_HPP
