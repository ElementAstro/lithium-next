#ifndef LITHIUM_TOOLS_CALCULATE_HPP
#define LITHIUM_TOOLS_CALCULATE_HPP

#include "convert.hpp"
#include "tools/croods.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <ctime>
#include <numbers>  // For std::numbers::pi

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
 * @brief Structure to hold date and time information.
 */
struct alignas(32) DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    double second;
};

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

/**
 * @brief Calculates the Julian date for a given date and time.
 *
 * @tparam T A floating-point type
 * @param dateTime The date and time.
 * @return The Julian date.
 */
template <std::floating_point T>
auto calculateJulianDate(const DateTime& dateTime) -> T {
    constexpr int DAYS_IN_MONTH = 12;
    constexpr int HOURS_IN_DAY = 24;
    constexpr int MINUTES_IN_DAY = 1440;
    constexpr int SECONDS_IN_DAY = 86400;
    constexpr int DAYS_OFFSET = 32045;
    constexpr int EPOCH_OFFSET = 4800;
    constexpr int MONTH_ADJUSTMENT_DIVISOR = 12;
    constexpr int MONTH_OFFSET = 3;
    constexpr int MONTH_DAYS_COEF = 153;
    constexpr int MONTH_DAYS_DIVISOR = 5;
    constexpr int DAYS_IN_YEAR = 365;
    constexpr int LEAP_YEAR_DIVISOR = 4;
    constexpr int CENTURY_DIVISOR = 100;
    constexpr int LEAP_CENTURY_DIVISOR = 400;
    constexpr int NOON_HOUR = 12;

    int a = (MONTH_ADJUSTMENT_DIVISOR - dateTime.month) / DAYS_IN_MONTH;
    int y = dateTime.year + EPOCH_OFFSET - a;
    int m = dateTime.month + DAYS_IN_MONTH * a - MONTH_OFFSET;

    T julianDate =
        static_cast<T>(dateTime.day) +
        static_cast<T>(MONTH_DAYS_COEF * m + 2) / MONTH_DAYS_DIVISOR +
        static_cast<T>(DAYS_IN_YEAR) * y +
        static_cast<T>(y) / LEAP_YEAR_DIVISOR -
        static_cast<T>(y) / CENTURY_DIVISOR +
        static_cast<T>(y) / LEAP_CENTURY_DIVISOR - DAYS_OFFSET +
        static_cast<T>(dateTime.hour - NOON_HOUR) / HOURS_IN_DAY +
        static_cast<T>(dateTime.minute) / MINUTES_IN_DAY +
        static_cast<T>(dateTime.second) / SECONDS_IN_DAY;

    return julianDate;
}

/**
 * @brief Calculates the sidereal time for a given date and location.
 *
 * @tparam T A floating-point type
 * @param dateTime The date and time.
 * @param longitude The observer's longitude in degrees.
 * @return The sidereal time in hours.
 */
template <typename T>
auto calculateSiderealTime(const DateTime& dateTime, T longitude) -> T {
    constexpr double J2000_EPOCH = 2451545.0;
    constexpr double JULIAN_CENTURY = 36525.0;
    constexpr double GST_COEF1 = 280.46061837;
    constexpr double GST_COEF2 = 360.98564736629;
    constexpr double GST_COEF3 = 0.000387933;
    constexpr double GST_COEF4 = 38710000.0;
    constexpr double DEGREES_PER_HOUR = 15.0;
    constexpr double HOURS_IN_CIRCLE = 24.0;

    T julianDate = calculateJulianDate<T>(dateTime);
    T t = (julianDate - J2000_EPOCH) / JULIAN_CENTURY;

    T theta = GST_COEF1 + GST_COEF2 * (julianDate - J2000_EPOCH) +
              GST_COEF3 * t * t - t * t * t / GST_COEF4;

    // Convert to hours and add longitude contribution
    T siderealTime = theta / DEGREES_PER_HOUR + longitude / DEGREES_PER_HOUR;

    // Normalize to [0, 24) hours
    siderealTime = std::fmod(siderealTime, HOURS_IN_CIRCLE);
    if (siderealTime < 0) {
        siderealTime += HOURS_IN_CIRCLE;
    }

    return siderealTime;
}

/**
 * @brief Normalizes the right ascension to the range [0, 24) hours.
 *
 * @tparam T A floating-point type
 * @param ra Right ascension in hours.
 * @return Normalized right ascension in hours.
 */
template <std::floating_point T>
auto normalizeRightAscension(T ra) -> T {
    constexpr T HOURS_IN_CIRCLE = 24.0;
    ra = std::fmod(ra, HOURS_IN_CIRCLE);
    if (ra < 0) {
        ra += HOURS_IN_CIRCLE;
    }
    return ra;
}

/**
 * @brief Normalizes the declination to the range [-90, 90] degrees.
 *
 * @tparam T A floating-point type
 * @param dec Declination in degrees.
 * @return Normalized declination in degrees.
 */
template <std::floating_point T>
auto normalizeDeclination(T dec) -> T {
    return std::max<T>(-90.0, std::min<T>(90.0, dec));
}

/**
 * @brief Calculates the precession for celestial coordinates between two dates.
 *
 * @tparam T A floating-point type
 * @param coords The celestial coordinates.
 * @param from The starting date.
 * @param to The ending date.
 * @return The precession value in degrees.
 */
template <std::floating_point T>
auto calculatePrecession(const CelestialCoords<T>& coords, const DateTime& from,
                         const DateTime& to) -> T {
    constexpr double J2000_EPOCH = 2451545.0;
    constexpr double JULIAN_CENTURY = 36525.0;
    constexpr double PREC_COEF1 = 2306.2181;
    constexpr double PREC_COEF2 = 1.39656;
    constexpr double PREC_COEF3 = 0.000139;
    constexpr double PREC_COEF4 = 0.30188;
    constexpr double PREC_COEF5 = 0.000344;
    constexpr double PREC_COEF6 = 0.017998;
    constexpr double PREC_COEF7 = 1.09468;
    constexpr double PREC_COEF8 = 0.000066;
    constexpr double PREC_COEF9 = 0.018203;
    constexpr double PREC_COEF10 = 2004.3109;
    constexpr double PREC_COEF11 = 0.85330;
    constexpr double PREC_COEF12 = 0.000217;
    constexpr double PREC_COEF13 = 0.42665;
    constexpr double PREC_COEF14 = 0.041833;
    constexpr double ARC_SECONDS_PER_DEGREE = 3600.0;
    constexpr double DEGREES_TO_RADIANS = std::numbers::pi / 180.0;
    constexpr double RADIANS_TO_DEGREES = 180.0 / std::numbers::pi;
    constexpr double HOURS_TO_RADIANS = std::numbers::pi / 12.0;
    constexpr double DEGREES_PER_HOUR = 15.0;

    auto jd1 = calculateJulianDate<T>(from);
    auto jd2 = calculateJulianDate<T>(to);

    T T1 = (jd1 - J2000_EPOCH) / JULIAN_CENTURY;
    T t = (jd2 - jd1) / JULIAN_CENTURY;

    T zeta = (PREC_COEF1 + PREC_COEF2 * T1 - PREC_COEF3 * T1 * T1) * t +
             (PREC_COEF4 - PREC_COEF5 * T1) * t * t + PREC_COEF6 * t * t * t;
    T z = (PREC_COEF1 + PREC_COEF2 * T1 - PREC_COEF3 * T1 * T1) * t +
          (PREC_COEF7 + PREC_COEF8 * T1) * t * t + PREC_COEF9 * t * t * t;
    T theta = (PREC_COEF10 - PREC_COEF11 * T1 - PREC_COEF12 * T1 * T1) * t -
              (PREC_COEF13 + PREC_COEF12 * T1) * t * t -
              PREC_COEF14 * t * t * t;

    zeta /= ARC_SECONDS_PER_DEGREE;
    z /= ARC_SECONDS_PER_DEGREE;
    theta /= ARC_SECONDS_PER_DEGREE;

    // Convert RA from hours to radians for calculations
    T raRad = coords.ra * HOURS_TO_RADIANS;
    T decRad = coords.dec * DEGREES_TO_RADIANS;

    T zetaRad = zeta * DEGREES_TO_RADIANS;
    T thetaRad = theta * DEGREES_TO_RADIANS;

    T A = std::cos(decRad) * std::sin(raRad + zetaRad);
    T B = std::cos(thetaRad) * std::cos(decRad) * std::cos(raRad + zetaRad) -
          std::sin(thetaRad) * std::sin(decRad);
    T C = std::sin(thetaRad) * std::cos(decRad) * std::cos(raRad + zetaRad) +
          std::cos(thetaRad) * std::sin(decRad);

    T newRA = std::atan2(A, B) * (12.0 / std::numbers::pi) +
              z * (1.0 / DEGREES_PER_HOUR);
    newRA = normalizeRightAscension(newRA);

    T newDec = std::asin(C) * RADIANS_TO_DEGREES;
    newDec = normalizeDeclination(newDec);

    return std::sqrt(std::pow(newRA - coords.ra, 2) +
                     std::pow(newDec - coords.dec, 2));
}

/**
 * @brief Applies parallax correction to celestial coordinates.
 *
 * @tparam T A floating-point type
 * @param coords The celestial coordinates.
 * @param observer The observer's geographic coordinates.
 * @param distance The distance to the celestial object.
 * @param dt The date and time.
 * @return The parallax-corrected celestial coordinates.
 */
template <std::floating_point T>
auto applyParallax(const CelestialCoords<T>& coords,
                   const GeographicCoords<T>& observer, T distance,
                   const DateTime& dt) -> CelestialCoords<T> {
    constexpr double EARTH_RADIUS_EQUATORIAL = 6378137.0;  // in meters
    constexpr double PARSEC = 3.085677581e16;              // in meters
    constexpr double HOURS_IN_CIRCLE = 24.0;
    constexpr double DEGREES_TO_RADIANS = std::numbers::pi / 180.0;
    constexpr double RADIANS_TO_DEGREES = 180.0 / std::numbers::pi;
    constexpr double HOURS_TO_RADIANS = std::numbers::pi / 12.0;

    T lst = calculateSiderealTime(dt, observer.longitude);
    T ha = lst - coords.ra;

    // Normalize hour angle to [-12, 12)
    ha = std::fmod(ha, HOURS_IN_CIRCLE);
    if (ha < -12) {
        ha += HOURS_IN_CIRCLE;
    } else if (ha >= 12) {
        ha -= HOURS_IN_CIRCLE;
    }

    T latRad = observer.latitude * DEGREES_TO_RADIANS;
    T decRad = coords.dec * DEGREES_TO_RADIANS;
    T haRad = ha * HOURS_TO_RADIANS;

    T sinLat = std::sin(latRad);
    T cosLat = std::cos(latRad);
    T sinDec = std::sin(decRad);
    T cosDec = std::cos(decRad);
    T sinHA = std::sin(haRad);
    T cosHA = std::cos(haRad);

    T rho = EARTH_RADIUS_EQUATORIAL / (PARSEC * distance);

    T A = cosLat * sinHA;
    T B = sinLat * cosDec - cosLat * sinDec * cosHA;
    T C = sinLat * sinDec + cosLat * cosDec * cosHA;

    T newRA = coords.ra - std::atan2(A, C - rho) * (12.0 / std::numbers::pi);
    T newDec = std::atan2((B * (C - rho) + A * A * sinDec / cosDec) /
                              ((C - rho) * (C - rho) + A * A),
                          cosDec) *
               RADIANS_TO_DEGREES;

    newRA = normalizeRightAscension(newRA);
    newDec = normalizeDeclination(newDec);

    return {newRA, newDec};
}

/**
 * @brief Calculates the atmospheric refraction for a given altitude.
 *
 * @tparam T A floating-point type
 * @param altitude The altitude in degrees.
 * @param temperature The temperature in Celsius (default: 10.0).
 * @param pressure The atmospheric pressure in hPa (default: 1010.0).
 * @return The refraction correction in degrees.
 */
template <std::floating_point T>
auto calculateRefraction(T altitude, T temperature = 10.0, T pressure = 1010.0)
    -> T {
    constexpr double REFR_MIN_ALT = -0.5;
    constexpr double REFR_ALT_THRESHOLD = 15.0;
    constexpr double REFR_COEF1 = 0.00452;
    constexpr double REFR_COEF2 = 273.0;
    constexpr double REFR_COEF3 = 0.1594;
    constexpr double REFR_COEF4 = 0.0196;
    constexpr double REFR_COEF5 = 0.00002;
    constexpr double REFR_COEF6 = 7.31;
    constexpr double REFR_COEF7 = 4.4;
    constexpr double REFR_TEMP_COEF = 0.00012;
    constexpr double REFR_STD_TEMP = 10.0;
    constexpr double REFR_STD_PRESSURE = 1010.0;
    constexpr double REFR_ARCMIN_TO_DEG = 60.0;
    constexpr double DEGREES_TO_RADIANS = std::numbers::pi / 180.0;

    if (altitude < REFR_MIN_ALT) {
        return 0.0;  // Object is below the horizon, no refraction applied
    }

    T refraction;
    if (altitude > REFR_ALT_THRESHOLD) {
        // Formula for higher altitudes (>15°)
        refraction = REFR_COEF1 * pressure /
                     ((REFR_COEF2 + temperature) *
                      std::tan(altitude * DEGREES_TO_RADIANS));
    } else {
        // Formula for lower altitudes
        T a = altitude;
        T b = altitude + REFR_COEF6 / (altitude + REFR_COEF7);
        refraction = REFR_COEF3 + REFR_COEF4 * a + REFR_COEF5 * a * a;
        refraction *= pressure *
                      (1.0 - REFR_TEMP_COEF * (temperature - REFR_STD_TEMP)) /
                      REFR_STD_PRESSURE;
        refraction /= REFR_ARCMIN_TO_DEG;  // Convert from arcminutes to degrees
    }

    return refraction;
}

}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_CALCULATE_HPP
