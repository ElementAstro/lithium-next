#include "convert.hpp"
#include "constant.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace lithium::tools {

namespace {
// Mathematical constants
constexpr double PI = std::numbers::pi;
constexpr double TWO_PI = 2.0 * PI;
constexpr double HALF_PI = PI / 2.0;
constexpr double DEGREES_IN_CIRCLE = 360.0;
constexpr double HOURS_IN_DAY = 24.0;
constexpr double MINUTES_IN_HOUR = 60.0;
constexpr double SECONDS_IN_MINUTE = 60.0;
constexpr double SECONDS_IN_HOUR = MINUTES_IN_HOUR * SECONDS_IN_MINUTE;
constexpr double DEGREES_TO_RADIANS = PI / 180.0;
constexpr double RADIANS_TO_DEGREES = 180.0 / PI;
constexpr double HOURS_TO_DEGREES = 15.0;
constexpr double DEGREES_TO_HOURS = 1.0 / HOURS_TO_DEGREES;
constexpr double EPSILON = 1.0e-10;
}  // namespace

auto rangeTo(double value, double maxVal, double minVal) -> double {
    spdlog::info("rangeTo: value={:.6f}, max={:.6f}, min={:.6f}", value, maxVal,
                 minVal);

    double period = maxVal - minVal;

    // Faster modulo calculation for range wrapping
    if (period > 0) {
        value = std::fmod(value - minVal, period);
        if (value < 0) {
            value += period;
        }
        value += minVal;

        // Handle potential floating-point rounding issues
        if (std::abs(value - maxVal) < EPSILON) {
            value = minVal;
        }
    }

    spdlog::info("Final value: {:.6f}", value);
    return value;
}

auto degreeToRad(double degree) -> double {
    double radians = degree * DEGREES_TO_RADIANS;
    spdlog::info("degreeToRad: {:.6f}° -> {:.6f} rad", degree, radians);
    return radians;
}

auto radToDegree(double radians) -> double {
    double degrees = radians * RADIANS_TO_DEGREES;
    spdlog::info("radToDegree: {:.6f} rad -> {:.6f}°", radians, degrees);
    return degrees;
}

auto hourToDegree(double hours) -> double {
    double degrees = hours * HOURS_TO_DEGREES;
    degrees = rangeTo(degrees, DEGREES_IN_CIRCLE, 0.0);
    spdlog::info("hourToDegree: {:.6f}h -> {:.6f}°", hours, degrees);
    return degrees;
}

auto hourToRad(double hours) -> double {
    double degrees = hours * HOURS_TO_DEGREES;
    double radians = degreeToRad(degrees);
    spdlog::info("hourToRad: {:.6f}h -> {:.6f} rad", hours, radians);
    return radians;
}

auto degreeToHour(double degrees) -> double {
    double hours = degrees * DEGREES_TO_HOURS;
    hours = rangeTo(hours, HOURS_IN_DAY, 0.0);
    spdlog::info("degreeToHour: {:.6f}° -> {:.6f}h", degrees, hours);
    return hours;
}

auto radToHour(double radians) -> double {
    double degrees = radToDegree(radians);
    double hours = degreeToHour(degrees);
    spdlog::info("radToHour: {:.6f} rad -> {:.6f}h", radians, hours);
    return hours;
}

auto getHaDegree(double rightAscensionRad, double lstDegree) -> double {
    double rightAscensionDeg = radToDegree(rightAscensionRad);
    double hourAngle = lstDegree - rightAscensionDeg;
    hourAngle = rangeTo(hourAngle, DEGREES_IN_CIRCLE, 0.0);

    spdlog::info(
        "getHaDegree: RA={:.6f} rad ({:.6f}°), LST={:.6f}° -> HA={:.6f}°",
        rightAscensionRad, rightAscensionDeg, lstDegree, hourAngle);
    return hourAngle;
}

auto raDecToAltAz(double hourAngleRad, double declinationRad,
                  double latitudeRad) -> std::vector<double> {
    spdlog::info(
        "raDecToAltAz input: HA={:.6f} rad, Dec={:.6f} rad, Lat={:.6f} rad",
        hourAngleRad, declinationRad, latitudeRad);

    // Pre-calculate trigonometric values for performance
    double sinLat = std::sin(latitudeRad);
    double cosLat = std::cos(latitudeRad);
    double sinDec = std::sin(declinationRad);
    double cosDec = std::cos(declinationRad);
    double cosHA = std::cos(hourAngleRad);
    double sinHA = std::sin(hourAngleRad);

    // Calculate altitude
    double sinAlt = sinLat * sinDec + cosLat * cosDec * cosHA;
    // Clamp to prevent numerical errors
    sinAlt = std::clamp(sinAlt, -1.0, 1.0);
    double altitudeRad = std::asin(sinAlt);

    // Calculate azimuth
    double azimuthRad;
    if (std::abs(cosLat) < EPSILON) {
        // Near the poles, azimuth is less defined and depends on hour angle
        azimuthRad = hourAngleRad;
        spdlog::info("Polar case detected, using HA as azimuth");
    } else {
        // Calculate azimuth
        double num = sinDec - sinAlt * sinLat;
        double den = cosDec * cosHA * cosLat;

        // Handle numerical instability when den approaches 0
        if (std::abs(den) < EPSILON) {
            azimuthRad = sinHA >= 0 ? HALF_PI : 3 * HALF_PI;
        } else {
            double cosAz = num / (cosLat * std::cos(altitudeRad));
            // Clamp to prevent numerical errors
            cosAz = std::clamp(cosAz, -1.0, 1.0);
            azimuthRad = std::acos(cosAz);

            // Adjust for the correct quadrant
            if (sinHA > 0) {
                azimuthRad = TWO_PI - azimuthRad;
            }
        }

        spdlog::info("Calculated azimuth: {:.6f} rad", azimuthRad);
    }

    spdlog::info("raDecToAltAz output: Alt={:.6f} rad, Az={:.6f} rad",
                 altitudeRad, azimuthRad);
    return {altitudeRad, azimuthRad};
}

void altAzToRaDec(double altRadian, double azRadian, double& hrRadian,
                  double& decRadian, double latRadian) {
    spdlog::info(
        "altAzToRaDec input: Alt={:.6f} rad, Az={:.6f} rad, Lat={:.6f} rad",
        altRadian, azRadian, latRadian);

    // Pre-calculate trigonometric values for performance
    double sinLat = std::sin(latRadian);
    double cosLat = std::cos(latRadian);
    double sinAlt = std::sin(altRadian);
    double cosAlt = std::cos(altRadian);
    double sinAz = std::sin(azRadian);
    double cosAz = std::cos(azRadian);

    // Normalize altitude to handle extreme cases
    if (altRadian > HALF_PI) {
        altRadian = PI - altRadian;
        azRadian += PI;
        sinAlt = std::sin(altRadian);
        cosAlt = std::cos(altRadian);
        sinAz = std::sin(azRadian);
        cosAz = std::cos(azRadian);
    }
    if (altRadian < -HALF_PI) {
        altRadian = -PI - altRadian;
        azRadian -= PI;
        sinAlt = std::sin(altRadian);
        cosAlt = std::cos(altRadian);
        sinAz = std::sin(azRadian);
        cosAz = std::cos(azRadian);
    }

    // Calculate declination
    double sinDec = sinLat * sinAlt + cosLat * cosAlt * cosAz;
    // Clamp to prevent numerical errors
    sinDec = std::clamp(sinDec, -1.0, 1.0);
    decRadian = std::asin(sinDec);

    // Calculate hour angle
    if (std::abs(cosLat) < EPSILON) {
        // Near poles, hour angle is determined mainly by azimuth
        hrRadian = azRadian + PI;
    } else {
        double cosDec = std::cos(decRadian);
        // Handle potential division by zero
        if (std::abs(cosDec) < EPSILON || std::abs(cosLat) < EPSILON) {
            hrRadian = 0.0;  // Default value when division would be unstable
        } else {
            double temp = (sinAlt - sinLat * sinDec) / (cosLat * cosDec);
            // Clamp to prevent numerical errors
            temp = std::clamp(temp, -1.0, 1.0);
            double ha = std::acos(temp);
            // Adjust based on azimuth
            hrRadian = sinAz > 0.0 ? TWO_PI - ha : ha;
        }
    }

    // Normalize hour angle
    hrRadian = rangeTo(hrRadian, TWO_PI, 0.0);

    spdlog::info("altAzToRaDec output: HR={:.6f} rad, Dec={:.6f} rad", hrRadian,
                 decRadian);
}

auto convertEquatorialToCartesian(double rightAscension, double declination,
                                  double radius) -> CartesianCoordinates {
    spdlog::info(
        "convertEquatorialToCartesian: RA={:.6f}°, Dec={:.6f}°, Radius={:.6f}",
        rightAscension, declination, radius);

    double raRad = degreeToRad(rightAscension);
    double decRad = degreeToRad(declination);

    // Pre-calculate trigonometric values
    double cosDec = std::cos(decRad);
    double sinDec = std::sin(decRad);
    double cosRA = std::cos(raRad);
    double sinRA = std::sin(raRad);

    // Calculate Cartesian coordinates
    double x = radius * cosDec * cosRA;
    double y = radius * cosDec * sinRA;
    double z = radius * sinDec;

    spdlog::info("Cartesian coordinates: x={:.6f}, y={:.6f}, z={:.6f}", x, y,
                 z);
    return {x, y, z};
}

auto convertToSphericalCoordinates(const CartesianCoordinates& cartesianPoint)
    -> std::optional<SphericalCoordinates> {
    spdlog::info(
        "convertToSphericalCoordinates: Cartesian=({:.6f}, {:.6f}, {:.6f})",
        cartesianPoint.x, cartesianPoint.y, cartesianPoint.z);

    double x = cartesianPoint.x;
    double y = cartesianPoint.y;
    double z = cartesianPoint.z;

    // Calculate radius squared first to avoid a square root if not needed
    double radiusSquared = x * x + y * y + z * z;

    // Check if the point is at the origin
    if (radiusSquared < EPSILON) {
        spdlog::warn("Point is at origin (or very close), returning nullopt");
        return std::nullopt;
    }

    double radius = std::sqrt(radiusSquared);

    // Calculate declination with bounds check
    double declination =
        std::asin(std::clamp(z / radius, -1.0, 1.0)) * RADIANS_TO_DEGREES;

    // Calculate right ascension
    double rightAscension;
    if (std::abs(x) < EPSILON && std::abs(y) < EPSILON) {
        // Special case: point is on the Z-axis
        rightAscension = 0.0;
    } else {
        rightAscension = std::atan2(y, x) * RADIANS_TO_DEGREES;
        // Normalize to [0, 360)
        if (rightAscension < 0) {
            rightAscension += DEGREES_IN_CIRCLE;
        }
    }

    spdlog::info("Spherical coordinates: RA={:.6f}°, Dec={:.6f}°",
                 rightAscension, declination);
    return SphericalCoordinates{rightAscension, declination};
}

auto dmsToDegree(int degrees, int minutes, double seconds) -> double {
    spdlog::info("dmsToDegree: Degrees={}, Minutes={}, Seconds={:.6f}", degrees,
                 minutes, seconds);

    // Determine sign
    double sign =
        degrees < 0 ||
                (degrees == 0 && (minutes < 0 || (minutes == 0 && seconds < 0)))
            ? -1.0
            : 1.0;

    // Calculate absolute values
    int absDegrees = std::abs(degrees);
    int absMinutes = std::abs(minutes);
    double absSeconds = std::abs(seconds);

    // Handle potential overflow in seconds and minutes
    if (absSeconds >= SECONDS_IN_MINUTE) {
        absMinutes += static_cast<int>(absSeconds / SECONDS_IN_MINUTE);
        absSeconds = std::fmod(absSeconds, SECONDS_IN_MINUTE);
    }

    if (absMinutes >= MINUTES_IN_HOUR) {
        absDegrees += absMinutes / static_cast<int>(MINUTES_IN_HOUR);
        absMinutes %= static_cast<int>(MINUTES_IN_HOUR);
    }

    // Calculate decimal degrees
    double result = sign * (absDegrees + absMinutes / MINUTES_IN_HOUR +
                            absSeconds / SECONDS_IN_HOUR);

    spdlog::info("Result: {:.6f}°", result);
    return result;
}

auto radToDmsStr(double radians) -> std::string {
    spdlog::info("radToDmsStr: Input radians={:.6f}", radians);

    // Convert radians to degrees
    double degrees = radToDegree(radians);

    // Determine sign
    char sign = degrees < 0 ? '-' : '+';
    degrees = std::abs(degrees);

    // Extract degrees, minutes, seconds
    int deg = static_cast<int>(degrees);
    double minPartial = (degrees - deg) * MINUTES_IN_HOUR;
    int min = static_cast<int>(minPartial);
    double sec = (minPartial - min) * SECONDS_IN_MINUTE;

    // Handle rounding errors
    if (sec >= SECONDS_IN_MINUTE - EPSILON) {
        sec = 0.0;
        min++;
        if (min >= MINUTES_IN_HOUR) {
            min = 0;
            deg++;
        }
    }

    // Format output
    std::stringstream ss;
    ss << sign << std::setfill('0') << std::setw(2) << deg << "°"
       << std::setfill('0') << std::setw(2) << min << "'" << std::fixed
       << std::setprecision(1) << sec << "\"";

    std::string result = ss.str();
    spdlog::info("radToDmsStr: Output={}", result);
    return result;
}

auto radToHmsStr(double radians) -> std::string {
    spdlog::info("radToHmsStr: Input radians={:.6f}", radians);

    // Convert radians to hours
    double hours = radToHour(radians);

    // Ensure hours is in [0, 24) range
    hours = rangeTo(hours, HOURS_IN_DAY, 0.0);

    // Extract hours, minutes, seconds
    int hrs = static_cast<int>(hours);
    double minPartial = (hours - hrs) * MINUTES_IN_HOUR;
    int min = static_cast<int>(minPartial);
    double sec = (minPartial - min) * SECONDS_IN_MINUTE;

    // Handle rounding errors
    if (sec >= SECONDS_IN_MINUTE - EPSILON) {
        sec = 0.0;
        min++;
        if (min >= MINUTES_IN_HOUR) {
            min = 0;
            hrs++;
            if (hrs >= HOURS_IN_DAY) {
                hrs = 0;
            }
        }
    }

    // Format output
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hrs << ':' << std::setfill('0')
       << std::setw(2) << min << ':' << std::fixed << std::setprecision(1)
       << std::setfill('0') << std::setw(4) << sec;

    std::string result = ss.str();
    spdlog::info("radToHmsStr: Output={}", result);
    return result;
}

}  // namespace lithium::tools