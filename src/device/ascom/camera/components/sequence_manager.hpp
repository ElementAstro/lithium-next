/*
 * sequence_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Sequence Manager Component

This component manages image sequences, batch captures, and automated
shooting sequences for the ASCOM camera.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

#include "device/template/camera.hpp"

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Sequence Manager for ASCOM Camera
 * 
 * Manages batch image capture sequences, automated shooting,
 * and sequence progress tracking.
 */
class SequenceManager {
public:
    struct SequenceSettings {
        int totalCount = 1;
        double exposureTime = 1.0;
        double intervalTime = 0.0;
        std::string outputPath;
        std::string filenamePattern = "image_{count:04d}";
        bool enableDithering = false;
        bool enableFilterWheel = false;
    };

    using ProgressCallback = std::function<void(int current, int total, double progress)>;
    using CompletionCallback = std::function<void(bool success, const std::string& message)>;

public:
    explicit SequenceManager(std::shared_ptr<HardwareInterface> hardware);
    ~SequenceManager() = default;

    // Non-copyable and non-movable
    SequenceManager(const SequenceManager&) = delete;
    SequenceManager& operator=(const SequenceManager&) = delete;
    SequenceManager(SequenceManager&&) = delete;
    SequenceManager& operator=(SequenceManager&&) = delete;

    // =========================================================================
    // Sequence Control
    // =========================================================================

    /**
     * @brief Initialize sequence manager
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Start image sequence
     * @param count Number of images to capture
     * @param exposure Exposure time in seconds
     * @param interval Interval between exposures in seconds
     * @return true if sequence started successfully
     */
    bool startSequence(int count, double exposure, double interval);

    /**
     * @brief Start sequence with settings
     * @param settings Sequence configuration
     * @return true if sequence started successfully
     */
    bool startSequence(const SequenceSettings& settings);

    /**
     * @brief Stop current sequence
     * @return true if sequence stopped successfully
     */
    bool stopSequence();

    /**
     * @brief Pause current sequence
     * @return true if sequence paused successfully
     */
    bool pauseSequence();

    /**
     * @brief Resume paused sequence
     * @return true if sequence resumed successfully
     */
    bool resumeSequence();

    /**
     * @brief Check if sequence is running
     * @return true if sequence is active
     */
    bool isSequenceRunning() const;

    /**
     * @brief Check if sequence is paused
     * @return true if sequence is paused
     */
    bool isSequencePaused() const;

    // =========================================================================
    // Progress and Status
    // =========================================================================

    /**
     * @brief Get sequence progress
     * @return Pair of (current, total) images
     */
    std::pair<int, int> getSequenceProgress() const;

    /**
     * @brief Get sequence progress percentage
     * @return Progress percentage (0.0 - 1.0)
     */
    double getProgressPercentage() const;

    /**
     * @brief Get current sequence settings
     * @return Current sequence configuration
     */
    SequenceSettings getCurrentSettings() const;

    /**
     * @brief Get estimated time remaining
     * @return Remaining time in seconds
     */
    std::chrono::seconds getEstimatedTimeRemaining() const;

    /**
     * @brief Get sequence statistics
     * @return Map of statistics
     */
    std::map<std::string, double> getSequenceStatistics() const;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    /**
     * @brief Set progress callback
     * @param callback Function to call on progress updates
     */
    void setProgressCallback(const ProgressCallback& callback);

    /**
     * @brief Set completion callback
     * @param callback Function to call on sequence completion
     */
    void setCompletionCallback(const CompletionCallback& callback);

private:
    std::shared_ptr<HardwareInterface> hardware_;
    
    // Sequence state
    std::atomic<bool> sequenceRunning_{false};
    std::atomic<bool> sequencePaused_{false};
    std::atomic<int> currentImage_{0};
    std::atomic<int> totalImages_{0};
    
    SequenceSettings currentSettings_;
    mutable std::mutex settingsMutex_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    std::mutex callbackMutex_;
    
    // Statistics
    std::chrono::steady_clock::time_point sequenceStartTime_;
    std::atomic<uint64_t> successfulImages_{0};
    std::atomic<uint64_t> failedImages_{0};
};

} // namespace lithium::device::ascom::camera::components
