#ifndef LITHIUM_SEARCH_CROODS_HPP
#define LITHIUM_SEARCH_CROODS_HPP

#include <cmath>
#include <concepts>
#include <numbers>
#include <span>

namespace lithium::tools {
auto periodBelongs(double value, double min, double max, double period,
                   bool minequ, bool maxequ) -> bool;
constexpr double EARTHRADIUSEQUATORIAL = 6378137.0;
constexpr double EARTHRADIUSPOLAR = 6356752.0;
constexpr double ASTRONOMICALUNIT = 1.495978707e11;
constexpr double LIGHTSPEED = 299792458.0;
constexpr double AIRY = 1.21966;
constexpr double SOLARMASS = 1.98847e30;
constexpr double SOLARRADIUS = 6.957e8;
constexpr double PARSEC = 3.0857e16;

constexpr auto lumen(double wavelength) -> double {
    constexpr double MAGIC_NUMBER = 1.464128843e-3;
    return MAGIC_NUMBER / (wavelength * wavelength);
}

constexpr auto redshift(double observed, double rest) -> double {
    return (observed - rest) / rest;
}

constexpr auto doppler(double redshift, double speed) -> double {
    return redshift * speed;
}

template <typename T>
auto rangeHA(T range) -> T {
    constexpr double MAGIC_NUMBER24 = 24.0;
    constexpr double MAGIC_NUMBER12 = 12.0;

    if (range < -MAGIC_NUMBER12) {
        range += MAGIC_NUMBER24;
    }
    while (range >= MAGIC_NUMBER12) {
        range -= MAGIC_NUMBER24;
    }
    return range;
}

template <typename T>
auto range24(T range) -> T {
    constexpr double MAGIC_NUMBER24 = 24.0;

    if (range < 0.0) {
        range += MAGIC_NUMBER24;
    }
    while (range > MAGIC_NUMBER24) {
        range -= MAGIC_NUMBER24;
    }
    return range;
}

template <typename T>
auto range360(T range) -> T {
    constexpr double MAGIC_NUMBER360 = 360.0;

    if (range < 0.0) {
        range += MAGIC_NUMBER360;
    }
    while (range > MAGIC_NUMBER360) {
        range -= MAGIC_NUMBER360;
    }
    return range;
}

template <typename T>
auto rangeDec(T decDegrees) -> T {
    constexpr double MAGIC_NUMBER360 = 360.0;
    constexpr double MAGIC_NUMBER180 = 180.0;
    constexpr double MAGIC_NUMBER90 = 90.0;
    constexpr double MAGIC_NUMBER270 = 270.0;

    if (decDegrees >= MAGIC_NUMBER270 && decDegrees <= MAGIC_NUMBER360) {
        return decDegrees - MAGIC_NUMBER360;
    }
    if (decDegrees >= MAGIC_NUMBER180 && decDegrees < MAGIC_NUMBER270) {
        return MAGIC_NUMBER180 - decDegrees;
    }
    if (decDegrees >= MAGIC_NUMBER90 && decDegrees < MAGIC_NUMBER180) {
        return MAGIC_NUMBER180 - decDegrees;
    }
    return decDegrees;
}

template <typename T>
auto getLocalHourAngle(T siderealTime, T rightAscension) -> T {
    T hourAngle = siderealTime - rightAscension;
    return range24(hourAngle);
}

template <typename T>
auto getAltAzCoordinates(T hourAngle, T declination,
                         T latitude) -> std::pair<T, T> {
    using namespace std::numbers;
    hourAngle *= pi_v<T> / 180.0;
    declination *= pi_v<T> / 180.0;
    latitude *= pi_v<T> / 180.0;

    T altitude = std::asin(std::sin(declination) * std::sin(latitude) +
                           std::cos(declination) * std::cos(latitude) *
                               std::cos(hourAngle));
    T azimuth = std::acos(
        (std::sin(declination) - std::sin(altitude) * std::sin(latitude)) /
        (std::cos(altitude) * std::cos(latitude)));

    altitude *= 180.0 / pi_v<T>;
    azimuth *= 180.0 / pi_v<T>;

    if (hourAngle > 0) {
        azimuth = 360 - azimuth;
    }

    return {altitude, azimuth};
}

template <typename T>
auto estimateGeocentricElevation(T latitude, T elevation) -> T {
    using namespace std::numbers;
    latitude *= pi_v<T> / 180.0;
    return elevation - (elevation * std::cos(latitude));
}

template <typename T>
auto estimateFieldRotationRate(T altitude, T azimuth, T latitude) -> T {
    using namespace std::numbers;
    altitude *= pi_v<T> / 180.0;
    azimuth *= pi_v<T> / 180.0;
    latitude *= pi_v<T> / 180.0;

    T rate = std::cos(latitude) * std::sin(azimuth) / std::cos(altitude);
    return rate * 180.0 / pi_v<T>;
}

template <typename T>
auto estimateFieldRotation(T hourAngle, T rate) -> T {
    constexpr double MAGIC_NUMBER360 = 360.0;

    while (hourAngle >= MAGIC_NUMBER360) {
        hourAngle -= MAGIC_NUMBER360;
    }
    while (hourAngle < 0) {
        hourAngle += MAGIC_NUMBER360;
    }
    return hourAngle * rate;
}

constexpr auto as2rad(double arcSeconds) -> double {
    using namespace std::numbers;
    constexpr double MAGIC_NUMBER = 60.0 * 60.0 * 12.0;
    return arcSeconds * pi_v<double> / MAGIC_NUMBER;
}

constexpr auto rad2as(double radians) -> double {
    using namespace std::numbers;
    constexpr double MAGIC_NUMBER = 60.0 * 60.0 * 12.0;
    return radians * MAGIC_NUMBER / pi_v<double>;
}

template <typename T>
auto estimateDistance(T parsecs, T parallaxRadius) -> T {
    return parsecs / parallaxRadius;
}

constexpr auto m2au(double meters) -> double {
    constexpr double MAGIC_NUMBER = 1.496e+11;
    return meters / MAGIC_NUMBER;
}

template <typename T>
auto calcDeltaMagnitude(T magnitudeRatio, std::span<const T> spectrum) -> T {
    T deltaMagnitude = 0;
    for (size_t index = 0; index < spectrum.size(); ++index) {
        deltaMagnitude += spectrum[index] * magnitudeRatio;
    }
    return deltaMagnitude;
}

template <typename T>
auto calcStarMass(T deltaMagnitude, T referenceSize) -> T {
    return referenceSize * std::pow(10, deltaMagnitude / -2.5);
}

template <typename T>
auto estimateOrbitRadius(T observedWavelength, T referenceWavelength,
                         T period) -> T {
    return (observedWavelength - referenceWavelength) / period;
}

template <typename T>
auto estimateSecondaryMass(T starMass, T starDrift, T orbitRadius) -> T {
    return starMass * std::pow(starDrift / orbitRadius, 2);
}

template <typename T>
auto estimateSecondarySize(T starSize, T dropoffRatio) -> T {
    return starSize * std::sqrt(dropoffRatio);
}

template <typename T>
auto calcPhotonFlux(T relativeMagnitude, T filterBandwidth, T wavelength,
                    T steradian) -> T {
    constexpr double MAGIC_NUMBER10 = 10;
    constexpr double MAGIC_NUMBER04 = 0.4;

    return std::pow(MAGIC_NUMBER10, relativeMagnitude * -MAGIC_NUMBER04) *
           filterBandwidth * wavelength * steradian;
}

template <typename T>
auto calcRelMagnitude(T photonFlux, T filterBandwidth, T wavelength,
                      T steradian) -> T {
    constexpr double MAGIC_NUMBER04 = 0.4;
    return std::log10(photonFlux /
                      (LUMEN(wavelength) * steradian * filterBandwidth)) /
           -MAGIC_NUMBER04;
}

template <typename T>
auto estimateAbsoluteMagnitude(T deltaDistance, T deltaMagnitude) -> T {
    return deltaMagnitude - 5 * (std::log10(deltaDistance) - 1);
}

template <typename T>
auto baseline2dProjection(T altitude, T azimuth) -> std::array<T, 2> {
    using namespace std::numbers;
    altitude *= pi_v<T> / 180.0;
    azimuth *= pi_v<T> / 180.0;

    T x = std::cos(altitude) * std::cos(azimuth);
    T y = std::cos(altitude) * std::sin(azimuth);

    return {x, y};
}

template <typename T>
auto baselineDelay(T altitude, T azimuth,
                   const std::array<T, 3>& baseline) -> T {
    using namespace std::numbers;
    altitude *= pi_v<T> / 180.0;
    azimuth *= pi_v<T> / 180.0;

    T delay = baseline[0] * std::cos(altitude) * std::cos(azimuth) +
              baseline[1] * std::cos(altitude) * std::sin(azimuth) +
              baseline[2] * std::sin(altitude);

    return delay;
}

// 定义一个表示天体坐标的结构体
template <std::floating_point T>
struct CelestialCoords {
    T ra;   // 赤经 (小时)
    T dec;  // 赤纬 (度)
};

// 定义一个表示地理坐标的结构体
template <std::floating_point T>
struct GeographicCoords {
    T latitude;
    T longitude;
};

template <std::floating_point T>
auto equatorialToEcliptic(const CelestialCoords<T>& coords,
                          T obliquity) -> std::pair<T, T> {
    T sinDec = std::sin(coords.dec * std::numbers::pi / 180.0);
    T cosDec = std::cos(coords.dec * std::numbers::pi / 180.0);
    T sinRA = std::sin(coords.ra * std::numbers::pi / 12.0);
    T cosRA = std::cos(coords.ra * std::numbers::pi / 12.0);
    T sinObl = std::sin(obliquity * std::numbers::pi / 180.0);
    T cosObl = std::cos(obliquity * std::numbers::pi / 180.0);

    T latitude = std::asin(sinDec * cosObl - cosDec * sinObl * sinRA) * 180.0 /
                 std::numbers::pi;
    T longitude =
        std::atan2(sinRA * cosDec * cosObl + sinDec * sinObl, cosDec * cosRA) *
        180.0 / std::numbers::pi;

    return {range360(longitude), latitude};
}

}  // namespace lithium::tools

#endif
