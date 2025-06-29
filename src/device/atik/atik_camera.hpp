/*
 * atik_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Atik Camera Implementation with SDK integration

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

// Forward declarations for Atik SDK
struct _AtikCamera;
typedef struct _AtikCamera AtikCamera_t;

namespace lithium::device::atik::camera {

/**
 * @brief Atik Camera implementation using Atik SDK
 *
 * Supports Atik One, Titan, Infinity, and other Atik camera series
 * with full cooling, filtering, and advanced imaging capabilities.
 */
class AtikCamera : public AtomCamera {
public:
    explicit AtikCamera(const std::string& name);
    ~AtikCamera() override;

    // Disable copy and move
    AtikCamera(const AtikCamera&) = delete;
    AtikCamera& operator=(const AtikCamera&) = delete;
    AtikCamera(AtikCamera&&) = delete;
    AtikCamera& operator=(AtikCamera&&) = delete;

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

    // Temperature control (excellent cooling on Atik cameras)
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

    // Shutter control (available on some Atik models)
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // Filter wheel control (integrated on some models)
    auto hasFilterWheel() -> bool;
    auto getFilterCount() -> int;
    auto getCurrentFilter() -> int;
    auto setFilter(int position) -> bool;
    auto getFilterNames() -> std::vector<std::string>;
    auto setFilterNames(const std::vector<std::string>& names) -> bool;

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

    // Atik-specific methods
    auto getAtikSDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() const -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getCameraType() const -> std::string;
    auto enableAdvancedMode(bool enable) -> bool;
    auto isAdvancedModeEnabled() const -> bool;
    auto setReadMode(int mode) -> bool;
    auto getReadMode() -> int;
    auto getReadModes() -> std::vector<std::string>;
    auto enableAmpGlow(bool enable) -> bool;
    auto isAmpGlowEnabled() const -> bool;
    auto setPreflash(double duration) -> bool;
    auto getPreflash() -> double;

private:
    // Atik SDK state
    AtikCamera_t* atik_handle_;
    int camera_index_;
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

    // Filter wheel state
    bool has_filter_wheel_;
    int current_filter_;
    int filter_count_;
    std::vector<std::string> filter_names_;

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
    bool advanced_mode_;
    int read_mode_;
    bool amp_glow_enabled_;
    double preflash_duration_;

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
    std::shared_ptr<AtomCameraFrame> last_frame_result_;

    // Thread safety
    mutable std::mutex camera_mutex_;
    mutable std::mutex exposure_mutex_;
    mutable std::mutex video_mutex_;
    mutable std::mutex temperature_mutex_;
    mutable std::mutex sequence_mutex_;
    mutable std::mutex filter_mutex_;
    mutable std::condition_variable exposure_cv_;

    // Private helper methods
    auto initializeAtikSDK() -> bool;
    auto shutdownAtikSDK() -> bool;
    auto openCamera(int cameraIndex) -> bool;
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
    auto convertBayerPattern(int atikPattern) -> BayerPattern;
    auto convertBayerPatternToAtik(BayerPattern pattern) -> int;
    auto handleAtikError(int errorCode, const std::string& operation) -> void;
    auto isValidExposureTime(double duration) const -> bool;
    auto isValidGain(int gain) const -> bool;
    auto isValidOffset(int offset) const -> bool;
    auto isValidResolution(int x, int y, int width, int height) const -> bool;
    auto isValidBinning(int binX, int binY) const -> bool;
    auto initializeFilterWheel() -> bool;
};

} // namespace lithium::device::atik::camera
