/*
 * camera_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Camera device service layer

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_CAMERA_HPP
#define LITHIUM_DEVICE_SERVICE_CAMERA_HPP

#include <string>

#include "base_service.hpp"
#include "device/template/camera.hpp"

namespace lithium::device {

/**
 * @brief Camera service providing high-level camera operations
 *
 * This service wraps the low-level AtomCamera driver and provides
 * a consistent JSON-based API for camera operations.
 */
class CameraService : public TypedDeviceService<CameraService, AtomCamera> {
public:
    CameraService();
    ~CameraService() override;

    // Disable copy
    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;

    /**
     * @brief List all available cameras
     * @return JSON response with camera list
     */
    auto list() -> json;

    /**
     * @brief Get status of a specific camera
     * @param deviceId Camera device identifier
     * @return JSON response with camera status
     */
    auto getStatus(const std::string& deviceId) -> json;

    /**
     * @brief Connect or disconnect a camera
     * @param deviceId Camera device identifier
     * @param connected True to connect, false to disconnect
     * @return JSON response with operation result
     */
    auto connect(const std::string& deviceId, bool connected) -> json;

    /**
     * @brief Update camera settings
     * @param deviceId Camera device identifier
     * @param settings JSON object with settings to update
     * @return JSON response with operation result
     */
    auto updateSettings(const std::string& deviceId, const json& settings) -> json;

    /**
     * @brief Start a single exposure
     * @param deviceId Camera device identifier
     * @param duration Exposure duration in seconds
     * @param frameType Frame type (Light, Dark, Flat, Bias)
     * @param filename Optional output filename
     * @return JSON response with exposure ID
     */
    auto startExposure(const std::string& deviceId, double duration,
                       const std::string& frameType,
                       const std::string& filename) -> json;

    /**
     * @brief Abort current exposure
     * @param deviceId Camera device identifier
     * @return JSON response with operation result
     */
    auto abortExposure(const std::string& deviceId) -> json;

    /**
     * @brief Get camera capabilities and limits
     * @param deviceId Camera device identifier
     * @return JSON response with capabilities
     */
    auto getCapabilities(const std::string& deviceId) -> json;

    /**
     * @brief Get available gain values
     * @param deviceId Camera device identifier
     * @return JSON response with gain information
     */
    auto getGains(const std::string& deviceId) -> json;

    /**
     * @brief Get available offset values
     * @param deviceId Camera device identifier
     * @return JSON response with offset information
     */
    auto getOffsets(const std::string& deviceId) -> json;

    /**
     * @brief Set cooler power (manual mode)
     * @param deviceId Camera device identifier
     * @param power Cooler power percentage
     * @param mode Cooling mode
     * @return JSON response with operation result
     */
    auto setCoolerPower(const std::string& deviceId, double power,
                        const std::string& mode) -> json;

    /**
     * @brief Initiate camera warm-up sequence
     * @param deviceId Camera device identifier
     * @return JSON response with operation result
     */
    auto warmUp(const std::string& deviceId) -> json;

    // ========== INDI-specific operations ==========

    /**
     * @brief Get INDI-specific camera properties
     */
    auto getINDIProperties(const std::string& deviceId) -> json;

    /**
     * @brief Set INDI-specific camera property
     */
    auto setINDIProperty(const std::string& deviceId,
                         const std::string& propertyName,
                         const json& value) -> json;

    /**
     * @brief Get camera frame types
     */
    auto getFrameTypes(const std::string& deviceId) -> json;

    /**
     * @brief Set camera frame type
     */
    auto setFrameType(const std::string& deviceId,
                      const std::string& frameType) -> json;

    /**
     * @brief Get available readout modes
     */
    auto getReadoutModes(const std::string& deviceId) -> json;

    /**
     * @brief Set readout mode
     */
    auto setReadoutMode(const std::string& deviceId, int modeIndex) -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_CAMERA_HPP
