#ifndef LITHIUM_TOOLS_CALCULATE_HPP
#define LITHIUM_TOOLS_CALCULATE_HPP

#include "convert.hpp"
#include "tools/croods.hpp"

#include <cmath>
#include <ctime>

namespace lithium::tools {

/**
 * @brief Represents Altitude and Azimuth coordinates.
 */
struct AltAz {
    double altitude;  ///< Altitude in degrees
    double azimuth;   ///< Azimuth in degrees
} ATOM_ALIGNAS(16);

/**
 * @brief Represents the minimum and maximum Field of View (FOV).
 */
struct MinMaxFOV {
    double minFOV;  ///< Minimum Field of View in degrees
    double maxFOV;  ///< Maximum Field of View in degrees
} ATOM_ALIGNAS(16);

/**
 * @brief Calculates the vector from point A to point B in Cartesian
 * coordinates.
 *
 * @param pointA The starting point in Cartesian coordinates.
 * @param pointB The ending point in Cartesian coordinates.
 * @return The vector from point A to point B in Cartesian coordinates.
 */
auto calculateVector(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& pointB)
    -> CartesianCoordinates;

/**
 * @brief Calculates the point C given point A and vector V in Cartesian
 * coordinates.
 *
 * @param pointA The starting point in Cartesian coordinates.
 * @param vectorV The vector in Cartesian coordinates.
 * @return The point C in Cartesian coordinates.
 */
auto calculatePointC(const CartesianCoordinates& pointA,
                     const CartesianCoordinates& vectorV)
    -> CartesianCoordinates;

/**
 * @brief Calculates the Greenwich Sidereal Time (GST) for a given date.
 *
 * @param date The date and time.
 * @return The Greenwich Sidereal Time in degrees.
 */
auto calculateGST(const std::tm& date) -> double;

/**
 * @brief Calculates the Field of View (FOV) for a given focal length and camera
 * size.
 *
 * @param focalLength The focal length of the telescope in millimeters.
 * @param cameraSizeWidth The width of the camera sensor in millimeters.
 * @param cameraSizeHeight The height of the camera sensor in millimeters.
 * @return The minimum and maximum Field of View in degrees.
 */
auto calculateFOV(int focalLength, double cameraSizeWidth,
                  double cameraSizeHeight) -> MinMaxFOV;

/**
 * @brief Calculates the Altitude and Azimuth for given equatorial coordinates
 * and observer's location.
 *
 * @param rightAscension The Right Ascension in degrees.
 * @param declination The Declination in degrees.
 * @param latitude The observer's latitude in degrees.
 * @param longitude The observer's longitude in degrees.
 * @param date The date and time.
 * @return The Altitude and Azimuth in degrees.
 */
auto calculateAltAz(double rightAscension, double declination, double latitude,
                    double longitude, const std::tm& date) -> AltAz;

// 添加一个日期时间结构体
struct alignas(32) DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    double second;
};

// 添加一个函数来计算儒略日
template <std::floating_point T>
auto calculateJulianDate(const DateTime& dateTime) -> T {
    constexpr int MAGIC_NUMBER12 = 12;
    constexpr int MAGIC_NUMBER24 = 24;
    constexpr int MAGIC_NUMBER1440 = 1440;
    constexpr int MAGIC_NUMBER86400 = 86400;
    constexpr int MAGIC_NUMBER32045 = 32045;
    constexpr int MAGIC_NUMBER4800 = 4800;
    constexpr int MAGIC_NUMBER14 = 14;
    constexpr int MAGIC_NUMBER5 = 5;
    constexpr int MAGIC_NUMBER153 = 153;
    constexpr int MAGIC_NUMBER365 = 365;
    constexpr int MAGIC_NUMBER100 = 100;
    constexpr int MAGIC_NUMBER400 = 400;

    int a = (MAGIC_NUMBER14 - dateTime.month) / MAGIC_NUMBER12;
    int y = dateTime.year + MAGIC_NUMBER4800 - a;
    int m = dateTime.month + MAGIC_NUMBER12 * a - 3;

    T julianDate =
        static_cast<T>(dateTime.day) +
        static_cast<T>(MAGIC_NUMBER153 * m + 2) / MAGIC_NUMBER5 +
        static_cast<T>(MAGIC_NUMBER365) * y + static_cast<T>(y) / 4 -
        static_cast<T>(y) / MAGIC_NUMBER100 +
        static_cast<T>(y) / MAGIC_NUMBER400 - MAGIC_NUMBER32045 +
        static_cast<T>(dateTime.hour - MAGIC_NUMBER12) / MAGIC_NUMBER24 +
        static_cast<T>(dateTime.minute) / MAGIC_NUMBER1440 +
        static_cast<T>(dateTime.second) / MAGIC_NUMBER86400;

    return julianDate;
}

template <typename T>
auto calculateSiderealTime(const DateTime& dateTime, T /*longitude*/) -> T {
    constexpr double MAGIC_NUMBER2451545 = 2451545.0;
    constexpr double MAGIC_NUMBER36525 = 36525.0;
    constexpr double MAGIC_NUMBER28046061837 = 280.46061837;
    constexpr double MAGIC_NUMBER36098564736629 = 360.98564736629;
    constexpr double MAGIC_NUMBER0000387933 = 0.000387933;
    constexpr double MAGIC_NUMBER38710000 = 38710000.0;
    constexpr double MAGIC_NUMBER15 = 15.0;

    T julianDate = calculateJulianDate<T>(dateTime);
    T t = (julianDate - MAGIC_NUMBER2451545) / MAGIC_NUMBER36525;

    T theta = MAGIC_NUMBER28046061837 +
              MAGIC_NUMBER36098564736629 * (julianDate - MAGIC_NUMBER2451545) +
              MAGIC_NUMBER0000387933 * t * t - t * t * t / MAGIC_NUMBER38710000;

    return theta / MAGIC_NUMBER15;  // 转换为小时
}

// 添加一个函数来计算前进
template <std::floating_point T>
auto calculatePrecession(const CelestialCoords<T>& coords, const DateTime& from,
                         const DateTime& to) -> T {
    auto jd1 = calculate_julian_date<T>(from);
    auto jd2 = calculate_julian_date<T>(to);

    T T1 = (jd1 - 2451545.0) / 36525.0;
    T t = (jd2 - jd1) / 36525.0;

    T zeta = (2306.2181 + 1.39656 * T1 - 0.000139 * T1 * T1) * t +
             (0.30188 - 0.000344 * T1) * t * t + 0.017998 * t * t * t;
    T z = (2306.2181 + 1.39656 * T1 - 0.000139 * T1 * T1) * t +
          (1.09468 + 0.000066 * T1) * t * t + 0.018203 * t * t * t;
    T theta = (2004.3109 - 0.85330 * T1 - 0.000217 * T1 * T1) * t -
              (0.42665 + 0.000217 * T1) * t * t - 0.041833 * t * t * t;

    zeta /= 3600.0;
    z /= 3600.0;
    theta /= 3600.0;

    T A = std::cos(coords.dec * std::numbers::pi / 180.0) *
          std::sin(coords.ra * std::numbers::pi / 12.0 +
                   zeta * std::numbers::pi / 180.0);
    T B = std::cos(theta * std::numbers::pi / 180.0) *
              std::cos(coords.dec * std::numbers::pi / 180.0) *
              std::cos(coords.ra * std::numbers::pi / 12.0 +
                       zeta * std::numbers::pi / 180.0) -
          std::sin(theta * std::numbers::pi / 180.0) *
              std::sin(coords.dec * std::numbers::pi / 180.0);
    T C = std::sin(theta * std::numbers::pi / 180.0) *
              std::cos(coords.dec * std::numbers::pi / 180.0) *
              std::cos(coords.ra * std::numbers::pi / 12.0 +
                       zeta * std::numbers::pi / 180.0) +
          std::cos(theta * std::numbers::pi / 180.0) *
              std::sin(coords.dec * std::numbers::pi / 180.0);

    T newRA = std::atan2(A, B) * 12.0 / std::numbers::pi + z / 15.0;
    T newDec = std::asin(C) * 180.0 / std::numbers::pi;

    return std::sqrt(std::pow(newRA - coords.ra, 2) +
                     std::pow(newDec - coords.dec, 2));
}

template <std::floating_point T>
auto applyParallax(const CelestialCoords<T>& coords,
                   const GeographicCoords<T>& observer, T distance,
                   const DateTime& dt) -> CelestialCoords<T> {
    T lst = calculate_sidereal_time(dt, observer.longitude);
    T ha = lst - coords.ra;

    T sinLat = std::sin(observer.latitude * std::numbers::pi / 180.0);
    T cosLat = std::cos(observer.latitude * std::numbers::pi / 180.0);
    T sinDec = std::sin(coords.dec * std::numbers::pi / 180.0);
    T cosDec = std::cos(coords.dec * std::numbers::pi / 180.0);
    T sinHA = std::sin(ha * std::numbers::pi / 12.0);
    T cosHA = std::cos(ha * std::numbers::pi / 12.0);

    T rho = EARTHRADIUSEQUATORIAL / (PARSEC * distance);

    T A = cosLat * sinHA;
    T B = sinLat * cosDec - cosLat * sinDec * cosHA;
    T C = sinLat * sinDec + cosLat * cosDec * cosHA;

    T newRA = coords.ra - std::atan2(A, C - rho) * 12.0 / std::numbers::pi;
    T newDec = std::atan2((B * (C - rho) + A * A * sinDec / cosDec) /
                              ((C - rho) * (C - rho) + A * A),
                          cosDec) *
               180.0 / std::numbers::pi;

    return {range24(newRA), rangeDec(newDec)};
}

// 添加一个函数来计算大气折射
template <std::floating_point T>
auto calculateRefraction(T altitude, T temperature = 10.0,
                         T pressure = 1010.0) -> T {
    if (altitude < -0.5) {
        return 0.0;  // 天体在地平线以下，不考虑折射
    }

    T R;
    if (altitude > 15.0) {
        R = 0.00452 * pressure /
            ((273 + temperature) *
             std::tan(altitude * std::numbers::pi / 180.0));
    } else {
        T a = altitude;
        T b = altitude + 7.31 / (altitude + 4.4);
        R = 0.1594 + 0.0196 * a + 0.00002 * a * a;
        R *= pressure * (1 - 0.00012 * (temperature - 10)) / 1010.0;
        R /= 60.0;
    }

    return R;
}

}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_CALCULATE_HPP
