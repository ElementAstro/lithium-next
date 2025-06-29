/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASI Camera Controller V2

This modular controller orchestrates the camera components to provide
a clean, maintainable, and testable interface for ASI camera control.

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "./components/hardware_interface.hpp"
#include "./components/exposure_manager.hpp"
#include "./components/temperature_controller.hpp"
#include "./components/sequence_manager.hpp"
#include "./components/property_manager.hpp"
#include "./components/video_manager.hpp"
#include "./components/image_processor.hpp"

namespace lithium::device::asi::camera {

// Forward declarations
namespace components {
class HardwareInterface;
class ExposureManager;
class TemperatureController;
class SequenceManager;
class PropertyManager;
class VideoManager;
class ImageProcessor;
}

/**
 * @brief Modular ASI Camera Controller V2
 *
 * This controller provides a clean interface to ASI camera functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of camera operation, promoting separation of concerns and
 * testability.
 */
class ASICameraController {
public:
    ASICameraController();
    ~ASICameraController();

    // Non-copyable and non-movable
    ASICameraController(const ASICameraController&) = delete;
    ASICameraController& operator=(const ASICameraController&) = delete;
    ASICameraController(ASICameraController&&) = delete;
    ASICameraController& operator=(ASICameraController&&) = delete;

    // =========================================================================
    // Initialization and Device Management
    // =========================================================================

    /**
     * @brief Initialize the camera controller
     * @return true if initialization successful, false otherwise
     */
    auto initialize() -> bool;

    /**
     * @brief Shutdown and cleanup the controller
     * @return true if shutdown successful, false otherwise
     */
    auto shutdown() -> bool;

    /**
     * @brief Check if controller is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] auto isInitialized() const -> bool;

    /**
     * @brief Connect to a specific camera
     * @param camera_id Camera identifier
     * @return true if connection successful, false otherwise
     */
    auto connectToCamera(int camera_id) -> bool;

    /**
     * @brief Disconnect from current camera
     * @return true if disconnection successful, false otherwise
     */
    auto disconnectFromCamera() -> bool;

    /**
     * @brief Check if connected to a camera
     * @return true if connected, false otherwise
     */
    [[nodiscard]] auto isConnected() const -> bool;

    // =========================================================================
    // Camera Information and Status
    // =========================================================================

    /**
     * @brief Get camera information
     * @return Camera information string
     */
    [[nodiscard]] auto getCameraInfo() const -> std::string;

    /**
     * @brief Get current camera status
     * @return Status string
     */
    [[nodiscard]] auto getStatus() const -> std::string;

    /**
     * @brief Get last error message
     * @return Error message string
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    // =========================================================================
    // Exposure Control
    // =========================================================================

    /**
     * @brief Start an exposure
     * @param duration_ms Exposure duration in milliseconds
     * @param is_dark Whether this is a dark frame
     * @return true if exposure started successfully, false otherwise
     */
    auto startExposure(double duration_ms, bool is_dark = false) -> bool;

    /**
     * @brief Stop current exposure
     * @return true if exposure stopped successfully, false otherwise
     */
    auto stopExposure() -> bool;

    /**
     * @brief Check if exposure is in progress
     * @return true if exposing, false otherwise
     */
    [[nodiscard]] auto isExposing() const -> bool;

    /**
     * @brief Get exposure progress (0.0 to 1.0)
     * @return Progress value
     */
    [[nodiscard]] auto getExposureProgress() const -> double;

    /**
     * @brief Get remaining exposure time in seconds
     * @return Remaining time
     */
    [[nodiscard]] auto getRemainingExposureTime() const -> double;

    // =========================================================================
    // Image Management
    // =========================================================================

    /**
     * @brief Check if image is ready
     * @return true if image ready, false otherwise
     */
    [[nodiscard]] auto isImageReady() const -> bool;

    /**
     * @brief Download the captured image
     * @return Image data as vector of bytes
     */
    auto downloadImage() -> std::vector<uint8_t>;

    /**
     * @brief Save image to file
     * @param filename Output filename
     * @param format Image format (FITS, TIFF, etc.)
     * @return true if save successful, false otherwise
     */
    auto saveImage(const std::string& filename, const std::string& format = "FITS") -> bool;

    // =========================================================================
    // Temperature Control
    // =========================================================================

    /**
     * @brief Set target temperature
     * @param target_temp Target temperature in Celsius
     * @return true if set successfully, false otherwise
     */
    auto setTargetTemperature(double target_temp) -> bool;

    /**
     * @brief Get current temperature
     * @return Current temperature in Celsius
     */
    [[nodiscard]] auto getCurrentTemperature() const -> double;

    /**
     * @brief Enable/disable cooling
     * @param enable true to enable cooling, false to disable
     * @return true if operation successful, false otherwise
     */
    auto setCoolingEnabled(bool enable) -> bool;

    /**
     * @brief Check if cooling is enabled
     * @return true if cooling enabled, false otherwise
     */
    [[nodiscard]] auto isCoolingEnabled() const -> bool;

    /**
     * @brief Check if camera has cooler
     * @return true if has cooler, false otherwise
     */
    [[nodiscard]] auto hasCooler() const -> bool;

    /**
     * @brief Get cooling power percentage
     * @return Cooling power (0-100%)
     */
    [[nodiscard]] auto getCoolingPower() const -> double;

    /**
     * @brief Get target temperature
     * @return Target temperature in Celsius
     */
    [[nodiscard]] auto getTargetTemperature() const -> double;

    // =========================================================================
    // Video/Live View
    // =========================================================================

    /**
     * @brief Start video/live view mode
     * @return true if started successfully, false otherwise
     */
    auto startVideo() -> bool;

    /**
     * @brief Stop video/live view mode
     * @return true if stopped successfully, false otherwise
     */
    auto stopVideo() -> bool;

    /**
     * @brief Check if video mode is active
     * @return true if video active, false otherwise
     */
    [[nodiscard]] auto isVideoActive() const -> bool;

    /**
     * @brief Start video mode
     * @return true if started successfully, false otherwise
     */
    auto startVideoMode() -> bool;

    /**
     * @brief Stop video mode
     * @return true if stopped successfully, false otherwise
     */
    auto stopVideoMode() -> bool;

    /**
     * @brief Check if video mode is active
     * @return true if active, false otherwise
     */
    [[nodiscard]] auto isVideoModeActive() const -> bool;

    /**
     * @brief Set video format
     * @param format Video format string
     * @return true if set successfully, false otherwise
     */
    auto setVideoFormat(const std::string& format) -> bool;

    /**
     * @brief Get supported video formats
     * @return Vector of supported format strings
     */
    [[nodiscard]] auto getSupportedVideoFormats() const -> std::vector<std::string>;

    /**
     * @brief Start video recording
     * @param filename Output filename
     * @return true if started successfully, false otherwise
     */
    auto startVideoRecording(const std::string& filename) -> bool;

    /**
     * @brief Stop video recording
     * @return true if stopped successfully, false otherwise
     */
    auto stopVideoRecording() -> bool;

    /**
     * @brief Check if video recording is active
     * @return true if recording, false otherwise
     */
    [[nodiscard]] auto isVideoRecording() const -> bool;

    /**
     * @brief Set video exposure time
     * @param exposure Exposure time in seconds
     * @return true if set successfully, false otherwise
     */
    auto setVideoExposure(double exposure) -> bool;

    /**
     * @brief Get video exposure time
     * @return Current video exposure time in seconds
     */
    [[nodiscard]] auto getVideoExposure() const -> double;

    /**
     * @brief Set video gain
     * @param gain Video gain value
     * @return true if set successfully, false otherwise
     */
    auto setVideoGain(int gain) -> bool;

    /**
     * @brief Get video gain
     * @return Current video gain value
     */
    [[nodiscard]] auto getVideoGain() const -> int;

    // =========================================================================
    // Sequence Management
    // =========================================================================

    /**
     * @brief Start an automated sequence
     * @param sequence_config Sequence configuration
     * @return true if sequence started successfully, false otherwise
     */
    auto startSequence(const std::string& sequence_config) -> bool;

    /**
     * @brief Stop current sequence
     * @return true if sequence stopped successfully, false otherwise
     */
    auto stopSequence() -> bool;

    /**
     * @brief Check if sequence is running
     * @return true if sequence active, false otherwise
     */
    [[nodiscard]] auto isSequenceActive() const -> bool;

    /**
     * @brief Get sequence progress
     * @return Progress information
     */
    [[nodiscard]] auto getSequenceProgress() const -> std::string;

    /**
     * @brief Check if sequence is running (alias for isSequenceActive)
     * @return true if sequence running, false otherwise
     */
    [[nodiscard]] auto isSequenceRunning() const -> bool;

    // =========================================================================
    // Properties and Configuration
    // =========================================================================

    /**
     * @brief Set camera property
     * @param property Property name
     * @param value Property value
     * @return true if set successfully, false otherwise
     */
    auto setProperty(const std::string& property, const std::string& value) -> bool;

    /**
     * @brief Get camera property
     * @param property Property name
     * @return Property value
     */
    [[nodiscard]] auto getProperty(const std::string& property) const -> std::string;

    /**
     * @brief Get all available properties
     * @return Vector of property names
     */
    [[nodiscard]] auto getAvailableProperties() const -> std::vector<std::string>;

    // =========================================================================
    // Gain and Offset Control
    // =========================================================================

    /**
     * @brief Set camera gain
     * @param gain Gain value
     * @return true if set successfully, false otherwise
     */
    auto setGain(int gain) -> bool;

    /**
     * @brief Get current gain
     * @return Current gain value, or nullopt if not available
     */
    [[nodiscard]] auto getGain() const -> std::optional<int>;

    /**
     * @brief Get gain range
     * @return Pair of (min, max) gain values
     */
    [[nodiscard]] auto getGainRange() const -> std::pair<int, int>;

    /**
     * @brief Set camera offset
     * @param offset Offset value
     * @return true if set successfully, false otherwise
     */
    auto setOffset(int offset) -> bool;

    /**
     * @brief Get current offset
     * @return Current offset value, or nullopt if not available
     */
    [[nodiscard]] auto getOffset() const -> std::optional<int>;

    /**
     * @brief Get offset range
     * @return Pair of (min, max) offset values
     */
    [[nodiscard]] auto getOffsetRange() const -> std::pair<int, int>;

    // =========================================================================
    // Binning and Resolution
    // =========================================================================

    /**
     * @brief Set binning mode
     * @param horizontal Horizontal binning
     * @param vertical Vertical binning
     * @return true if set successfully, false otherwise
     */
    auto setBinning(int horizontal, int vertical) -> bool;

    /**
     * @brief Get current binning
     * @return Current binning as pair (horizontal, vertical)
     */
    [[nodiscard]] auto getBinning() const -> std::pair<int, int>;

    /**
     * @brief Set Region of Interest (ROI)
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Width
     * @param height Height
     * @return true if set successfully, false otherwise
     */
    auto setROI(int x, int y, int width, int height) -> bool;

    // =========================================================================
    // Camera Information
    // =========================================================================

    /**
     * @brief Check if camera is a color camera
     * @return true if color camera, false if monochrome
     */
    [[nodiscard]] auto isColorCamera() const -> bool;

    /**
     * @brief Get pixel size in micrometers
     * @return Pixel size
     */
    [[nodiscard]] auto getPixelSize() const -> double;

    /**
     * @brief Get bit depth
     * @return Bit depth
     */
    [[nodiscard]] auto getBitDepth() const -> int;

    /**
     * @brief Check if camera has shutter
     * @return true if has shutter, false otherwise
     */
    [[nodiscard]] auto hasShutter() const -> bool;

    // =========================================================================
    // Callback Management
    // =========================================================================

    /**
     * @brief Set exposure completion callback
     * @param callback Callback function
     */
    void setExposureCallback(std::function<void(bool)> callback);

    /**
     * @brief Set temperature change callback
     * @param callback Callback function
     */
    void setTemperatureCallback(std::function<void(double)> callback);

    /**
     * @brief Set error callback
     * @param callback Callback function
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

private:
    // Component instances
    std::unique_ptr<components::HardwareInterface> m_hardware;
    std::unique_ptr<components::ExposureManager> m_exposure;
    std::unique_ptr<components::TemperatureController> m_temperature;
    std::unique_ptr<components::SequenceManager> m_sequence;
    std::unique_ptr<components::PropertyManager> m_properties;
    std::unique_ptr<components::VideoManager> m_video;
    std::unique_ptr<components::ImageProcessor> m_image_processor;

    // State management
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_connected{false};
    mutable std::mutex m_state_mutex;

    // Error handling
    mutable std::string m_last_error;
    mutable std::mutex m_error_mutex;

    // Callbacks
    std::function<void(bool)> m_exposure_callback;
    std::function<void(double)> m_temperature_callback;
    std::function<void(const std::string&)> m_error_callback;
    std::mutex m_callback_mutex;

    // Helper methods
    void setLastError(const std::string& error);
    void notifyError(const std::string& error);
    auto initializeComponents() -> bool;
    void shutdownComponents();
};

} // namespace lithium::device::asi::camera
