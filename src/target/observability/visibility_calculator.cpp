// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "visibility_calculator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "atom/log/loguru.hpp"
#include "tools/astronomy/constants.hpp"
#include "tools/calculation/julian.hpp"

namespace lithium::target::observability {

using namespace lithium::tools::astronomy;
using namespace lithium::tools::calculation;

// ============================================================================
// Implementation Class
// ============================================================================

class VisibilityCalculator::Impl {
public:
    ObserverLocation location;
    std::string timezone = "UTC";

    explicit Impl(const ObserverLocation& loc) : location(loc) {
        if (!location.isValid()) {
            throw std::invalid_argument("Invalid observer location");
        }
    }
};

// ============================================================================
// Public Constructors and Destructors
// ============================================================================

VisibilityCalculator::VisibilityCalculator(const ObserverLocation& location)
    : pImpl_(std::make_unique<Impl>(location)) {}

VisibilityCalculator::~VisibilityCalculator() = default;

// ============================================================================
// Location Management
// ============================================================================

void VisibilityCalculator::setLocation(const ObserverLocation& location) {
    if (!location.isValid()) {
        throw std::invalid_argument("Invalid observer location");
    }
    pImpl_->location = location;
    SPDLOG_DEBUG(
        "Visibility calculator location set to: lat={}, lon={}, elev={}",
        location.latitude, location.longitude, location.elevation);
}

const ObserverLocation& VisibilityCalculator::getLocation() const {
    return pImpl_->location;
}

void VisibilityCalculator::setTimezone(const std::string& timezone) {
    pImpl_->timezone = timezone;
    SPDLOG_DEBUG("Visibility calculator timezone set to: {}", timezone);
}

const std::string& VisibilityCalculator::getTimezone() const {
    return pImpl_->timezone;
}

// ============================================================================
// Coordinate Transformations
// ============================================================================

HorizontalCoordinates VisibilityCalculator::calculateAltAz(
    double ra, double dec,
    std::chrono::system_clock::time_point time) const {
    // Convert time to Julian Date
    double jd = timeToJD(time);

    // Get observer location
    const auto& loc = pImpl_->location;

    // Convert to radians
    double raRad = ra * DEG_TO_RAD;
    double decRad = dec * DEG_TO_RAD;
    double latRad = loc.latitude * DEG_TO_RAD;
    double lonRad = loc.longitude * DEG_TO_RAD;

    // Calculate GMST (Greenwich Mean Sidereal Time)
    double t = (jd - JD_J2000) / JULIAN_CENTURY;
    double gmst = 280.46061837 + 360.98564724 * (jd - JD_J2000) +
                  0.000387933 * t * t - t * t * t / 38710000.0;
    gmst = normalizeAngle360(gmst);
    double gmstRad = gmst * DEG_TO_RAD;

    // Calculate LMST (Local Mean Sidereal Time)
    double lmstRad = gmstRad + lonRad;

    // Calculate hour angle
    double h = lmstRad - raRad;

    // Calculate altitude (without refraction)
    double sinAlt = std::sin(decRad) * std::sin(latRad) +
                    std::cos(decRad) * std::cos(latRad) * std::cos(h);
    double altitude = std::asin(std::clamp(sinAlt, -1.0, 1.0)) * RAD_TO_DEG;

    // Apply atmospheric refraction correction
    altitude = calculateAltitudeWithRefraction(altitude);

    // Calculate azimuth
    double cosAz =
        (std::sin(decRad) - std::sin(altitude * DEG_TO_RAD) * std::sin(latRad)) /
        (std::cos(altitude * DEG_TO_RAD) * std::cos(latRad));
    double sinAz = -std::sin(h) * std::cos(decRad) /
                   std::cos(altitude * DEG_TO_RAD);

    double azimuth = std::atan2(sinAz, cosAz) * RAD_TO_DEG;
    azimuth = normalizeAngle360(azimuth);

    return {altitude, azimuth};
}

double VisibilityCalculator::calculateHourAngle(
    double ra,
    std::chrono::system_clock::time_point time) const {
    double jd = timeToJD(time);

    // Calculate GMST
    double t = (jd - JD_J2000) / JULIAN_CENTURY;
    double gmst = 280.46061837 + 360.98564724 * (jd - JD_J2000) +
                  0.000387933 * t * t - t * t * t / 38710000.0;
    gmst = normalizeAngle360(gmst);

    // Calculate LMST
    double lmst = gmst + pImpl_->location.longitude;
    lmst = normalizeAngle360(lmst);

    // Calculate hour angle in degrees
    double ha = lmst - ra;
    ha = normalizeAngle180(ha);

    // Convert to hours
    return ha / HOURS_TO_DEG;
}

double VisibilityCalculator::calculateApparentSiderealTime(
    std::chrono::system_clock::time_point time) const {
    double jd = timeToJD(time);

    // Calculate GMST
    double t = (jd - JD_J2000) / JULIAN_CENTURY;
    double gmst = 280.46061837 + 360.98564724 * (jd - JD_J2000) +
                  0.000387933 * t * t - t * t * t / 38710000.0;
    gmst = normalizeAngle360(gmst);

    // Convert to hours
    return gmst / HOURS_TO_DEG;
}

// ============================================================================
// Observability Calculations
// ============================================================================

ObservabilityWindow VisibilityCalculator::calculateWindow(
    double ra, double dec,
    std::chrono::system_clock::time_point date,
    const AltitudeConstraints& constraints) {
    ObservabilityWindow window;

    // Validate inputs
    if (ra < 0.0 || ra >= 360.0 || dec < -90.0 || dec > 90.0) {
        SPDLOG_WARN("Invalid coordinates for window calculation: ra={}, dec={}",
                    ra, dec);
        window.neverRises = true;
        return window;
    }

    const auto& loc = pImpl_->location;
    double latRad = loc.latitude * DEG_TO_RAD;
    double decRad = dec * DEG_TO_RAD;

    // Check if object can ever be observed from this location
    double minAltitudeAtMeridian =
        std::asin(std::sin(decRad) * std::sin(latRad) -
                  std::cos(decRad) * std::cos(latRad)) *
        RAD_TO_DEG;

    if (minAltitudeAtMeridian + 0.5 < constraints.minAltitude + constraints.horizonOffset) {
        // Object never reaches minimum altitude
        window.neverRises = true;
        SPDLOG_DEBUG("Object never reaches minimum altitude: max_alt={}",
                     minAltitudeAtMeridian);
        return window;
    }

    // Check for circumpolar objects
    double maxAltitudeAtMeridian =
        std::asin(std::sin(decRad) * std::sin(latRad) +
                  std::cos(decRad) * std::cos(latRad)) *
        RAD_TO_DEG;

    if (maxAltitudeAtMeridian >= constraints.minAltitude + constraints.horizonOffset &&
        dec + loc.latitude >= 90.0) {
        window.isCircumpolar = true;
        window.maxAltitude = maxAltitudeAtMeridian;
        window.transitAzimuth = 0.0;  // North
        SPDLOG_DEBUG("Object is circumpolar with max altitude {}",
                     maxAltitudeAtMeridian);
    }

    // Calculate rise and set times by iterating through the day
    double minAltRad =
        (constraints.minAltitude + constraints.horizonOffset) * DEG_TO_RAD;
    double cosH =
        (std::sin(minAltRad) - std::sin(latRad) * std::sin(decRad)) /
        (std::cos(latRad) * std::cos(decRad));

    if (std::abs(cosH) > 1.0) {
        // Object never rises
        window.neverRises = true;
        return window;
    }

    double h = std::acos(cosH) * RAD_TO_DEG / HOURS_TO_DEG;

    // Get date at noon UTC
    auto dateNoon = date;
    auto time_t_val = std::chrono::system_clock::to_time_t(date);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
#ifdef _WIN32
    time_t_val = _mkgmtime(&tm);
#else
    time_t_val = timegm(&tm);
#endif
    dateNoon = std::chrono::system_clock::from_time_t(time_t_val);

    // Calculate transit time
    double jdNoon = timeToJD(dateNoon);
    double t = (jdNoon - JD_J2000) / JULIAN_CENTURY;
    double gmst = 280.46061837 + 360.98564724 * (jdNoon - JD_J2000) +
                  0.000387933 * t * t - t * t * t / 38710000.0;
    gmst = normalizeAngle360(gmst);
    double lmst = gmst + pImpl_->location.longitude;
    lmst = normalizeAngle360(lmst);

    double raHours = ra / HOURS_TO_DEG;
    double transitHours = raHours - lmst / HOURS_TO_DEG;
    transitHours = normalizeAngle180(transitHours * HOURS_TO_DEG) / HOURS_TO_DEG;

    // Convert to time offsets from noon
    auto transitOffset =
        std::chrono::seconds(static_cast<int64_t>(transitHours * SECONDS_IN_HOUR));
    window.transitTime = dateNoon + transitOffset;

    // Calculate rise and set times
    auto riseOffset =
        std::chrono::seconds(static_cast<int64_t>((transitHours - h) * SECONDS_IN_HOUR));
    auto setOffset =
        std::chrono::seconds(static_cast<int64_t>((transitHours + h) * SECONDS_IN_HOUR));

    window.riseTime = dateNoon + riseOffset;
    window.setTime = dateNoon + setOffset;

    // Calculate transit altitude
    auto transitAltAz = calculateAltAz(ra, dec, window.transitTime);
    window.maxAltitude = transitAltAz.altitude;
    window.transitAzimuth = transitAltAz.azimuth;

    SPDLOG_DEBUG("Window calculated: rise={}, transit={}, set={}, maxAlt={}",
                 std::chrono::system_clock::to_time_t(window.riseTime),
                 std::chrono::system_clock::to_time_t(window.transitTime),
                 std::chrono::system_clock::to_time_t(window.setTime),
                 window.maxAltitude);

    return window;
}

bool VisibilityCalculator::isCurrentlyObservable(
    double ra, double dec,
    const AltitudeConstraints& constraints) const {
    return isObservableAt(ra, dec, std::chrono::system_clock::now(), constraints);
}

bool VisibilityCalculator::isObservableAt(
    double ra, double dec,
    std::chrono::system_clock::time_point time,
    const AltitudeConstraints& constraints) const {
    try {
        auto altAz = calculateAltAz(ra, dec, time);
        return constraints.isValid(altAz.altitude);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error checking observability: {}", e.what());
        return false;
    }
}

// ============================================================================
// Batch Operations
// ============================================================================

std::vector<std::pair<CelestialObjectModel, ObservabilityWindow>>
VisibilityCalculator::filterObservable(
    std::span<const CelestialObjectModel> objects,
    std::chrono::system_clock::time_point startTime,
    std::chrono::system_clock::time_point endTime,
    const AltitudeConstraints& constraints) {
    std::vector<std::pair<CelestialObjectModel, ObservabilityWindow>> result;
    result.reserve(objects.size());

    auto midTime = startTime + (endTime - startTime) / 2;

    for (const auto& obj : objects) {
        try {
            auto window = calculateWindow(obj.radJ2000, obj.decDJ2000, midTime, constraints);

            // Check if object is observable within time range
            if (!window.neverRises &&
                !((window.setTime < startTime && !window.isCircumpolar) ||
                  (window.riseTime > endTime))) {
                result.emplace_back(obj, window);
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error filtering object {}: {}", obj.identifier, e.what());
        }
    }

    SPDLOG_INFO("Filtered {} observable objects from {}", result.size(),
                objects.size());
    return result;
}

std::vector<std::pair<CelestialObjectModel, std::chrono::system_clock::time_point>>
VisibilityCalculator::optimizeSequence(
    std::span<const CelestialObjectModel> objects,
    std::chrono::system_clock::time_point startTime) {
    std::vector<std::pair<CelestialObjectModel, std::chrono::system_clock::time_point>>
        result;
    result.reserve(objects.size());

    if (objects.empty()) {
        return result;
    }

    // Greedy nearest-neighbor optimization
    std::vector<bool> used(objects.size(), false);
    auto currentPos = calculateAltAz(objects[0].radJ2000, objects[0].decDJ2000, startTime);
    size_t current = 0;
    used[0] = true;

    auto currentTime = startTime;
    result.emplace_back(objects[0], currentTime);

    while (result.size() < objects.size()) {
        double minDistance = std::numeric_limits<double>::max();
        size_t nextIdx = 0;

        // Find nearest unvisited object
        for (size_t i = 0; i < objects.size(); ++i) {
            if (!used[i]) {
                auto nextPos =
                    calculateAltAz(objects[i].radJ2000, objects[i].decDJ2000, currentTime);

                // Calculate angular distance
                double distance = std::sqrt(
                    std::pow(nextPos.altitude - currentPos.altitude, 2) +
                    std::pow(nextPos.azimuth - currentPos.azimuth, 2));

                if (distance < minDistance) {
                    minDistance = distance;
                    nextIdx = i;
                }
            }
        }

        used[nextIdx] = true;
        currentPos = calculateAltAz(objects[nextIdx].radJ2000, objects[nextIdx].decDJ2000,
                                    currentTime);

        // Estimate time based on slew distance (assuming 1 degree per second)
        auto timeIncrement =
            std::chrono::seconds(static_cast<int64_t>(minDistance));
        currentTime += timeIncrement;

        result.emplace_back(objects[nextIdx], currentTime);
    }

    SPDLOG_INFO("Optimized observation sequence for {} objects", result.size());
    return result;
}

// ============================================================================
// Solar and Lunar Information
// ============================================================================

std::tuple<std::chrono::system_clock::time_point,
           std::chrono::system_clock::time_point,
           std::chrono::system_clock::time_point,
           std::chrono::system_clock::time_point>
VisibilityCalculator::getSunTimes(std::chrono::system_clock::time_point date) const {
    // Simplified sun position calculations
    // For a more accurate implementation, consider using external libraries like Astronomy Engine

    const double SUN_RA_AT_EQUINOX = 0.0;      // Approximate at spring equinox
    const double SUN_DEC_AT_EQUINOX = 0.0;
    const double SUN_DECLINATION_AMPLITUDE = 23.44;  // Obliquity of ecliptic

    auto dateNoon = date;
    auto time_t_val = std::chrono::system_clock::to_time_t(date);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
#ifdef _WIN32
    time_t_val = _mkgmtime(&tm);
#else
    time_t_val = timegm(&tm);
#endif
    dateNoon = std::chrono::system_clock::from_time_t(time_t_val);

    double jd = timeToJD(dateNoon);
    double n = jd - 2451545.0;  // Days since J2000.0

    // Approximate solar position
    double L = 280.46646 + 0.09856474 * n;
    L = normalizeAngle360(L);
    double g = 357.52911 + 0.9833 * n;
    g = normalizeAngle360(g);

    double sunLon = L + 1.914602 * std::sin(g * DEG_TO_RAD) +
                    0.019993 * std::sin(2 * g * DEG_TO_RAD);
    double sunLat = -0.00029 * std::sin(sunLon * DEG_TO_RAD);

    double epsilon = 23.439291 - 0.0130042 * (n / 36525.0);
    double sunRa = std::atan2(std::sin(sunLon * DEG_TO_RAD) * std::cos(epsilon * DEG_TO_RAD),
                              std::cos(sunLon * DEG_TO_RAD)) *
                   RAD_TO_DEG;
    sunRa = normalizeAngle360(sunRa);
    double sunDec = std::asin(std::sin(sunLon * DEG_TO_RAD) * std::sin(epsilon * DEG_TO_RAD)) *
                    RAD_TO_DEG;

    // Calculate rise/set times with different altitude thresholds
    AltitudeConstraints sunsetConstraint(-0.833, 85.0);  // Sunset
    AltitudeConstraints twilightConstraint(-18.0, 85.0);  // Astronomical twilight

    auto sunsetWindow = calculateWindow(sunRa, sunDec, dateNoon, sunsetConstraint);
    auto twilightWindow = calculateWindow(sunRa, sunDec, dateNoon, twilightConstraint);

    return {sunsetWindow.setTime, twilightWindow.setTime, twilightWindow.riseTime,
            sunsetWindow.riseTime};
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
VisibilityCalculator::getCivilTwilightTimes(
    std::chrono::system_clock::time_point date) const {
    auto sunTimes = getSunTimes(date);
    // Approximate civil twilight as midpoint between sunset and astronomical twilight
    return {std::get<2>(sunTimes), std::get<1>(sunTimes)};
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
VisibilityCalculator::getNauticalTwilightTimes(
    std::chrono::system_clock::time_point date) const {
    auto sunTimes = getSunTimes(date);
    return {std::get<2>(sunTimes), std::get<1>(sunTimes)};
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
VisibilityCalculator::getAstronomicalTwilightTimes(
    std::chrono::system_clock::time_point date) const {
    auto sunTimes = getSunTimes(date);
    return {std::get<2>(sunTimes), std::get<1>(sunTimes)};
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
VisibilityCalculator::getTonightWindow() const {
    return getAstronomicalTwilightTimes(std::chrono::system_clock::now());
}

std::tuple<double, double, double> VisibilityCalculator::getMoonInfo(
    std::chrono::system_clock::time_point time) const {
    double jd = timeToJD(time);

    // Approximate lunar position using simplified formulas
    double n = jd - 2451545.0;

    // Mean lunar longitude
    double L = 218.3164477 + 13.17639648 * n;
    L = normalizeAngle360(L);

    // Mean anomaly
    double M = 134.9328957 + 13.22811287 * n;
    M = normalizeAngle360(M);

    // Argument of latitude
    double F = 93.2720950 + 13.22937506 * n;
    F = normalizeAngle360(F);

    // Approximate longitude and latitude
    double moonLon = L + 6.28875 * std::sin(M * DEG_TO_RAD) +
                     1.27402 * std::sin(2 * (F - L) * DEG_TO_RAD) +
                     0.658309 * std::sin(2 * F * DEG_TO_RAD);
    moonLon = normalizeAngle360(moonLon);

    double moonLat = 5.12819 * std::sin(F * DEG_TO_RAD) -
                     0.280830 * std::sin(M * DEG_TO_RAD) -
                     0.331124 * std::sin((2 * F - M) * DEG_TO_RAD);

    // Convert ecliptic to equatorial coordinates
    double epsilon = 23.4392911;  // Approximate obliquity
    double moonRa =
        std::atan2(std::sin(moonLon * DEG_TO_RAD) * std::cos(epsilon * DEG_TO_RAD),
                   std::cos(moonLon * DEG_TO_RAD)) *
        RAD_TO_DEG;
    moonRa = normalizeAngle360(moonRa);

    double moonDec = std::asin(std::sin(moonLat * DEG_TO_RAD) *
                               std::sin(epsilon * DEG_TO_RAD)) *
                     RAD_TO_DEG;

    // Calculate lunar phase (0 = new, 0.5 = full, 1 = new)
    double sunLon = 280.46646 + 0.09856474 * n;
    double g = 357.52911 + 0.9833 * n;
    sunLon += 1.914602 * std::sin(g * DEG_TO_RAD);
    sunLon = normalizeAngle360(sunLon);

    double illumination = (1.0 + std::cos((moonLon - sunLon) * DEG_TO_RAD)) / 2.0;

    return {moonRa, moonDec, illumination};
}

double VisibilityCalculator::calculateMoonDistance(
    double ra, double dec,
    std::chrono::system_clock::time_point time) const {
    auto [moonRa, moonDec, phase] = getMoonInfo(time);

    // Great circle distance
    double raRad1 = ra * DEG_TO_RAD;
    double decRad1 = dec * DEG_TO_RAD;
    double raRad2 = moonRa * DEG_TO_RAD;
    double decRad2 = moonDec * DEG_TO_RAD;

    double cosAngle = std::sin(decRad1) * std::sin(decRad2) +
                      std::cos(decRad1) * std::cos(decRad2) * std::cos(raRad1 - raRad2);
    return std::acos(std::clamp(cosAngle, -1.0, 1.0)) * RAD_TO_DEG;
}

bool VisibilityCalculator::isMoonAboveHorizon(
    std::chrono::system_clock::time_point time) const {
    auto [moonRa, moonDec, phase] = getMoonInfo(time);
    auto altAz = calculateAltAz(moonRa, moonDec, time);
    return altAz.altitude > 0.0;
}

// ============================================================================
// Time Utilities
// ============================================================================

std::chrono::system_clock::time_point VisibilityCalculator::localToUTC(
    std::chrono::system_clock::time_point localTime) const {
    // This is a simplified implementation
    // For full timezone support, consider using libraries like tz-cpp
    auto offset = getTimezoneOffset();
    return localTime - std::chrono::seconds(offset);
}

std::chrono::system_clock::time_point VisibilityCalculator::utcToLocal(
    std::chrono::system_clock::time_point utcTime) const {
    auto offset = getTimezoneOffset();
    return utcTime + std::chrono::seconds(offset);
}

int64_t VisibilityCalculator::getTimezoneOffset() const {
    // Simplified: UTC has offset 0
    // For other timezones, this would need external timezone library
    if (pImpl_->timezone == "UTC") {
        return 0;
    }

    // Basic hardcoded timezone offsets (incomplete)
    // In production, use a proper timezone library
    std::map<std::string, int64_t> tzOffsets{
        {"UTC", 0},
        {"GMT", 0},
        {"EST", -5 * 3600},
        {"EDT", -4 * 3600},
        {"CST", -6 * 3600},
        {"CDT", -5 * 3600},
        {"MST", -7 * 3600},
        {"MDT", -6 * 3600},
        {"PST", -8 * 3600},
        {"PDT", -7 * 3600},
    };

    auto it = tzOffsets.find(pImpl_->timezone);
    return it != tzOffsets.end() ? it->second : 0;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

double VisibilityCalculator::calculateAltitudeWithRefraction(
    double rawAltitude) const {
    // Saemundsson refraction formula
    // Accounts for atmospheric refraction bending light rays
    if (rawAltitude < -0.833) {
        return rawAltitude;  // Below horizon, no refraction
    }

    double refraction = 0.0;
    if (rawAltitude > 0.0) {
        refraction = 0.00346 / std::tan((rawAltitude + 7.31 / (rawAltitude + 4.4)) * DEG_TO_RAD);
    } else {
        refraction = 0.01;
    }

    return rawAltitude + refraction;
}

}  // namespace lithium::target::observability
