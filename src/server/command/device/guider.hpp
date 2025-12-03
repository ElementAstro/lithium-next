/*
 * guider.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Guider middleware commands for PHD2 integration

*************************************************/

#ifndef LITHIUM_SERVER_COMMAND_GUIDER_HPP
#define LITHIUM_SERVER_COMMAND_GUIDER_HPP

#include "atom/type/json.hpp"

#include <memory>
#include <optional>
#include <string>

namespace lithium::device {
class GuiderService;
}

namespace lithium::middleware {

/**
 * @brief Get the global guider service instance
 */
auto getGuiderService() -> std::shared_ptr<lithium::device::GuiderService>;

// ==================== Connection ====================

/**
 * @brief Connect to PHD2 guider
 * @param host PHD2 host (default: localhost)
 * @param port PHD2 port (default: 4400)
 * @param timeout Connection timeout in ms
 */
auto connectGuider(const std::string& host = "localhost", int port = 4400,
                   int timeout = 5000) -> nlohmann::json;

/**
 * @brief Disconnect from PHD2 guider
 */
auto disconnectGuider() -> nlohmann::json;

/**
 * @brief Get connection status
 */
auto getConnectionStatus() -> nlohmann::json;

// ==================== Guiding Control ====================

/**
 * @brief Start guiding
 * @param settlePixels Max settle distance in pixels
 * @param settleTime Settle time in seconds
 * @param settleTimeout Settle timeout in seconds
 * @param recalibrate Force recalibration
 */
auto startGuiding(double settlePixels = 1.5, double settleTime = 10.0,
                  double settleTimeout = 60.0, bool recalibrate = false)
    -> nlohmann::json;

/**
 * @brief Stop guiding
 */
auto stopGuiding() -> nlohmann::json;

/**
 * @brief Pause guiding
 * @param full Also pause looping
 */
auto pauseGuiding(bool full = false) -> nlohmann::json;

/**
 * @brief Resume guiding
 */
auto resumeGuiding() -> nlohmann::json;

/**
 * @brief Perform dither
 * @param amount Dither amount in pixels
 * @param raOnly Only dither in RA
 * @param settlePixels Max settle distance
 * @param settleTime Settle time
 * @param settleTimeout Settle timeout
 */
auto ditherGuider(double amount = 5.0, bool raOnly = false,
                  double settlePixels = 1.5, double settleTime = 10.0,
                  double settleTimeout = 60.0) -> nlohmann::json;

/**
 * @brief Start looping exposures
 */
auto loopGuider() -> nlohmann::json;

/**
 * @brief Stop capture/looping
 */
auto stopCapture() -> nlohmann::json;

// ==================== Status ====================

/**
 * @brief Get guider status
 */
auto getGuiderStatus() -> nlohmann::json;

/**
 * @brief Get guide statistics
 */
auto getGuiderStats() -> nlohmann::json;

/**
 * @brief Get current star info
 */
auto getCurrentStar() -> nlohmann::json;

// ==================== Calibration ====================

/**
 * @brief Check calibration status
 */
auto isCalibrated() -> nlohmann::json;

/**
 * @brief Clear calibration
 * @param which "mount", "ao", or "both"
 */
auto clearCalibration(const std::string& which = "both") -> nlohmann::json;

/**
 * @brief Flip calibration for meridian flip
 */
auto flipCalibration() -> nlohmann::json;

/**
 * @brief Get calibration data
 */
auto getCalibrationData() -> nlohmann::json;

// ==================== Star Selection ====================

/**
 * @brief Auto-select guide star
 * @param roiX ROI X (optional)
 * @param roiY ROI Y (optional)
 * @param roiWidth ROI width (optional)
 * @param roiHeight ROI height (optional)
 */
auto findStar(std::optional<int> roiX = std::nullopt,
              std::optional<int> roiY = std::nullopt,
              std::optional<int> roiWidth = std::nullopt,
              std::optional<int> roiHeight = std::nullopt) -> nlohmann::json;

/**
 * @brief Set lock position
 * @param x X coordinate
 * @param y Y coordinate
 * @param exact Use exact position
 */
auto setLockPosition(double x, double y, bool exact = true) -> nlohmann::json;

/**
 * @brief Get lock position
 */
auto getLockPosition() -> nlohmann::json;

// ==================== Camera Control ====================

/**
 * @brief Get exposure time
 */
auto getExposure() -> nlohmann::json;

/**
 * @brief Set exposure time
 * @param exposureMs Exposure in milliseconds
 */
auto setExposure(int exposureMs) -> nlohmann::json;

/**
 * @brief Get available exposure durations
 */
auto getExposureDurations() -> nlohmann::json;

/**
 * @brief Get camera frame size
 */
auto getCameraFrameSize() -> nlohmann::json;

/**
 * @brief Get CCD temperature
 */
auto getCcdTemperature() -> nlohmann::json;

/**
 * @brief Get cooler status
 */
auto getCoolerStatus() -> nlohmann::json;

/**
 * @brief Save current image
 */
auto saveImage() -> nlohmann::json;

/**
 * @brief Get star image
 * @param size Image size
 */
auto getStarImage(int size = 15) -> nlohmann::json;

/**
 * @brief Capture single frame
 * @param exposureMs Optional exposure time
 */
auto captureSingleFrame(std::optional<int> exposureMs = std::nullopt)
    -> nlohmann::json;

// ==================== Guide Pulse ====================

/**
 * @brief Send guide pulse
 * @param direction "N", "S", "E", "W"
 * @param durationMs Duration in ms
 * @param useAO Use adaptive optics
 */
auto guidePulse(const std::string& direction, int durationMs,
                bool useAO = false) -> nlohmann::json;

// ==================== Algorithm Settings ====================

/**
 * @brief Get Dec guide mode
 */
auto getDecGuideMode() -> nlohmann::json;

/**
 * @brief Set Dec guide mode
 * @param mode "Off", "Auto", "North", "South"
 */
auto setDecGuideMode(const std::string& mode) -> nlohmann::json;

/**
 * @brief Get algorithm parameter
 * @param axis "ra" or "dec"
 * @param name Parameter name
 */
auto getAlgoParam(const std::string& axis, const std::string& name)
    -> nlohmann::json;

/**
 * @brief Set algorithm parameter
 * @param axis "ra" or "dec"
 * @param name Parameter name
 * @param value Parameter value
 */
auto setAlgoParam(const std::string& axis, const std::string& name,
                  double value) -> nlohmann::json;

// ==================== Equipment ====================

/**
 * @brief Check equipment connection
 */
auto isEquipmentConnected() -> nlohmann::json;

/**
 * @brief Connect equipment
 */
auto connectEquipment() -> nlohmann::json;

/**
 * @brief Disconnect equipment
 */
auto disconnectEquipment() -> nlohmann::json;

/**
 * @brief Get equipment info
 */
auto getEquipmentInfo() -> nlohmann::json;

// ==================== Profile Management ====================

/**
 * @brief Get available profiles
 */
auto getProfiles() -> nlohmann::json;

/**
 * @brief Get current profile
 */
auto getCurrentProfile() -> nlohmann::json;

/**
 * @brief Set profile
 * @param profileId Profile ID
 */
auto setProfile(int profileId) -> nlohmann::json;

// ==================== Settings ====================

/**
 * @brief Update multiple settings
 * @param settings JSON object with settings
 */
auto setGuiderSettings(const nlohmann::json& settings) -> nlohmann::json;

// ==================== Lock Shift ====================

/**
 * @brief Check lock shift status
 */
auto isLockShiftEnabled() -> nlohmann::json;

/**
 * @brief Enable/disable lock shift
 * @param enable Enable state
 */
auto setLockShiftEnabled(bool enable) -> nlohmann::json;

// ==================== Shutdown ====================

/**
 * @brief Shutdown guider application
 */
auto shutdownGuider() -> nlohmann::json;

}  // namespace lithium::middleware

// Command registration for WebSocket dispatcher
namespace lithium::app {

class CommandDispatcher;

/**
 * @brief Register guider commands with the WebSocket command dispatcher
 * @param dispatcher The command dispatcher instance
 */
void registerGuider(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_GUIDER_HPP
