/**
 * @file coordinate.hpp
 * @brief Coordinate conversion utilities.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CONVERSION_COORDINATE_HPP
#define LITHIUM_TOOLS_CONVERSION_COORDINATE_HPP

#include <cmath>
#include <optional>
#include <vector>

#include "angle.hpp"
#include "tools/astronomy/constants.hpp"

namespace lithium::tools::conversion {

using namespace lithium::tools::astronomy;

// ============================================================================
// Coordinate Structures
// ============================================================================

/**
 * @struct CartesianCoordinates
 * @brief 3D Cartesian coordinates.
 */
struct alignas(32) CartesianCoordinates {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    CartesianCoordinates() = default;
    CartesianCoordinates(double x_, double y_, double z_)
        : x(x_), y(y_), z(z_) {}

    [[nodiscard]] double magnitude() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    CartesianCoordinates operator+(const CartesianCoordinates& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    CartesianCoordinates operator-(const CartesianCoordinates& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
};

/**
 * @struct SphericalCoordinates
 * @brief Spherical coordinates (RA/Dec in degrees).
 */
struct alignas(16) SphericalCoordinates {
    double rightAscension{0.0};  ///< Right Ascension in degrees
    double declination{0.0};     ///< Declination in degrees

    SphericalCoordinates() = default;
    SphericalCoordinates(double ra, double dec)
        : rightAscension(ra), declination(dec) {}
};

// ============================================================================
// Coordinate Conversions
// ============================================================================

/**
 * @brief Convert equatorial to Cartesian coordinates.
 * @param ra Right Ascension in degrees.
 * @param dec Declination in degrees.
 * @param radius Distance (default 1.0 for unit sphere).
 * @return Cartesian coordinates.
 */
[[nodiscard]] inline CartesianCoordinates equatorialToCartesian(
    double ra, double dec, double radius = 1.0) noexcept {
    double raRad = degreeToRad(ra);
    double decRad = degreeToRad(dec);

    return {radius * std::cos(decRad) * std::cos(raRad),
            radius * std::cos(decRad) * std::sin(raRad),
            radius * std::sin(decRad)};
}

/**
 * @brief Convert Cartesian to spherical coordinates.
 * @param cart Cartesian coordinates.
 * @return Spherical coordinates, or nullopt if at origin.
 */
[[nodiscard]] inline std::optional<SphericalCoordinates> cartesianToSpherical(
    const CartesianCoordinates& cart) noexcept {
    double r = cart.magnitude();
    if (r < EPSILON) {
        return std::nullopt;
    }

    double dec = radToDegree(std::asin(cart.z / r));
    double ra = radToDegree(std::atan2(cart.y, cart.x));

    // Normalize RA to [0, 360)
    if (ra < 0) {
        ra += 360.0;
    }

    return SphericalCoordinates{ra, dec};
}

/**
 * @brief Convert RA/Dec to Alt/Az (returns vector [alt, az] in radians).
 * @param hourAngleRad Hour angle in radians.
 * @param declinationRad Declination in radians.
 * @param latitudeRad Observer latitude in radians.
 * @return Vector containing [altitude, azimuth] in radians.
 */
[[nodiscard]] inline std::vector<double> raDecToAltAz(double hourAngleRad,
                                                      double declinationRad,
                                                      double latitudeRad) {
    double sinDec = std::sin(declinationRad);
    double cosDec = std::cos(declinationRad);
    double sinLat = std::sin(latitudeRad);
    double cosLat = std::cos(latitudeRad);
    double cosHA = std::cos(hourAngleRad);
    double sinHA = std::sin(hourAngleRad);

    // Calculate altitude
    double sinAlt = sinDec * sinLat + cosDec * cosLat * cosHA;
    double altitude = std::asin(std::clamp(sinAlt, -1.0, 1.0));

    // Calculate azimuth
    double cosAz =
        (sinDec - std::sin(altitude) * sinLat) / (std::cos(altitude) * cosLat);
    double azimuth = std::acos(std::clamp(cosAz, -1.0, 1.0));

    if (sinHA > 0) {
        azimuth = K_TWO_PI - azimuth;
    }

    return {altitude, azimuth};
}

/**
 * @brief Convert Alt/Az to RA/Dec.
 * @param altRad Altitude in radians.
 * @param azRad Azimuth in radians.
 * @param[out] haRad Hour angle in radians.
 * @param[out] decRad Declination in radians.
 * @param latRad Observer latitude in radians.
 */
inline void altAzToRaDec(double altRad, double azRad, double& haRad,
                         double& decRad, double latRad) noexcept {
    double sinAlt = std::sin(altRad);
    double cosAlt = std::cos(altRad);
    double sinAz = std::sin(azRad);
    double cosAz = std::cos(azRad);
    double sinLat = std::sin(latRad);
    double cosLat = std::cos(latRad);

    // Calculate declination
    double sinDec = sinAlt * sinLat + cosAlt * cosLat * cosAz;
    decRad = std::asin(std::clamp(sinDec, -1.0, 1.0));

    // Calculate hour angle
    double cosHA =
        (sinAlt - sinLat * std::sin(decRad)) / (cosLat * std::cos(decRad));
    haRad = std::acos(std::clamp(cosHA, -1.0, 1.0));

    if (sinAz > 0) {
        haRad = K_TWO_PI - haRad;
    }
}

/**
 * @brief Calculate hour angle in degrees.
 * @param raRad Right Ascension in radians.
 * @param lstDeg Local Sidereal Time in degrees.
 * @return Hour angle in degrees.
 */
[[nodiscard]] inline double getHaDegree(double raRad, double lstDeg) noexcept {
    double raDeg = radToDegree(raRad);
    double ha = lstDeg - raDeg;
    return normalizeAngle180(ha);
}

// ============================================================================
// Vector Operations
// ============================================================================

/**
 * @brief Calculate vector from point A to point B.
 * @param pointA Starting point.
 * @param pointB Ending point.
 * @return Vector from A to B.
 */
[[nodiscard]] inline CartesianCoordinates calculateVector(
    const CartesianCoordinates& pointA,
    const CartesianCoordinates& pointB) noexcept {
    return pointB - pointA;
}

/**
 * @brief Calculate point C given point A and vector V.
 * @param pointA Starting point.
 * @param vectorV Vector to add.
 * @return Resulting point.
 */
[[nodiscard]] inline CartesianCoordinates calculatePointC(
    const CartesianCoordinates& pointA,
    const CartesianCoordinates& vectorV) noexcept {
    return pointA + vectorV;
}

}  // namespace lithium::tools::conversion

#endif  // LITHIUM_TOOLS_CONVERSION_COORDINATE_HPP
