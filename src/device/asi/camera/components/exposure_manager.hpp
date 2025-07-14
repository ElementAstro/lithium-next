/*
 * exposure_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Exposure Manager Component

This component manages all exposure-related functionality including
single exposures, exposure sequences, progress tracking, and result handling.

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

#include "../../../template/camera_frame.hpp"

namespace lithium::device::asi::camera::components {

class HardwareInterface;

/**
 * @brief Exposure Manager for ASI Camera
 *
 * Manages all exposure operations including single exposures, sequences,
 * progress tracking, timeout handling, and result processing.
 */
class ExposureManager {
public:
    enum class ExposureState {
        IDLE,
        PREPARING,
        EXPOSING,
        DOWNLOADING,
        COMPLETE,
        ABORTED,
        ERROR
    };

    struct ExposureSettings {
        double duration = 1.0;          // Exposure duration in seconds
        int width = 0;                  // Image width (0 = full frame)
        int height = 0;                 // Image height (0 = full frame)
        int binning = 1;                // Binning factor
        std::string format = "RAW16";   // Image format
        bool isDark = false;            // Dark frame flag
        int startX = 0;                 // ROI start X
        int startY = 0;                 // ROI start Y
    };

    struct ExposureResult {
        bool success = false;
        std::shared_ptr<AtomCameraFrame> frame;
        double actualDuration = 0.0;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        std::string errorMessage;
    };

    using ExposureCallback = std::function<void(const ExposureResult&)>;
    using ProgressCallback = std::function<void(double progress, double remainingTime)>;

public:
    explicit ExposureManager(std::shared_ptr<HardwareInterface> hardware);
    ~ExposureManager();

    // Non-copyable and non-movable
    ExposureManager(const ExposureManager&) = delete;
    ExposureManager& operator=(const ExposureManager&) = delete;
    ExposureManager(ExposureManager&&) = delete;
    ExposureManager& operator=(ExposureManager&&) = delete;

    // Exposure Control
    bool startExposure(const ExposureSettings& settings);
    bool abortExposure();
    bool isExposing() const { return state_ == ExposureState::EXPOSING || state_ == ExposureState::DOWNLOADING; }

    // State and Progress
    ExposureState getState() const { return state_; }
    std::string getStateString() const;
    double getProgress() const;
    double getRemainingTime() const;
    double getElapsedTime() const;

    // Results
    ExposureResult getLastResult() const;
    bool hasResult() const { return lastResult_.success || !lastResult_.errorMessage.empty(); }
    void clearResult();

    // Settings
    void setExposureCallback(ExposureCallback callback);
    void setProgressCallback(ProgressCallback callback);
    void setProgressUpdateInterval(std::chrono::milliseconds interval) { progressUpdateInterval_ = interval; }
    void setTimeoutDuration(std::chrono::seconds timeout) { timeoutDuration_ = timeout; }

    // Statistics
    uint32_t getCompletedExposures() const { return completedExposures_; }
    uint32_t getAbortedExposures() const { return abortedExposures_; }
    uint32_t getFailedExposures() const { return failedExposures_; }
    double getTotalExposureTime() const { return totalExposureTime_; }
    void resetStatistics();

    // Configuration
    void setMaxRetries(int retries) { maxRetries_ = retries; }
    int getMaxRetries() const { return maxRetries_; }
    void setRetryDelay(std::chrono::milliseconds delay) { retryDelay_ = delay; }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<ExposureState> state_{ExposureState::IDLE};
    ExposureSettings currentSettings_;
    ExposureResult lastResult_;

    // Threading
    std::thread exposureThread_;
    std::atomic<bool> abortRequested_{false};
    std::mutex stateMutex_;
    std::condition_variable stateCondition_;

    // Progress tracking
    std::chrono::steady_clock::time_point exposureStartTime_;
    std::atomic<double> currentProgress_{0.0};
    std::chrono::milliseconds progressUpdateInterval_{100};
    std::chrono::seconds timeoutDuration_{600}; // 10 minutes default

    // Callbacks
    ExposureCallback exposureCallback_;
    ProgressCallback progressCallback_;
    std::mutex callbackMutex_;

    // Statistics
    std::atomic<uint32_t> completedExposures_{0};
    std::atomic<uint32_t> abortedExposures_{0};
    std::atomic<uint32_t> failedExposures_{0};
    std::atomic<double> totalExposureTime_{0.0};

    // Configuration
    int maxRetries_ = 3;
    std::chrono::milliseconds retryDelay_{1000};

    // Worker methods
    void exposureWorker();
    bool executeExposure(const ExposureSettings& settings, ExposureResult& result);
    bool prepareExposure(const ExposureSettings& settings);
    bool waitForExposureComplete(double duration);
    bool downloadImage(ExposureResult& result);
    void updateProgress();
    void notifyExposureComplete(const ExposureResult& result);
    void notifyProgress(double progress, double remainingTime);

    // Helper methods
    void updateState(ExposureState newState);
    std::shared_ptr<AtomCameraFrame> createFrameFromBuffer(const unsigned char* buffer,
                                                           const ExposureSettings& settings);
    size_t calculateBufferSize(const ExposureSettings& settings);
    bool validateExposureSettings(const ExposureSettings& settings);
    std::string formatExposureError(const std::string& operation, const std::string& error);
};

} // namespace lithium::device::asi::camera::components
