/*
 * coordinate_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Coordinate Manager Implementation

This component manages telescope coordinate systems, transformations,
location/time settings, and coordinate validation.

*************************************************/

#include "coordinate_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "atom/utils/string.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace lithium::device::indi::telescope::components {

CoordinateManager::CoordinateManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }

    // Initialize default location (Greenwich)
    currentLocation_.latitude = 51.4769;
    currentLocation_.longitude = -0.0005;
    currentLocation_.elevation = 46.0;
    currentLocation_.name = "Greenwich";
    locationValid_ = true;

    // Initialize time
    lastTimeUpdate_ = std::chrono::system_clock::now();
}

CoordinateManager::~CoordinateManager() {
    shutdown();
}

bool CoordinateManager::initialize() {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (initialized_) {
        logWarning("Coordinate manager already initialized");
        return true;
    }

    if (!hardware_->isConnected()) {
        logError("Hardware interface not connected");
        return false;
    }

    try {
        // Get current location from hardware
        auto locationData = hardware_->getProperty("GEOGRAPHIC_COORD");
        if (locationData && !locationData->empty()) {
            auto latElement = locationData->find("LAT");
            auto lonElement = locationData->find("LONG");
            auto elevElement = locationData->find("ELEV");

            if (latElement != locationData->end() && lonElement != locationData->end()) {
                currentLocation_.latitude = std::stod(latElement->second.value);
                currentLocation_.longitude = std::stod(lonElement->second.value);
                if (elevElement != locationData->end()) {
                    currentLocation_.elevation = std::stod(elevElement->second.value);
                }
                locationValid_ = true;
            }
        }

        // Get current time from hardware
        auto timeData = hardware_->getProperty("TIME_UTC");
        if (timeData && !timeData->empty()) {
            auto timeElement = timeData->find("UTC");
            if (timeElement != timeData->end()) {
                // Parse time string and set lastTimeUpdate_
                // Implementation depends on time format from hardware
                lastTimeUpdate_ = std::chrono::system_clock::now();
            }
        }

        // Update coordinate status
        updateCoordinateStatus();

        initialized_ = true;
        logInfo("Coordinate manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Failed to initialize coordinate manager: " + std::string(e.what()));
        return false;
    }
}

bool CoordinateManager::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!initialized_) {
        return true;
    }

    initialized_ = false;
    logInfo("Coordinate manager shut down successfully");
    return true;
}

std::optional<EquatorialCoordinates> CoordinateManager::getCurrentRADEC() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!coordinatesValid_) {
        return std::nullopt;
    }

    return currentStatus_.currentRADEC;
}

std::optional<EquatorialCoordinates> CoordinateManager::getTargetRADEC() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);
    return currentStatus_.targetRADEC;
}

std::optional<HorizontalCoordinates> CoordinateManager::getCurrentAltAz() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!coordinatesValid_) {
        return std::nullopt;
    }

    return currentStatus_.currentAltAz;
}

std::optional<HorizontalCoordinates> CoordinateManager::getTargetAltAz() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);
    return currentStatus_.targetAltAz;
}

bool CoordinateManager::setTargetRADEC(const EquatorialCoordinates& coords) {
    if (!validateRADEC(coords)) {
        logError("Invalid RA/DEC coordinates");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    try {
        currentStatus_.targetRADEC = coords;

        // Convert to Alt/Az for display
        auto altAz = raDECToAltAz(coords);
        if (altAz) {
            currentStatus_.targetAltAz = *altAz;
        }

        // Sync to hardware
        syncCoordinatesToHardware();

        logInfo("Target coordinates set to RA=" + std::to_string(coords.ra) +
               ", DEC=" + std::to_string(coords.dec));
        return true;

    } catch (const std::exception& e) {
        logError("Error setting target coordinates: " + std::string(e.what()));
        return false;
    }
}

bool CoordinateManager::setTargetRADEC(double ra, double dec) {
    EquatorialCoordinates coords;
    coords.ra = ra;
    coords.dec = dec;
    return setTargetRADEC(coords);
}

bool CoordinateManager::setTargetAltAz(const HorizontalCoordinates& coords) {
    if (!validateAltAz(coords)) {
        logError("Invalid Alt/Az coordinates");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    try {
        currentStatus_.targetAltAz = coords;

        // Convert to RA/DEC
        auto raDEC = altAzToRADEC(coords);
        if (raDEC) {
            currentStatus_.targetRADEC = *raDEC;
            syncCoordinatesToHardware();
        }

        logInfo("Target coordinates set to Az=" + std::to_string(coords.azimuth) +
               ", Alt=" + std::to_string(coords.altitude));
        return true;

    } catch (const std::exception& e) {
        logError("Error setting target Alt/Az coordinates: " + std::string(e.what()));
        return false;
    }
}

bool CoordinateManager::setTargetAltAz(double azimuth, double altitude) {
    HorizontalCoordinates coords;
    coords.azimuth = azimuth;
    coords.altitude = altitude;
    return setTargetAltAz(coords);
}

std::optional<HorizontalCoordinates> CoordinateManager::raDECToAltAz(const EquatorialCoordinates& radec) const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!locationValid_) {
        logError("Location not set - cannot perform coordinate transformation");
        return std::nullopt;
    }

    try {
        double lst = getLocalSiderealTime();
        return equatorialToHorizontal(radec, lst, currentLocation_.latitude);
    } catch (const std::exception& e) {
        logError("Error in RA/DEC to Alt/Az transformation: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::altAzToRADEC(const HorizontalCoordinates& altaz) const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!locationValid_) {
        logError("Location not set - cannot perform coordinate transformation");
        return std::nullopt;
    }

    try {
        double lst = getLocalSiderealTime();
        return horizontalToEquatorial(altaz, lst, currentLocation_.latitude);
    } catch (const std::exception& e) {
        logError("Error in Alt/Az to RA/DEC transformation: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CoordinateManager::setLocation(const GeographicLocation& location) {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    // Validate location
    if (location.latitude < -90.0 || location.latitude > 90.0) {
        logError("Invalid latitude: " + std::to_string(location.latitude));
        return false;
    }

    if (location.longitude < -180.0 || location.longitude > 180.0) {
        logError("Invalid longitude: " + std::to_string(location.longitude));
        return false;
    }

    try {
        currentLocation_ = location;
        locationValid_ = true;

        // Sync to hardware
        syncLocationToHardware();

        // Update coordinate calculations
        updateCoordinateStatus();

        logInfo("Location set to: " + location.name +
               " (Lat: " + std::to_string(location.latitude) +
               ", Lon: " + std::to_string(location.longitude) + ")");
        return true;

    } catch (const std::exception& e) {
        logError("Error setting location: " + std::string(e.what()));
        return false;
    }
}

std::optional<GeographicLocation> CoordinateManager::getLocation() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    if (!locationValid_) {
        return std::nullopt;
    }

    return currentLocation_;
}

bool CoordinateManager::setTime(const std::chrono::system_clock::time_point& time) {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);

    try {
        lastTimeUpdate_ = time;
        currentStatus_.currentTime = time;
        currentStatus_.julianDate = calculateJulianDate(time);
        currentStatus_.localSiderealTime = getLocalSiderealTime();

        // Sync to hardware
        syncTimeToHardware();

        logInfo("Time updated");
        return true;

    } catch (const std::exception& e) {
        logError("Error setting time: " + std::string(e.what()));
        return false;
    }
}

std::optional<std::chrono::system_clock::time_point> CoordinateManager::getTime() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);
    return lastTimeUpdate_;
}

bool CoordinateManager::syncTimeWithSystem() {
    return setTime(std::chrono::system_clock::now());
}

double CoordinateManager::getJulianDate() const {
    return calculateJulianDate(std::chrono::system_clock::now());
}

double CoordinateManager::getLocalSiderealTime() const {
    if (!locationValid_) {
        return 0.0;
    }

    double jd = getJulianDate();
    return calculateLocalSiderealTime(jd, currentLocation_.longitude);
}

double CoordinateManager::getGreenwichSiderealTime() const {
    double jd = getJulianDate();
    return calculateGreenwichSiderealTime(jd);
}

std::chrono::system_clock::time_point CoordinateManager::getLocalTime() const {
    return std::chrono::system_clock::now();
}

bool CoordinateManager::validateRADEC(const EquatorialCoordinates& coords) const {
    return isValidRA(coords.ra) && isValidDEC(coords.dec);
}

bool CoordinateManager::validateAltAz(const HorizontalCoordinates& coords) const {
    return isValidAzimuth(coords.azimuth) && isValidAltitude(coords.altitude);
}

bool CoordinateManager::isAboveHorizon(const EquatorialCoordinates& coords) const {
    auto altAz = raDECToAltAz(coords);
    return altAz && altAz->altitude > 0.0;
}

CoordinateManager::CoordinateStatus CoordinateManager::getCoordinateStatus() const {
    std::lock_guard<std::recursive_mutex> lock(coordinateMutex_);
    return currentStatus_;
}

bool CoordinateManager::areCoordinatesValid() const {
    return coordinatesValid_;
}

std::tuple<int, int, double> CoordinateManager::degreesToDMS(double degrees) const {
    bool negative = degrees < 0;
    degrees = std::abs(degrees);

    int deg = static_cast<int>(degrees);
    double minutes = (degrees - deg) * 60.0;
    int min = static_cast<int>(minutes);
    double sec = (minutes - min) * 60.0;

    if (negative) deg = -deg;

    return std::make_tuple(deg, min, sec);
}

std::tuple<int, int, double> CoordinateManager::degreesToHMS(double degrees) const {
    degrees /= DEGREES_PER_HOUR;  // Convert to hours

    int hours = static_cast<int>(degrees);
    double minutes = (degrees - hours) * 60.0;
    int min = static_cast<int>(minutes);
    double sec = (minutes - min) * 60.0;

    return std::make_tuple(hours, min, sec);
}

double CoordinateManager::dmsToDecimal(int degrees, int minutes, double seconds) const {
    double result = std::abs(degrees) + minutes / 60.0 + seconds / 3600.0;
    return degrees < 0 ? -result : result;
}

double CoordinateManager::hmsToDecimal(int hours, int minutes, double seconds) const {
    return (hours + minutes / 60.0 + seconds / 3600.0) * DEGREES_PER_HOUR;
}

double CoordinateManager::angularSeparation(const EquatorialCoordinates& coord1,
                                          const EquatorialCoordinates& coord2) const {
    // Convert to radians
    double ra1 = coord1.ra * M_PI / 12.0;    // RA in hours to radians
    double dec1 = coord2.dec * M_PI / 180.0; // DEC in degrees to radians
    double ra2 = coord2.ra * M_PI / 12.0;
    double dec2 = coord2.dec * M_PI / 180.0;

    // Use spherical law of cosines
    double cos_sep = std::sin(dec1) * std::sin(dec2) +
                     std::cos(dec1) * std::cos(dec2) * std::cos(ra1 - ra2);

    // Clamp to valid range to avoid numerical errors
    cos_sep = std::max(-1.0, std::min(1.0, cos_sep));

    return std::acos(cos_sep) * 180.0 / M_PI; // Return in degrees
}

void CoordinateManager::updateCoordinateStatus() {
    if (!initialized_ || !hardware_->isConnected()) {
        coordinatesValid_ = false;
        return;
    }

    try {
        // Get current coordinates from hardware
        auto eqData = hardware_->getProperty("EQUATORIAL_EOD_COORD");
        if (eqData && !eqData->empty()) {
            auto raElement = eqData->find("RA");
            auto decElement = eqData->find("DEC");

            if (raElement != eqData->end() && decElement != eqData->end()) {
                currentStatus_.currentRADEC.ra = std::stod(raElement->second.value);
                currentStatus_.currentRADEC.dec = std::stod(decElement->second.value);
                coordinatesValid_ = true;
            }
        }

        // Calculate derived coordinates
        calculateDerivedCoordinates();

        // Update time information
        currentStatus_.currentTime = std::chrono::system_clock::now();
        currentStatus_.julianDate = getJulianDate();
        currentStatus_.localSiderealTime = getLocalSiderealTime();
        currentStatus_.location = currentLocation_;
        currentStatus_.coordinatesValid = coordinatesValid_;

        // Trigger callback if available
        if (coordinateUpdateCallback_) {
            coordinateUpdateCallback_(currentStatus_);
        }

    } catch (const std::exception& e) {
        logError("Error updating coordinate status: " + std::string(e.what()));
        coordinatesValid_ = false;
    }
}

void CoordinateManager::calculateDerivedCoordinates() {
    if (!coordinatesValid_ || !locationValid_) {
        return;
    }

    // Calculate current Alt/Az from current RA/DEC
    auto altAz = raDECToAltAz(currentStatus_.currentRADEC);
    if (altAz) {
        currentStatus_.currentAltAz = *altAz;
    }
}

double CoordinateManager::calculateJulianDate(const std::chrono::system_clock::time_point& time) const {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto tm = *std::gmtime(&time_t);

    int year = tm.tm_year + 1900;
    int month = tm.tm_mon + 1;
    int day = tm.tm_mday;

    // Julian day calculation
    if (month <= 2) {
        year--;
        month += 12;
    }

    int a = year / 100;
    int b = 2 - a + a / 4;

    double jd = std::floor(365.25 * (year + 4716)) +
                std::floor(30.6001 * (month + 1)) +
                day + b - 1524.5;

    // Add time of day
    double dayFraction = (tm.tm_hour + tm.tm_min / 60.0 + tm.tm_sec / 3600.0) / 24.0;

    return jd + dayFraction;
}

double CoordinateManager::calculateLocalSiderealTime(double jd, double longitude) const {
    double gst = calculateGreenwichSiderealTime(jd);
    double lst = gst + longitude / DEGREES_PER_HOUR;

    // Normalize to 0-24 hours
    while (lst < 0) lst += 24.0;
    while (lst >= 24.0) lst -= 24.0;

    return lst;
}

double CoordinateManager::calculateGreenwichSiderealTime(double jd) const {
    double t = (jd - J2000_EPOCH) / 36525.0;

    // Greenwich mean sidereal time at 0h UT
    double gst0 = 280.46061837 + 360.98564736629 * (jd - J2000_EPOCH) +
                  0.000387933 * t * t - t * t * t / 38710000.0;

    // Normalize to 0-360 degrees
    while (gst0 < 0) gst0 += 360.0;
    while (gst0 >= 360.0) gst0 -= 360.0;

    return gst0 / DEGREES_PER_HOUR; // Convert to hours
}

HorizontalCoordinates CoordinateManager::equatorialToHorizontal(const EquatorialCoordinates& eq,
                                                               double lst, double latitude) const {
    // Convert to radians
    double ha = (lst - eq.ra) * M_PI / 12.0;  // Hour angle
    double dec = eq.dec * M_PI / 180.0;
    double lat = latitude * M_PI / 180.0;

    // Calculate altitude
    double sin_alt = std::sin(dec) * std::sin(lat) +
                     std::cos(dec) * std::cos(lat) * std::cos(ha);
    double altitude = std::asin(sin_alt) * 180.0 / M_PI;

    // Calculate azimuth
    double cos_az = (std::sin(dec) - std::sin(lat) * sin_alt) /
                    (std::cos(lat) * std::cos(altitude * M_PI / 180.0));
    double sin_az = -std::sin(ha) * std::cos(dec) /
                     std::cos(altitude * M_PI / 180.0);

    double azimuth = std::atan2(sin_az, cos_az) * 180.0 / M_PI;

    // Normalize azimuth to 0-360 degrees
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;

    HorizontalCoordinates result;
    result.azimuth = azimuth;
    result.altitude = altitude;

    return result;
}

EquatorialCoordinates CoordinateManager::horizontalToEquatorial(const HorizontalCoordinates& hz,
                                                              double lst, double latitude) const {
    // Convert to radians
    double az = hz.azimuth * M_PI / 180.0;
    double alt = hz.altitude * M_PI / 180.0;
    double lat = latitude * M_PI / 180.0;

    // Calculate declination
    double sin_dec = std::sin(alt) * std::sin(lat) +
                     std::cos(alt) * std::cos(lat) * std::cos(az);
    double declination = std::asin(sin_dec) * 180.0 / M_PI;

    // Calculate hour angle
    double cos_ha = (std::sin(alt) - std::sin(lat) * sin_dec) /
                    (std::cos(lat) * std::cos(declination * M_PI / 180.0));
    double sin_ha = -std::sin(az) * std::cos(alt) /
                     std::cos(declination * M_PI / 180.0);

    double ha = std::atan2(sin_ha, cos_ha) * 12.0 / M_PI; // Convert to hours

    // Calculate RA
    double ra = lst - ha;

    // Normalize RA to 0-24 hours
    while (ra < 0) ra += 24.0;
    while (ra >= 24.0) ra -= 24.0;

    EquatorialCoordinates result;
    result.ra = ra;
    result.dec = declination;

    return result;
}

bool CoordinateManager::isValidRA(double ra) const {
    return ra >= 0.0 && ra < 24.0;
}

bool CoordinateManager::isValidDEC(double dec) const {
    return dec >= -90.0 && dec <= 90.0;
}

bool CoordinateManager::isValidAzimuth(double az) const {
    return az >= 0.0 && az < 360.0;
}

bool CoordinateManager::isValidAltitude(double alt) const {
    return alt >= -90.0 && alt <= 90.0;
}

void CoordinateManager::syncCoordinatesToHardware() {
    try {
        std::map<std::string, PropertyElement> elements;
        elements["RA"] = {std::to_string(currentStatus_.targetRADEC.ra), ""};
        elements["DEC"] = {std::to_string(currentStatus_.targetRADEC.dec), ""};

        hardware_->sendCommand("EQUATORIAL_EOD_COORD", elements);

    } catch (const std::exception& e) {
        logError("Error syncing coordinates to hardware: " + std::string(e.what()));
    }
}

void CoordinateManager::syncLocationToHardware() {
    try {
        std::map<std::string, PropertyElement> elements;
        elements["LAT"] = {std::to_string(currentLocation_.latitude), ""};
        elements["LONG"] = {std::to_string(currentLocation_.longitude), ""};
        elements["ELEV"] = {std::to_string(currentLocation_.elevation), ""};

        hardware_->sendCommand("GEOGRAPHIC_COORD", elements);

    } catch (const std::exception& e) {
        logError("Error syncing location to hardware: " + std::string(e.what()));
    }
}

void CoordinateManager::syncTimeToHardware() {
    try {
        auto time_t = std::chrono::system_clock::to_time_t(lastTimeUpdate_);
        auto tm = *std::gmtime(&time_t);

        char timeString[64];
        std::strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &tm);

        std::map<std::string, PropertyElement> elements;
        elements["UTC"] = {std::string(timeString), ""};

        hardware_->sendCommand("TIME_UTC", elements);

    } catch (const std::exception& e) {
        logError("Error syncing time to hardware: " + std::string(e.what()));
    }
}

void CoordinateManager::logInfo(const std::string& message) {
    LOG_F(INFO, "[CoordinateManager] %s", message.c_str());
}

void CoordinateManager::logWarning(const std::string& message) {
    LOG_F(WARNING, "[CoordinateManager] %s", message.c_str());
}

void CoordinateManager::logError(const std::string& message) {
    LOG_F(ERROR, "[CoordinateManager] %s", message.c_str());
}

} // namespace lithium::device::indi::telescope::components
