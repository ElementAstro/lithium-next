/**
 * @file coordinates.hpp
 * @brief Coordinate types for astronomical observations.
 *
 * This file defines various coordinate systems used in astronomy:
 * - Celestial coordinates (RA/Dec)
 * - Horizontal coordinates (Alt/Az)
 * - Observer location (geographic coordinates)
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_COORDINATES_HPP
#define LITHIUM_TOOLS_ASTRONOMY_COORDINATES_HPP

#include <algorithm>
#include <cmath>
#include <string>

#include "atom/type/json.hpp"
#include "constants.hpp"

namespace lithium::tools::astronomy {

using json = nlohmann::json;

// ============================================================================
// Celestial Coordinates (Equatorial)
// ============================================================================

/**
 * @struct Coordinates
 * @brief Celestial coordinates for an astronomical target.
 *
 * Represents equatorial coordinates using Right Ascension and Declination.
 * RA is stored in degrees (0-360) for internal calculations.
 */
struct Coordinates {
    double ra{0.0};        ///< Right Ascension in degrees (0-360)
    double dec{0.0};       ///< Declination in degrees (-90 to +90)
    double epoch{2000.0};  ///< Coordinate epoch (default J2000.0)

    // ========================================================================
    // Constructors
    // ========================================================================

    Coordinates() = default;

    Coordinates(double ra_, double dec_, double epoch_ = 2000.0)
        : ra(ra_), dec(dec_), epoch(epoch_) {}

    // ========================================================================
    // Conversion Methods
    // ========================================================================

    /**
     * @brief Convert RA to hours (0-24).
     * @return Right Ascension in hours.
     */
    [[nodiscard]] double raHours() const noexcept { return ra / HOURS_TO_DEG; }

    /**
     * @brief Convert RA hours to degrees.
     * @param hours RA in hours.
     * @return RA in degrees.
     */
    [[nodiscard]] static double hoursToRA(double hours) noexcept {
        return hours * HOURS_TO_DEG;
    }

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if coordinates are valid.
     * @return true if coordinates are within valid ranges.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return ra >= 0.0 && ra < 360.0 && dec >= -90.0 && dec <= 90.0;
    }

    // ========================================================================
    // Calculations
    // ========================================================================

    /**
     * @brief Calculate angular separation from another coordinate.
     * @param other The other coordinate.
     * @return Angular separation in degrees.
     */
    [[nodiscard]] double separationFrom(const Coordinates& other) const {
        double ra1 = ra * DEG_TO_RAD;
        double dec1 = dec * DEG_TO_RAD;
        double ra2 = other.ra * DEG_TO_RAD;
        double dec2 = other.dec * DEG_TO_RAD;

        double cosAngle = std::sin(dec1) * std::sin(dec2) +
                          std::cos(dec1) * std::cos(dec2) * std::cos(ra1 - ra2);
        return std::acos(std::clamp(cosAngle, -1.0, 1.0)) * RAD_TO_DEG;
    }

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create from RA in hours and Dec in degrees.
     * @param raHours Right Ascension in hours.
     * @param decDeg Declination in degrees.
     * @param epoch Coordinate epoch.
     * @return Coordinates object.
     */
    [[nodiscard]] static Coordinates fromHMS(double raHours, double decDeg,
                                             double epoch = 2000.0) {
        return {raHours * HOURS_TO_DEG, decDeg, epoch};
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {{"ra", ra}, {"dec", dec}, {"epoch", epoch}};
    }

    [[nodiscard]] static Coordinates fromJson(const json& j) {
        return {j.value("ra", 0.0), j.value("dec", 0.0),
                j.value("epoch", 2000.0)};
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const Coordinates& other) const noexcept {
        return std::abs(ra - other.ra) < EPSILON &&
               std::abs(dec - other.dec) < EPSILON &&
               std::abs(epoch - other.epoch) < EPSILON;
    }

    bool operator!=(const Coordinates& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// Horizontal Coordinates (Alt/Az)
// ============================================================================

/**
 * @struct HorizontalCoordinates
 * @brief Altitude and azimuth coordinates.
 *
 * Represents the position of an object in the local sky as seen by an observer.
 */
struct HorizontalCoordinates {
    double altitude{
        0.0};  ///< Altitude in degrees (0-90, negative below horizon)
    double azimuth{0.0};  ///< Azimuth in degrees (0-360, N=0, E=90)

    // ========================================================================
    // Constructors
    // ========================================================================

    HorizontalCoordinates() = default;

    HorizontalCoordinates(double alt, double az) : altitude(alt), azimuth(az) {}

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if object is above the horizon.
     * @return true if altitude > 0.
     */
    [[nodiscard]] bool isAboveHorizon() const noexcept {
        return altitude > 0.0;
    }

    /**
     * @brief Check if coordinates are valid.
     * @return true if within valid ranges.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return altitude >= -90.0 && altitude <= 90.0 && azimuth >= 0.0 &&
               azimuth < 360.0;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {{"altitude", altitude}, {"azimuth", azimuth}};
    }

    [[nodiscard]] static HorizontalCoordinates fromJson(const json& j) {
        return {j.value("altitude", 0.0), j.value("azimuth", 0.0)};
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const HorizontalCoordinates& other) const noexcept {
        return std::abs(altitude - other.altitude) < EPSILON &&
               std::abs(azimuth - other.azimuth) < EPSILON;
    }

    bool operator!=(const HorizontalCoordinates& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// Observer Location
// ============================================================================

/**
 * @struct ObserverLocation
 * @brief Geographic location of the observer.
 *
 * Represents the observer's position on Earth for coordinate transformations.
 */
struct ObserverLocation {
    double latitude{0.0};   ///< Latitude in degrees (-90 to +90)
    double longitude{0.0};  ///< Longitude in degrees (-180 to +180)
    double elevation{0.0};  ///< Elevation in meters above sea level

    // ========================================================================
    // Constructors
    // ========================================================================

    ObserverLocation() = default;

    ObserverLocation(double lat, double lon, double elev = 0.0)
        : latitude(lat), longitude(lon), elevation(elev) {}

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if location is valid.
     * @return true if coordinates are within valid ranges.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 &&
               longitude <= 180.0;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {{"latitude", latitude},
                {"longitude", longitude},
                {"elevation", elevation}};
    }

    [[nodiscard]] static ObserverLocation fromJson(const json& j) {
        return {j.value("latitude", 0.0), j.value("longitude", 0.0),
                j.value("elevation", 0.0)};
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const ObserverLocation& other) const noexcept {
        return std::abs(latitude - other.latitude) < EPSILON &&
               std::abs(longitude - other.longitude) < EPSILON &&
               std::abs(elevation - other.elevation) < EPSILON;
    }

    bool operator!=(const ObserverLocation& other) const noexcept {
        return !(*this == other);
    }
};

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_COORDINATES_HPP
