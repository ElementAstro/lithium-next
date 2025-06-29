/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Camera Controller

This modular controller orchestrates the camera components to provide
a clean, maintainable, and testable interface for ASCOM camera control.

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
#include "device/template/camera.hpp"

namespace lithium::device::ascom::camera {

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
 * @brief Modular ASCOM Camera Controller
 *
 * This controller provides a clean interface to ASCOM camera functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of camera operation, promoting separation of concerns and
 * testability.
 */
class ASCOMCameraController : public AtomCamera {
public:
    explicit ASCOMCameraController(const std::string& name);
    ~ASCOMCameraController() override;

    // Non-copyable and non-movable
    ASCOMCameraController(const ASCOMCameraController&) = delete;
    ASCOMCameraController& operator=(const ASCOMCameraController&) = delete;
    ASCOMCameraController(ASCOMCameraController&&) = delete;
    ASCOMCameraController& operator=(ASCOMCameraController&&) = delete;

    // =========================================================================
    // AtomDriver Interface Implementation
    // =========================================================================

    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // =========================================================================
    // AtomCamera Interface Implementation - Exposure Control
    // =========================================================================

    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string &path) -> bool override;

    // Exposure history and statistics
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // =========================================================================
    // AtomCamera Interface Implementation - Video/streaming control
    // =========================================================================

    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string &format) -> bool override;
    auto getVideoFormats() -> std::vector<std::string> override;

    // =========================================================================
    // AtomCamera Interface Implementation - Temperature control
    // =========================================================================

    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    auto isCoolerOn() const -> bool override;
    auto getTemperature() const -> std::optional<double> override;
    auto getTemperatureInfo() const -> TemperatureInfo override;
    auto getCoolingPower() const -> std::optional<double> override;
    auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // =========================================================================
    // AtomCamera Interface Implementation - Color information
    // =========================================================================

    auto isColor() const -> bool override;
    auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // =========================================================================
    // AtomCamera Interface Implementation - Parameter control
    // =========================================================================

    auto setGain(int gain) -> bool override;
    auto getGain() -> std::optional<int> override;
    auto getGainRange() -> std::pair<int, int> override;
    auto setOffset(int offset) -> bool override;
    auto getOffset() -> std::optional<int> override;
    auto getOffsetRange() -> std::pair<int, int> override;
    auto setISO(int iso) -> bool override;
    auto getISO() -> std::optional<int> override;
    auto getISOList() -> std::vector<int> override;

    // =========================================================================
    // AtomCamera Interface Implementation - Frame settings
    // =========================================================================

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
    auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> override;

    // =========================================================================
    // AtomCamera Interface Implementation - Pixel information
    // =========================================================================

    auto getPixelSize() -> double override;
    auto getPixelSizeX() -> double override;
    auto getPixelSizeY() -> double override;
    auto getBitDepth() -> int override;

    // =========================================================================
    // AtomCamera Interface Implementation - Advanced features
    // =========================================================================

    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;
    auto hasFan() -> bool override;
    auto setFanSpeed(int speed) -> bool override;
    auto getFanSpeed() -> int override;

    // Advanced video features
    auto startVideoRecording(const std::string &filename) -> bool override;
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
    auto getSequenceProgress() const -> std::pair<int, int> override;

    // Advanced image processing
    auto setImageFormat(const std::string &format) -> bool override;
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
    // Component Access - For advanced operations
    // =========================================================================

    /**
     * @brief Get hardware interface component
     * @return Shared pointer to hardware interface
     */
    auto getHardwareInterface() -> std::shared_ptr<components::HardwareInterface>;

    /**
     * @brief Get exposure manager component
     * @return Shared pointer to exposure manager
     */
    auto getExposureManager() -> std::shared_ptr<components::ExposureManager>;

    /**
     * @brief Get temperature controller component
     * @return Shared pointer to temperature controller
     */
    auto getTemperatureController() -> std::shared_ptr<components::TemperatureController>;

    /**
     * @brief Get sequence manager component
     * @return Shared pointer to sequence manager
     */
    auto getSequenceManager() -> std::shared_ptr<components::SequenceManager>;

    /**
     * @brief Get property manager component
     * @return Shared pointer to property manager
     */
    auto getPropertyManager() -> std::shared_ptr<components::PropertyManager>;

    /**
     * @brief Get video manager component
     * @return Shared pointer to video manager
     */
    auto getVideoManager() -> std::shared_ptr<components::VideoManager>;

    /**
     * @brief Get image processor component
     * @return Shared pointer to image processor
     */
    auto getImageProcessor() -> std::shared_ptr<components::ImageProcessor>;

    // =========================================================================
    // ASCOM-specific methods
    // =========================================================================

    /**
     * @brief Get ASCOM driver information
     * @return Driver information string
     */
    auto getASCOMDriverInfo() -> std::optional<std::string>;

    /**
     * @brief Get ASCOM version
     * @return ASCOM version string
     */
    auto getASCOMVersion() -> std::optional<std::string>;

    /**
     * @brief Get ASCOM interface version
     * @return Interface version number
     */
    auto getASCOMInterfaceVersion() -> std::optional<int>;

    /**
     * @brief Set ASCOM client ID
     * @param clientId Client identifier
     * @return true if set successfully
     */
    auto setASCOMClientID(const std::string &clientId) -> bool;

    /**
     * @brief Get ASCOM client ID
     * @return Client identifier
     */
    auto getASCOMClientID() -> std::optional<std::string>;

private:
    // Component instances
    std::shared_ptr<components::HardwareInterface> hardwareInterface_;
    std::shared_ptr<components::ExposureManager> exposureManager_;
    std::shared_ptr<components::TemperatureController> temperatureController_;
    std::shared_ptr<components::SequenceManager> sequenceManager_;
    std::shared_ptr<components::PropertyManager> propertyManager_;
    std::shared_ptr<components::VideoManager> videoManager_;
    std::shared_ptr<components::ImageProcessor> imageProcessor_;

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    mutable std::mutex stateMutex_;

    // Statistics
    std::atomic<uint32_t> exposureCount_{0};
    std::atomic<double> lastExposureDuration_{0.0};
    std::atomic<uint64_t> totalFramesReceived_{0};
    std::atomic<uint64_t> droppedFrames_{0};

    // Helper methods
    auto initializeComponents() -> bool;
    auto shutdownComponents() -> bool;
    auto validateComponentsReady() const -> bool;
};

/**
 * @brief Factory class for creating ASCOM camera controllers
 */
class ControllerFactory {
public:
    /**
     * @brief Create a new modular ASCOM camera controller
     * @param name Camera name/identifier
     * @return Unique pointer to controller instance
     */
    static auto createModularController(const std::string& name)
        -> std::unique_ptr<ASCOMCameraController>;

    /**
     * @brief Create a shared ASCOM camera controller
     * @param name Camera name/identifier
     * @return Shared pointer to controller instance
     */
    static auto createSharedController(const std::string& name)
        -> std::shared_ptr<ASCOMCameraController>;
};

} // namespace lithium::device::ascom::camera
