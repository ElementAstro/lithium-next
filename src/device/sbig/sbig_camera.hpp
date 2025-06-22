/*
 * sbig_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: SBIG Camera Implementation with Universal Driver support

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

// Forward declarations for SBIG SDK
typedef unsigned short PAR_ERROR;
typedef unsigned short PAR_COMMAND;

namespace lithium::device::sbig::camera {

/**
 * @brief SBIG Camera implementation using SBIG Universal Driver
 * 
 * Supports SBIG ST series cameras with dual-chip capability (main CCD + guide chip),
 * excellent cooling systems, and professional-grade features.
 */
class SBIGCamera : public AtomCamera {
public:
    explicit SBIGCamera(const std::string& name);
    ~SBIGCamera() override;

    // Disable copy and move
    SBIGCamera(const SBIGCamera&) = delete;
    SBIGCamera& operator=(const SBIGCamera&) = delete;
    SBIGCamera(SBIGCamera&&) = delete;
    SBIGCamera& operator=(SBIGCamera&&) = delete;

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

    // Video streaming (limited on SBIG cameras)
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

    // Temperature control (excellent on SBIG cameras)
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

    // Shutter control (mechanical shutter on SBIG cameras)
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // SBIG-specific dual-chip functionality
    auto hasGuideChip() -> bool;
    auto getGuideChipResolution() -> AtomCameraFrame::Resolution;
    auto startGuideExposure(double duration) -> bool;
    auto abortGuideExposure() -> bool;
    auto isGuideExposing() const -> bool;
    auto getGuideExposureResult() -> std::shared_ptr<AtomCameraFrame>;

    // Filter wheel control (CFW series)
    auto hasFilterWheel() -> bool;
    auto getFilterCount() -> int;
    auto getCurrentFilter() -> int;
    auto setFilter(int position) -> bool;
    auto getFilterNames() -> std::vector<std::string>;
    auto setFilterNames(const std::vector<std::string>& names) -> bool;
    auto homeFilterWheel() -> bool;
    auto getFilterWheelStatus() -> std::string;

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

    // SBIG-specific methods
    auto getSBIGSDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getCameraType() const -> std::string;
    auto setReadoutMode(int mode) -> bool;
    auto getReadoutMode() -> int;
    auto getReadoutModes() -> std::vector<std::string>;
    auto enableAntiBloom(bool enable) -> bool;
    auto isAntiBloomEnabled() const -> bool;
    auto setFastReadout(bool enable) -> bool;
    auto isFastReadoutEnabled() const -> bool;
    auto getElectronsPerADU() -> double;
    auto getFullWellCapacity() -> double;
    auto enableSubtractDark(bool enable) -> bool;
    auto isDarkSubtractionEnabled() const -> bool;

private:
    // SBIG SDK state
    unsigned short device_handle_;
    int camera_index_;
    std::string camera_model_;
    std::string serial_number_;
    std::string firmware_version_;
    std::string camera_type_;
    
    // Connection state
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_initialized_;
    
    // Dual-chip state
    bool has_guide_chip_;
    std::atomic<bool> is_guide_exposing_;
    std::shared_ptr<AtomCameraFrame> guide_frame_;
    
    // Exposure state
    std::atomic<bool> is_exposing_;
    std::atomic<bool> exposure_abort_requested_;
    std::chrono::system_clock::time_point exposure_start_time_;
    double current_exposure_duration_;
    std::thread exposure_thread_;
    std::thread guide_exposure_thread_;
    
    // Video state (limited on SBIG)
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
    bool filter_wheel_homed_;
    
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
    int readout_mode_;
    bool anti_bloom_enabled_;
    bool fast_readout_enabled_;
    bool dark_subtraction_enabled_;
    double electrons_per_adu_;
    double full_well_capacity_;
    
    // Frame parameters
    int roi_x_, roi_y_, roi_width_, roi_height_;
    int bin_x_, bin_y_;
    int max_width_, max_height_;
    int guide_width_, guide_height_;
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
    mutable std::mutex guide_mutex_;
    mutable std::mutex video_mutex_;
    mutable std::mutex temperature_mutex_;
    mutable std::mutex sequence_mutex_;
    mutable std::mutex filter_mutex_;
    mutable std::condition_variable exposure_cv_;
    mutable std::condition_variable guide_cv_;
    
    // Private helper methods
    auto initializeSBIGSDK() -> bool;
    auto shutdownSBIGSDK() -> bool;
    auto openCamera(int cameraIndex) -> bool;
    auto closeCamera() -> bool;
    auto setupCameraParameters() -> bool;
    auto readCameraCapabilities() -> bool;
    auto updateTemperatureInfo() -> bool;
    auto captureFrame(bool useGuideChip = false) -> std::shared_ptr<AtomCameraFrame>;
    auto processRawData(void* data, size_t size, bool isGuideChip = false) -> std::shared_ptr<AtomCameraFrame>;
    auto exposureThreadFunction() -> void;
    auto guideExposureThreadFunction() -> void;
    auto videoThreadFunction() -> void;
    auto temperatureThreadFunction() -> void;
    auto sequenceThreadFunction() -> void;
    auto calculateImageQuality(const void* data, int width, int height, int channels) -> std::map<std::string, double>;
    auto saveFrameToFile(const std::shared_ptr<AtomCameraFrame>& frame, const std::string& path) -> bool;
    auto convertBayerPattern(int sbigPattern) -> BayerPattern;
    auto convertBayerPatternToSBIG(BayerPattern pattern) -> int;
    auto handleSBIGError(PAR_ERROR errorCode, const std::string& operation) -> void;
    auto isValidExposureTime(double duration) const -> bool;
    auto isValidGain(int gain) const -> bool;
    auto isValidOffset(int offset) const -> bool;
    auto isValidResolution(int x, int y, int width, int height) const -> bool;
    auto isValidBinning(int binX, int binY) const -> bool;
    auto initializeFilterWheel() -> bool;
    auto sendSBIGCommand(PAR_COMMAND command, void* params, void* results) -> PAR_ERROR;
};

} // namespace lithium::device::sbig::camera
