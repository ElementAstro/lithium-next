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

#ifndef LITHIUM_TARGET_OBSERVABILITY_VISIBILITY_CALCULATOR_HPP
#define LITHIUM_TARGET_OBSERVABILITY_VISIBILITY_CALCULATOR_HPP

#include <chrono>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

#include "atom/type/json.hpp"
#include "target/celestial_model.hpp"
#include "tools/astronomy/constraints.hpp"
#include "tools/astronomy/coordinates.hpp"

namespace lithium::target::observability {

using json = nlohmann::json;
using ObserverLocation = lithium::tools::astronomy::ObserverLocation;
using AltitudeConstraints = lithium::tools::astronomy::AltitudeConstraints;
using ObservabilityWindow = lithium::tools::astronomy::ObservabilityWindow;
using HorizontalCoordinates = lithium::tools::astronomy::HorizontalCoordinates;
using Coordinates = lithium::tools::astronomy::Coordinates;

/**
 * @class VisibilityCalculator
 * @brief Calculates astronomical visibility and rise/set times for celestial
 * objects.
 *
 * This class provides high-precision astronomical calculations for determining
 * when celestial objects are visible from a specific observer location.
 * It integrates with existing astronomy tools for accurate coordinate
 * transformations and time calculations.
 *
 * Features:
 * - Rise/set/transit time calculations
 * - Current altitude/azimuth determination
 * - Observability window calculation with constraints
 * - Moon distance calculations
 * - Twilight time calculations (civil, nautical, astronomical)
 * - Batch object filtering
 * - Timezone support
 */
class VisibilityCalculator {
public:
    /**
     * @brief Constructor with observer location.
     * @param location Observer location with latitude, longitude, and elevation
     * @throws std::invalid_argument if location is invalid
     */
    explicit VisibilityCalculator(const ObserverLocation& location);

    /**
     * @brief Destructor.
     */
    ~VisibilityCalculator();

    // ========================================================================
    // Location Management
    // ========================================================================

    /**
     * @brief Set the observer location for calculations.
     * @param location New observer location
     * @throws std::invalid_argument if location is invalid
     */
    void setLocation(const ObserverLocation& location);

    /**
     * @brief Get the current observer location.
     * @return Reference to observer location
     */
    [[nodiscard]] const ObserverLocation& getLocation() const;

    /**
     * @brief Set observer timezone (IANA timezone name).
     * @param timezone Timezone string (e.g., "UTC", "America/New_York")
     */
    void setTimezone(const std::string& timezone);

    /**
     * @brief Get observer timezone.
     * @return Timezone string
     */
    [[nodiscard]] const std::string& getTimezone() const;

    // ========================================================================
    // Coordinate Transformations
    // ========================================================================

    /**
     * @brief Calculate horizontal coordinates (Alt/Az) for an object.
     *
     * Transforms equatorial coordinates (RA/Dec) to horizontal coordinates
     * (Altitude/Azimuth) for the observer location at a specific time.
     *
     * @param ra Right Ascension in degrees (0-360)
     * @param dec Declination in degrees (-90 to +90)
     * @param time Observation time (defaults to current time)
     * @return HorizontalCoordinates with altitude and azimuth
     */
    [[nodiscard]] HorizontalCoordinates calculateAltAz(
        double ra, double dec,
        std::chrono::system_clock::time_point time =
            std::chrono::system_clock::now()) const;

    /**
     * @brief Calculate hour angle for an object at a specific time.
     *
     * Hour angle is the angle between the object and the local meridian,
     * measured in hours (-12 to +12, negative=east, positive=west).
     *
     * @param ra Right Ascension in degrees
     * @param time Observation time
     * @return Hour angle in hours
     */
    [[nodiscard]] double calculateHourAngle(
        double ra, std::chrono::system_clock::time_point time =
                       std::chrono::system_clock::now()) const;

    /**
     * @brief Calculate apparent sidereal time for observer location.
     *
     * @param time Observation time
     * @return Apparent sidereal time in hours (0-24)
     */
    [[nodiscard]] double calculateApparentSiderealTime(
        std::chrono::system_clock::time_point time =
            std::chrono::system_clock::now()) const;

    // ========================================================================
    // Observability Calculations
    // ========================================================================

    /**
     * @brief Calculate observability window for a celestial object.
     *
     * Determines when an object rises, transits meridian, and sets,
     * along with maximum altitude and other information.
     * Also checks if observations are feasible within given constraints.
     *
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @param date Date for calculations
     * @param constraints Altitude and other observability constraints
     * @return ObservabilityWindow with rise/transit/set times
     */
    [[nodiscard]] ObservabilityWindow calculateWindow(
        double ra, double dec, std::chrono::system_clock::time_point date,
        const AltitudeConstraints& constraints = {});

    /**
     * @brief Check if object is currently observable.
     *
     * Determines if an object is above the horizon and meets
     * all observability constraints.
     *
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @param constraints Observability constraints
     * @return true if observable now
     */
    [[nodiscard]] bool isCurrentlyObservable(
        double ra, double dec,
        const AltitudeConstraints& constraints = {}) const;

    /**
     * @brief Check if object is currently observable at a specific time.
     *
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @param time Observation time
     * @param constraints Observability constraints
     * @return true if observable at the specified time
     */
    [[nodiscard]] bool isObservableAt(
        double ra, double dec, std::chrono::system_clock::time_point time,
        const AltitudeConstraints& constraints = {}) const;

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * @brief Filter observable celestial objects within a time range.
     *
     * Returns only objects that are observable within the specified
     * time window, pairing each with its observability window.
     *
     * @param objects Span of celestial objects to filter
     * @param startTime Start of observation window
     * @param endTime End of observation window
     * @param constraints Observability constraints
     * @return Vector of objects with observability windows
     */
    [[nodiscard]] std::vector<
        std::pair<CelestialObjectModel, ObservabilityWindow>>
    filterObservable(std::span<const CelestialObjectModel> objects,
                     std::chrono::system_clock::time_point startTime,
                     std::chrono::system_clock::time_point endTime,
                     const AltitudeConstraints& constraints = {});

    /**
     * @brief Optimize observation sequence to minimize telescope movement.
     *
     * Reorders objects using a greedy nearest-neighbor algorithm to
     * minimize the total angular distance traveled by the telescope.
     * Useful for efficient observation planning.
     *
     * @param objects Objects to observe
     * @param startTime Start of observation
     * @return Optimized sequence with optimal observation times
     */
    [[nodiscard]] std::vector<
        std::pair<CelestialObjectModel, std::chrono::system_clock::time_point>>
    optimizeSequence(std::span<const CelestialObjectModel> objects,
                     std::chrono::system_clock::time_point startTime);

    // ========================================================================
    // Solar and Lunar Information
    // ========================================================================

    /**
     * @brief Get sunrise and sunset times with twilight information.
     *
     * @param date Date for calculation
     * @return Tuple of (sunset, end_of_twilight_astronomical,
     * start_of_twilight_astronomical, sunrise)
     */
    [[nodiscard]] std::tuple<std::chrono::system_clock::time_point,
                             std::chrono::system_clock::time_point,
                             std::chrono::system_clock::time_point,
                             std::chrono::system_clock::time_point>
    getSunTimes(std::chrono::system_clock::time_point date) const;

    /**
     * @brief Get civil twilight times (sun at -6 degrees below horizon).
     *
     * Civil twilight is when natural light is still sufficient for
     * outdoor activities without artificial light.
     *
     * @param date Date for calculation
     * @return Tuple of (start_time, end_time)
     */
    [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                            std::chrono::system_clock::time_point>
    getCivilTwilightTimes(std::chrono::system_clock::time_point date) const;

    /**
     * @brief Get nautical twilight times (sun at -12 degrees below horizon).
     *
     * Nautical twilight is when the horizon is visible for navigation
     * by celestial bodies.
     *
     * @param date Date for calculation
     * @return Tuple of (start_time, end_time)
     */
    [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                            std::chrono::system_clock::time_point>
    getNauticalTwilightTimes(std::chrono::system_clock::time_point date) const;

    /**
     * @brief Get astronomical twilight times (sun at -18 degrees below
     * horizon).
     *
     * Astronomical twilight is when the sky is completely dark for
     * astronomical observations.
     *
     * @param date Date for calculation
     * @return Tuple of (start_time, end_time)
     */
    [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                            std::chrono::system_clock::time_point>
    getAstronomicalTwilightTimes(
        std::chrono::system_clock::time_point date) const;

    /**
     * @brief Get tonight's observing window (astronomical twilight times).
     *
     * Returns the time range from end of evening astronomical twilight
     * to start of morning astronomical twilight for today.
     *
     * @return Tuple of (start_time, end_time)
     */
    [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                            std::chrono::system_clock::time_point>
    getTonightWindow() const;

    /**
     * @brief Get Moon position and phase information.
     *
     * @param time Observation time
     * @return Tuple of (ra_degrees, dec_degrees, phase_0_to_1)
     */
    [[nodiscard]] std::tuple<double, double, double> getMoonInfo(
        std::chrono::system_clock::time_point time =
            std::chrono::system_clock::now()) const;

    /**
     * @brief Calculate angular distance from object to Moon.
     *
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @param time Observation time
     * @return Distance in degrees
     */
    [[nodiscard]] double calculateMoonDistance(
        double ra, double dec,
        std::chrono::system_clock::time_point time =
            std::chrono::system_clock::now()) const;

    /**
     * @brief Check if Moon is above horizon.
     *
     * @param time Observation time
     * @return true if Moon is visible
     */
    [[nodiscard]] bool isMoonAboveHorizon(
        std::chrono::system_clock::time_point time =
            std::chrono::system_clock::now()) const;

    // ========================================================================
    // Time Utilities
    // ========================================================================

    /**
     * @brief Convert local time to UTC.
     *
     * @param localTime Local time with timezone
     * @return Equivalent UTC time
     */
    [[nodiscard]] std::chrono::system_clock::time_point localToUTC(
        std::chrono::system_clock::time_point localTime) const;

    /**
     * @brief Convert UTC to local time.
     *
     * @param utcTime UTC time
     * @return Equivalent local time with timezone offset
     */
    [[nodiscard]] std::chrono::system_clock::time_point utcToLocal(
        std::chrono::system_clock::time_point utcTime) const;

    /**
     * @brief Get current timezone offset from UTC in seconds.
     *
     * @return Offset in seconds (positive=east, negative=west)
     */
    [[nodiscard]] int64_t getTimezoneOffset() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    // Helper methods
    [[nodiscard]] double calculateAltitudeWithRefraction(
        double rawAltitude) const;
};

}  // namespace lithium::target::observability

#endif  // LITHIUM_TARGET_OBSERVABILITY_VISIBILITY_CALCULATOR_HPP
