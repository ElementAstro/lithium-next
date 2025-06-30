/*
 * fli_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: FLI Camera Implementation with SDK support

*************************************************/

#pragma once

#include "../template/camera.hpp"
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Forward declarations for FLI SDK
typedef long flidev_t;
typedef long flidomain_t;
typedef long fliframe_t;
typedef long flibitdepth_t;

namespace lithium::device::fli::camera {

/**
 * @brief FLI Camera implementation using FLI SDK
 * 
 * Supports Finger Lakes Instrumentation cameras including MicroLine,
 * ProLine, and MaxCam series with excellent cooling and precision control.
 */
class FLICamera : public AtomCamera {
public:
    explicit FLICamera(const std::string& name);
    ~FLICamera() override;

    // Disable copy and move
    FLICamera(const FLICamera&) = delete;
    FLICamera& operator=(const FLICamera&) = delete;
    FLICamera(FLICamera&&) = delete;
    FLICamera& operator=(FLICamera&&) = delete;

    // Basic device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName = "", int timeout = 5000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // Full AtomCamera interface implementation
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string& path) -> bool override;

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

    auto startVideoRecording(const std::string& filename) -> bool override;
    auto stopVideoRecording() -> bool override;
    auto isVideoRecording() const -> bool override;
    auto setVideoExposure(double exposure) -> bool override;
    auto getVideoExposure() const -> double override;
    auto setVideoGain(int gain) -> bool override;
    auto getVideoGain() const -> int override;

    // Temperature control (excellent on FLI cameras)
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    auto isCoolerOn() const -> bool override;
    auto getTemperature() const -> std::optional<double> override;
    auto getTemperatureInfo() const -> TemperatureInfo override;
    auto getCoolingPower() const -> std::optional<double> override;
    auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // Color and Bayer patterns
    auto isColor() const -> bool override;
    auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // Gain and exposure controls
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

    // Shutter control (mechanical shutter available)
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // Filter wheel control (FLI filter wheels)
    auto hasFilterWheel() -> bool;
    auto getFilterCount() -> int;
    auto getCurrentFilter() -> int;
    auto setFilter(int position) -> bool;
    auto getFilterNames() -> std::vector<std::string>;
    auto setFilterNames(const std::vector<std::string>& names) -> bool;
    auto homeFilterWheel() -> bool;
    auto getFilterWheelStatus() -> std::string;

    // Focuser control (FLI focusers)
    auto hasFocuser() -> bool;
    auto getFocuserPosition() -> int;
    auto setFocuserPosition(int position) -> bool;
    auto getFocuserRange() -> std::pair<int, int>;
    auto homeFocuser() -> bool;
    auto getFocuserStepSize() -> double;

    // Advanced capabilities
    auto hasFan() -> bool override;
    auto setFanSpeed(int speed) -> bool override;
    auto getFanSpeed() -> int override;

    auto startSequence(int count, double exposure, double interval) -> bool override;
    auto stopSequence() -> bool override;
    auto isSequenceRunning() const -> bool override;
    auto getSequenceProgress() const -> std::pair<int, int> override;

    auto setImageFormat(const std::string& format) -> bool override;
    auto getImageFormat() const -> std::string override;
    auto enableImageCompression(bool enable) -> bool override;
    auto isImageCompressionEnabled() const -> bool override;
    auto getSupportedImageFormats() const -> std::vector<std::string> override;

    auto getFrameStatistics() const -> std::map<std::string, double> override;
    auto getTotalFramesReceived() const -> uint64_t override;
    auto getDroppedFrames() const -> uint64_t override;
    auto getAverageFrameRate() const -> double override;
    auto getLastImageQuality() const -> std::map<std::string, double> override;

    // FLI-specific methods
    auto getFLISDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getCameraType() const -> std::string;
    auto setReadoutSpeed(int speed) -> bool;
    auto getReadoutSpeed() -> int;
    auto getReadoutSpeeds() -> std::vector<std::string>;
    auto setGainMode(int mode) -> bool;
    auto getGainMode() -> int;
    auto getGainModes() -> std::vector<std::string>;
    auto enableFlushes(int count) -> bool;
    auto getFlushCount() -> int;
    auto setDebugLevel(int level) -> bool;
    auto getDebugLevel() -> int;
    auto getBaseTemperature() -> double;
    auto getCoolerPower() -> double;

private:
    // FLI SDK state
    flidev_t fli_device_;
    std::string device_name_;
    std::string camera_model_;
    std::string serial_number_;
    std::string firmware_version_;
    std::string camera_type_;
    
    // Connection state
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_initialized_;
    
    // Exposure state
    std::atomic<bool> is_exposing_;
    std::atomic<bool> exposure_abort_requested_;
    std::chrono::system_clock::time_point exposure_start_time_;
    double current_exposure_duration_;
    std::thread exposure_thread_;
    
    // Video state
    std::atomic<bool> is_video_running_;
    std::atomic<bool> is_video_recording_;
    std::thread video_thread_;
    std::string video_recording_file_;
    double video_exposure_;
    int video_gain_;
    
    // Temperature control
    std::atomic<bool> cooler_enabled_;
    double target_temperature_;
    double base_temperature_;
    std::thread temperature_thread_;
    
    // Filter wheel state
    bool has_filter_wheel_;
    flidev_t filter_device_;
    int current_filter_;
    int filter_count_;
    std::vector<std::string> filter_names_;
    bool filter_wheel_homed_;
    
    // Focuser state
    bool has_focuser_;
    flidev_t focuser_device_;
    int focuser_position_;
    int focuser_min_, focuser_max_;
    double step_size_;
    bool focuser_homed_;
    
    // Sequence control
    std::atomic<bool> sequence_running_;
    int sequence_current_frame_;
    int sequence_total_frames_;
    double sequence_exposure_;
    double sequence_interval_;
    std::thread sequence_thread_;
    
    // Camera parameters
    int current_gain_;
    int current_offset_;
    int current_iso_;
    int readout_speed_;
    int gain_mode_;
    int flush_count_;
    int debug_level_;
    
    // Frame parameters
    int roi_x_, roi_y_, roi_width_, roi_height_;
    int bin_x_, bin_y_;
    int max_width_, max_height_;
    double pixel_size_x_, pixel_size_y_;
    int bit_depth_;
    BayerPattern bayer_pattern_;
    bool is_color_camera_;
    bool has_shutter_;
    
    // Statistics
    uint64_t total_frames_;
    uint64_t dropped_frames_;
    std::chrono::system_clock::time_point last_frame_time_;
    
    // Thread safety
    mutable std::mutex camera_mutex_;
    mutable std::mutex exposure_mutex_;
    mutable std::mutex video_mutex_;
    mutable std::mutex temperature_mutex_;
    mutable std::mutex sequence_mutex_;
    mutable std::mutex filter_mutex_;
    mutable std::mutex focuser_mutex_;
    mutable std::condition_variable exposure_cv_;
    
    // Private helper methods
    auto initializeFLISDK() -> bool;
    auto shutdownFLISDK() -> bool;
    auto openCamera(const std::string& deviceName) -> bool;
    auto closeCamera() -> bool;
    auto setupCameraParameters() -> bool;
    auto readCameraCapabilities() -> bool;
    auto updateTemperatureInfo() -> bool;
    auto captureFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto processRawData(void* data, size_t size) -> std::shared_ptr<AtomCameraFrame>;
    auto exposureThreadFunction() -> void;
    auto videoThreadFunction() -> void;
    auto temperatureThreadFunction() -> void;
    auto sequenceThreadFunction() -> void;
    auto calculateImageQuality(const void* data, int width, int height, int channels) -> std::map<std::string, double>;
    auto saveFrameToFile(const std::shared_ptr<AtomCameraFrame>& frame, const std::string& path) -> bool;
    auto convertBayerPattern(int fliPattern) -> BayerPattern;
    auto convertBayerPatternToFLI(BayerPattern pattern) -> int;
    auto handleFLIError(long errorCode, const std::string& operation) -> void;
    auto isValidExposureTime(double duration) const -> bool;
    auto isValidGain(int gain) const -> bool;
    auto isValidOffset(int offset) const -> bool;
    auto isValidResolution(int x, int y, int width, int height) const -> bool;
    auto isValidBinning(int binX, int binY) const -> bool;
    auto initializeFilterWheel() -> bool;
    auto initializeFocuser() -> bool;
    auto scanFLIDevices(flidomain_t domain) -> std::vector<std::string>;
};

} // namespace lithium::device::fli::camera
