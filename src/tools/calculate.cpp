#include "calculate.hpp"

#include <spdlog/spdlog.h>
#include <cmath>

namespace lithium::tools {

namespace {
constexpr double DEGREES_IN_CIRCLE = 360.0;
constexpr double RADIANS_TO_DEGREES = 180.0 / std::numbers::pi;
constexpr double DEGREES_TO_RADIANS = std::numbers::pi / 180.0;
constexpr double HOURS_TO_DEGREES = 15.0;  // 1 hour = 15 degrees
}  // namespace

auto calculateVector(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& pointB)
    -> CartesianCoordinates {
    spdlog::info(
        "calculateVector: PointA=({:.6f}, {:.6f}, {:.6f}), PointB=({:.6f}, "
        "{:.6f}, {:.6f})",
        pointA.x, pointA.y, pointA.z, pointB.x, pointB.y, pointB.z);

    CartesianCoordinates vector = {pointB.x - pointA.x, pointB.y - pointA.y,
                                   pointB.z - pointA.z};
    spdlog::info("Vector: x={:.6f}, y={:.6f}, z={:.6f}", vector.x, vector.y,
                 vector.z);
    return vector;
}

auto calculatePointC(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& vectorV)
    -> CartesianCoordinates {
    spdlog::info(
        "calculatePointC: PointA=({:.6f}, {:.6f}, {:.6f}), Vector=({:.6f}, "
        "{:.6f}, {:.6f})",
        pointA.x, pointA.y, pointA.z, vectorV.x, vectorV.y, vectorV.z);

    CartesianCoordinates pointC = {pointA.x + vectorV.x, pointA.y + vectorV.y,
                                   pointA.z + vectorV.z};
    spdlog::info("PointC: x={:.6f}, y={:.6f}, z={:.6f}", pointC.x, pointC.y,
                 pointC.z);
    return pointC;
}

auto calculateFOV(int focalLength, double cameraSizeWidth,
                  double cameraSizeHeight) -> MinMaxFOV {
    spdlog::info(
        "calculateFOV: FocalLength={}, CameraWidth={:.6f}, CameraHeight={:.6f}",
        focalLength, cameraSizeWidth, cameraSizeHeight);

    double cameraSizeDiagonal = std::hypot(cameraSizeWidth, cameraSizeHeight);

    double minFOV = 2.0 * std::atan(cameraSizeHeight / (2.0 * focalLength)) *
                    RADIANS_TO_DEGREES;
    double maxFOV = 2.0 * std::atan(cameraSizeDiagonal / (2.0 * focalLength)) *
                    RADIANS_TO_DEGREES;

    spdlog::info("FOV: Min={:.6f}°, Max={:.6f}°", minFOV, maxFOV);
    return {minFOV, maxFOV};
}

auto calculateGST(const std::tm& date) -> double {
    spdlog::info("calculateGST: Date={:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                 date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                 date.tm_hour, date.tm_min, date.tm_sec);

    constexpr double J2000_EPOCH = 2451545.0;
    constexpr double JULIAN_CENTURY = 36525.0;
    constexpr double SECONDS_IN_DAY = 86400.0;
    constexpr double GST_COEF1 = 280.46061837;
    constexpr double GST_COEF2 = 360.98564736629;
    constexpr double GST_COEF3 = 0.000387933;
    constexpr double GST_COEF4 = 38710000.0;

    std::tm epoch = {0, 0, 12, 1, 0, 100, 0, 0, 0};  // Jan 1, 2000 12:00:00 UTC
    std::time_t epochTime = std::mktime(&epoch);
    std::time_t nowTime = std::mktime(const_cast<std::tm*>(&date));
    double julianDate = J2000_EPOCH + (nowTime - epochTime) / SECONDS_IN_DAY;
    double julianCenturies = (julianDate - J2000_EPOCH) / JULIAN_CENTURY;
    double gst =
        GST_COEF1 + GST_COEF2 * (julianDate - J2000_EPOCH) +
        GST_COEF3 * julianCenturies * julianCenturies -
        (julianCenturies * julianCenturies * julianCenturies / GST_COEF4);
    gst = std::fmod(gst, DEGREES_IN_CIRCLE);
    if (gst < 0) {
        gst += DEGREES_IN_CIRCLE;
    }

    spdlog::info("GST: {:.6f}°", gst);
    return gst;
}

auto calculateAltAz(double rightAscension, double declination, double latitude,
                    double longitude, const std::tm& date) -> AltAz {
    spdlog::info(
        "calculateAltAz: RA={:.6f}°, Dec={:.6f}°, Lat={:.6f}°, Lon={:.6f}°, "
        "Date={:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
        rightAscension, declination, latitude, longitude, date.tm_year + 1900,
        date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

    // Convert input values to radians
    double raRad = (rightAscension * HOURS_TO_DEGREES) *
                   DEGREES_TO_RADIANS;  // RA from hours to radians
    double decRad = declination * DEGREES_TO_RADIANS;
    double latRad = latitude * DEGREES_TO_RADIANS;

    // Calculate GST and LST
    double gst = calculateGST(date);
    double lst = std::fmod(gst + longitude,
                           DEGREES_IN_CIRCLE);  // LST in [0, 360) degrees
    double hourAngleRad =
        (lst * DEGREES_TO_RADIANS) - raRad;  // Hour angle in radians

    // Calculate altitude
    double sinAlt =
        std::sin(decRad) * std::sin(latRad) +
        std::cos(decRad) * std::cos(latRad) * std::cos(hourAngleRad);
    // Clamp sinAlt to [-1, 1] to avoid domain errors in asin
    sinAlt = std::max(-1.0, std::min(1.0, sinAlt));
    double altRad = std::asin(sinAlt);
    double altDeg = altRad * RADIANS_TO_DEGREES;

    // Calculate azimuth
    double cosAz = (std::sin(decRad) - std::sin(altRad) * std::sin(latRad)) /
                   (std::cos(altRad) * std::cos(latRad));
    // Clamp cosAz to [-1, 1] to avoid domain errors in acos
    cosAz = std::max(-1.0, std::min(1.0, cosAz));

    double azRad = std::acos(cosAz);
    double azDeg = azRad * RADIANS_TO_DEGREES;

    // Adjust azimuth based on hour angle
    if (std::sin(hourAngleRad) > 0) {
        azDeg = DEGREES_IN_CIRCLE - azDeg;
    }

    spdlog::info("AltAz: Alt={:.6f}°, Az={:.6f}°", altDeg, azDeg);
    return {altDeg, azDeg};
}

}  // namespace lithium::tools
