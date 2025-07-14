/*
 * video_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Video Manager Component

This component manages video capture, streaming, and recording functionality
including real-time video feed, frame processing, and video file output.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <chrono>

#include "../../../template/camera_frame.hpp"

namespace lithium::device::asi::camera::components {

class HardwareInterface;

/**
 * @brief Video Manager for ASI Camera
 *
 * Manages video capture, streaming, and recording operations with
 * frame buffering, real-time processing, and format conversion.
 */
class VideoManager {
public:
    enum class VideoState {
        IDLE,
        STARTING,
        STREAMING,
        STOPPING,
        ERROR
    };

    struct VideoSettings {
        int width = 0;                      // Video width (0 = full frame)
        int height = 0;                     // Video height (0 = full frame)
        int binning = 1;                    // Binning factor
        std::string format = "RAW16";       // Video format
        double fps = 30.0;                  // Target frame rate
        int exposure = 33000;               // Exposure time in microseconds
        int gain = 0;                       // Gain value
        bool autoExposure = false;          // Auto exposure mode
        bool autoGain = false;              // Auto gain mode
        int bufferSize = 10;                // Frame buffer size
        int startX = 0;                     // ROI start X
        int startY = 0;                     // ROI start Y
    };

    struct VideoStatistics {
        uint64_t framesReceived = 0;
        uint64_t framesProcessed = 0;
        uint64_t framesDropped = 0;
        double actualFPS = 0.0;
        double dataRate = 0.0;              // MB/s
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastFrameTime;
    };

    using FrameCallback = std::function<void(std::shared_ptr<AtomCameraFrame>)>;
    using StatisticsCallback = std::function<void(const VideoStatistics&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

public:
    explicit VideoManager(std::shared_ptr<HardwareInterface> hardware);
    ~VideoManager();

    // Non-copyable and non-movable
    VideoManager(const VideoManager&) = delete;
    VideoManager& operator=(const VideoManager&) = delete;
    VideoManager(VideoManager&&) = delete;
    VideoManager& operator=(VideoManager&&) = delete;

    // Video Control
    bool startVideo(const VideoSettings& settings);
    bool stopVideo();
    bool isStreaming() const { return state_ == VideoState::STREAMING; }

    // State and Status
    VideoState getState() const { return state_; }
    std::string getStateString() const;
    VideoStatistics getStatistics() const;
    void resetStatistics();

    // Frame Access
    std::shared_ptr<AtomCameraFrame> getLatestFrame();
    bool hasFrameAvailable() const;
    size_t getBufferSize() const;
    size_t getBufferUsage() const;

    // Settings Management
    VideoSettings getCurrentSettings() const;
    bool updateSettings(const VideoSettings& settings);
    bool updateExposure(int exposureUs);
    bool updateGain(int gain);
    bool updateFrameRate(double fps);

    // Recording Control
    bool startRecording(const std::string& filename, const std::string& codec = "H264");
    bool stopRecording();
    bool isRecording() const { return recording_; }
    std::string getRecordingFilename() const;
    uint64_t getRecordedFrames() const { return recordedFrames_; }

    // Callbacks
    void setFrameCallback(FrameCallback callback);
    void setStatisticsCallback(StatisticsCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // Configuration
    void setFrameBufferSize(size_t size);
    void setStatisticsUpdateInterval(std::chrono::milliseconds interval) { statisticsInterval_ = interval; }
    void setDropFramesWhenBufferFull(bool drop) { dropFramesWhenFull_ = drop; }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<VideoState> state_{VideoState::IDLE};
    VideoSettings currentSettings_;
    VideoStatistics statistics_;

    // Threading
    std::thread captureThread_;
    std::thread processingThread_;
    std::atomic<bool> stopRequested_{false};

    // Frame buffering
    std::queue<std::shared_ptr<AtomCameraFrame>> frameBuffer_;
    mutable std::mutex bufferMutex_;
    std::condition_variable bufferCondition_;
    size_t maxBufferSize_ = 10;
    bool dropFramesWhenFull_ = true;

    // Statistics and monitoring
    std::chrono::steady_clock::time_point lastStatisticsUpdate_;
    std::chrono::milliseconds statisticsInterval_{1000};
    std::thread statisticsThread_;

    // Recording
    std::atomic<bool> recording_{false};
    std::string recordingFilename_;
    std::string recordingCodec_;
    std::atomic<uint64_t> recordedFrames_{0};

    // Callbacks
    FrameCallback frameCallback_;
    StatisticsCallback statisticsCallback_;
    ErrorCallback errorCallback_;
    std::mutex callbackMutex_;

    // Worker methods
    void captureWorker();
    void processingWorker();
    void statisticsWorker();
    bool configureVideoMode(const VideoSettings& settings);
    bool startVideoCapture();
    bool stopVideoCapture();
    std::shared_ptr<AtomCameraFrame> captureFrame();
    void processFrame(std::shared_ptr<AtomCameraFrame> frame);
    void updateStatistics();
    void notifyFrame(std::shared_ptr<AtomCameraFrame> frame);
    void notifyStatistics(const VideoStatistics& stats);
    void notifyError(const std::string& error);

    // Helper methods
    void updateState(VideoState newState);
    bool validateVideoSettings(const VideoSettings& settings);
    std::shared_ptr<AtomCameraFrame> createFrameFromBuffer(const unsigned char* buffer,
                                                           const VideoSettings& settings);
    size_t calculateFrameSize(const VideoSettings& settings);
    bool saveFrameToFile(std::shared_ptr<AtomCameraFrame> frame);
    void cleanupResources();
    std::string formatVideoError(const std::string& operation, const std::string& error);
};

} // namespace lithium::device::asi::camera::components
