/*
 * video_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Video Manager Component

This component manages video streaming, live view, and video recording
functionality for ASCOM cameras.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <vector>
#include <queue>

#include "device/template/camera_frame.hpp"

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Video Manager for ASCOM Camera
 * 
 * Manages video streaming, live view, and recording operations
 * with frame buffering and statistics tracking.
 */
class VideoManager {
public:
    enum class VideoState {
        STOPPED,
        STARTING,
        STREAMING,
        RECORDING,
        STOPPING,
        ERROR
    };

    struct VideoSettings {
        int width = 0;                          // Video width (0 = full frame)
        int height = 0;                         // Video height (0 = full frame)
        int binning = 1;                        // Binning factor
        double fps = 30.0;                      // Target frames per second
        std::string format = "RAW16";           // Video format
        double exposure = 33.0;                 // Exposure time in milliseconds
        int gain = 0;                           // Camera gain
        int offset = 0;                         // Camera offset
        int startX = 0;                         // ROI start X
        int startY = 0;                         // ROI start Y
        bool enableBuffering = true;            // Enable frame buffering
        size_t bufferSize = 10;                 // Frame buffer size
    };

    struct VideoStatistics {
        double actualFPS = 0.0;                 // Actual frames per second
        uint64_t framesReceived = 0;            // Total frames received
        uint64_t framesDropped = 0;             // Frames dropped due to buffer full
        uint64_t frameErrors = 0;               // Frame errors/corruptions
        double averageFrameTime = 0.0;          // Average time between frames (ms)
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastFrameTime;
        size_t bufferUtilization = 0;          // Current buffer usage
    };

    struct RecordingSettings {
        std::string filename;                   // Output filename
        std::string format = "SER";             // Recording format (SER, AVI, etc.)
        bool compressFrames = false;            // Enable frame compression
        int maxFrames = 0;                      // Max frames to record (0 = unlimited)
        std::chrono::seconds maxDuration{0};    // Max recording duration (0 = unlimited)
        bool includeTimestamps = true;          // Include timestamps in recording
    };

    using FrameCallback = std::function<void(std::shared_ptr<AtomCameraFrame>)>;
    using StatisticsCallback = std::function<void(const VideoStatistics&)>;
    using StateCallback = std::function<void(VideoState, const std::string&)>;
    using RecordingCallback = std::function<void(bool success, const std::string& message)>;

public:
    explicit VideoManager(std::shared_ptr<HardwareInterface> hardware);
    ~VideoManager();

    // Non-copyable and non-movable
    VideoManager(const VideoManager&) = delete;
    VideoManager& operator=(const VideoManager&) = delete;
    VideoManager(VideoManager&&) = delete;
    VideoManager& operator=(VideoManager&&) = delete;

    // =========================================================================
    // Video Streaming Control
    // =========================================================================

    /**
     * @brief Start video streaming
     * @param settings Video configuration
     * @return true if streaming started successfully
     */
    bool startVideo(const VideoSettings& settings);

    /**
     * @brief Start video streaming with default settings
     * @return true if streaming started successfully
     */
    bool startVideo();

    /**
     * @brief Stop video streaming
     * @return true if streaming stopped successfully
     */
    bool stopVideo();

    /**
     * @brief Check if video is streaming
     * @return true if streaming active
     */
    bool isVideoActive() const { 
        auto state = state_.load();
        return state == VideoState::STREAMING || state == VideoState::RECORDING; 
    }

    /**
     * @brief Pause video streaming
     * @return true if paused successfully
     */
    bool pauseVideo();

    /**
     * @brief Resume video streaming
     * @return true if resumed successfully
     */
    bool resumeVideo();

    // =========================================================================
    // Video Recording
    // =========================================================================

    /**
     * @brief Start video recording
     * @param settings Recording configuration
     * @return true if recording started successfully
     */
    bool startRecording(const RecordingSettings& settings);

    /**
     * @brief Stop video recording
     * @return true if recording stopped successfully
     */
    bool stopRecording();

    /**
     * @brief Check if recording is active
     * @return true if recording
     */
    bool isRecording() const { return state_.load() == VideoState::RECORDING; }

    /**
     * @brief Get current recording duration
     * @return Recording duration
     */
    std::chrono::duration<double> getRecordingDuration() const;

    /**
     * @brief Get recorded frame count
     * @return Number of frames recorded
     */
    uint64_t getRecordedFrameCount() const { return recordedFrames_.load(); }

    // =========================================================================
    // Frame Management
    // =========================================================================

    /**
     * @brief Get latest video frame
     * @return Latest frame or nullptr if none available
     */
    std::shared_ptr<AtomCameraFrame> getLatestFrame();

    /**
     * @brief Get frame from buffer
     * @param index Buffer index (0 = latest)
     * @return Frame or nullptr if not available
     */
    std::shared_ptr<AtomCameraFrame> getBufferedFrame(size_t index = 0);

    /**
     * @brief Get current buffer size
     * @return Number of frames in buffer
     */
    size_t getBufferSize() const;

    /**
     * @brief Clear frame buffer
     */
    void clearBuffer();

    // =========================================================================
    // State and Statistics
    // =========================================================================

    /**
     * @brief Get current video state
     * @return Current state
     */
    VideoState getState() const { return state_.load(); }

    /**
     * @brief Get state as string
     * @return State description
     */
    std::string getStateString() const;

    /**
     * @brief Get video statistics
     * @return Statistics structure
     */
    VideoStatistics getStatistics() const;

    /**
     * @brief Reset video statistics
     */
    void resetStatistics();

    /**
     * @brief Get current video settings
     * @return Current settings
     */
    VideoSettings getCurrentSettings() const { return currentSettings_; }

    /**
     * @brief Get supported video formats
     * @return Vector of format strings
     */
    std::vector<std::string> getSupportedFormats() const;

    // =========================================================================
    // Settings and Configuration
    // =========================================================================

    /**
     * @brief Update video settings during streaming
     * @param settings New settings
     * @return true if updated successfully
     */
    bool updateSettings(const VideoSettings& settings);

    /**
     * @brief Set video format
     * @param format Format string
     * @return true if set successfully
     */
    bool setVideoFormat(const std::string& format);

    /**
     * @brief Set target frame rate
     * @param fps Frames per second
     * @return true if set successfully
     */
    bool setFrameRate(double fps);

    /**
     * @brief Set video exposure time
     * @param exposureMs Exposure time in milliseconds
     * @return true if set successfully
     */
    bool setVideoExposure(double exposureMs);

    /**
     * @brief Set video gain
     * @param gain Gain value
     * @return true if set successfully
     */
    bool setVideoGain(int gain);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set frame callback
     * @param callback Callback function
     */
    void setFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        frameCallback_ = std::move(callback);
    }

    /**
     * @brief Set statistics callback
     * @param callback Callback function
     */
    void setStatisticsCallback(StatisticsCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        statisticsCallback_ = std::move(callback);
    }

    /**
     * @brief Set state change callback
     * @param callback Callback function
     */
    void setStateCallback(StateCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        stateCallback_ = std::move(callback);
    }

    /**
     * @brief Set recording completion callback
     * @param callback Callback function
     */
    void setRecordingCallback(RecordingCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        recordingCallback_ = std::move(callback);
    }

    // =========================================================================
    // Advanced Configuration
    // =========================================================================

    /**
     * @brief Set statistics update interval
     * @param intervalMs Interval in milliseconds
     */
    void setStatisticsInterval(int intervalMs) {
        statisticsInterval_ = std::chrono::milliseconds(intervalMs);
    }

    /**
     * @brief Enable/disable frame dropping when buffer is full
     * @param enable True to enable frame dropping
     */
    void setFrameDropping(bool enable) { allowFrameDropping_ = enable; }

    /**
     * @brief Set maximum buffer size
     * @param size Maximum number of frames to buffer
     */
    void setMaxBufferSize(size_t size);

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<VideoState> state_{VideoState::STOPPED};
    mutable std::mutex stateMutex_;

    // Current settings
    VideoSettings currentSettings_;
    RecordingSettings currentRecordingSettings_;

    // Frame management
    mutable std::mutex frameMutex_;
    std::queue<std::shared_ptr<AtomCameraFrame>> frameBuffer_;
    size_t maxBufferSize_ = 10;
    bool allowFrameDropping_ = true;

    // Statistics
    mutable std::mutex statisticsMutex_;
    VideoStatistics statistics_;
    std::chrono::steady_clock::time_point lastStatisticsUpdate_;

    // Recording state
    std::atomic<uint64_t> recordedFrames_{0};
    std::chrono::steady_clock::time_point recordingStartTime_;
    std::string currentRecordingFile_;

    // Streaming thread
    std::unique_ptr<std::thread> streamingThread_;
    std::atomic<bool> streamingRunning_{false};
    std::condition_variable streamingCondition_;

    // Callbacks
    mutable std::mutex callbackMutex_;
    FrameCallback frameCallback_;
    StatisticsCallback statisticsCallback_;
    StateCallback stateCallback_;
    RecordingCallback recordingCallback_;

    // Configuration
    std::chrono::milliseconds statisticsInterval_{1000}; // 1 second
    std::chrono::milliseconds frameTimeout_{5000}; // 5 seconds

    // Helper methods
    void setState(VideoState newState);
    void streamingLoop();
    void captureFrame();
    void addFrameToBuffer(std::shared_ptr<AtomCameraFrame> frame);
    void updateStatistics();
    void recordFrame(std::shared_ptr<AtomCameraFrame> frame);
    void finalizeRecording();
    void invokeFrameCallback(std::shared_ptr<AtomCameraFrame> frame);
    void invokeStatisticsCallback();
    void invokeStateCallback(VideoState state, const std::string& message);
    void invokeRecordingCallback(bool success, const std::string& message);
    std::shared_ptr<AtomCameraFrame> createVideoFrame(const std::vector<uint16_t>& imageData);
    bool setupVideoMode();
    bool teardownVideoMode();
    double calculateActualFPS() const;
};

} // namespace lithium::device::ascom::camera::components
