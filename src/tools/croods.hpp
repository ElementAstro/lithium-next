#ifndef LITHIUM_TOOLS_CROODS_HPP
#define LITHIUM_TOOLS_CROODS_HPP

#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <numbers>
#include <span>
#include <string>

namespace lithium::tools {

// ===== Astronomical Constants =====
/// Julian Date of Unix epoch (1970-01-01)
constexpr double JD_EPOCH = 2440587.5;
/// Offset between Julian Date and Modified Julian Date
constexpr double MJD_OFFSET = 2400000.5;
/// Earth equatorial radius in meters
constexpr double EARTH_RADIUS_EQUATORIAL = 6378137.0;
/// Earth polar radius in meters
constexpr double EARTH_RADIUS_POLAR = 6356752.0;
/// Astronomical Unit in meters
constexpr double ASTRONOMICAL_UNIT = 1.495978707e11;
/// Speed of light in m/s
constexpr double SPEED_OF_LIGHT = 299792458.0;
/// Airy disk constant
constexpr double AIRY_CONSTANT = 1.21966;
/// Solar mass in kg
constexpr double SOLAR_MASS = 1.98847e30;
/// Solar radius in meters
constexpr double SOLAR_RADIUS = 6.957e8;
/// Parsec in meters
constexpr double PARSEC = 3.0857e16;

/**
 * @brief Represents celestial coordinates (right ascension and declination).
 *
 * @tparam T Floating-point type for coordinate values
 */
template <std::floating_point T>
struct CelestialCoords {
    T ra;   ///< Right Ascension in hours
    T dec;  ///< Declination in degrees
};

/**
 * @brief Represents geographic coordinates (latitude and longitude).
 *
 * @tparam T Floating-point type for coordinate values
 */
template <std::floating_point T>
struct GeographicCoords {
    T latitude;   ///< Latitude in degrees
    T longitude;  ///< Longitude in degrees
};

// ===== Time and Date Functions =====

/**
 * @brief Convert system time to Julian Date.
 *
 * @param time System time point
 * @return Julian Date
 */
double timeToJD(const std::chrono::system_clock::time_point& time);

/**
 * @brief Convert Julian Date to Modified Julian Date.
 *
 * @param jd Julian Date
 * @return Modified Julian Date
 */
double jdToMJD(double jd);

/**
 * @brief Convert Modified Julian Date to Julian Date.
 *
 * @param mjd Modified Julian Date
 * @return Julian Date
 */
double mjdToJD(double mjd);

/**
 * @brief Calculate Barycentric Julian Date (BJD) from JD.
 *
 * @param jd Julian Date
 * @param ra Right Ascension in degrees
 * @param dec Declination in degrees
 * @param longitude Observer longitude in degrees
 * @param latitude Observer latitude in degrees
 * @param elevation Observer elevation in meters
 * @return Barycentric Julian Date
 */
double calculateBJD(double jd, double ra, double dec, double longitude,
                    double latitude, double elevation);

/**
 * @brief Format time to string with timezone indication.
 *
 * @param time System time point
 * @param isLocal Whether to use local time (true) or UTC (false)
 * @param format Format string for time
 * @return Formatted time string
 */
std::string formatTime(const std::chrono::system_clock::time_point& time,
                       bool isLocal, const std::string& format = "%H:%M:%S");

/**
 * @brief Generate information text block A with mount status.
 *
 * @param localTime Local time
 * @param raDegree Right ascension in degrees
 * @param decDegree Declination in degrees
 * @param dRaDegree Delta RA error in degrees
 * @param dDecDegree Delta Dec error in degrees
 * @param mountStatus Mount status string
 * @param guideStatus Guiding status string
 * @return Formatted information text
 */
std::string getInfoTextA(const std::chrono::system_clock::time_point& localTime,
                         double raDegree, double decDegree, double dRaDegree,
                         double dDecDegree, const std::string& mountStatus,
                         const std::string& guideStatus);

/**
 * @brief Generate information text block B with camera status.
 *
 * @param utcTime UTC time
 * @param azRad Azimuth in radians
 * @param altRad Altitude in radians
 * @param camStatus Camera status string
 * @param camTemp Camera temperature
 * @param camTargetTemp Camera target temperature
 * @param camX Camera X resolution
 * @param camY Camera Y resolution
 * @param cfwPos Filter wheel position
 * @param cfwName Filter wheel name
 * @param cfwStatus Filter wheel status
 * @return Formatted information text
 */
std::string getInfoTextB(const std::chrono::system_clock::time_point& utcTime,
                         double azRad, double altRad,
                         const std::string& camStatus, double camTemp,
                         double camTargetTemp, int camX, int camY, int cfwPos,
                         const std::string& cfwName,
                         const std::string& cfwStatus);

/**
 * @brief Generate information text block C with system status.
 *
 * @param cpuTemp CPU temperature
 * @param cpuLoad CPU load percentage
 * @param diskFree Free disk space in GB
 * @param longitudeRad Longitude in radians
 * @param latitudeRad Latitude in radians
 * @param raJ2000 Right ascension in J2000 epoch
 * @param decJ2000 Declination in J2000 epoch
 * @param az Azimuth in degrees
 * @param alt Altitude in degrees
 * @param objName Object name
 * @return Formatted information text
 */
std::string getInfoTextC(int cpuTemp, int cpuLoad, double diskFree,
                         double longitudeRad, double latitudeRad,
                         double raJ2000, double decJ2000, double az, double alt,
                         const std::string& objName);

/**
 * @brief Check if a value belongs to a periodic range.
 *
 * @param value Value to check
 * @param min Minimum of range
 * @param max Maximum of range
 * @param period Period of the range
 * @param minInclusive Whether minimum is inclusive
 * @param maxInclusive Whether maximum is inclusive
 * @return true if value belongs to range
 * @return false otherwise
 */
bool periodBelongs(double value, double min, double max, double period,
                   bool minInclusive, bool maxInclusive);

// ===== Astronomical Calculation Functions =====

/**
 * @brief Calculate luminous flux for a given wavelength.
 *
 * @param wavelength Light wavelength in meters
 * @return Luminous flux
 */
constexpr auto calculateLuminousFlux(double wavelength) -> double {
    constexpr double LUMINOUS_EFFICIENCY_CONSTANT = 1.464128843e-3;
    return LUMINOUS_EFFICIENCY_CONSTANT / (wavelength * wavelength);
}

/**
 * @brief Calculate redshift from observed and rest wavelengths.
 *
 * @param observed Observed wavelength
 * @param rest Rest wavelength
 * @return Redshift value
 */
constexpr auto calculateRedshift(double observed, double rest) -> double {
    return (observed - rest) / rest;
}

/**
 * @brief Convert redshift to velocity using Doppler equation.
 *
 * @param redshift Redshift value
 * @param speed Reference speed (typically speed of light)
 * @return Velocity in same units as speed
 */
constexpr auto dopplerVelocity(double redshift, double speed) -> double {
    return redshift * speed;
}

/**
 * @brief Normalize hour angle to range [-12, 12) hours.
 *
 * @tparam T Floating-point type
 * @param hourAngle Hour angle to normalize
 * @return Normalized hour angle
 */
template <std::floating_point T>
auto normalizeHourAngle(T hourAngle) -> T {
    constexpr T HOURS_IN_DAY = 24.0;
    constexpr T HALF_DAY = 12.0;

    if (hourAngle < -HALF_DAY) {
        hourAngle += HOURS_IN_DAY;
    }
    while (hourAngle >= HALF_DAY) {
        hourAngle -= HOURS_IN_DAY;
    }
    return hourAngle;
}

/**
 * @brief Normalize right ascension to range [0, 24) hours.
 *
 * @tparam T Floating-point type
 * @param rightAscension Right ascension to normalize
 * @return Normalized right ascension
 */
template <std::floating_point T>
auto normalizeRightAscension(T rightAscension) -> T {
    constexpr T HOURS_IN_DAY = 24.0;

    if (rightAscension < 0.0) {
        rightAscension += HOURS_IN_DAY;
    }
    while (rightAscension >= HOURS_IN_DAY) {
        rightAscension -= HOURS_IN_DAY;
    }
    return rightAscension;
}

/**
 * @brief Normalize angle to range [0, 360) degrees.
 *
 * @tparam T Floating-point type
 * @param angle Angle to normalize in degrees
 * @return Normalized angle
 */
template <std::floating_point T>
auto normalizeAngle360(T angle) -> T {
    constexpr T FULL_CIRCLE = 360.0;

    if (angle < 0.0) {
        angle += FULL_CIRCLE;
    }
    while (angle >= FULL_CIRCLE) {
        angle -= FULL_CIRCLE;
    }
    return angle;
}

/**
 * @brief Normalize declination to range [-90, +90] degrees.
 *
 * @tparam T Floating-point type
 * @param declination Declination to normalize in degrees
 * @return Normalized declination
 */
template <std::floating_point T>
auto normalizeDeclination(T declination) -> T {
    constexpr T FULL_CIRCLE = 360.0;
    constexpr T HALF_CIRCLE = 180.0;
    constexpr T QUARTER_CIRCLE = 90.0;
    constexpr T THREE_QUARTER_CIRCLE = 270.0;

    // Handle full circle wrapping
    declination = std::fmod(declination, FULL_CIRCLE);
    if (declination < 0.0)
        declination += FULL_CIRCLE;

    // Convert to declination range
    if (declination >= THREE_QUARTER_CIRCLE && declination <= FULL_CIRCLE) {
        return declination - FULL_CIRCLE;
    }
    if (declination >= HALF_CIRCLE && declination < THREE_QUARTER_CIRCLE) {
        return HALF_CIRCLE - declination;
    }
    if (declination >= QUARTER_CIRCLE && declination < HALF_CIRCLE) {
        return HALF_CIRCLE - declination;
    }
    return declination;
}

/**
 * @brief Calculate local hour angle.
 *
 * @tparam T Floating-point type
 * @param siderealTime Local sidereal time in hours
 * @param rightAscension Right ascension in hours
 * @return Local hour angle in hours
 */
template <std::floating_point T>
auto calculateLocalHourAngle(T siderealTime, T rightAscension) -> T {
    T hourAngle = siderealTime - rightAscension;
    return normalizeRightAscension(hourAngle);
}

/**
 * @brief Convert equatorial coordinates to horizontal coordinates.
 *
 * @tparam T Floating-point type
 * @param hourAngle Hour angle in degrees
 * @param declination Declination in degrees
 * @param latitude Observer latitude in degrees
 * @return std::pair<T, T> Pair of altitude and azimuth in degrees
 */
template <std::floating_point T>
auto calculateAltAzCoordinates(T hourAngle, T declination, T latitude)
    -> std::pair<T, T> {
    using namespace std::numbers;

    // Convert to radians for calculation
    T hourAngleRad = hourAngle * pi_v<T> / 180.0;
    T declinationRad = declination * pi_v<T> / 180.0;
    T latitudeRad = latitude * pi_v<T> / 180.0;

    // Pre-calculate trigonometric functions for performance
    T sinDec = std::sin(declinationRad);
    T cosDec = std::cos(declinationRad);
    T sinLat = std::sin(latitudeRad);
    T cosLat = std::cos(latitudeRad);
    T cosHA = std::cos(hourAngleRad);
    T sinHA = std::sin(hourAngleRad);

    // Calculate altitude
    T sinAlt = sinDec * sinLat + cosDec * cosLat * cosHA;
    // Clamp to [-1, 1] to avoid numerical issues
    sinAlt = std::clamp(sinAlt, static_cast<T>(-1.0), static_cast<T>(1.0));
    T altitude = std::asin(sinAlt);

    // Calculate azimuth
    T cosAz = (sinDec - sinAlt * sinLat) / (std::cos(altitude) * cosLat);
    // Clamp to [-1, 1] to avoid numerical issues
    cosAz = std::clamp(cosAz, static_cast<T>(-1.0), static_cast<T>(1.0));
    T azimuth = std::acos(cosAz);

    // Adjust azimuth for correct quadrant
    if (hourAngleRad > 0) {
        azimuth = 2 * pi_v<T> - azimuth;
    }

    // Convert back to degrees
    T altitudeDeg = altitude * 180.0 / pi_v<T>;
    T azimuthDeg = azimuth * 180.0 / pi_v<T>;

    return {altitudeDeg, azimuthDeg};
}

/**
 * @brief Estimate geocentric elevation based on latitude.
 *
 * @tparam T Floating-point type
 * @param latitude Observer latitude in degrees
 * @param elevation Local elevation above sea level in meters
 * @return Geocentric elevation correction in meters
 */
template <std::floating_point T>
auto estimateGeocentricElevation(T latitude, T elevation) -> T {
    using namespace std::numbers;
    T latitudeRad = latitude * pi_v<T> / 180.0;
    // Calculate geocentric elevation correction
    return elevation - (elevation * std::cos(latitudeRad));
}

/**
 * @brief Calculate field rotation rate at given horizontal coordinates.
 *
 * @tparam T Floating-point type
 * @param altitude Altitude in degrees
 * @param azimuth Azimuth in degrees
 * @param latitude Observer latitude in degrees
 * @return Field rotation rate in degrees per hour
 */
template <std::floating_point T>
auto calculateFieldRotationRate(T altitude, T azimuth, T latitude) -> T {
    using namespace std::numbers;

    // Convert to radians
    T altitudeRad = altitude * pi_v<T> / 180.0;
    T azimuthRad = azimuth * pi_v<T> / 180.0;
    T latitudeRad = latitude * pi_v<T> / 180.0;

    // Calculate field rotation rate
    T rate =
        std::cos(latitudeRad) * std::sin(azimuthRad) / std::cos(altitudeRad);

    // Convert rate to degrees per hour
    return rate * 180.0 / pi_v<T>;
}

/**
 * @brief Calculate field rotation for a given hour angle and rate.
 *
 * @tparam T Floating-point type
 * @param hourAngle Hour angle in degrees
 * @param rate Field rotation rate in degrees per hour
 * @return Field rotation in degrees
 */
template <std::floating_point T>
auto calculateFieldRotation(T hourAngle, T rate) -> T {
    constexpr T FULL_CIRCLE = 360.0;

    // Normalize hour angle
    hourAngle = normalizeAngle360(hourAngle);

    // Calculate field rotation
    return hourAngle * rate;
}

/**
 * @brief Convert arcseconds to radians.
 *
 * @param arcSeconds Angle in arcseconds
 * @return Angle in radians
 */
constexpr auto arcsecondsToRadians(double arcSeconds) -> double {
    using namespace std::numbers;
    constexpr double ARCSECONDS_IN_CIRCLE = 1296000.0;  // 360 * 60 * 60
    return arcSeconds * pi_v<double> / (ARCSECONDS_IN_CIRCLE / 2);
}

/**
 * @brief Convert radians to arcseconds.
 *
 * @param radians Angle in radians
 * @return Angle in arcseconds
 */
constexpr auto radiansToArcseconds(double radians) -> double {
    using namespace std::numbers;
    constexpr double ARCSECONDS_IN_CIRCLE = 1296000.0;  // 360 * 60 * 60
    return radians * (ARCSECONDS_IN_CIRCLE / 2) / pi_v<double>;
}

/**
 * @brief Estimate distance based on parallax.
 *
 * @tparam T Floating-point type
 * @param parsecs Distance in parsecs
 * @param parallaxRadius Parallax radius in same units as parsecs
 * @return Estimated distance
 */
template <std::floating_point T>
auto estimateDistance(T parsecs, T parallaxRadius) -> T {
    return parsecs / parallaxRadius;
}

/**
 * @brief Convert meters to astronomical units.
 *
 * @param meters Distance in meters
 * @return Distance in astronomical units
 */
constexpr auto metersToAU(double meters) -> double {
    return meters / ASTRONOMICAL_UNIT;
}

/**
 * @brief Calculate delta magnitude from magnitude ratio and spectrum.
 *
 * @tparam T Floating-point type
 * @param magnitudeRatio Magnitude ratio
 * @param spectrum Spectrum coefficients
 * @return Delta magnitude
 */
template <std::floating_point T>
auto calculateDeltaMagnitude(T magnitudeRatio, std::span<const T> spectrum)
    -> T {
    T deltaMagnitude = 0;
    for (size_t index = 0; index < spectrum.size(); ++index) {
        deltaMagnitude += spectrum[index] * magnitudeRatio;
    }
    return deltaMagnitude;
}

/**
 * @brief Calculate star mass from delta magnitude and reference size.
 *
 * @tparam T Floating-point type
 * @param deltaMagnitude Delta magnitude
 * @param referenceSize Reference size
 * @return Star mass
 */
template <std::floating_point T>
auto calculateStarMass(T deltaMagnitude, T referenceSize) -> T {
    constexpr T MAGNITUDE_SCALE_FACTOR = -2.5;
    return referenceSize * std::pow(static_cast<T>(10),
                                    deltaMagnitude / MAGNITUDE_SCALE_FACTOR);
}

/**
 * @brief Estimate orbital radius from wavelength shift.
 *
 * @tparam T Floating-point type
 * @param observedWavelength Observed wavelength
 * @param referenceWavelength Reference wavelength
 * @param period Orbital period
 * @return Estimated orbital radius
 */
template <std::floating_point T>
auto estimateOrbitRadius(T observedWavelength, T referenceWavelength, T period)
    -> T {
    return (observedWavelength - referenceWavelength) / period;
}

/**
 * @brief Estimate secondary mass from primary star data.
 *
 * @tparam T Floating-point type
 * @param starMass Primary star mass
 * @param starDrift Primary star drift
 * @param orbitRadius Orbital radius
 * @return Estimated secondary mass
 */
template <std::floating_point T>
auto estimateSecondaryMass(T starMass, T starDrift, T orbitRadius) -> T {
    return starMass * std::pow(starDrift / orbitRadius, static_cast<T>(2));
}

/**
 * @brief Estimate secondary size from primary star size and dropoff.
 *
 * @tparam T Floating-point type
 * @param starSize Primary star size
 * @param dropoffRatio Luminosity dropoff ratio
 * @return Estimated secondary size
 */
template <std::floating_point T>
auto estimateSecondarySize(T starSize, T dropoffRatio) -> T {
    return starSize * std::sqrt(dropoffRatio);
}

/**
 * @brief Calculate photon flux based on magnitude and filter parameters.
 *
 * @tparam T Floating-point type
 * @param relativeMagnitude Relative magnitude
 * @param filterBandwidth Filter bandwidth
 * @param wavelength Central wavelength
 * @param steradian Observed solid angle in steradians
 * @return Photon flux
 */
template <std::floating_point T>
auto calculatePhotonFlux(T relativeMagnitude, T filterBandwidth, T wavelength,
                         T steradian) -> T {
    constexpr T MAGNITUDE_BASE = 10.0;
    constexpr T MAGNITUDE_SCALE = 0.4;

    return std::pow(MAGNITUDE_BASE, relativeMagnitude * -MAGNITUDE_SCALE) *
           filterBandwidth * wavelength * steradian;
}

/**
 * @brief Calculate relative magnitude from photon flux.
 *
 * @tparam T Floating-point type
 * @param photonFlux Photon flux
 * @param filterBandwidth Filter bandwidth
 * @param wavelength Central wavelength
 * @param steradian Observed solid angle in steradians
 * @return Relative magnitude
 */
template <std::floating_point T>
auto calculateRelativeMagnitude(T photonFlux, T filterBandwidth, T wavelength,
                                T steradian) -> T {
    constexpr T MAGNITUDE_SCALE = 0.4;
    return std::log10(photonFlux / (calculateLuminousFlux(wavelength) *
                                    steradian * filterBandwidth)) /
           -MAGNITUDE_SCALE;
}

/**
 * @brief Calculate absolute magnitude from delta distance and delta magnitude.
 *
 * @tparam T Floating-point type
 * @param deltaDistance Delta distance
 * @param deltaMagnitude Delta magnitude
 * @return Absolute magnitude
 */
template <std::floating_point T>
auto calculateAbsoluteMagnitude(T deltaDistance, T deltaMagnitude) -> T {
    constexpr T STANDARD_PARSEC_EXPONENT = 1.0;
    constexpr T MAGNITUDE_DISTANCE_FACTOR = 5.0;

    return deltaMagnitude -
           MAGNITUDE_DISTANCE_FACTOR *
               (std::log10(deltaDistance) - STANDARD_PARSEC_EXPONENT);
}

/**
 * @brief Calculate 2D projection of baseline.
 *
 * @tparam T Floating-point type
 * @param altitude Altitude in degrees
 * @param azimuth Azimuth in degrees
 * @return 2D projection vector [x, y]
 */
template <std::floating_point T>
auto calculateBaseline2DProjection(T altitude, T azimuth) -> std::array<T, 2> {
    using namespace std::numbers;

    // Convert to radians
    T altitudeRad = altitude * pi_v<T> / 180.0;
    T azimuthRad = azimuth * pi_v<T> / 180.0;

    // Calculate projected components
    T x = std::cos(altitudeRad) * std::cos(azimuthRad);
    T y = std::cos(altitudeRad) * std::sin(azimuthRad);

    return {x, y};
}

/**
 * @brief Calculate baseline delay for interferometry.
 *
 * @tparam T Floating-point type
 * @param altitude Altitude in degrees
 * @param azimuth Azimuth in degrees
 * @param baseline 3D baseline vector [x, y, z]
 * @return Baseline delay
 */
template <std::floating_point T>
auto calculateBaselineDelay(T altitude, T azimuth,
                            const std::array<T, 3>& baseline) -> T {
    using namespace std::numbers;

    // Convert to radians
    T altitudeRad = altitude * pi_v<T> / 180.0;
    T azimuthRad = azimuth * pi_v<T> / 180.0;

    // Pre-calculate trig functions
    T cosAlt = std::cos(altitudeRad);
    T sinAlt = std::sin(altitudeRad);
    T cosAz = std::cos(azimuthRad);
    T sinAz = std::sin(azimuthRad);

    // Calculate delay
    T delay = baseline[0] * cosAlt * cosAz + baseline[1] * cosAlt * sinAz +
              baseline[2] * sinAlt;

    return delay;
}

/**
 * @brief Calculate precession matrix elements for J2000 to given epoch.
 *
 * @tparam T Floating-point type
 * @param epoch Julian date of target epoch
 * @return 3x3 precession matrix
 */
template <std::floating_point T>
auto calculatePrecessionMatrix(T epoch) -> std::array<std::array<T, 3>, 3> {
    constexpr T J2000_EPOCH = 2451545.0;   // J2000.0 epoch in JD
    constexpr T JULIAN_CENTURY = 36525.0;  // Days in a Julian century

    // Calculate time in Julian centuries from J2000
    T t = (epoch - J2000_EPOCH) / JULIAN_CENTURY;

    // Calculate precession angles in arcseconds
    T zeta = (2306.2181 + 1.39656 * t - 0.000139 * t * t) * t;
    T z = zeta + 0.30188 * t * t + 0.017998 * t * t * t;
    T theta = (2004.3109 - 0.85330 * t - 0.000217 * t * t) * t;

    // Convert to radians
    using namespace std::numbers;
    constexpr T ARCSECONDS_TO_RADIANS = pi_v<T> / (180.0 * 3600.0);
    zeta *= ARCSECONDS_TO_RADIANS;
    z *= ARCSECONDS_TO_RADIANS;
    theta *= ARCSECONDS_TO_RADIANS;

    // Pre-calculate trig functions
    T cosZeta = std::cos(zeta);
    T sinZeta = std::sin(zeta);
    T cosTheta = std::cos(theta);
    T sinTheta = std::sin(theta);
    T cosZ = std::cos(z);
    T sinZ = std::sin(z);

    // Create precession matrix with optimized calculations
    std::array<std::array<T, 3>, 3> P = {
        {{cosZeta * cosTheta * cosZ - sinZeta * sinZ,
          -cosZeta * cosTheta * sinZ - sinZeta * cosZ, -cosZeta * sinTheta},
         {sinZeta * cosTheta * cosZ + cosZeta * sinZ,
          -sinZeta * cosTheta * sinZ + cosZeta * cosZ, -sinZeta * sinTheta},
         {sinTheta * cosZ, -sinTheta * sinZ, cosTheta}}};

    return P;
}

/**
 * @brief Transform equatorial coordinates between epochs using precession.
 *
 * @tparam T Floating-point type
 * @param coords Celestial coordinates at source epoch
 * @param fromEpoch Source epoch as Julian date
 * @param toEpoch Target epoch as Julian date
 * @return Celestial coordinates at target epoch
 */
template <std::floating_point T>
auto precessEquatorial(const CelestialCoords<T>& coords, T fromEpoch, T toEpoch)
    -> CelestialCoords<T> {
    using namespace std::numbers;

    // Convert to Cartesian coordinates
    T raRad = coords.ra * pi_v<T> / 12.0;     // RA hours to radians
    T decRad = coords.dec * pi_v<T> / 180.0;  // Dec degrees to radians

    // Pre-calculate trig functions
    T cosDec = std::cos(decRad);
    T sinDec = std::sin(decRad);
    T cosRA = std::cos(raRad);
    T sinRA = std::sin(raRad);

    // Calculate Cartesian unit vector
    T x = cosDec * cosRA;
    T y = cosDec * sinRA;
    T z = sinDec;

    // Get precession matrices
    auto P2 = calculatePrecessionMatrix(toEpoch);

    // Apply precession directly (single matrix multiplication)
    T xp = P2[0][0] * x + P2[0][1] * y + P2[0][2] * z;
    T yp = P2[1][0] * x + P2[1][1] * y + P2[1][2] * z;
    T zp = P2[2][0] * x + P2[2][1] * y + P2[2][2] * z;

    // Convert back to spherical coordinates
    // Use atan2 for proper quadrant handling
    T dec =
        std::asin(std::clamp(zp, static_cast<T>(-1.0), static_cast<T>(1.0))) *
        180.0 / pi_v<T>;
    T ra = std::atan2(yp, xp) * 12.0 / pi_v<T>;

    // Normalize RA to [0, 24) hours
    ra = normalizeRightAscension(ra);

    return {ra, dec};
}

/**
 * @brief Convert equatorial coordinates to ecliptic coordinates.
 *
 * @tparam T Floating-point type
 * @param coords Equatorial coordinates (RA in hours, Dec in degrees)
 * @param obliquity Obliquity of the ecliptic in degrees
 * @return Pair of ecliptic longitude and latitude in degrees
 */
template <std::floating_point T>
auto convertEquatorialToEcliptic(const CelestialCoords<T>& coords, T obliquity)
    -> std::pair<T, T> {
    using namespace std::numbers;

    // Pre-calculate all required trig functions
    T decRad = coords.dec * pi_v<T> / 180.0;
    T raRad = coords.ra * pi_v<T> / 12.0;
    T oblRad = obliquity * pi_v<T> / 180.0;

    T sinDec = std::sin(decRad);
    T cosDec = std::cos(decRad);
    T sinRA = std::sin(raRad);
    T cosRA = std::cos(raRad);
    T sinObl = std::sin(oblRad);
    T cosObl = std::cos(oblRad);

    // Calculate ecliptic coordinates
    T sinLat = sinDec * cosObl - cosDec * sinObl * sinRA;
    // Clamp to [-1, 1] to avoid numerical issues
    sinLat = std::clamp(sinLat, static_cast<T>(-1.0), static_cast<T>(1.0));
    T latitude = std::asin(sinLat) * 180.0 / pi_v<T>;

    // Use atan2 for proper quadrant handling in longitude calculation
    T longitude =
        std::atan2(sinRA * cosDec * cosObl + sinDec * sinObl, cosDec * cosRA) *
        180.0 / pi_v<T>;

    // Normalize longitude to [0, 360)
    longitude = normalizeAngle360(longitude);

    return {longitude, latitude};
}

}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_CROODS_HPP
