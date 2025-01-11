#include "convert.hpp"
#include "constant.hpp"

#include "atom/log/loguru.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>

namespace lithium::tools {
auto rangeTo(double value, double maxVal, double minVal) -> double {
    LOG_F(INFO, "rangeTo: value={:.6f}, max={:.6f}, min={:.6f}", value, maxVal,
          minVal);

    double period = maxVal - minVal;

    while (value < minVal) {
        value += period;
        LOG_F(INFO, "Adjusted value up: {:.6f}", value);
    }

    while (value > maxVal) {
        value -= period;
        LOG_F(INFO, "Adjusted value down: {:.6f}", value);
    }

    LOG_F(INFO, "Final value: {:.6f}", value);
    return value;
}

auto degreeToRad(double degree) -> double {
    double radians = degree * K_DEGREES_TO_RADIANS;
    LOG_F(INFO, "degreeToRad: {:.6f}° -> {:.6f} rad", degree, radians);
    return radians;
}

auto radToDegree(double radians) -> double {
    double degrees = radians * K_RADIANS_TO_DEGREES;
    LOG_F(INFO, "radToDegree: {:.6f} rad -> {:.6f}°", radians, degrees);
    return degrees;
}

auto hourToDegree(double hours) -> double {
    double degrees = hours * K_HOURS_TO_DEGREES;
    degrees = rangeTo(degrees, K_DEGREES_IN_CIRCLE, 0.0);
    LOG_F(INFO, "hourToDegree: {:.6f}h -> {:.6f}°", hours, degrees);
    return degrees;
}

auto hourToRad(double hours) -> double {
    double degrees = hours * K_HOURS_TO_DEGREES;
    degrees = rangeTo(degrees, K_DEGREES_IN_CIRCLE, 0.0);
    double radians = degreeToRad(degrees);
    LOG_F(INFO, "hourToRad: {:.6f}h -> {:.6f} rad", hours, radians);
    return radians;
}

auto degreeToHour(double degrees) -> double {
    double hours = degrees / K_HOURS_TO_DEGREES;
    hours = rangeTo(hours, K_HOURS_IN_DAY, 0.0);
    LOG_F(INFO, "degreeToHour: {:.6f}° -> {:.6f}h", degrees, hours);
    return hours;
}

auto radToHour(double radians) -> double {
    double degrees = radToDegree(radians);
    degrees = rangeTo(degrees, K_DEGREES_IN_CIRCLE, 0.0);
    double hours = degreeToHour(degrees);
    LOG_F(INFO, "radToHour: {:.6f} rad -> {:.6f}h", radians, hours);
    return hours;
}

auto getHaDegree(double rightAscensionRad, double lstDegree) -> double {
    double hourAngle = lstDegree - radToDegree(rightAscensionRad);
    hourAngle = rangeTo(hourAngle, K_DEGREES_IN_CIRCLE, 0.0);
    LOG_F(INFO, "getHaDegree: RA={:.6f} rad, LST={:.6f}° -> HA={:.6f}°",
          rightAscensionRad, lstDegree, hourAngle);
    return hourAngle;
}

auto raDecToAltAz(double hourAngleRad, double declinationRad,
                  double latitudeRad) -> std::vector<double> {
    LOG_F(INFO,
          "raDecToAltAz input: HA={:.6f} rad, Dec={:.6f} rad, Lat={:.6f} rad",
          hourAngleRad, declinationRad, latitudeRad);

    double cosLatitude = std::cos(latitudeRad);

    auto altitudeRad = std::asin(
        std::sin(latitudeRad) * std::sin(declinationRad) +
        cosLatitude * std::cos(declinationRad) * std::cos(hourAngleRad));

    double azimuthRad;
    if (cosLatitude < K_EPSILON_VALUE) {
        azimuthRad = hourAngleRad;  // polar case
        LOG_F(INFO, "Polar case detected, using HA as azimuth");
    } else {
        double temp =
            std::acos((std::sin(declinationRad) -
                       std::sin(altitudeRad) * std::sin(latitudeRad)) /
                      (std::cos(altitudeRad) * cosLatitude));

        azimuthRad = std::sin(hourAngleRad) > 0 ? 2 * M_PI - temp : temp;
        LOG_F(INFO, "Calculated azimuth temp={:.6f}, final={:.6f} rad", temp,
              azimuthRad);
    }

    LOG_F(INFO, "raDecToAltAz output: Alt={:.6f} rad, Az={:.6f} rad",
          altitudeRad, azimuthRad);
    return {altitudeRad, azimuthRad};
}

void altAzToRaDec(double altRadian, double azRadian, double& hrRadian,
                  double& decRadian, double latRadian) {
    LOG_F(INFO,
          "altAzToRaDec input: Alt={:.6f} rad, Az={:.6f} rad, Lat={:.6f} rad",
          altRadian, azRadian, latRadian);

    double cosLatitude = std::cos(latRadian);
    if (altRadian > M_PI / 2.0) {
        altRadian = M_PI - altRadian;
        azRadian += M_PI;
    }
    if (altRadian < -M_PI / 2.0) {
        altRadian = -M_PI - altRadian;
        azRadian -= M_PI;
    }
    double sinDec = std::sin(latRadian) * std::sin(altRadian) +
                    cosLatitude * std::cos(altRadian) * std::cos(azRadian);
    decRadian = std::asin(sinDec);
    if (cosLatitude < K_EPSILON_VALUE) {
        hrRadian = azRadian + M_PI;
    } else {
        double temp = cosLatitude * std::cos(decRadian);
        temp = (std::sin(altRadian) - std::sin(latRadian) * sinDec) / temp;
        temp = std::acos(std::clamp(-temp, -1.0, 1.0));
        if (std::sin(azRadian) > 0.0) {
            hrRadian = M_PI + temp;
        } else {
            hrRadian = M_PI - temp;
        }
    }
    LOG_F(INFO, "altAzToRaDec output: HR={:.6f} rad, Dec={:.6f} rad", hrRadian,
          decRadian);
}

auto convertEquatorialToCartesian(double rightAscension, double declination,
                                  double radius) -> CartesianCoordinates {
    LOG_F(
        INFO,
        "convertEquatorialToCartesian: RA={:.6f}°, Dec={:.6f}°, Radius={:.6f}",
        rightAscension, declination, radius);

    double raRad = degreeToRad(rightAscension);
    double decRad = degreeToRad(declination);

    double x = radius * std::cos(decRad) * std::cos(raRad);
    double y = radius * std::cos(decRad) * std::sin(raRad);
    double z = radius * std::sin(decRad);

    LOG_F(INFO, "Cartesian coordinates: x={:.6f}, y={:.6f}, z={:.6f}", x, y, z);
    return {x, y, z};
}

auto convertToSphericalCoordinates(const CartesianCoordinates& cartesianPoint)
    -> std::optional<SphericalCoordinates> {
    LOG_F(INFO,
          "convertToSphericalCoordinates: Cartesian=({:.6f}, {:.6f}, {:.6f})",
          cartesianPoint.x, cartesianPoint.y, cartesianPoint.z);

    double x = cartesianPoint.x;
    double y = cartesianPoint.y;
    double z = cartesianPoint.z;

    double radius = std::sqrt(x * x + y * y + z * z);
    if (radius == 0.0) {
        LOG_F(WARNING, "Radius is zero, returning nullopt");
        return std::nullopt;
    }

    double declination = std::asin(z / radius) * K_RADIANS_TO_DEGREES;
    double rightAscension = std::atan2(y, x) * K_RADIANS_TO_DEGREES;

    if (rightAscension < 0) {
        rightAscension += K_DEGREES_IN_CIRCLE;
    }

    LOG_F(INFO, "Spherical coordinates: RA={:.6f}°, Dec={:.6f}°",
          rightAscension, declination);
    return SphericalCoordinates{rightAscension, declination};
}

auto dmsToDegree(int degrees, int minutes, double seconds) -> double {
    LOG_F(INFO, "dmsToDegree: Degrees={}, Minutes={}, Seconds={:.6f}", degrees,
          minutes, seconds);

    // 确定符号
    double sign = degrees < 0 ? -1.0 : 1.0;
    // 计算绝对值
    double absDegrees = std::abs(degrees) + minutes / K_MINUTES_IN_HOUR +
                        seconds / K_SECONDS_IN_HOUR;
    double result = sign * absDegrees;

    LOG_F(INFO, "Result: {:.6f}°", result);
    return result;
}

auto radToDmsStr(double radians) -> std::string {
    LOG_F(INFO, "radToDmsStr: Input radians={:.6f}", radians);

    // 将弧度转换为度数
    double degrees = radToDegree(radians);

    // 确定符号
    char sign = degrees < 0 ? '-' : '+';
    degrees = std::abs(degrees);

    // 提取度分秒
    int deg = static_cast<int>(degrees);
    double minPartial = (degrees - deg) * 60.0;
    int min = static_cast<int>(minPartial);
    double sec = (minPartial - min) * 60.0;

    // 处理舍入误差
    if (sec >= 60.0) {
        sec = 0.0;
        min++;
        if (min >= 60) {
            min = 0;
            deg++;
        }
    }

    // 格式化输出
    std::stringstream ss;
    ss << sign << std::setfill('0') << std::setw(2) << deg << "°"
       << std::setfill('0') << std::setw(2) << min << "'" << std::fixed
       << std::setprecision(1) << sec << "\"";

    std::string result = ss.str();
    LOG_F(INFO, "radToDmsStr: Output={}", result);
    return result;
}

auto radToHmsStr(double radians) -> std::string {
    LOG_F(INFO, "radToHmsStr: Input radians={:.6f}", radians);

    // 将弧度转换为小时
    double hours = radToHour(radians);

    // 确保hours在0-24范围内
    hours = rangeTo(hours, 24.0, 0.0);

    // 提取时分秒
    int hrs = static_cast<int>(hours);
    double minPartial = (hours - hrs) * 60.0;
    int min = static_cast<int>(minPartial);
    double sec = (minPartial - min) * 60.0;

    // 处理舍入误差
    if (sec >= 60.0) {
        sec = 0.0;
        min++;
        if (min >= 60) {
            min = 0;
            hrs++;
            if (hrs >= 24) {
                hrs = 0;
            }
        }
    }

    // 格式化输出
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hrs << ':' << std::setfill('0')
       << std::setw(2) << min << ':' << std::fixed << std::setprecision(1)
       << std::setfill('0') << std::setw(4) << sec;

    std::string result = ss.str();
    LOG_F(INFO, "radToHmsStr: Output={}", result);
    return result;
}

}  // namespace lithium::tools
