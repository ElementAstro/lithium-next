/*
 * guider_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Guider device service for managing guiding operations

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_GUIDER_HPP
#define LITHIUM_DEVICE_SERVICE_GUIDER_HPP

#include "base_service.hpp"
#include "client/phd2/phd2_client.hpp"

#include <memory>
#include <string>

namespace lithium::device {

// Forward declaration
using GuiderClient = lithium::client::PHD2Client;

/**
 * @brief Service class for guider device operations
 *
 * Provides high-level API for guider control, integrating with
 * DeviceManager and providing JSON-based responses for REST API.
 * Uses PHD2Client from client/phd2 for actual PHD2 communication.
 */
class GuiderService : public BaseDeviceService {
public:
    GuiderService();
    ~GuiderService() override = default;

    // ==================== Connection ====================

    /**
     * @brief Connect to guider
     * @param host PHD2 host address
     * @param port PHD2 port
     * @param timeout Connection timeout in ms
     * @return JSON response
     */
    auto connect(const std::string& host = "localhost", int port = 4400,
                 int timeout = 5000) -> json;

    /**
     * @brief Disconnect from guider
     * @return JSON response
     */
    auto disconnect() -> json;

    /**
     * @brief Get connection status
     * @return JSON response with connection info
     */
    auto getConnectionStatus() -> json;

    // ==================== Guiding Control ====================

    /**
     * @brief Start guiding
     * @param settlePixels Max settle distance in pixels
     * @param settleTime Settle time in seconds
     * @param settleTimeout Settle timeout in seconds
     * @param recalibrate Force recalibration
     * @return JSON response
     */
    auto startGuiding(double settlePixels = 1.5, double settleTime = 10.0,
                      double settleTimeout = 60.0, bool recalibrate = false)
        -> json;

    /**
     * @brief Stop guiding
     * @return JSON response
     */
    auto stopGuiding() -> json;

    /**
     * @brief Pause guiding
     * @param full Also pause looping
     * @return JSON response
     */
    auto pause(bool full = false) -> json;

    /**
     * @brief Resume guiding
     * @return JSON response
     */
    auto resume() -> json;

    /**
     * @brief Perform dither
     * @param amount Dither amount in pixels
     * @param raOnly Only dither in RA
     * @param settlePixels Max settle distance
     * @param settleTime Settle time
     * @param settleTimeout Settle timeout
     * @return JSON response
     */
    auto dither(double amount = 5.0, bool raOnly = false,
                double settlePixels = 1.5, double settleTime = 10.0,
                double settleTimeout = 60.0) -> json;

    /**
     * @brief Start looping exposures
     * @return JSON response
     */
    auto loop() -> json;

    /**
     * @brief Stop capture/looping
     * @return JSON response
     */
    auto stopCapture() -> json;

    // ==================== Status ====================

    /**
     * @brief Get guider status
     * @return JSON response with full status
     */
    auto getStatus() -> json;

    /**
     * @brief Get guide statistics
     * @return JSON response with stats
     */
    auto getStats() -> json;

    /**
     * @brief Get current star info
     * @return JSON response with star data
     */
    auto getCurrentStar() -> json;

    // ==================== Calibration ====================

    /**
     * @brief Check calibration status
     * @return JSON response
     */
    auto isCalibrated() -> json;

    /**
     * @brief Clear calibration
     * @param which "mount", "ao", or "both"
     * @return JSON response
     */
    auto clearCalibration(const std::string& which = "both") -> json;

    /**
     * @brief Flip calibration for meridian flip
     * @return JSON response
     */
    auto flipCalibration() -> json;

    /**
     * @brief Get calibration data
     * @return JSON response with calibration info
     */
    auto getCalibrationData() -> json;

    // ==================== Star Selection ====================

    /**
     * @brief Auto-select guide star
     * @param roiX ROI X (optional)
     * @param roiY ROI Y (optional)
     * @param roiWidth ROI width (optional)
     * @param roiHeight ROI height (optional)
     * @return JSON response with star position
     */
    auto findStar(std::optional<int> roiX = std::nullopt,
                  std::optional<int> roiY = std::nullopt,
                  std::optional<int> roiWidth = std::nullopt,
                  std::optional<int> roiHeight = std::nullopt) -> json;

    /**
     * @brief Set lock position
     * @param x X coordinate
     * @param y Y coordinate
     * @param exact Use exact position
     * @return JSON response
     */
    auto setLockPosition(double x, double y, bool exact = true) -> json;

    /**
     * @brief Get lock position
     * @return JSON response with position
     */
    auto getLockPosition() -> json;

    // ==================== Camera Control ====================

    /**
     * @brief Get exposure time
     * @return JSON response with exposure
     */
    auto getExposure() -> json;

    /**
     * @brief Set exposure time
     * @param exposureMs Exposure in milliseconds
     * @return JSON response
     */
    auto setExposure(int exposureMs) -> json;

    /**
     * @brief Get available exposure durations
     * @return JSON response with durations
     */
    auto getExposureDurations() -> json;

    /**
     * @brief Get camera frame size
     * @return JSON response with width and height
     */
    auto getCameraFrameSize() -> json;

    /**
     * @brief Get CCD temperature
     * @return JSON response with temperature
     */
    auto getCcdTemperature() -> json;

    /**
     * @brief Get cooler status
     * @return JSON response with cooler info
     */
    auto getCoolerStatus() -> json;

    /**
     * @brief Save current image
     * @return JSON response with filename
     */
    auto saveImage() -> json;

    /**
     * @brief Get star image
     * @param size Image size (>= 15)
     * @return JSON response with image data
     */
    auto getStarImage(int size = 15) -> json;

    /**
     * @brief Capture single frame
     * @param exposureMs Optional exposure time
     * @return JSON response
     */
    auto captureSingleFrame(std::optional<int> exposureMs = std::nullopt)
        -> json;

    // ==================== Guide Pulse ====================

    /**
     * @brief Send guide pulse
     * @param direction "N", "S", "E", "W"
     * @param durationMs Duration in ms
     * @param useAO Use adaptive optics
     * @return JSON response
     */
    auto guidePulse(const std::string& direction, int durationMs,
                    bool useAO = false) -> json;

    // ==================== Algorithm Settings ====================

    /**
     * @brief Get Dec guide mode
     * @return JSON response with mode
     */
    auto getDecGuideMode() -> json;

    /**
     * @brief Set Dec guide mode
     * @param mode "Off", "Auto", "North", "South"
     * @return JSON response
     */
    auto setDecGuideMode(const std::string& mode) -> json;

    /**
     * @brief Get algorithm parameter
     * @param axis "ra" or "dec"
     * @param name Parameter name
     * @return JSON response with value
     */
    auto getAlgoParam(const std::string& axis, const std::string& name) -> json;

    /**
     * @brief Set algorithm parameter
     * @param axis "ra" or "dec"
     * @param name Parameter name
     * @param value Parameter value
     * @return JSON response
     */
    auto setAlgoParam(const std::string& axis, const std::string& name,
                      double value) -> json;

    // ==================== Equipment ====================

    /**
     * @brief Check equipment connection
     * @return JSON response
     */
    auto isEquipmentConnected() -> json;

    /**
     * @brief Connect equipment
     * @return JSON response
     */
    auto connectEquipment() -> json;

    /**
     * @brief Disconnect equipment
     * @return JSON response
     */
    auto disconnectEquipment() -> json;

    /**
     * @brief Get equipment info
     * @return JSON response with equipment details
     */
    auto getEquipmentInfo() -> json;

    // ==================== Profile Management ====================

    /**
     * @brief Get available profiles
     * @return JSON response with profiles
     */
    auto getProfiles() -> json;

    /**
     * @brief Get current profile
     * @return JSON response with profile
     */
    auto getCurrentProfile() -> json;

    /**
     * @brief Set profile
     * @param profileId Profile ID
     * @return JSON response
     */
    auto setProfile(int profileId) -> json;

    // ==================== Settings ====================

    /**
     * @brief Update multiple settings
     * @param settings JSON object with settings
     * @return JSON response
     */
    auto updateSettings(const json& settings) -> json;

    // ==================== Lock Shift ====================

    /**
     * @brief Check lock shift status
     * @return JSON response
     */
    auto isLockShiftEnabled() -> json;

    /**
     * @brief Enable/disable lock shift
     * @param enable Enable state
     * @return JSON response
     */
    auto setLockShiftEnabled(bool enable) -> json;

    // ==================== Shutdown ====================

    /**
     * @brief Shutdown guider application
     * @return JSON response
     */
    auto shutdown() -> json;

private:
    std::shared_ptr<GuiderClient> guider_;

    // Helper to get guider with connection check
    auto getConnectedGuider()
        -> std::pair<std::shared_ptr<GuiderClient>, std::optional<json>>;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_GUIDER_HPP
