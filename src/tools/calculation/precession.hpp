/**
 * @file precession.hpp
 * @brief Precession, nutation, and aberration calculations.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CALCULATION_PRECESSION_HPP
#define LITHIUM_TOOLS_CALCULATION_PRECESSION_HPP

#include <array>
#include <cmath>
#include <tuple>

#include "julian.hpp"
#include "tools/astronomy/constants.hpp"

namespace lithium::tools::calculation {

using namespace lithium::tools::astronomy;

// ============================================================================
// Equatorial Coordinates (for internal use)
// ============================================================================

/**
 * @struct EquatorialCoords
 * @brief Equatorial coordinates for precession calculations.
 */
struct EquatorialCoords {
    double ra;   ///< Right Ascension in hours
    double dec;  ///< Declination in degrees

    EquatorialCoords() = default;
    EquatorialCoords(double r, double d) : ra(r), dec(d) {}
};

// ============================================================================
// Obliquity of the Ecliptic
// ============================================================================

/**
 * @brief Calculate mean obliquity of the ecliptic.
 * @param jd Julian Date.
 * @return Obliquity in degrees.
 */
[[nodiscard]] inline double calculateObliquity(double jd) noexcept {
    double T = centuriesSinceJ2000(jd);
    return 23.439291 - 0.0130042 * T - 1.64e-7 * T * T + 5.04e-7 * T * T * T;
}

// ============================================================================
// Nutation
// ============================================================================

/**
 * @brief Calculate nutation in longitude and obliquity.
 * @param jd Julian Date.
 * @return Tuple of (nutation in longitude, nutation in obliquity) in degrees.
 */
[[nodiscard]] inline std::tuple<double, double> calculateNutation(
    double jd) noexcept {
    double T = centuriesSinceJ2000(jd);

    // Mean longitude of ascending node of Moon's orbit
    double omega =
        125.04452 - 1934.136261 * T + 0.0020708 * T * T + T * T * T / 450000.0;

    // Mean longitude of Sun and Moon
    double L_sun = 280.4665 + 36000.7698 * T;
    double L_moon = 218.3165 + 481267.8813 * T;

    // Nutation in longitude (arcseconds)
    double dpsi = -17.2 * std::sin(toRadians(omega)) -
                  1.32 * std::sin(2.0 * toRadians(L_sun)) -
                  0.23 * std::sin(2.0 * toRadians(L_moon)) +
                  0.21 * std::sin(2.0 * toRadians(omega));

    // Nutation in obliquity (arcseconds)
    double deps = 9.2 * std::cos(toRadians(omega)) +
                  0.57 * std::cos(2.0 * toRadians(L_sun)) +
                  0.1 * std::cos(2.0 * toRadians(L_moon)) -
                  0.09 * std::cos(2.0 * toRadians(omega));

    // Convert to degrees
    return {dpsi / 3600.0, deps / 3600.0};
}

/**
 * @brief Apply nutation correction to equatorial coordinates.
 * @param coords Equatorial coordinates.
 * @param jd Julian Date.
 * @param reverse If true, remove nutation instead of applying.
 * @return Corrected coordinates.
 */
[[nodiscard]] inline EquatorialCoords applyNutation(
    const EquatorialCoords& coords, double jd, bool reverse = false) {
    auto [dpsi, deps] = calculateNutation(jd);
    double obliquity = toRadians(calculateObliquity(jd));

    double ra = toRadians(coords.ra * HOURS_TO_DEG);
    double dec = toRadians(coords.dec);

    double sign = reverse ? -1.0 : 1.0;

    // Calculate corrections
    double dra = (std::cos(obliquity) +
                  std::sin(obliquity) * std::sin(ra) * std::tan(dec)) *
                     dpsi -
                 std::cos(ra) * std::tan(dec) * deps;

    double ddec =
        std::sin(obliquity) * std::cos(ra) * dpsi + std::sin(ra) * deps;

    // Apply corrections
    double newRA = toDegrees(ra + sign * toRadians(dra)) / HOURS_TO_DEG;
    double newDec = toDegrees(dec + sign * toRadians(ddec));

    return {newRA, newDec};
}

// ============================================================================
// Aberration
// ============================================================================

/**
 * @brief Apply annual aberration correction.
 * @param coords Equatorial coordinates.
 * @param jd Julian Date.
 * @return Corrected coordinates.
 */
[[nodiscard]] inline EquatorialCoords applyAberration(
    const EquatorialCoords& coords, double jd) {
    double T = centuriesSinceJ2000(jd);

    // Orbital elements (eccentricity not used in simplified formula)
    double pi_lon = 102.93735 + 1.71946 * T + 0.00046 * T * T;
    double L = 280.46646 + 36000.77983 * T + 0.0003032 * T * T;

    double ra = toRadians(coords.ra * HOURS_TO_DEG);
    double dec = toRadians(coords.dec);

    // Constant of aberration (arcseconds)
    constexpr double kappa = 20.49552 / 3600.0;

    // Calculate corrections
    double dra =
        -kappa *
        (std::cos(ra) * std::cos(toRadians(L)) * std::cos(toRadians(pi_lon)) +
         std::sin(ra) * std::sin(toRadians(L))) /
        std::cos(dec);

    double ddec = -kappa * std::sin(toRadians(pi_lon)) *
                  (std::sin(dec) * std::cos(toRadians(L)) -
                   std::cos(dec) * std::sin(ra) * std::sin(toRadians(L)));

    return {toDegrees(ra + toRadians(dra)) / HOURS_TO_DEG,
            toDegrees(dec + toRadians(ddec))};
}

// ============================================================================
// Precession
// ============================================================================

/**
 * @brief Apply precession from one epoch to another.
 * @param coords Equatorial coordinates.
 * @param fromJD Starting Julian Date.
 * @param toJD Target Julian Date.
 * @return Precessed coordinates.
 */
[[nodiscard]] inline EquatorialCoords applyPrecession(
    const EquatorialCoords& coords, double fromJD, double toJD) {
    double T0 = centuriesSinceJ2000(fromJD);
    double T = (toJD - fromJD) / JULIAN_CENTURY;

    // Precession angles in arcseconds
    double zeta = (2306.2181 + 1.39656 * T0 - 0.000139 * T0 * T0) * T +
                  (0.30188 - 0.000344 * T0) * T * T + 0.017998 * T * T * T;

    double z = (2306.2181 + 1.39656 * T0 - 0.000139 * T0 * T0) * T +
               (1.09468 + 0.000066 * T0) * T * T + 0.018203 * T * T * T;

    double theta = (2004.3109 - 0.85330 * T0 - 0.000217 * T0 * T0) * T -
                   (0.42665 + 0.000217 * T0) * T * T - 0.041833 * T * T * T;

    // Convert to radians
    zeta = toRadians(zeta / 3600.0);
    z = toRadians(z / 3600.0);
    theta = toRadians(theta / 3600.0);

    double ra = toRadians(coords.ra * HOURS_TO_DEG);
    double dec = toRadians(coords.dec);

    // Apply rotation
    double A = std::cos(dec) * std::sin(ra + zeta);
    double B = std::cos(theta) * std::cos(dec) * std::cos(ra + zeta) -
               std::sin(theta) * std::sin(dec);
    double C = std::sin(theta) * std::cos(dec) * std::cos(ra + zeta) +
               std::cos(theta) * std::sin(dec);

    double newRA = std::atan2(A, B) + z;
    double newDec = std::asin(std::clamp(C, -1.0, 1.0));

    return {normalizeRA(toDegrees(newRA) / HOURS_TO_DEG), toDegrees(newDec)};
}

/**
 * @brief Calculate precession matrix for epoch transformation.
 * @tparam T Floating-point type.
 * @param epoch Target epoch as Julian Date.
 * @return 3x3 precession matrix.
 */
template <std::floating_point T>
[[nodiscard]] auto calculatePrecessionMatrix(T epoch)
    -> std::array<std::array<T, 3>, 3> {
    T t = (epoch - static_cast<T>(JD_J2000)) / static_cast<T>(JULIAN_CENTURY);

    // Precession angles in arcseconds
    T zeta = (static_cast<T>(2306.2181) + static_cast<T>(1.39656) * t -
              static_cast<T>(0.000139) * t * t) *
             t;
    T z = zeta + static_cast<T>(0.30188) * t * t +
          static_cast<T>(0.017998) * t * t * t;
    T theta = (static_cast<T>(2004.3109) - static_cast<T>(0.85330) * t -
               static_cast<T>(0.000217) * t * t) *
              t;

    // Convert to radians
    constexpr T ARCSEC_TO_RAD =
        static_cast<T>(DEG_TO_RAD) / static_cast<T>(3600.0);
    zeta *= ARCSEC_TO_RAD;
    z *= ARCSEC_TO_RAD;
    theta *= ARCSEC_TO_RAD;

    // Pre-calculate trig functions
    T cosZeta = std::cos(zeta);
    T sinZeta = std::sin(zeta);
    T cosTheta = std::cos(theta);
    T sinTheta = std::sin(theta);
    T cosZ = std::cos(z);
    T sinZ = std::sin(z);

    // Build precession matrix
    return {{{cosZeta * cosTheta * cosZ - sinZeta * sinZ,
              -cosZeta * cosTheta * sinZ - sinZeta * cosZ, -cosZeta * sinTheta},
             {sinZeta * cosTheta * cosZ + cosZeta * sinZ,
              -sinZeta * cosTheta * sinZ + cosZeta * cosZ, -sinZeta * sinTheta},
             {sinTheta * cosZ, -sinTheta * sinZ, cosTheta}}};
}

// ============================================================================
// Combined Transformations
// ============================================================================

/**
 * @brief Convert observed coordinates to J2000.0.
 * @param coords Observed equatorial coordinates.
 * @param jd Julian Date of observation.
 * @return J2000.0 coordinates.
 */
[[nodiscard]] inline EquatorialCoords observedToJ2000(
    const EquatorialCoords& coords, double jd) {
    auto temp = applyAberration(coords, jd);
    temp = applyNutation(temp, jd, true);
    return applyPrecession(temp, jd, JD_J2000);
}

/**
 * @brief Convert J2000.0 coordinates to observed.
 * @param coords J2000.0 equatorial coordinates.
 * @param jd Julian Date for observation.
 * @return Observed coordinates.
 */
[[nodiscard]] inline EquatorialCoords j2000ToObserved(
    const EquatorialCoords& coords, double jd) {
    auto temp = applyPrecession(coords, JD_J2000, jd);
    temp = applyNutation(temp, jd, false);
    return applyAberration(temp, jd);
}

}  // namespace lithium::tools::calculation

#endif  // LITHIUM_TOOLS_CALCULATION_PRECESSION_HPP
