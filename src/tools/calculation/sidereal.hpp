/**
 * @file sidereal.hpp
 * @brief Sidereal time calculations.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CALCULATION_SIDEREAL_HPP
#define LITHIUM_TOOLS_CALCULATION_SIDEREAL_HPP

#include <cmath>
#include <concepts>

#include "julian.hpp"
#include "tools/astronomy/constants.hpp"

namespace lithium::tools::calculation {

using namespace lithium::tools::astronomy;

// ============================================================================
// Sidereal Time Calculations
// ============================================================================

/**
 * @brief Calculate Greenwich Mean Sidereal Time (GMST).
 * @param jd Julian Date.
 * @return GMST in degrees (0-360).
 */
[[nodiscard]] inline double calculateGMST(double jd) noexcept {
    double T = centuriesSinceJ2000(jd);

    // GMST at 0h UT in degrees
    double gmst = 280.46061837 + 360.98564736629 * (jd - JD_J2000) +
                  0.000387933 * T * T - T * T * T / 38710000.0;

    return normalizeAngle360(gmst);
}

/**
 * @brief Calculate Local Sidereal Time (LST).
 * @param jd Julian Date.
 * @param longitude Observer longitude in degrees (positive east).
 * @return LST in degrees (0-360).
 */
[[nodiscard]] inline double calculateLST(double jd, double longitude) noexcept {
    return normalizeAngle360(calculateGMST(jd) + longitude);
}

/**
 * @brief Calculate Local Sidereal Time in hours.
 * @param jd Julian Date.
 * @param longitude Observer longitude in degrees.
 * @return LST in hours (0-24).
 */
[[nodiscard]] inline double calculateLSTHours(double jd,
                                              double longitude) noexcept {
    return calculateLST(jd, longitude) / HOURS_TO_DEG;
}

/**
 * @brief Calculate sidereal time for a given DateTime.
 * @tparam T Floating-point type.
 * @param dt DateTime structure.
 * @param longitude Observer longitude in degrees.
 * @return Sidereal time in hours.
 */
template <std::floating_point T = double>
[[nodiscard]] auto calculateSiderealTime(const DateTime& dt, T longitude) -> T {
    T jd = calculateJulianDate<T>(dt);
    T T_century =
        (jd - static_cast<T>(JD_J2000)) / static_cast<T>(JULIAN_CENTURY);

    T theta =
        static_cast<T>(280.46061837) +
        static_cast<T>(360.98564736629) * (jd - static_cast<T>(JD_J2000)) +
        static_cast<T>(0.000387933) * T_century * T_century -
        T_century * T_century * T_century / static_cast<T>(38710000.0);

    // Convert to hours and add longitude contribution
    T siderealTime = theta / static_cast<T>(HOURS_TO_DEG) +
                     longitude / static_cast<T>(HOURS_TO_DEG);

    // Normalize to [0, 24) hours
    siderealTime = std::fmod(siderealTime, static_cast<T>(HOURS_IN_DAY));
    if (siderealTime < 0) {
        siderealTime += static_cast<T>(HOURS_IN_DAY);
    }

    return siderealTime;
}

/**
 * @brief Calculate hour angle from LST and RA.
 * @param lst Local sidereal time in hours.
 * @param ra Right ascension in hours.
 * @return Hour angle in hours (-12 to +12).
 */
[[nodiscard]] inline double calculateHourAngle(double lst, double ra) noexcept {
    double ha = lst - ra;
    // Normalize to [-12, +12)
    while (ha < -12.0)
        ha += 24.0;
    while (ha >= 12.0)
        ha -= 24.0;
    return ha;
}

/**
 * @brief Calculate hour angle in degrees.
 * @param lstDeg Local sidereal time in degrees.
 * @param raDeg Right ascension in degrees.
 * @return Hour angle in degrees.
 */
[[nodiscard]] inline double calculateHourAngleDeg(double lstDeg,
                                                  double raDeg) noexcept {
    double ha = lstDeg - raDeg;
    return normalizeAngle180(ha);
}

}  // namespace lithium::tools::calculation

#endif  // LITHIUM_TOOLS_CALCULATION_SIDEREAL_HPP
