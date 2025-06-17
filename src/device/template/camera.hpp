/*
 * camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced AtomCamera following INDI architecture

*************************************************/

#pragma once

#include "camera_frame.hpp"
#include "device.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

// Camera-specific states
enum class CameraState {
    IDLE,
    EXPOSING,
    DOWNLOADING,
    ABORTED,
    ERROR
};

// Camera types
enum class CameraType {
    PRIMARY,
    GUIDE,
    FINDER
};

// Bayer patterns
enum class BayerPattern {
    RGGB,
    BGGR,
    GRBG,
    GBRG,
    MONO
};

// Image formats for advanced processing
enum class ImageFormat {
    FITS,
    NATIVE,
    XISF,
    JPEG,
    PNG,
    TIFF,
    RAW
};

// Video recording states
enum class VideoRecordingState {
    STOPPED,
    RECORDING,
    PAUSED,
    ERROR
};

// Sequence states
enum class SequenceState {
    IDLE,
    RUNNING,
    PAUSED,
    COMPLETED,
    ABORTED,
    ERROR
};

// Camera capabilities
struct CameraCapabilities {
    bool canAbort{true};
    bool canSubFrame{true};
    bool canBin{true};
    bool hasCooler{false};
    bool hasGuideHead{false};
    bool hasShutter{true};
    bool hasFilters{false};
    bool hasBayer{false};
    bool canStream{false};
    bool hasGain{false};
    bool hasOffset{false};
    bool hasTemperature{false};
    BayerPattern bayerPattern{BayerPattern::MONO};
    
    // Enhanced capabilities
    bool canRecordVideo{false};
    bool supportsSequences{false};
    bool hasImageQualityAnalysis{false};
    bool supportsCompression{false};
    bool hasAdvancedControls{false};
    bool supportsBurstMode{false};
    std::vector<ImageFormat> supportedFormats;
    std::vector<std::string> supportedVideoFormats;
} ATOM_ALIGNAS(16);

// Temperature control
struct TemperatureInfo {
    double current{0.0};
    double target{0.0};
    double ambient{0.0};
    double coolingPower{0.0};
    bool coolerOn{false};
    bool canSetTemperature{false};
} ATOM_ALIGNAS(64);

// Enhanced video information
struct VideoInfo {
    bool isStreaming{false};
    bool isRecording{false};
    VideoRecordingState recordingState{VideoRecordingState::STOPPED};
    std::string currentFormat{"MJPEG"};
    std::vector<std::string> supportedFormats;
    double frameRate{0.0};
    double exposure{0.033}; // 30 FPS default
    int gain{0};
    std::string recordingFile;
} ATOM_ALIGNAS(128);

// Sequence information
struct SequenceInfo {
    SequenceState state{SequenceState::IDLE};
    int currentFrame{0};
    int totalFrames{0};
    double exposureDuration{1.0};
    double intervalDuration{0.0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point estimatedCompletion;
} ATOM_ALIGNAS(128);

// Image quality metrics
struct ImageQuality {
    double mean{0.0};
    double standardDeviation{0.0};
    double minimum{0.0};
    double maximum{0.0};
    double signal{0.0};
    double noise{0.0};
    double snr{0.0}; // Signal-to-noise ratio
} ATOM_ALIGNAS(64);

// Frame statistics
struct FrameStatistics {
    uint64_t totalFrames{0};
    uint64_t droppedFrames{0};
    double averageFrameRate{0.0};
    double peakFrameRate{0.0};
    std::chrono::system_clock::time_point lastFrameTime;
    size_t totalDataReceived{0}; // in bytes
} ATOM_ALIGNAS(128);

// Upload settings for image save
struct UploadSettings {
    std::string directory{"."};
    std::string prefix{"image"};
    std::string suffix{""};
    bool useTimestamp{true};
    bool createDirectories{true};
} ATOM_ALIGNAS(16);

class AtomCamera : public AtomDriver {
public:
    explicit AtomCamera(const std::string &name);
    ~AtomCamera() override = default;

    // Camera type
    CameraType getCameraType() const { return camera_type_; }
    void setCameraType(CameraType type) { camera_type_ = type; }

    // Capabilities
    const CameraCapabilities& getCameraCapabilities() const { return camera_capabilities_; }
    void setCameraCapabilities(const CameraCapabilities& caps) { camera_capabilities_ = caps; }

    // 曝光控制
    virtual auto startExposure(double duration) -> bool = 0;
    virtual auto abortExposure() -> bool = 0;
    [[nodiscard]] virtual auto isExposing() const -> bool = 0;
    [[nodiscard]] virtual auto getExposureProgress() const -> double = 0;
    [[nodiscard]] virtual auto getExposureRemaining() const -> double = 0;
    virtual auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> = 0;
    virtual auto saveImage(const std::string &path) -> bool = 0;

    // 曝光历史和统计
    virtual auto getLastExposureDuration() const -> double = 0;
    virtual auto getExposureCount() const -> uint32_t = 0;
    virtual auto resetExposureCount() -> bool = 0;

    // 视频/流控制
    virtual auto startVideo() -> bool = 0;
    virtual auto stopVideo() -> bool = 0;
    [[nodiscard]] virtual auto isVideoRunning() const -> bool = 0;
    virtual auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> = 0;
    virtual auto setVideoFormat(const std::string& format) -> bool = 0;
    virtual auto getVideoFormats() -> std::vector<std::string> = 0;

    // 温度控制
    virtual auto startCooling(double targetTemp) -> bool = 0;
    virtual auto stopCooling() -> bool = 0;
    [[nodiscard]] virtual auto isCoolerOn() const -> bool = 0;
    [[nodiscard]] virtual auto getTemperature() const -> std::optional<double> = 0;
    [[nodiscard]] virtual auto getTemperatureInfo() const -> TemperatureInfo = 0;
    [[nodiscard]] virtual auto getCoolingPower() const -> std::optional<double> = 0;
    [[nodiscard]] virtual auto hasCooler() const -> bool = 0;
    virtual auto setTemperature(double temperature) -> bool = 0;

    // 色彩信息
    [[nodiscard]] virtual auto isColor() const -> bool = 0;
    [[nodiscard]] virtual auto getBayerPattern() const -> BayerPattern = 0;
    virtual auto setBayerPattern(BayerPattern pattern) -> bool = 0;

    // 参数控制
    virtual auto setGain(int gain) -> bool = 0;
    [[nodiscard]] virtual auto getGain() -> std::optional<int> = 0;
    [[nodiscard]] virtual auto getGainRange() -> std::pair<int, int> = 0;
    
    virtual auto setOffset(int offset) -> bool = 0;
    [[nodiscard]] virtual auto getOffset() -> std::optional<int> = 0;
    [[nodiscard]] virtual auto getOffsetRange() -> std::pair<int, int> = 0;
    
    virtual auto setISO(int iso) -> bool = 0;
    [[nodiscard]] virtual auto getISO() -> std::optional<int> = 0;
    [[nodiscard]] virtual auto getISOList() -> std::vector<int> = 0;

    // 帧设置
    virtual auto getResolution() -> std::optional<AtomCameraFrame::Resolution> = 0;
    virtual auto setResolution(int x, int y, int width, int height) -> bool = 0;
    virtual auto getMaxResolution() -> AtomCameraFrame::Resolution = 0;
    
    virtual auto getBinning() -> std::optional<AtomCameraFrame::Binning> = 0;
    virtual auto setBinning(int horizontal, int vertical) -> bool = 0;
    virtual auto getMaxBinning() -> AtomCameraFrame::Binning = 0;
    
    virtual auto setFrameType(FrameType type) -> bool = 0;
    virtual auto getFrameType() -> FrameType = 0;
    virtual auto setUploadMode(UploadMode mode) -> bool = 0;
    virtual auto getUploadMode() -> UploadMode = 0;
    [[nodiscard]] virtual auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> = 0;

    // 像素信息
    virtual auto getPixelSize() -> double = 0;
    virtual auto getPixelSizeX() -> double = 0;
    virtual auto getPixelSizeY() -> double = 0;
    virtual auto getBitDepth() -> int = 0;

    // 快门控制
    virtual auto hasShutter() -> bool = 0;
    virtual auto setShutter(bool open) -> bool = 0;
    virtual auto getShutterStatus() -> bool = 0;

    // 风扇控制
    virtual auto hasFan() -> bool = 0;
    virtual auto setFanSpeed(int speed) -> bool = 0;
    virtual auto getFanSpeed() -> int = 0;

    // Advanced video features (new)
    virtual auto startVideoRecording(const std::string& filename) -> bool = 0;
    virtual auto stopVideoRecording() -> bool = 0;
    virtual auto isVideoRecording() const -> bool = 0;
    virtual auto setVideoExposure(double exposure) -> bool = 0;
    virtual auto getVideoExposure() const -> double = 0;
    virtual auto setVideoGain(int gain) -> bool = 0;
    virtual auto getVideoGain() const -> int = 0;
    
    // Image sequence capabilities (new)
    virtual auto startSequence(int count, double exposure, double interval) -> bool = 0;
    virtual auto stopSequence() -> bool = 0;
    virtual auto isSequenceRunning() const -> bool = 0;
    virtual auto getSequenceProgress() const -> std::pair<int, int> = 0; // current, total
    
    // Advanced image processing (new)
    virtual auto setImageFormat(const std::string& format) -> bool = 0;
    virtual auto getImageFormat() const -> std::string = 0;
    virtual auto enableImageCompression(bool enable) -> bool = 0;
    virtual auto isImageCompressionEnabled() const -> bool = 0;
    virtual auto getSupportedImageFormats() const -> std::vector<std::string> = 0;
    
    // Image quality and statistics (new)
    virtual auto getFrameStatistics() const -> std::map<std::string, double> = 0;
    virtual auto getTotalFramesReceived() const -> uint64_t = 0;
    virtual auto getDroppedFrames() const -> uint64_t = 0;
    virtual auto getAverageFrameRate() const -> double = 0;
    virtual auto getLastImageQuality() const -> std::map<std::string, double> = 0;

    // 事件回调
    using ExposureCallback = std::function<void(bool success, const std::string& message)>;
    using TemperatureCallback = std::function<void(double temperature, double coolingPower)>;
    using VideoFrameCallback = std::function<void(std::shared_ptr<AtomCameraFrame>)>;
    using SequenceCallback = std::function<void(SequenceState state, int current, int total)>;
    using ImageQualityCallback = std::function<void(const ImageQuality& quality)>;

    virtual void setExposureCallback(ExposureCallback callback) { exposure_callback_ = std::move(callback); }
    virtual void setTemperatureCallback(TemperatureCallback callback) { temperature_callback_ = std::move(callback); }
    virtual void setVideoFrameCallback(VideoFrameCallback callback) { video_callback_ = std::move(callback); }
    virtual void setSequenceCallback(SequenceCallback callback) { sequence_callback_ = std::move(callback); }
    virtual void setImageQualityCallback(ImageQualityCallback callback) { image_quality_callback_ = std::move(callback); }

protected:
    std::shared_ptr<AtomCameraFrame> current_frame_;
    CameraType camera_type_{CameraType::PRIMARY};
    CameraCapabilities camera_capabilities_;
    TemperatureInfo temperature_info_;
    CameraState camera_state_{CameraState::IDLE};
    
    // 曝光参数
    double current_exposure_duration_{0.0};
    std::chrono::system_clock::time_point exposure_start_time_;
    
    // 统计信息
    uint32_t exposure_count_{0};
    double last_exposure_duration_{0.0};
    
    // 回调函数
    ExposureCallback exposure_callback_;
    TemperatureCallback temperature_callback_;
    VideoFrameCallback video_callback_;
    SequenceCallback sequence_callback_;
    ImageQualityCallback image_quality_callback_;
    
    // Enhanced information structures
    VideoInfo video_info_;
    SequenceInfo sequence_info_;
    ImageQuality last_image_quality_;
    FrameStatistics frame_statistics_;
    
    // 辅助方法
    virtual void updateCameraState(CameraState state) { camera_state_ = state; }
    virtual void notifyExposureComplete(bool success, const std::string& message = "");
    virtual void notifyTemperatureChange();
    virtual void notifyVideoFrame(std::shared_ptr<AtomCameraFrame> frame);
    virtual void notifySequenceProgress(SequenceState state, int current, int total);
    virtual void notifyImageQuality(const ImageQuality& quality);
    
    // Enhanced getter methods for information structures
    const VideoInfo& getVideoInfo() const { return video_info_; }
    const SequenceInfo& getSequenceInfo() const { return sequence_info_; }
    const ImageQuality& getImageQuality() const { return last_image_quality_; }
    const FrameStatistics& getStatistics() const { return frame_statistics_; }
};

inline void AtomCamera::notifyExposureComplete(bool success, const std::string& message) {
    if (exposure_callback_) {
        exposure_callback_(success, message);
    }
}

inline void AtomCamera::notifyTemperatureChange() {
    if (temperature_callback_) {
        temperature_callback_(temperature_info_.current, temperature_info_.coolingPower);
    }
}

inline void AtomCamera::notifyVideoFrame(std::shared_ptr<AtomCameraFrame> frame) {
    if (video_callback_) {
        video_callback_(frame);
    }
}

inline void AtomCamera::notifySequenceProgress(SequenceState state, int current, int total) {
    if (sequence_callback_) {
        sequence_callback_(state, current, total);
    }
}

inline void AtomCamera::notifyImageQuality(const ImageQuality& quality) {
    if (image_quality_callback_) {
        image_quality_callback_(quality);
    }
}
