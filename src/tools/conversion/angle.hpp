/**
 * @file angle.hpp
 * @brief Angle conversion utilities.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CONVERSION_ANGLE_HPP
#define LITHIUM_TOOLS_CONVERSION_ANGLE_HPP

#include <cmath>
#include <concepts>

#include "tools/astronomy/constants.hpp"

namespace lithium::tools::conversion {

using namespace lithium::tools::astronomy;

// ============================================================================
// Basic Angle Conversions
// ============================================================================

/**
 * @brief Convert degrees to radians.
 * @param degrees Angle in degrees.
 * @return Angle in radians.
 */
[[nodiscard]] constexpr double degreeToRad(double degrees) noexcept {
    return degrees * DEG_TO_RAD;
}

/**
 * @brief Convert radians to degrees.
 * @param radians Angle in radians.
 * @return Angle in degrees.
 */
[[nodiscard]] constexpr double radToDegree(double radians) noexcept {
    return radians * RAD_TO_DEG;
}

/**
 * @brief Convert hours to degrees.
 * @param hours Angle in hours.
 * @return Angle in degrees.
 */
[[nodiscard]] constexpr double hourToDegree(double hours) noexcept {
    return hours * HOURS_TO_DEG;
}

/**
 * @brief Convert degrees to hours.
 * @param degrees Angle in degrees.
 * @return Angle in hours.
 */
[[nodiscard]] constexpr double degreeToHour(double degrees) noexcept {
    return degrees * DEG_TO_HOURS;
}

/**
 * @brief Convert hours to radians.
 * @param hours Angle in hours.
 * @return Angle in radians.
 */
[[nodiscard]] constexpr double hourToRad(double hours) noexcept {
    return hours * HOURS_TO_DEG * DEG_TO_RAD;
}

/**
 * @brief Convert radians to hours.
 * @param radians Angle in radians.
 * @return Angle in hours.
 */
[[nodiscard]] constexpr double radToHour(double radians) noexcept {
    return radians * RAD_TO_DEG * DEG_TO_HOURS;
}

// ============================================================================
// Normalization Functions
// ============================================================================

/**
 * @brief Normalize right ascension to [0, 24) hours.
 * @tparam T Floating-point type.
 * @param ra Right ascension in hours.
 * @return Normalized right ascension.
 */
template <std::floating_point T>
[[nodiscard]] auto normalizeRightAscension(T ra) -> T {
    constexpr T HOURS = static_cast<T>(HOURS_IN_DAY);
    ra = std::fmod(ra, HOURS);
    if (ra < 0) {
        ra += HOURS;
    }
    return ra;
}

/**
 * @brief Normalize declination to [-90, 90] degrees.
 * @tparam T Floating-point type.
 * @param dec Declination in degrees.
 * @return Clamped declination.
 */
template <std::floating_point T>
[[nodiscard]] auto normalizeDeclination(T dec) -> T {
    return std::clamp(dec, static_cast<T>(-90.0), static_cast<T>(90.0));
}

/**
 * @brief Normalize hour angle to [-12, 12) hours.
 * @tparam T Floating-point type.
 * @param ha Hour angle in hours.
 * @return Normalized hour angle.
 */
template <std::floating_point T>
[[nodiscard]] auto normalizeHourAngle(T ha) -> T {
    constexpr T HOURS = static_cast<T>(HOURS_IN_DAY);
    constexpr T HALF = static_cast<T>(12.0);

    if (ha < -HALF) {
        ha += HOURS;
    }
    while (ha >= HALF) {
        ha -= HOURS;
    }
    return ha;
}

// ============================================================================
// Range Constraint
// ============================================================================

/**
 * @brief Constrain a value within a range with wrap-around.
 * @param value Value to constrain.
 * @param maxVal Maximum value.
 * @param minVal Minimum value.
 * @return Constrained value.
 */
[[nodiscard]] inline double rangeTo(double value, double maxVal,
                                     double minVal) noexcept {
    double range = maxVal - minVal;
    while (value >= maxVal) {
        value -= range;
    }
    while (value < minVal) {
        value += range;
    }
    return value;
}

// ============================================================================
// DMS/HMS Conversions
// ============================================================================

/**
 * @brief Convert degrees, minutes, seconds to decimal degrees.
 * @param degrees Degrees component.
 * @param minutes Minutes component.
 * @param seconds Seconds component.
 * @return Decimal degrees.
 */
[[nodiscard]] inline double dmsToDegree(int degrees, int minutes,
                                         double seconds) noexcept {
    double sign = (degrees < 0) ? -1.0 : 1.0;
    return sign * (std::abs(degrees) + minutes / 60.0 + seconds / 3600.0);
}

/**
 * @brief Convert hours, minutes, seconds to decimal hours.
 * @param hours Hours component.
 * @param minutes Minutes component.
 * @param seconds Seconds component.
 * @return Decimal hours.
 */
[[nodiscard]] inline double hmsToHours(int hours, int minutes,
                                        double seconds) noexcept {
    return hours + minutes / 60.0 + seconds / 3600.0;
}

/**
 * @brief Convert decimal degrees to DMS components.
 * @param decimal Decimal degrees.
 * @param[out] degrees Degrees component.
 * @param[out] minutes Minutes component.
 * @param[out] seconds Seconds component.
 */
inline void degreeToDms(double decimal, int& degrees, int& minutes,
                        double& seconds) noexcept {
    double sign = (decimal < 0) ? -1.0 : 1.0;
    decimal = std::abs(decimal);
    degrees = static_cast<int>(decimal) * static_cast<int>(sign);
    double remainder = (decimal - std::abs(degrees)) * 60.0;
    minutes = static_cast<int>(remainder);
    seconds = (remainder - minutes) * 60.0;
}

/**
 * @brief Convert decimal hours to HMS components.
 * @param decimal Decimal hours.
 * @param[out] hours Hours component.
 * @param[out] minutes Minutes component.
 * @param[out] seconds Seconds component.
 */
inline void hoursToHms(double decimal, int& hours, int& minutes,
                       double& seconds) noexcept {
    hours = static_cast<int>(decimal);
    double remainder = (decimal - hours) * 60.0;
    minutes = static_cast<int>(remainder);
    seconds = (remainder - minutes) * 60.0;
}

}  // namespace lithium::tools::conversion

#endif  // LITHIUM_TOOLS_CONVERSION_ANGLE_HPP
