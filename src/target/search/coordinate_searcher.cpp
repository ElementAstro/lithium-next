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

#include "coordinate_searcher.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace lithium::target::search {

/**
 * @brief Implementation class for coordinate searcher
 */
class CoordinateSearcher::Impl {
public:
    explicit Impl(std::shared_ptr<index::SpatialIndex> spatialIndex)
        : spatialIndex_(std::move(spatialIndex)) {
        spdlog::debug("CoordinateSearcher::Impl constructed");
    }

    ~Impl() { spdlog::debug("CoordinateSearcher::Impl destroyed"); }

    /**
     * @brief Search radius implementation
     */
    auto searchRadius(double ra, double dec, double radius, int limit)
        -> std::vector<CelestialObjectModel> {
        if (!spatialIndex_) {
            spdlog::warn("Spatial index not available");
            return {};
        }

        if (!isValidCoordinate(ra, dec)) {
            spdlog::error("Invalid search coordinates: RA={}, Dec={}", ra, dec);
            return {};
        }

        if (radius < 0.0 || radius > 180.0) {
            spdlog::error("Invalid search radius: {}", radius);
            return {};
        }

        try {
            // Query spatial index
            auto nearby = spatialIndex_->searchRadius(ra, dec, radius, limit);

            spdlog::debug("Radius search found {} objects", nearby.size());

            // Convert to results (note: returns object IDs from index,
            // actual data should be fetched from repository by caller)
            std::vector<CelestialObjectModel> results;
            results.reserve(nearby.size());

            for (const auto& [objectId, distance] : nearby) {
                CelestialObjectModel result;
                result.identifier = objectId;
                results.push_back(result);
            }

            return results;
        } catch (const std::exception& e) {
            spdlog::error("Error in radius search: {}", e.what());
            return {};
        }
    }

    /**
     * @brief Search box implementation
     */
    auto searchBox(double centerRa, double centerDec, double raWidth,
                  double decHeight, int limit)
        -> std::vector<CelestialObjectModel> {
        if (!spatialIndex_) {
            spdlog::warn("Spatial index not available");
            return {};
        }

        if (!isValidCoordinate(centerRa, centerDec)) {
            spdlog::error("Invalid center coordinates: RA={}, Dec={}",
                         centerRa, centerDec);
            return {};
        }

        if (raWidth <= 0.0 || decHeight <= 0.0) {
            spdlog::error("Invalid box dimensions: raWidth={}, decHeight={}",
                         raWidth, decHeight);
            return {};
        }

        try {
            // Calculate box boundaries
            double minRa = std::fmod(centerRa - raWidth / 2.0 + 360.0, 360.0);
            double maxRa = std::fmod(centerRa + raWidth / 2.0 + 360.0, 360.0);
            double minDec =
                std::clamp(centerDec - decHeight / 2.0, -90.0, 90.0);
            double maxDec =
                std::clamp(centerDec + decHeight / 2.0, -90.0, 90.0);

            // Query spatial index
            auto inBox = spatialIndex_->searchBox(minRa, maxRa, minDec, maxDec,
                                                 limit);

            spdlog::debug("Box search found {} objects", inBox.size());

            // Convert to results
            std::vector<CelestialObjectModel> results;
            results.reserve(inBox.size());

            for (const auto& objectId : inBox) {
                CelestialObjectModel result;
                result.identifier = objectId;
                results.push_back(result);
            }

            return results;
        } catch (const std::exception& e) {
            spdlog::error("Error in box search: {}", e.what());
            return {};
        }
    }

    /**
     * @brief Get object count
     */
    [[nodiscard]] auto getObjectCount() const -> size_t {
        if (!spatialIndex_) {
            return 0;
        }
        return spatialIndex_->size();
    }
};

// ============================================================================
// CoordinateSearcher Public Implementation
// ============================================================================

CoordinateSearcher::CoordinateSearcher(
    std::shared_ptr<index::SpatialIndex> spatialIndex)
    : pImpl_(std::make_unique<Impl>(std::move(spatialIndex))) {
    spdlog::debug("CoordinateSearcher created");
}

CoordinateSearcher::~CoordinateSearcher() {
    spdlog::debug("CoordinateSearcher destroyed");
}

CoordinateSearcher::CoordinateSearcher(CoordinateSearcher&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {
    spdlog::debug("CoordinateSearcher moved");
}

CoordinateSearcher& CoordinateSearcher::operator=(
    CoordinateSearcher&& other) noexcept {
    pImpl_ = std::move(other.pImpl_);
    spdlog::debug("CoordinateSearcher move-assigned");
    return *this;
}

auto CoordinateSearcher::searchRadius(double ra, double dec, double radius,
                                     int limit)
    -> std::vector<CelestialObjectModel> {
    return pImpl_->searchRadius(ra, dec, radius, limit);
}

auto CoordinateSearcher::searchBox(double centerRa, double centerDec,
                                   double raWidth, double decHeight, int limit)
    -> std::vector<CelestialObjectModel> {
    return pImpl_->searchBox(centerRa, centerDec, raWidth, decHeight, limit);
}

auto CoordinateSearcher::isVisible(double ra, double dec,
                                   double observerLatitude, double minAltitude)
    -> bool {
    // Object is visible if:
    // 1. It is above the horizon (altitude >= minAltitude)
    // 2. Its declination allows visibility from observer's latitude

    // Simple check: object declination must be within observable range
    // For an observer at latitude L, declination D is visible if:
    // D >= L - 90 (below south pole) and D <= L + 90 (below north pole)
    double minVisibleDec = observerLatitude - 90.0;
    double maxVisibleDec = observerLatitude + 90.0;

    bool decVisible = (dec >= minVisibleDec && dec <= maxVisibleDec);

    // Additional check: object must be above minimum altitude
    // For simplicity, if dec is visible, check approximate altitude
    // More accurate calculation would need exact time and coordinates
    if (!decVisible) {
        return false;
    }

    // Rough altitude estimate (more accurate calculation needs time)
    // At transit, altitude = 90 - |observer_lat - dec|
    double transitAltitude = 90.0 - std::abs(observerLatitude - dec);

    return transitAltitude >= minAltitude;
}

auto CoordinateSearcher::angularDistance(double ra1, double dec1, double ra2,
                                        double dec2) -> double {
    // Convert to radians
    double ra1Rad = ra1 * M_PI / 180.0;
    double dec1Rad = dec1 * M_PI / 180.0;
    double ra2Rad = ra2 * M_PI / 180.0;
    double dec2Rad = dec2 * M_PI / 180.0;

    // Haversine formula
    double dRa = ra2Rad - ra1Rad;
    double dDec = dec2Rad - dec1Rad;

    double a = std::sin(dDec / 2.0) * std::sin(dDec / 2.0) +
               std::cos(dec1Rad) * std::cos(dec2Rad) * std::sin(dRa / 2.0) *
                   std::sin(dRa / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    double distance = 180.0 / M_PI * c;

    return distance;
}

auto CoordinateSearcher::isValidCoordinate(double ra, double dec) -> bool {
    return ra >= 0.0 && ra < 360.0 && dec >= -90.0 && dec <= 90.0;
}

auto CoordinateSearcher::getObjectCount() const -> size_t {
    return pImpl_->getObjectCount();
}

}  // namespace lithium::target::search
