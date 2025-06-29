/*
 * playerone_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: PlayerOne Camera Implementation with SDK support

*************************************************/

#pragma once

#include "../template/camera.hpp"
#include "atom/log/loguru.hpp"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Forward declarations for PlayerOne SDK
struct _POACameraProperties;
typedef struct _POACameraProperties POACameraProperties;

namespace lithium::device::playerone::camera {

/**
 * @brief PlayerOne Camera implementation using PlayerOne SDK
 *
 * Supports PlayerOne astronomical cameras with advanced features including
 * cooling, high-speed readout, and excellent image quality.
 */
class PlayerOneCamera : public AtomCamera {
public:
    explicit PlayerOneCamera(const std::string& name);
    ~PlayerOneCamera() override;

    // Disable copy and move
    PlayerOneCamera(const PlayerOneCamera&) = delete;
    PlayerOneCamera& operator=(const PlayerOneCamera&) = delete;
    PlayerOneCamera(PlayerOneCamera&&) = delete;
    PlayerOneCamera& operator=(PlayerOneCamera&&) = delete;

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

    // Video streaming (excellent on PlayerOne cameras)
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

    // Temperature control (available on cooled models)
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

    // Gain and exposure controls (advanced on PlayerOne)
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

    // Shutter control (electronic on PlayerOne)
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

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

    // PlayerOne-specific methods
    auto getPlayerOneSDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getCameraType() const -> std::string;
    auto setSensorPattern(const std::string& pattern) -> bool;
    auto getSensorPattern() -> std::string;
    auto enableHardwareBinning(bool enable) -> bool;
    auto isHardwareBinningEnabled() const -> bool;
    auto setUSBTraffic(int traffic) -> bool;
    auto getUSBTraffic() -> int;
    auto enableAutoExposure(bool enable) -> bool;
    auto isAutoExposureEnabled() const -> bool;
    auto setAutoExposureTarget(int target) -> bool;
    auto getAutoExposureTarget() -> int;
    auto enableAutoGain(bool enable) -> bool;
    auto isAutoGainEnabled() const -> bool;
    auto setAutoGainTarget(int target) -> bool;
    auto getAutoGainTarget() -> int;
    auto setFlip(int flip) -> bool;
    auto getFlip() -> int;
    auto enableMonochromeMode(bool enable) -> bool;
    auto isMonochromeModeEnabled() const -> bool;
    auto setReadoutMode(int mode) -> bool;
    auto getReadoutMode() -> int;
    auto getReadoutModes() -> std::vector<std::string>;
    auto enableLowNoise(bool enable) -> bool;
    auto isLowNoiseEnabled() const -> bool;
    auto setPixelBinSum(bool enable) -> bool;
    auto isPixelBinSumEnabled() const -> bool;

private:
    // PlayerOne SDK state
    int camera_id_;
    POACameraProperties* camera_properties_;
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
    std::thread temperature_thread_;

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
    int usb_traffic_;
    bool auto_exposure_enabled_;
    int auto_exposure_target_;
    bool auto_gain_enabled_;
    int auto_gain_target_;
    int flip_mode_;
    bool monochrome_mode_;
    int readout_mode_;
    bool low_noise_enabled_;
    bool pixel_bin_sum_;
    bool hardware_binning_;
    std::string sensor_pattern_;

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
    mutable std::condition_variable exposure_cv_;

    // Private helper methods
    auto initializePlayerOneSDK() -> bool;
    auto shutdownPlayerOneSDK() -> bool;
    auto openCamera(int cameraId) -> bool;
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
    auto convertBayerPattern(int poaPattern) -> BayerPattern;
    auto convertBayerPatternToPOA(BayerPattern pattern) -> int;
    auto handlePlayerOneError(int errorCode, const std::string& operation) -> void;
    auto isValidExposureTime(double duration) const -> bool;
    auto isValidGain(int gain) const -> bool;
    auto isValidOffset(int offset) const -> bool;
    auto isValidResolution(int x, int y, int width, int height) const -> bool;
    auto isValidBinning(int binX, int binY) const -> bool;
    auto getControlValue(int controlType, bool& isAuto) -> int;
    auto setControlValue(int controlType, int value, bool isAuto) -> bool;
};

} // namespace lithium::device::playerone::camera
