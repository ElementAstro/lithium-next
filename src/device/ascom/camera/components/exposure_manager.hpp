/*
 * exposure_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Exposure Manager Component

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

#include "device/template/camera_frame.hpp"

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Exposure Manager for ASCOM Camera
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
        FrameType frameType = FrameType::FITS; // Frame type
        bool isDark = false;            // Dark frame flag
        int startX = 0;                 // ROI start X
        int startY = 0;                 // ROI start Y
        double timeoutSec = 0.0;        // Timeout (0 = no timeout)
    };

    struct ExposureResult {
        bool success = false;
        std::shared_ptr<AtomCameraFrame> frame;
        double actualDuration = 0.0;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        std::string errorMessage;
        ExposureSettings settings;
    };

    struct ExposureStatistics {
        uint32_t totalExposures = 0;
        uint32_t successfulExposures = 0;
        uint32_t failedExposures = 0;
        uint32_t abortedExposures = 0;
        double totalExposureTime = 0.0;
        double averageExposureTime = 0.0;
        std::chrono::steady_clock::time_point lastExposureTime;
    };

    using ExposureCallback = std::function<void(const ExposureResult&)>;
    using ProgressCallback = std::function<void(double progress, double remainingTime)>;
    using StateCallback = std::function<void(ExposureState oldState, ExposureState newState)>;

public:
    explicit ExposureManager(std::shared_ptr<HardwareInterface> hardware);
    ~ExposureManager();

    // Non-copyable and non-movable
    ExposureManager(const ExposureManager&) = delete;
    ExposureManager& operator=(const ExposureManager&) = delete;
    ExposureManager(ExposureManager&&) = delete;
    ExposureManager& operator=(ExposureManager&&) = delete;

    // =========================================================================
    // Exposure Control
    // =========================================================================

    /**
     * @brief Start an exposure
     * @param settings Exposure settings
     * @return true if exposure started successfully
     */
    bool startExposure(const ExposureSettings& settings);

    /**
     * @brief Start a simple exposure
     * @param duration Exposure duration in seconds
     * @param isDark Whether this is a dark frame
     * @return true if exposure started successfully
     */
    bool startExposure(double duration, bool isDark = false);

    /**
     * @brief Abort current exposure
     * @return true if exposure aborted successfully
     */
    bool abortExposure();

    /**
     * @brief Check if exposure is in progress
     * @return true if exposing
     */
    bool isExposing() const { 
        auto state = state_.load();
        return state == ExposureState::EXPOSING || state == ExposureState::DOWNLOADING; 
    }
    
    // =========================================================================
    // State and Progress
    // =========================================================================

    /**
     * @brief Get current exposure state
     * @return Current state
     */
    ExposureState getState() const { return state_.load(); }

    /**
     * @brief Get state as string
     * @return State description
     */
    std::string getStateString() const;

    /**
     * @brief Get exposure progress (0.0 to 1.0)
     * @return Progress value
     */
    double getProgress() const;

    /**
     * @brief Get remaining exposure time
     * @return Remaining time in seconds
     */
    double getRemainingTime() const;

    /**
     * @brief Get elapsed exposure time
     * @return Elapsed time in seconds
     */
    double getElapsedTime() const;

    /**
     * @brief Get current exposure duration
     * @return Duration in seconds
     */
    double getCurrentDuration() const { return currentSettings_.duration; }
    
    // =========================================================================
    // Results and Statistics
    // =========================================================================

    /**
     * @brief Get last exposure result
     * @return Last result structure
     */
    ExposureResult getLastResult() const;

    /**
     * @brief Check if result is available
     * @return true if result available
     */
    bool hasResult() const;

    /**
     * @brief Get exposure statistics
     * @return Statistics structure
     */
    ExposureStatistics getStatistics() const;

    /**
     * @brief Reset exposure statistics
     */
    void resetStatistics();

    /**
     * @brief Get total exposure count
     * @return Total number of exposures
     */
    uint32_t getExposureCount() const { return statistics_.totalExposures; }

    /**
     * @brief Get last exposure duration
     * @return Duration of last exposure in seconds
     */
    double getLastExposureDuration() const;

    // =========================================================================
    // Image Management
    // =========================================================================

    /**
     * @brief Check if image is ready for download
     * @return true if image ready
     */
    bool isImageReady() const;

    /**
     * @brief Download the captured image
     * @return Image frame or nullptr if failed
     */
    std::shared_ptr<AtomCameraFrame> downloadImage();

    /**
     * @brief Get the last captured frame
     * @return Last frame or nullptr if none
     */
    std::shared_ptr<AtomCameraFrame> getLastFrame() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set exposure completion callback
     * @param callback Callback function
     */
    void setExposureCallback(ExposureCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        exposureCallback_ = std::move(callback);
    }

    /**
     * @brief Set progress update callback
     * @param callback Callback function
     */
    void setProgressCallback(ProgressCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        progressCallback_ = std::move(callback);
    }

    /**
     * @brief Set state change callback
     * @param callback Callback function
     */
    void setStateCallback(StateCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        stateCallback_ = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set progress update interval
     * @param intervalMs Interval in milliseconds
     */
    void setProgressUpdateInterval(int intervalMs) {
        progressUpdateInterval_ = std::chrono::milliseconds(intervalMs);
    }

    /**
     * @brief Enable/disable automatic timeout
     * @param enable True to enable timeout
     * @param timeoutMultiplier Timeout = exposure_duration * multiplier
     */
    void setAutoTimeout(bool enable, double timeoutMultiplier = 2.0) {
        autoTimeoutEnabled_ = enable;
        timeoutMultiplier_ = timeoutMultiplier;
    }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<ExposureState> state_{ExposureState::IDLE};
    mutable std::mutex stateMutex_;
    std::condition_variable stateCondition_;

    // Current exposure
    ExposureSettings currentSettings_;
    std::chrono::steady_clock::time_point exposureStartTime_;
    std::atomic<bool> stopRequested_{false};

    // Results
    mutable std::mutex resultMutex_;
    ExposureResult lastResult_;
    std::shared_ptr<AtomCameraFrame> lastFrame_;

    // Statistics
    mutable std::mutex statisticsMutex_;
    ExposureStatistics statistics_;

    // Callbacks
    mutable std::mutex callbackMutex_;
    ExposureCallback exposureCallback_;
    ProgressCallback progressCallback_;
    StateCallback stateCallback_;

    // Monitoring thread
    std::unique_ptr<std::thread> monitorThread_;
    std::atomic<bool> monitorRunning_{false};

    // Configuration
    std::chrono::milliseconds progressUpdateInterval_{100};
    bool autoTimeoutEnabled_ = true;
    double timeoutMultiplier_ = 2.0;

    // Helper methods
    void setState(ExposureState newState);
    void monitorExposure();
    void updateProgress();
    void handleExposureComplete();
    void handleExposureError(const std::string& error);
    void invokeCallback(const ExposureResult& result);
    void updateStatistics(const ExposureResult& result);
    bool waitForImageReady(double timeoutSec);
    std::shared_ptr<AtomCameraFrame> createFrameFromImageData(const std::vector<uint16_t>& imageData);
    double calculateTimeout(double exposureDuration) const;
};

} // namespace lithium::device::ascom::camera::components
