/*
 * mount_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Mount/Telescope device service layer

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_MOUNT_HPP
#define LITHIUM_DEVICE_SERVICE_MOUNT_HPP

#include <memory>
#include <string>

#include "base_service.hpp"
#include "device/template/telescope.hpp"

namespace lithium::device {

/**
 * @brief Mount service providing high-level telescope/mount operations
 */
class MountService : public TypedDeviceService<MountService, AtomTelescope> {
public:
    MountService();
    ~MountService() override;

    // Disable copy
    MountService(const MountService&) = delete;
    MountService& operator=(const MountService&) = delete;

    /**
     * @brief List all available mounts
     */
    auto list() -> json;

    /**
     * @brief Get status of a specific mount
     */
    auto getStatus(const std::string& deviceId) -> json;

    /**
     * @brief Connect or disconnect a mount
     */
    auto connect(const std::string& deviceId, bool connected) -> json;

    /**
     * @brief Slew mount to RA/Dec coordinates
     */
    auto slew(const std::string& deviceId, const std::string& ra,
              const std::string& dec) -> json;

    /**
     * @brief Stop mount motion
     */
    auto stop(const std::string& deviceId) -> json;

    /**
     * @brief Set tracking on/off
     */
    auto setTracking(const std::string& deviceId, bool tracking) -> json;

    /**
     * @brief Execute position command (PARK/UNPARK/HOME/FIND_HOME)
     */
    auto setPosition(const std::string& deviceId, const std::string& command)
        -> json;

    /**
     * @brief Execute pulse guide
     */
    auto pulseGuide(const std::string& deviceId, const std::string& direction,
                    int durationMs) -> json;

    /**
     * @brief Sync mount to RA/Dec coordinates
     */
    auto sync(const std::string& deviceId, const std::string& ra,
              const std::string& dec) -> json;

    /**
     * @brief Get mount capabilities
     */
    auto getCapabilities(const std::string& deviceId) -> json;

    /**
     * @brief Set guide rates
     */
    auto setGuideRates(const std::string& deviceId, double raRate,
                       double decRate) -> json;

    /**
     * @brief Set tracking rate (Sidereal/Lunar/Solar)
     */
    auto setTrackingRate(const std::string& deviceId, const std::string& rate)
        -> json;

    /**
     * @brief Get pier side
     */
    auto getPierSide(const std::string& deviceId) -> json;

    /**
     * @brief Perform meridian flip
     */
    auto performMeridianFlip(const std::string& deviceId) -> json;

    // ========== INDI-specific operations ==========

    /**
     * @brief Get INDI-specific telescope properties
     */
    auto getINDIProperties(const std::string& deviceId) -> json;

    /**
     * @brief Set INDI-specific telescope property
     */
    auto setINDIProperty(const std::string& deviceId,
                         const std::string& propertyName, const json& value)
        -> json;

    /**
     * @brief Get telescope info (aperture, focal length)
     */
    auto getTelescopeInfo(const std::string& deviceId) -> json;

    /**
     * @brief Set telescope info
     */
    auto setTelescopeInfo(const std::string& deviceId, double aperture,
                          double focalLength, double guiderAperture,
                          double guiderFocalLength) -> json;

    /**
     * @brief Get site location
     */
    auto getSiteLocation(const std::string& deviceId) -> json;

    /**
     * @brief Set site location
     */
    auto setSiteLocation(const std::string& deviceId, double latitude,
                         double longitude, double elevation) -> json;

    /**
     * @brief Get UTC offset
     */
    auto getUTCOffset(const std::string& deviceId) -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_MOUNT_HPP
