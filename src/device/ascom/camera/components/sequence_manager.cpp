/*
 * sequence_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Sequence Manager Component Implementation

This component manages image sequences, batch captures, and automated
shooting sequences for the ASCOM camera.

*************************************************/

#include "sequence_manager.hpp"
#include "hardware_interface.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::device::ascom::camera::components {

SequenceManager::SequenceManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    LOG_F(INFO, "ASCOM Camera SequenceManager initialized");
}

bool SequenceManager::initialize() {
    LOG_F(INFO, "Initializing sequence manager");
    
    if (!hardware_) {
        LOG_F(ERROR, "Hardware interface not available");
        return false;
    }
    
    // Reset state
    sequenceRunning_ = false;
    sequencePaused_ = false;
    currentImage_ = 0;
    totalImages_ = 0;
    successfulImages_ = 0;
    failedImages_ = 0;
    
    LOG_F(INFO, "Sequence manager initialized successfully");
    return true;
}

bool SequenceManager::startSequence(int count, double exposure, double interval) {
    SequenceSettings settings;
    settings.totalCount = count;
    settings.exposureTime = exposure;
    settings.intervalTime = interval;
    
    return startSequence(settings);
}

bool SequenceManager::startSequence(const SequenceSettings& settings) {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    
    if (sequenceRunning_) {
        LOG_F(WARNING, "Sequence already running");
        return false;
    }
    
    if (!hardware_ || !hardware_->isConnected()) {
        LOG_F(ERROR, "Hardware not connected");
        return false;
    }
    
    currentSettings_ = settings;
    currentImage_ = 0;
    totalImages_ = settings.totalCount;
    sequenceRunning_ = true;
    sequencePaused_ = false;
    sequenceStartTime_ = std::chrono::steady_clock::now();
    
    LOG_F(INFO, "Sequence started: {} images, {}s exposure, {}s interval", 
          settings.totalCount, settings.exposureTime, settings.intervalTime);
    
    return true;
}

bool SequenceManager::stopSequence() {
    if (!sequenceRunning_) {
        LOG_F(WARNING, "No sequence running");
        return false;
    }
    
    sequenceRunning_ = false;
    sequencePaused_ = false;
    
    LOG_F(INFO, "Sequence stopped");
    
    if (completionCallback_) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        completionCallback_(false, "Sequence manually stopped");
    }
    
    return true;
}

bool SequenceManager::pauseSequence() {
    if (!sequenceRunning_) {
        LOG_F(WARNING, "No sequence running");
        return false;
    }
    
    sequencePaused_ = true;
    LOG_F(INFO, "Sequence paused");
    return true;
}

bool SequenceManager::resumeSequence() {
    if (!sequenceRunning_) {
        LOG_F(WARNING, "No sequence running");
        return false;
    }
    
    sequencePaused_ = false;
    LOG_F(INFO, "Sequence resumed");
    return true;
}

bool SequenceManager::isSequenceRunning() const {
    return sequenceRunning_.load();
}

bool SequenceManager::isSequencePaused() const {
    return sequencePaused_.load();
}

std::pair<int, int> SequenceManager::getSequenceProgress() const {
    return {currentImage_.load(), totalImages_.load()};
}

double SequenceManager::getProgressPercentage() const {
    int total = totalImages_.load();
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(currentImage_.load()) / total;
}

SequenceManager::SequenceSettings SequenceManager::getCurrentSettings() const {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    return currentSettings_;
}

std::chrono::seconds SequenceManager::getEstimatedTimeRemaining() const {
    if (!sequenceRunning_) {
        return std::chrono::seconds(0);
    }
    
    int remaining = totalImages_.load() - currentImage_.load();
    if (remaining <= 0) {
        return std::chrono::seconds(0);
    }
    
    std::lock_guard<std::mutex> lock(settingsMutex_);
    double timePerImage = currentSettings_.exposureTime + currentSettings_.intervalTime;
    return std::chrono::seconds(static_cast<long>(remaining * timePerImage));
}

std::map<std::string, double> SequenceManager::getSequenceStatistics() const {
    std::map<std::string, double> stats;
    
    stats["current_image"] = currentImage_.load();
    stats["total_images"] = totalImages_.load();
    stats["successful_images"] = successfulImages_.load();
    stats["failed_images"] = failedImages_.load();
    stats["progress_percentage"] = getProgressPercentage();
    
    if (sequenceRunning_) {
        auto elapsed = std::chrono::steady_clock::now() - sequenceStartTime_;
        stats["elapsed_time_seconds"] = std::chrono::duration<double>(elapsed).count();
        stats["estimated_remaining_seconds"] = getEstimatedTimeRemaining().count();
    }
    
    return stats;
}

void SequenceManager::setProgressCallback(const ProgressCallback& callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    progressCallback_ = callback;
}

void SequenceManager::setCompletionCallback(const CompletionCallback& callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    completionCallback_ = callback;
}

} // namespace lithium::device::ascom::camera::components
