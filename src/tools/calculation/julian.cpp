/**
 * @file julian.cpp
 * @brief Julian Date calculations implementation.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "julian.hpp"

#include <cmath>

namespace lithium::tools::calculation {

double calculateBJD(double jd, double ra, double dec, double longitude,
                    double latitude, double elevation) {
    using namespace lithium::tools::astronomy;

    // Convert declination to radians (ra, longitude, latitude reserved for future use)
    (void)ra;
    (void)longitude;
    (void)latitude;
    double decRad = toRadians(dec);

    // Calculate light travel time (simplified)
    double earthRadius = EARTH_RADIUS_EQUATORIAL;
    double altitude = elevation + earthRadius;
    double lightTime = altitude * std::sin(decRad) / SPEED_OF_LIGHT;

    // Convert to BJD
    return jd + lightTime / SECONDS_IN_DAY;
}

}  // namespace lithium::tools::calculation
