/*
 * coordinate_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Coordinate Manager Component

This component manages coordinate transformations, coordinate systems,
position tracking, and coordinate validation.

*************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope::components {

class HardwareInterface;

/**
 * @brief Coordinate Manager for ASCOM Telescope
 */
class CoordinateManager {
public:
    explicit CoordinateManager(std::shared_ptr<HardwareInterface> hardware);
    ~CoordinateManager();

    // Non-copyable and non-movable
    CoordinateManager(const CoordinateManager&) = delete;
    CoordinateManager& operator=(const CoordinateManager&) = delete;
    CoordinateManager(CoordinateManager&&) = delete;
    CoordinateManager& operator=(CoordinateManager&&) = delete;

    // =========================================================================
    // Coordinate Retrieval
    // =========================================================================

    /**
     * @brief Get current RA/DEC coordinates (J2000)
     * @return Optional coordinate pair
     */
    std::optional<EquatorialCoordinates> getRADECJ2000();

    /**
     * @brief Get current RA/DEC coordinates (JNow)
     * @return Optional coordinate pair
     */
    std::optional<EquatorialCoordinates> getRADECJNow();

    /**
     * @brief Get target RA/DEC coordinates
     * @return Optional coordinate pair
     */
    std::optional<EquatorialCoordinates> getTargetRADEC();

    /**
     * @brief Get current AZ/ALT coordinates
     * @return Optional coordinate pair
     */
    std::optional<HorizontalCoordinates> getAZALT();

    // =========================================================================
    // Location and Time Management
    // =========================================================================

    /**
     * @brief Get observer location
     * @return Optional geographic location
     */
    std::optional<GeographicLocation> getLocation();

    /**
     * @brief Set observer location
     * @param location Geographic location
     * @return true if operation successful
     */
    bool setLocation(const GeographicLocation& location);

    /**
     * @brief Get UTC time
     * @return Optional UTC time point
     */
    std::optional<std::chrono::system_clock::time_point> getUTCTime();

    /**
     * @brief Set UTC time
     * @param time UTC time point
     * @return true if operation successful
     */
    bool setUTCTime(const std::chrono::system_clock::time_point& time);

    /**
     * @brief Get local time
     * @return Optional local time point
     */
    std::optional<std::chrono::system_clock::time_point> getLocalTime();

    // =========================================================================
    // Coordinate Transformations
    // =========================================================================

    /**
     * @brief Convert RA/DEC to AZ/ALT
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return Optional horizontal coordinates
     */
    std::optional<HorizontalCoordinates> convertRADECToAZALT(double ra, double dec);

    /**
     * @brief Convert AZ/ALT to RA/DEC
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return Optional equatorial coordinates
     */
    std::optional<EquatorialCoordinates> convertAZALTToRADEC(double az, double alt);

    /**
     * @brief Convert J2000 to JNow coordinates
     * @param ra_j2000 RA in J2000 (hours)
     * @param dec_j2000 DEC in J2000 (degrees)
     * @return Optional JNow coordinates
     */
    std::optional<EquatorialCoordinates> convertJ2000ToJNow(double ra_j2000, double dec_j2000);

    /**
     * @brief Convert JNow to J2000 coordinates
     * @param ra_jnow RA in JNow (hours)
     * @param dec_jnow DEC in JNow (degrees)
     * @return Optional J2000 coordinates
     */
    std::optional<EquatorialCoordinates> convertJNowToJ2000(double ra_jnow, double dec_jnow);

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Convert degrees to DMS format
     * @param degrees Decimal degrees
     * @return Tuple of degrees, minutes, seconds
     */
    std::tuple<int, int, double> degreesToDMS(double degrees);

    /**
     * @brief Convert degrees to HMS format
     * @param degrees Decimal degrees
     * @return Tuple of hours, minutes, seconds
     */
    std::tuple<int, int, double> degreesToHMS(double degrees);

    /**
     * @brief Calculate angular separation
     * @param ra1 First RA in hours
     * @param dec1 First DEC in degrees
     * @param ra2 Second RA in hours
     * @param dec2 Second DEC in degrees
     * @return Angular separation in degrees
     */
    double calculateAngularSeparation(double ra1, double dec1, double ra2, double dec2);

    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;

    /**
     * @brief Clear last error
     */
    void clearError();

private:
    std::shared_ptr<HardwareInterface> hardware_;
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    void setLastError(const std::string& error) const;
};

} // namespace lithium::device::ascom::telescope::components
