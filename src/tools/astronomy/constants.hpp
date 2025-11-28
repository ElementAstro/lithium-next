/**
 * @file constants.hpp
 * @brief Astronomical constants for coordinate calculations and conversions.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_CONSTANTS_HPP
#define LITHIUM_TOOLS_ASTRONOMY_CONSTANTS_HPP

#include <algorithm>
#include <cmath>
#include <numbers>

namespace lithium::tools::astronomy {

// ============================================================================
// Mathematical Constants
// ============================================================================

/// Pi constant using std::numbers for precision
inline constexpr double K_PI = std::numbers::pi;
/// Two times Pi
inline constexpr double K_TWO_PI = 2.0 * K_PI;
/// Half of Pi
inline constexpr double K_HALF_PI = K_PI / 2.0;

// ============================================================================
// Angular Conversion Constants
// ============================================================================

/// Conversion factor: degrees to radians
inline constexpr double DEG_TO_RAD = K_PI / 180.0;
/// Conversion factor: radians to degrees
inline constexpr double RAD_TO_DEG = 180.0 / K_PI;
/// Conversion factor: hours to degrees (1 hour = 15 degrees)
inline constexpr double HOURS_TO_DEG = 15.0;
/// Conversion factor: degrees to hours
inline constexpr double DEG_TO_HOURS = 1.0 / HOURS_TO_DEG;
/// Degrees in a full circle
inline constexpr double DEGREES_IN_CIRCLE = 360.0;
/// Arcseconds per degree
inline constexpr double ARCSEC_PER_DEGREE = 3600.0;
/// Arcminutes per degree
inline constexpr double ARCMIN_PER_DEGREE = 60.0;

// ============================================================================
// Time Constants
// ============================================================================

/// Hours in a day
inline constexpr double HOURS_IN_DAY = 24.0;
/// Minutes in an hour
inline constexpr double MINUTES_IN_HOUR = 60.0;
/// Seconds in a minute
inline constexpr double SECONDS_IN_MINUTE = 60.0;
/// Seconds in an hour
inline constexpr double SECONDS_IN_HOUR = 3600.0;
/// Seconds in a day
inline constexpr double SECONDS_IN_DAY = 86400.0;

// ============================================================================
// Julian Date Constants
// ============================================================================

/// Julian Date of Unix epoch (1970-01-01 00:00:00 UTC)
inline constexpr double JD_UNIX_EPOCH = 2440587.5;
/// J2000.0 epoch in Julian Date
inline constexpr double JD_J2000 = 2451545.0;
/// Offset between Julian Date and Modified Julian Date
inline constexpr double MJD_OFFSET = 2400000.5;
/// Days in a Julian century
inline constexpr double JULIAN_CENTURY = 36525.0;

// ============================================================================
// Physical Constants
// ============================================================================

/// Earth equatorial radius in meters
inline constexpr double EARTH_RADIUS_EQUATORIAL = 6378137.0;
/// Earth polar radius in meters
inline constexpr double EARTH_RADIUS_POLAR = 6356752.0;
/// Earth mean radius in meters
inline constexpr double EARTH_RADIUS_MEAN = 6371000.0;
/// Astronomical Unit in meters
inline constexpr double ASTRONOMICAL_UNIT = 1.495978707e11;
/// Speed of light in m/s
inline constexpr double SPEED_OF_LIGHT = 299792458.0;
/// Solar mass in kg
inline constexpr double SOLAR_MASS = 1.98847e30;
/// Solar radius in meters
inline constexpr double SOLAR_RADIUS = 6.957e8;
/// Parsec in meters
inline constexpr double PARSEC = 3.0857e16;

// ============================================================================
// Optical Constants
// ============================================================================

/// Airy disk constant
inline constexpr double AIRY_CONSTANT = 1.21966;

// ============================================================================
// Numerical Constants
// ============================================================================

/// Small epsilon for floating-point comparisons
inline constexpr double EPSILON = 1e-10;
/// Standard epsilon for general comparisons
inline constexpr double EPSILON_STANDARD = 1e-5;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert degrees to radians.
 * @param degrees Angle in degrees.
 * @return Angle in radians.
 */
[[nodiscard]] constexpr double toRadians(double degrees) noexcept {
    return degrees * DEG_TO_RAD;
}

/**
 * @brief Convert radians to degrees.
 * @param radians Angle in radians.
 * @return Angle in degrees.
 */
[[nodiscard]] constexpr double toDegrees(double radians) noexcept {
    return radians * RAD_TO_DEG;
}

/**
 * @brief Convert hours to degrees.
 * @param hours Angle in hours.
 * @return Angle in degrees.
 */
[[nodiscard]] constexpr double hoursToDegrees(double hours) noexcept {
    return hours * HOURS_TO_DEG;
}

/**
 * @brief Convert degrees to hours.
 * @param degrees Angle in degrees.
 * @return Angle in hours.
 */
[[nodiscard]] constexpr double degreesToHours(double degrees) noexcept {
    return degrees * DEG_TO_HOURS;
}

/**
 * @brief Normalize angle to range [0, 360) degrees.
 * @param angle Angle in degrees.
 * @return Normalized angle in degrees.
 */
[[nodiscard]] inline double normalizeAngle360(double angle) noexcept {
    angle = std::fmod(angle, DEGREES_IN_CIRCLE);
    if (angle < 0.0) {
        angle += DEGREES_IN_CIRCLE;
    }
    return angle;
}

/**
 * @brief Normalize angle to range [-180, 180) degrees.
 * @param angle Angle in degrees.
 * @return Normalized angle in degrees.
 */
[[nodiscard]] inline double normalizeAngle180(double angle) noexcept {
    angle = normalizeAngle360(angle);
    if (angle >= 180.0) {
        angle -= DEGREES_IN_CIRCLE;
    }
    return angle;
}

/**
 * @brief Normalize right ascension to range [0, 24) hours.
 * @param ra Right ascension in hours.
 * @return Normalized right ascension in hours.
 */
[[nodiscard]] inline double normalizeRA(double ra) noexcept {
    ra = std::fmod(ra, HOURS_IN_DAY);
    if (ra < 0.0) {
        ra += HOURS_IN_DAY;
    }
    return ra;
}

/**
 * @brief Clamp declination to range [-90, 90] degrees.
 * @param dec Declination in degrees.
 * @return Clamped declination in degrees.
 */
[[nodiscard]] inline double clampDeclination(double dec) noexcept {
    return std::clamp(dec, -90.0, 90.0);
}

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_CONSTANTS_HPP
