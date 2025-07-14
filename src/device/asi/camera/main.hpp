/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera dedicated module

*************************************************/

#pragma once

#include "device/template/camera.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
namespace lithium::device::asi::camera {
class ASICameraController;
}

namespace lithium::device::asi::camera {

/**
 * @brief Dedicated ASI Camera controller
 *
 * This class provides complete control over ZWO ASI cameras,
 * including exposure control, temperature management, video streaming,
 * and advanced features like sequence automation and image processing.
 */
class ASICamera : public AtomCamera {
public:
    explicit ASICamera(const std::string& name = "ASI Camera");
    ~ASICamera() override;

    // Non-copyable and non-movable
    ASICamera(const ASICamera&) = delete;
    ASICamera& operator=(const ASICamera&) = delete;
    ASICamera(ASICamera&&) = delete;
    ASICamera& operator=(ASICamera&&) = delete;

    // Basic device interface (from AtomDriver)
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &port = "", int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    [[nodiscard]] auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // Camera interface (from AtomCamera) - Core exposure
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    [[nodiscard]] auto isExposing() const -> bool override;
    [[nodiscard]] auto getExposureProgress() const -> double override;
    [[nodiscard]] auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string &path) -> bool override;

    // Exposure statistics
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // Video/streaming
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    [[nodiscard]] auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string& format) -> bool override;
    auto getVideoFormats() -> std::vector<std::string> override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    [[nodiscard]] auto isCoolerOn() const -> bool override;
    [[nodiscard]] auto getTemperature() const -> std::optional<double> override;
    [[nodiscard]] auto getTemperatureInfo() const -> TemperatureInfo override;
    [[nodiscard]] auto getCoolingPower() const -> std::optional<double> override;
    [[nodiscard]] auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // Color information
    [[nodiscard]] auto isColor() const -> bool override;
    [[nodiscard]] auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // Parameter control
    auto setGain(int gain) -> bool override;
    [[nodiscard]] auto getGain() -> std::optional<int> override;
    [[nodiscard]] auto getGainRange() -> std::pair<int, int> override;

    auto setOffset(int offset) -> bool override;
    [[nodiscard]] auto getOffset() -> std::optional<int> override;
    [[nodiscard]] auto getOffsetRange() -> std::pair<int, int> override;

    auto setISO(int iso) -> bool override;
    [[nodiscard]] auto getISO() -> std::optional<int> override;
    [[nodiscard]] auto getISOList() -> std::vector<int> override;

    // Frame settings
    auto getResolution() -> std::optional<AtomCameraFrame::Resolution> override;
    auto setResolution(int x, int y, int width, int height) -> bool override;
    auto getMaxResolution() -> AtomCameraFrame::Resolution override;

    auto getBinning() -> std::optional<AtomCameraFrame::Binning> override;
    auto setBinning(int horizontal, int vertical) -> bool override;
    auto getMaxBinning() -> AtomCameraFrame::Binning override;

    auto setFrameType(FrameType type) -> bool override;
    auto getFrameType() -> FrameType override;
    auto setUploadMode(UploadMode mode) -> bool override;
    auto getUploadMode() -> UploadMode override;
    [[nodiscard]] auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> override;

    // Pixel information
    auto getPixelSize() -> double override;
    auto getPixelSizeX() -> double override;
    auto getPixelSizeY() -> double override;
    auto getBitDepth() -> int override;

    // Shutter control
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // Fan control
    auto hasFan() -> bool override;
    auto setFanSpeed(int speed) -> bool override;
    auto getFanSpeed() -> int override;

    // Advanced video features
    auto startVideoRecording(const std::string& filename) -> bool override;
    auto stopVideoRecording() -> bool override;
    auto isVideoRecording() const -> bool override;
    auto setVideoExposure(double exposure) -> bool override;
    auto getVideoExposure() const -> double override;
    auto setVideoGain(int gain) -> bool override;
    auto getVideoGain() const -> int override;

    // Image sequence capabilities
    auto startSequence(int count, double exposure, double interval) -> bool override;
    auto stopSequence() -> bool override;
    auto isSequenceRunning() const -> bool override;
    auto getSequenceProgress() const -> std::pair<int, int> override; // current, total

    // Advanced image processing
    auto setImageFormat(const std::string& format) -> bool override;
    auto getImageFormat() const -> std::string override;
    auto enableImageCompression(bool enable) -> bool override;
    auto isImageCompressionEnabled() const -> bool override;
    auto getSupportedImageFormats() const -> std::vector<std::string> override;

    // Image quality and statistics
    auto getFrameStatistics() const -> std::map<std::string, double> override;
    auto getTotalFramesReceived() const -> uint64_t override;
    auto getDroppedFrames() const -> uint64_t override;
    auto getAverageFrameRate() const -> double override;
    auto getLastImageQuality() const -> std::map<std::string, double> override;

    // =========================================================================
    // ASI-Specific Extended Features
    // =========================================================================

    /**
     * @brief Get list of available cameras
     * @return Vector of camera information
     */
    [[nodiscard]] static auto getAvailableCameras() -> std::vector<std::string>;

    /**
     * @brief Connect to specific camera by ID
     * @param camera_id Camera identifier
     * @return true if connection successful, false otherwise
     */
    auto connectToCamera(int camera_id) -> bool;

    /**
     * @brief Get camera information
     * @return Detailed camera information
     */
    [[nodiscard]] auto getCameraInfo() const -> std::string;

    /**
     * @brief Set USB traffic bandwidth
     * @param bandwidth Bandwidth value (0-100)
     * @return true if set successfully, false otherwise
     */
    auto setUSBTraffic(int bandwidth) -> bool;

    /**
     * @brief Get USB traffic bandwidth
     * @return Current bandwidth value
     */
    [[nodiscard]] auto getUSBTraffic() const -> int;

    /**
     * @brief Set hardware binning mode
     * @param enable true to enable hardware binning, false for software
     * @return true if set successfully, false otherwise
     */
    auto setHardwareBinning(bool enable) -> bool;

    /**
     * @brief Check if hardware binning is enabled
     * @return true if hardware binning enabled, false otherwise
     */
    [[nodiscard]] auto isHardwareBinningEnabled() const -> bool;

    /**
     * @brief Set high speed mode
     * @param enable true to enable high speed mode, false to disable
     * @return true if set successfully, false otherwise
     */
    auto setHighSpeedMode(bool enable) -> bool;

    /**
     * @brief Check if high speed mode is enabled
     * @return true if high speed mode enabled, false otherwise
     */
    [[nodiscard]] auto isHighSpeedModeEnabled() const -> bool;

    /**
     * @brief Set flip mode
     * @param horizontal true to flip horizontally
     * @param vertical true to flip vertically
     * @return true if set successfully, false otherwise
     */
    auto setFlip(bool horizontal, bool vertical) -> bool;

    /**
     * @brief Get flip settings
     * @return Pair of (horizontal, vertical) flip settings
     */
    [[nodiscard]] auto getFlip() const -> std::pair<bool, bool>;

    /**
     * @brief Set white balance for color cameras
     * @param red_gain Red channel gain
     * @param green_gain Green channel gain
     * @param blue_gain Blue channel gain
     * @return true if set successfully, false otherwise
     */
    auto setWhiteBalance(double red_gain, double green_gain, double blue_gain) -> bool;

    /**
     * @brief Get white balance settings
     * @return Tuple of (red, green, blue) gains
     */
    [[nodiscard]] auto getWhiteBalance() const -> std::tuple<double, double, double>;

    /**
     * @brief Enable/disable auto white balance
     * @param enable true to enable auto white balance, false to disable
     * @return true if set successfully, false otherwise
     */
    auto setAutoWhiteBalance(bool enable) -> bool;

    /**
     * @brief Check if auto white balance is enabled
     * @return true if auto white balance enabled, false otherwise
     */
    [[nodiscard]] auto isAutoWhiteBalanceEnabled() const -> bool;

    // =========================================================================
    // Sequence and Automation Features
    // =========================================================================

    /**
     * @brief Start automated imaging sequence
     * @param sequence_config JSON configuration for the sequence
     * @return true if sequence started successfully, false otherwise
     */
    auto startSequence(const std::string& sequence_config) -> bool;

    /**
     * @brief Check if sequence is running (ASI-specific variant)
     * @return true if sequence active, false otherwise
     */
    [[nodiscard]] auto isSequenceActive() const -> bool;

    /**
     * @brief Get detailed sequence progress information
     * @return Progress information as JSON string
     */
    [[nodiscard]] auto getDetailedSequenceProgress() const -> std::string;

    /**
     * @brief Pause current sequence
     * @return true if sequence paused successfully, false otherwise
     */
    auto pauseSequence() -> bool;

    /**
     * @brief Resume paused sequence
     * @return true if sequence resumed successfully, false otherwise
     */
    auto resumeSequence() -> bool;

    // =========================================================================
    // Advanced Image Processing
    // =========================================================================

    /**
     * @brief Enable/disable dark frame subtraction
     * @param enable true to enable, false to disable
     * @return true if set successfully, false otherwise
     */
    auto setDarkFrameSubtraction(bool enable) -> bool;

    /**
     * @brief Check if dark frame subtraction is enabled
     * @return true if enabled, false otherwise
     */
    [[nodiscard]] auto isDarkFrameSubtractionEnabled() const -> bool;

    /**
     * @brief Set flat field correction
     * @param flat_frame_path Path to flat field image
     * @return true if set successfully, false otherwise
     */
    auto setFlatFieldCorrection(const std::string& flat_frame_path) -> bool;

    /**
     * @brief Enable/disable flat field correction
     * @param enable true to enable, false to disable
     * @return true if set successfully, false otherwise
     */
    auto setFlatFieldCorrectionEnabled(bool enable) -> bool;

    /**
     * @brief Check if flat field correction is enabled
     * @return true if enabled, false otherwise
     */
    [[nodiscard]] auto isFlatFieldCorrectionEnabled() const -> bool;

    // =========================================================================
    // Callback Management
    // =========================================================================

    /**
     * @brief Set exposure completion callback
     * @param callback Function to call when exposure completes
     */
    void setExposureCallback(std::function<void(bool)> callback);

    /**
     * @brief Set temperature change callback
     * @param callback Function to call when temperature changes
     */
    void setTemperatureCallback(std::function<void(double)> callback);

    /**
     * @brief Set image ready callback
     * @param callback Function to call when image is ready
     */
    void setImageReadyCallback(std::function<void()> callback);

    /**
     * @brief Set error callback
     * @param callback Function to call when error occurs
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    // =========================================================================
    // Status and Diagnostics
    // =========================================================================

    /**
     * @brief Get detailed camera status
     * @return Status information as JSON string
     */
    [[nodiscard]] auto getDetailedStatus() const -> std::string;

    /**
     * @brief Get camera statistics
     * @return Statistics information
     */
    [[nodiscard]] auto getCameraStatistics() const -> std::string;

    /**
     * @brief Perform camera self-test
     * @return true if self-test passed, false otherwise
     */
    auto performSelfTest() -> bool;

    /**
     * @brief Reset camera to default settings
     * @return true if reset successful, false otherwise
     */
    auto resetToDefaults() -> bool;

    /**
     * @brief Save current configuration
     * @param config_name Configuration name
     * @return true if saved successfully, false otherwise
     */
    auto saveConfiguration(const std::string& config_name) -> bool;

    /**
     * @brief Load saved configuration
     * @param config_name Configuration name
     * @return true if loaded successfully, false otherwise
     */
    auto loadConfiguration(const std::string& config_name) -> bool;

private:
    std::unique_ptr<ASICameraController> m_controller;
    std::string m_device_name;
    mutable std::mutex m_state_mutex;

    // Statistics tracking
    double m_last_exposure_duration{0.0};
    uint32_t m_exposure_count{0};

    // Internal state
    std::string m_current_frame_type{"Light"};
    std::pair<int, int> m_current_binning{1, 1};
    std::string m_current_image_format{"FITS"};

    // Helper methods
    void initializeDefaultSettings();
    auto validateConnection() const -> bool;
    void setupCallbacks();
};

} // namespace lithium::device::asi::camera
