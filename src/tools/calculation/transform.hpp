/**
 * @file transform.hpp
 * @brief Coordinate system transformations.
 *
 * Provides functions for transforming between different astronomical
 * coordinate systems (equatorial, horizontal, ecliptic, galactic).
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CALCULATION_TRANSFORM_HPP
#define LITHIUM_TOOLS_CALCULATION_TRANSFORM_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <utility>

#include "sidereal.hpp"
#include "tools/astronomy/constants.hpp"
#include "tools/astronomy/coordinates.hpp"

namespace lithium::tools::calculation {

using namespace lithium::tools::astronomy;

// ============================================================================
// Equatorial to Horizontal Transformation
// ============================================================================

/**
 * @brief Convert equatorial coordinates to horizontal coordinates.
 * @param ra Right ascension in degrees.
 * @param dec Declination in degrees.
 * @param latitude Observer latitude in degrees.
 * @param lst Local sidereal time in degrees.
 * @return HorizontalCoordinates (altitude, azimuth in degrees).
 */
[[nodiscard]] inline HorizontalCoordinates equatorialToHorizontal(
    double ra, double dec, double latitude, double lst) {
    // Calculate hour angle
    double ha = toRadians(lst - ra);
    double decRad = toRadians(dec);
    double latRad = toRadians(latitude);

    // Pre-calculate trig functions
    double sinDec = std::sin(decRad);
    double cosDec = std::cos(decRad);
    double sinLat = std::sin(latRad);
    double cosLat = std::cos(latRad);
    double cosHA = std::cos(ha);
    double sinHA = std::sin(ha);

    // Calculate altitude
    double sinAlt = sinDec * sinLat + cosDec * cosLat * cosHA;
    sinAlt = std::clamp(sinAlt, -1.0, 1.0);
    double altitude = toDegrees(std::asin(sinAlt));

    // Calculate azimuth
    double cosAz =
        (sinDec - sinAlt * sinLat) / (std::cos(toRadians(altitude)) * cosLat);
    cosAz = std::clamp(cosAz, -1.0, 1.0);
    double azimuth = toDegrees(std::acos(cosAz));

    // Adjust azimuth for correct quadrant
    if (sinHA > 0) {
        azimuth = 360.0 - azimuth;
    }

    return {altitude, azimuth};
}

/**
 * @brief Convert equatorial coordinates to horizontal using Julian Date.
 * @param ra Right ascension in degrees.
 * @param dec Declination in degrees.
 * @param latitude Observer latitude in degrees.
 * @param longitude Observer longitude in degrees.
 * @param jd Julian Date.
 * @return HorizontalCoordinates.
 */
[[nodiscard]] inline HorizontalCoordinates equatorialToHorizontalJD(
    double ra, double dec, double latitude, double longitude, double jd) {
    double lst = calculateLST(jd, longitude);
    return equatorialToHorizontal(ra, dec, latitude, lst);
}

// ============================================================================
// Horizontal to Equatorial Transformation
// ============================================================================

/**
 * @brief Convert horizontal coordinates to equatorial coordinates.
 * @param alt Altitude in degrees.
 * @param az Azimuth in degrees.
 * @param latitude Observer latitude in degrees.
 * @param lst Local sidereal time in degrees.
 * @return Pair of (RA, Dec) in degrees.
 */
[[nodiscard]] inline std::pair<double, double> horizontalToEquatorial(
    double alt, double az, double latitude, double lst) {
    double altRad = toRadians(alt);
    double azRad = toRadians(az);
    double latRad = toRadians(latitude);

    // Pre-calculate trig functions
    double sinAlt = std::sin(altRad);
    double cosAlt = std::cos(altRad);
    double sinAz = std::sin(azRad);
    double cosAz = std::cos(azRad);
    double sinLat = std::sin(latRad);
    double cosLat = std::cos(latRad);

    // Calculate declination
    double sinDec = sinAlt * sinLat + cosAlt * cosLat * cosAz;
    sinDec = std::clamp(sinDec, -1.0, 1.0);
    double dec = toDegrees(std::asin(sinDec));

    // Calculate hour angle
    double cosHA =
        (sinAlt - sinLat * sinDec) / (cosLat * std::cos(toRadians(dec)));
    cosHA = std::clamp(cosHA, -1.0, 1.0);
    double ha = toDegrees(std::acos(cosHA));

    // Adjust hour angle for correct quadrant
    if (sinAz > 0) {
        ha = 360.0 - ha;
    }

    // Calculate RA from hour angle and LST
    double ra = normalizeAngle360(lst - ha);

    return {ra, dec};
}

// ============================================================================
// Alt/Az Calculation Templates
// ============================================================================

/**
 * @brief Calculate altitude and azimuth from hour angle and declination.
 * @tparam T Floating-point type.
 * @param hourAngle Hour angle in degrees.
 * @param declination Declination in degrees.
 * @param latitude Observer latitude in degrees.
 * @return Pair of (altitude, azimuth) in degrees.
 */
template <std::floating_point T>
[[nodiscard]] auto calculateAltAzCoordinates(T hourAngle, T declination,
                                             T latitude) -> std::pair<T, T> {
    T haRad = hourAngle * static_cast<T>(DEG_TO_RAD);
    T decRad = declination * static_cast<T>(DEG_TO_RAD);
    T latRad = latitude * static_cast<T>(DEG_TO_RAD);

    T sinDec = std::sin(decRad);
    T cosDec = std::cos(decRad);
    T sinLat = std::sin(latRad);
    T cosLat = std::cos(latRad);
    T cosHA = std::cos(haRad);
    T sinHA = std::sin(haRad);

    // Calculate altitude
    T sinAlt = sinDec * sinLat + cosDec * cosLat * cosHA;
    sinAlt = std::clamp(sinAlt, static_cast<T>(-1.0), static_cast<T>(1.0));
    T altitude = std::asin(sinAlt);

    // Calculate azimuth
    T cosAz = (sinDec - sinAlt * sinLat) / (std::cos(altitude) * cosLat);
    cosAz = std::clamp(cosAz, static_cast<T>(-1.0), static_cast<T>(1.0));
    T azimuth = std::acos(cosAz);

    // Adjust azimuth for correct quadrant
    if (sinHA > 0) {
        azimuth = static_cast<T>(K_TWO_PI) - azimuth;
    }

    // Convert back to degrees
    return {altitude * static_cast<T>(RAD_TO_DEG),
            azimuth * static_cast<T>(RAD_TO_DEG)};
}

// ============================================================================
// Field Rotation
// ============================================================================

/**
 * @brief Calculate field rotation rate at given horizontal coordinates.
 * @tparam T Floating-point type.
 * @param altitude Altitude in degrees.
 * @param azimuth Azimuth in degrees.
 * @param latitude Observer latitude in degrees.
 * @return Field rotation rate in degrees per hour.
 */
template <std::floating_point T>
[[nodiscard]] auto calculateFieldRotationRate(T altitude, T azimuth, T latitude)
    -> T {
    T altRad = altitude * static_cast<T>(DEG_TO_RAD);
    T azRad = azimuth * static_cast<T>(DEG_TO_RAD);
    T latRad = latitude * static_cast<T>(DEG_TO_RAD);

    // Field rotation rate in radians per sidereal hour
    T rate = std::cos(latRad) * std::sin(azRad) / std::cos(altRad);

    // Convert to degrees per hour (sidereal)
    return rate * static_cast<T>(RAD_TO_DEG);
}

// ============================================================================
// Atmospheric Refraction
// ============================================================================

/**
 * @brief Calculate atmospheric refraction correction.
 * @tparam T Floating-point type.
 * @param altitude Apparent altitude in degrees.
 * @param temperature Temperature in Celsius (default: 10.0).
 * @param pressure Atmospheric pressure in hPa (default: 1010.0).
 * @return Refraction correction in degrees (add to true altitude).
 */
template <std::floating_point T>
[[nodiscard]] auto calculateRefraction(T altitude, T temperature = 10.0,
                                       T pressure = 1010.0) -> T {
    if (altitude < static_cast<T>(-0.5)) {
        return static_cast<T>(0.0);  // Below horizon
    }

    T refraction;
    if (altitude > static_cast<T>(15.0)) {
        // High altitude formula
        refraction = static_cast<T>(0.00452) * pressure /
                     ((static_cast<T>(273.0) + temperature) *
                      std::tan(altitude * static_cast<T>(DEG_TO_RAD)));
    } else {
        // Low altitude formula (Saemundsson)
        T a = altitude;
        refraction = static_cast<T>(0.1594) + static_cast<T>(0.0196) * a +
                     static_cast<T>(0.00002) * a * a;
        refraction *=
            pressure *
            (static_cast<T>(1.0) -
             static_cast<T>(0.00012) * (temperature - static_cast<T>(10.0))) /
            static_cast<T>(1010.0);
        refraction /= static_cast<T>(60.0);  // Convert arcminutes to degrees
    }

    return refraction;
}

}  // namespace lithium::tools::calculation

#endif  // LITHIUM_TOOLS_CALCULATION_TRANSFORM_HPP
