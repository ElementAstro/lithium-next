#include "calculate.hpp"
#include "constant.hpp"

#include <cmath>

#include "atom/log/loguru.hpp"

namespace lithium::tools {
constexpr double K_GST_COEFF1 = 280.46061837;
constexpr double K_GST_COEFF2 = 360.98564736629;
constexpr double K_GST_COEFF3 = 0.000387933;
constexpr double K_GST_COEFF4 = 38710000.0;
constexpr double K_J2000_EPOCH = 2451545.0;
constexpr double K_JULIAN_CENTURY = 36525.0;
constexpr double K_SECONDS_IN_DAY = 86400.0;

auto calculateVector(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& pointB)
    -> CartesianCoordinates {
    LOG_F(INFO,
          "calculateVector: PointA=({:.6f}, {:.6f}, {:.6f}), PointB=({:.6f}, "
          "{:.6f}, {:.6f})",
          pointA.x, pointA.y, pointA.z, pointB.x, pointB.y, pointB.z);

    CartesianCoordinates vector = {pointB.x - pointA.x, pointB.y - pointA.y,
                                   pointB.z - pointA.z};
    LOG_F(INFO, "Vector: x={:.6f}, y={:.6f}, z={:.6f}", vector.x, vector.y,
          vector.z);
    return vector;
}

auto calculatePointC(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& vectorV)
    -> CartesianCoordinates {
    LOG_F(INFO,
          "calculatePointC: PointA=({:.6f}, {:.6f}, {:.6f}), Vector=({:.6f}, "
          "{:.6f}, {:.6f})",
          pointA.x, pointA.y, pointA.z, vectorV.x, vectorV.y, vectorV.z);

    CartesianCoordinates pointC = {pointA.x + vectorV.x, pointA.y + vectorV.y,
                                   pointA.z + vectorV.z};
    LOG_F(INFO, "PointC: x={:.6f}, y={:.6f}, z={:.6f}", pointC.x, pointC.y,
          pointC.z);
    return pointC;
}

auto calculateFOV(int focalLength, double cameraSizeWidth,
                  double cameraSizeHeight) -> MinMaxFOV {
    LOG_F(
        INFO,
        "calculateFOV: FocalLength={}, CameraWidth={:.6f}, CameraHeight={:.6f}",
        focalLength, cameraSizeWidth, cameraSizeHeight);

    double cameraSizeDiagonal = std::hypot(cameraSizeWidth, cameraSizeHeight);

    double minFOV = 2 * std::atan(cameraSizeHeight / (2.0 * focalLength)) *
                    K_RADIANS_TO_DEGREES;
    double maxFOV = 2 * std::atan(cameraSizeDiagonal / (2.0 * focalLength)) *
                    K_RADIANS_TO_DEGREES;

    LOG_F(INFO, "FOV: Min={:.6f}°, Max={:.6f}°", minFOV, maxFOV);
    return {minFOV, maxFOV};
}

auto calculateGST(const std::tm& date) -> double {
    LOG_F(INFO, "calculateGST: Date={:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
          date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour,
          date.tm_min, date.tm_sec);

    std::tm epoch = {0, 0, 12, 1, 0, 100, 0, 0, 0};  // Jan 1, 2000 12:00:00 UTC
    std::time_t epochTime = std::mktime(&epoch);
    std::time_t nowTime = std::mktime(const_cast<std::tm*>(&date));
    double julianDate =
        K_J2000_EPOCH + (nowTime - epochTime) / K_SECONDS_IN_DAY;
    double julianCenturies = (julianDate - K_J2000_EPOCH) / K_JULIAN_CENTURY;
    double gst =
        K_GST_COEFF1 + K_GST_COEFF2 * (julianDate - K_J2000_EPOCH) +
        K_GST_COEFF3 * julianCenturies * julianCenturies -
        (julianCenturies * julianCenturies * julianCenturies / K_GST_COEFF4);
    gst = std::fmod(gst, K_DEGREES_IN_CIRCLE);

    LOG_F(INFO, "GST: {:.6f}°", gst);
    return gst;
}

auto calculateAltAz(double rightAscension, double declination, double latitude,
                    double longitude, const std::tm& date) -> AltAz {
    LOG_F(INFO,
          "calculateAltAz: RA={:.6f}°, Dec={:.6f}°, Lat={:.6f}°, Lon={:.6f}°, "
          "Date={:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
          rightAscension, declination, latitude, longitude, date.tm_year + 1900,
          date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min,
          date.tm_sec);

    // 将输入值转换为弧度
    double raRad = degreeToRad(rightAscension *
                               K_HOURS_TO_DEGREES);  // 赤经转换为弧度并乘以15
    double decRad = degreeToRad(declination);
    double latRad = degreeToRad(latitude);
    double lonRad = degreeToRad(longitude);

    // 计算GST和LST
    double gst = calculateGST(date);
    double lst =
        std::fmod(gst + longitude, K_DEGREES_IN_CIRCLE);  // LST在0-360度范围内
    double hourAngle = degreeToRad(lst) - raRad;          // 时角

    // 计算高度角
    double altRad =
        std::asin(std::sin(decRad) * std::sin(latRad) +
                  std::cos(decRad) * std::cos(latRad) * std::cos(hourAngle));
    double altDeg = radToDegree(altRad);

    // 计算方位角
    double cosAz = (std::sin(decRad) - std::sin(altRad) * std::sin(latRad)) /
                   (std::cos(altRad) * std::cos(latRad));
    double azRad = std::acos(cosAz);
    double azDeg = radToDegree(azRad);

    // 调整方位角
    if (std::sin(hourAngle) > 0) {
        azDeg = K_DEGREES_IN_CIRCLE - azDeg;
    }

    LOG_F(INFO, "AltAz: Alt={:.6f}°, Az={:.6f}°", altDeg, azDeg);
    return {altDeg, azDeg};
}
}  // namespace lithium::tools