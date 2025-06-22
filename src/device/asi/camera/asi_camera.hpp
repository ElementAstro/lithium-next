/*
 * asi_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ZWO ASI Camera Implementation with full SDK integration

*************************************************/

#pragma once

#include "../../template/camera.hpp"
#include "atom/log/loguru.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward declaration
namespace lithium::device::asi::camera::controller {
class ASICameraController;
}

namespace lithium::device::asi::camera {

/**
 * @brief ZWO ASI Camera implementation using ASI SDK
 * 
 * This class provides a complete implementation of the AtomCamera interface
 * for ZWO ASI cameras, supporting all features including cooling, video streaming,
 * and advanced controls.
 */
class ASICamera : public AtomCamera {
public:
    explicit ASICamera(const std::string& name);
    ~ASICamera() override;

    // Disable copy and move
    ASICamera(const ASICamera&) = delete;
    ASICamera& operator=(const ASICamera&) = delete;
    ASICamera(ASICamera&&) = delete;
    ASICamera& operator=(ASICamera&&) = delete;

    // Basic device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName = "", int timeout = 5000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // Exposure control
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string& path) -> bool override;

    // Exposure history and statistics
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // Video streaming
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string& format) -> bool override;
    auto getVideoFormats() -> std::vector<std::string> override;

    // Advanced video features
    auto startVideoRecording(const std::string& filename) -> bool override;
    auto stopVideoRecording() -> bool override;
    auto isVideoRecording() const -> bool override;
    auto setVideoExposure(double exposure) -> bool override;
    auto getVideoExposure() const -> double override;
    auto setVideoGain(int gain) -> bool override;
    auto getVideoGain() const -> int override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    auto isCoolerOn() const -> bool override;
    auto getTemperature() const -> std::optional<double> override;
    auto getTemperatureInfo() const -> TemperatureInfo override;
    auto getCoolingPower() const -> std::optional<double> override;
    auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // Color and Bayer
    auto isColor() const -> bool override;
    auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // Gain control
    auto setGain(int gain) -> bool override;
    auto getGain() -> std::optional<int> override;
    auto getGainRange() -> std::pair<int, int> override;

    auto setOffset(int offset) -> bool override;
    auto getOffset() -> std::optional<int> override;
    auto getOffsetRange() -> std::pair<int, int> override;

    auto setISO(int iso) -> bool override;
    auto getISO() -> std::optional<int> override;
    auto getISOList() -> std::vector<int> override;

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
    auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> override;

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

    // Image sequence capabilities
    auto startSequence(int count, double exposure, double interval) -> bool override;
    auto stopSequence() -> bool override;
    auto isSequenceRunning() const -> bool override;
    auto getSequenceProgress() const -> std::pair<int, int> override;

    // Advanced image processing
    auto setImageFormat(const std::string& format) -> bool override;
    auto getImageFormat() const -> std::string override;
    auto enableImageCompression(bool enable) -> bool override;
    auto isImageCompressionEnabled() const -> bool override;
    auto getSupportedImageFormats() const -> std::vector<std::string> override;

    // Statistics and quality
    auto getFrameStatistics() const -> std::map<std::string, double> override;
    auto getTotalFramesReceived() const -> uint64_t override;
    auto getDroppedFrames() const -> uint64_t override;
    auto getAverageFrameRate() const -> double override;
    auto getLastImageQuality() const -> std::map<std::string, double> override;

    // ASI-specific methods
    auto getASISDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() const -> std::string;
    auto getSerialNumber() const -> std::string;
    auto setCameraMode(const std::string& mode) -> bool;
    auto getCameraModes() -> std::vector<std::string>;
    auto setUSBBandwidth(int bandwidth) -> bool;
    auto getUSBBandwidth() -> int;
    auto enableAutoExposure(bool enable) -> bool;
    auto isAutoExposureEnabled() const -> bool;
    auto enableAutoGain(bool enable) -> bool;
    auto isAutoGainEnabled() const -> bool;
    auto enableAutoWhiteBalance(bool enable) -> bool;
    auto isAutoWhiteBalanceEnabled() const -> bool;
    auto setFlip(int flip) -> bool;
    auto getFlip() -> int;
    auto enableHighSpeedMode(bool enable) -> bool;
    auto isHighSpeedModeEnabled() const -> bool;

    // ASI EAF (Electronic Auto Focuser) control
    auto hasEAFFocuser() -> bool;
    auto connectEAFFocuser() -> bool;
    auto disconnectEAFFocuser() -> bool;
    auto isEAFFocuserConnected() -> bool;
    auto setEAFFocuserPosition(int position) -> bool;
    auto getEAFFocuserPosition() -> int;
    auto getEAFFocuserMaxPosition() -> int;
    auto isEAFFocuserMoving() -> bool;
    auto stopEAFFocuser() -> bool;
    auto setEAFFocuserStepSize(int stepSize) -> bool;
    auto getEAFFocuserStepSize() -> int;
    auto homeEAFFocuser() -> bool;
    auto calibrateEAFFocuser() -> bool;
    auto getEAFFocuserTemperature() -> double;
    auto enableEAFFocuserBacklashCompensation(bool enable) -> bool;
    auto setEAFFocuserBacklashSteps(int steps) -> bool;

    // ASI EFW (Electronic Filter Wheel) control
    auto hasEFWFilterWheel() -> bool;
    auto connectEFWFilterWheel() -> bool;
    auto disconnectEFWFilterWheel() -> bool;
    auto isEFWFilterWheelConnected() -> bool;
    auto setEFWFilterPosition(int position) -> bool;
    auto getEFWFilterPosition() -> int;
    auto getEFWFilterCount() -> int;
    auto isEFWFilterWheelMoving() -> bool;
    auto homeEFWFilterWheel() -> bool;
    auto getEFWFilterWheelFirmware() -> std::string;
    auto setEFWFilterNames(const std::vector<std::string>& names) -> bool;
    auto getEFWFilterNames() -> std::vector<std::string>;
    auto getEFWUnidirectionalMode() -> bool;
    auto setEFWUnidirectionalMode(bool enable) -> bool;
    auto calibrateEFWFilterWheel() -> bool;

private:
    std::unique_ptr<controller::ASICameraController> controller_;
};

} // namespace lithium::device::asi::camera
