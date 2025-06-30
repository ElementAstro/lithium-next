/*
 * exposure_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Exposure Manager Component Implementation

This component manages all exposure-related functionality including
single exposures, exposure sequences, progress tracking, and result handling.

*************************************************/

#include "exposure_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <thread>

namespace lithium::device::ascom::camera::components {

ExposureManager::ExposureManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    LOG_F(INFO, "ASCOM Camera ExposureManager initialized");
}

/*
 * exposure_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Exposure Manager Component Implementation

This component manages all exposure-related functionality including
single exposures, exposure sequences, progress tracking, and result handling.

*************************************************/

#include "exposure_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <thread>

namespace lithium::device::ascom::camera::components {

ExposureManager::ExposureManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    LOG_F(INFO, "ASCOM Camera ExposureManager initialized");
}

ExposureManager::~ExposureManager() {
    // Stop any running monitoring
    monitorRunning_ = false;
    if (monitorThread_ && monitorThread_->joinable()) {
        monitorThread_->join();
    }
    LOG_F(INFO, "ASCOM Camera ExposureManager destroyed");
}

// =========================================================================
// Exposure Control
// =========================================================================

bool ExposureManager::startExposure(const ExposureSettings& settings) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != ExposureState::IDLE) {
        LOG_F(ERROR, "Cannot start exposure: current state is {}", 
              static_cast<int>(state_.load()));
        return false;
    }
    
    if (!hardware_ || !hardware_->isConnected()) {
        LOG_F(ERROR, "Cannot start exposure: hardware not connected");
        return false;
    }
    
    LOG_F(INFO, "Starting exposure: duration={:.2f}s, {}x{}, binning={}, type={}", 
          settings.duration, settings.width, settings.height, 
          settings.binning, static_cast<int>(settings.frameType));
    
    currentSettings_ = settings;
    stopRequested_ = false;
    
    setState(ExposureState::PREPARING);
    
    // Configure camera parameters before exposure
    if (!configureExposureParameters()) {
        setState(ExposureState::ERROR);
        return false;
    }
    
    // Start the actual exposure
    if (!hardware_->startExposure(settings.duration, settings.isDark)) {
        LOG_F(ERROR, "Failed to start hardware exposure");
        setState(ExposureState::ERROR);
        return false;
    }
    
    exposureStartTime_ = std::chrono::steady_clock::now();
    setState(ExposureState::EXPOSING);
    
    // Start monitoring
    startMonitoring();
    
    return true;
}

bool ExposureManager::startExposure(double duration, bool isDark) {
    ExposureSettings settings;
    settings.duration = duration;
    settings.isDark = isDark;
    return startExposure(settings);
}

bool ExposureManager::abortExposure() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    auto currentState = state_.load();
    if (currentState == ExposureState::IDLE || currentState == ExposureState::COMPLETE) {
        return true; // Nothing to abort
    }
    
    LOG_F(INFO, "Aborting exposure");
    stopRequested_ = true;
    
    // Stop monitoring
    stopMonitoring();
    
    // Abort hardware exposure
    if (hardware_) {
        hardware_->abortExposure();
    }
    
    setState(ExposureState::ABORTED);
    updateStatistics(createAbortedResult());
    
    return true;
}

// =========================================================================
// State and Progress
// =========================================================================

std::string ExposureManager::getStateString() const {
    switch (state_.load()) {
        case ExposureState::IDLE: return "Idle";
        case ExposureState::PREPARING: return "Preparing";
        case ExposureState::EXPOSING: return "Exposing";
        case ExposureState::DOWNLOADING: return "Downloading";
        case ExposureState::COMPLETE: return "Complete";
        case ExposureState::ABORTED: return "Aborted";
        case ExposureState::ERROR: return "Error";
        default: return "Unknown";
    }
}

double ExposureManager::getProgress() const {
    auto currentState = state_.load();
    if (currentState != ExposureState::EXPOSING) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    
    if (currentSettings_.duration <= 0) {
        return 0.0;
    }
    
    double progress = elapsed / currentSettings_.duration;
    return std::clamp(progress, 0.0, 1.0);
}

double ExposureManager::getRemainingTime() const {
    auto currentState = state_.load();
    if (currentState != ExposureState::EXPOSING) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    
    double remaining = currentSettings_.duration - elapsed;
    return std::max(remaining, 0.0);
}

double ExposureManager::getElapsedTime() const {
    auto currentState = state_.load();
    if (currentState != ExposureState::EXPOSING) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - exposureStartTime_).count();
}

// =========================================================================
// Results and Statistics
// =========================================================================

ExposureManager::ExposureResult ExposureManager::getLastResult() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return lastResult_;
}

bool ExposureManager::hasResult() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return lastResult_.success || !lastResult_.errorMessage.empty();
}

ExposureManager::ExposureStatistics ExposureManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void ExposureManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = ExposureStatistics{};
    LOG_F(INFO, "Exposure statistics reset");
}

double ExposureManager::getLastExposureDuration() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return lastResult_.actualDuration;
}

// =========================================================================
// Image Management
// =========================================================================

bool ExposureManager::isImageReady() const {
    if (!hardware_) {
        return false;
    }
    return hardware_->isExposureComplete();
}

std::shared_ptr<AtomCameraFrame> ExposureManager::downloadImage() {
    if (!hardware_) {
        return nullptr;
    }
    
    setState(ExposureState::DOWNLOADING);
    auto frame = hardware_->downloadImage();
    
    if (frame) {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastFrame_ = frame;
        setState(ExposureState::COMPLETE);
    } else {
        setState(ExposureState::ERROR);
    }
    
    return frame;
}

std::shared_ptr<AtomCameraFrame> ExposureManager::getLastFrame() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return lastFrame_;
}

// =========================================================================
// Private Methods
// =========================================================================

void ExposureManager::setState(ExposureState newState) {
    ExposureState oldState = state_.exchange(newState);
    
    LOG_F(INFO, "Exposure state changed: {} -> {}", 
          static_cast<int>(oldState), static_cast<int>(newState));
    
    // Notify state callback
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stateCallback_) {
        stateCallback_(oldState, newState);
    }
}

void ExposureManager::monitorExposure() {
    while (monitorRunning_) {
        auto currentState = state_.load();
        
        if (currentState == ExposureState::EXPOSING) {
            // Update progress
            updateProgress();
            
            // Check if exposure is complete
            if (hardware_ && hardware_->isExposureComplete()) {
                handleExposureComplete();
                break;
            }
            
            // Check for timeout
            double timeout = calculateTimeout(currentSettings_.duration);
            if (timeout > 0) {
                auto elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - exposureStartTime_).count();
                if (elapsed > timeout) {
                    LOG_F(ERROR, "Exposure timeout after {:.2f}s", elapsed);
                    handleExposureError("Exposure timeout");
                    break;
                }
            }
        }
        
        std::this_thread::sleep_for(progressUpdateInterval_);
    }
}

void ExposureManager::updateProgress() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (progressCallback_) {
        double progress = getProgress();
        double remaining = getRemainingTime();
        progressCallback_(progress, remaining);
    }
}

void ExposureManager::handleExposureComplete() {
    auto frame = downloadImage();
    
    ExposureResult result;
    result.success = (frame != nullptr);
    result.frame = frame;
    result.actualDuration = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - exposureStartTime_).count();
    result.startTime = exposureStartTime_;
    result.endTime = std::chrono::steady_clock::now();
    result.settings = currentSettings_;
    
    if (!result.success) {
        result.errorMessage = "Failed to download image";
    }
    
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastResult_ = result;
    }
    
    updateStatistics(result);
    invokeCallback(result);
    
    monitorRunning_ = false;
}

void ExposureManager::handleExposureError(const std::string& error) {
    ExposureResult result;
    result.success = false;
    result.errorMessage = error;
    result.settings = currentSettings_;
    result.startTime = exposureStartTime_;
    result.endTime = std::chrono::steady_clock::now();
    
    setState(ExposureState::ERROR);
    
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastResult_ = result;
    }
    
    updateStatistics(result);
    invokeCallback(result);
    
    monitorRunning_ = false;
}

void ExposureManager::invokeCallback(const ExposureResult& result) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (exposureCallback_) {
        exposureCallback_(result);
    }
}

void ExposureManager::updateStatistics(const ExposureResult& result) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    statistics_.totalExposures++;
    statistics_.lastExposureTime = std::chrono::steady_clock::now();
    
    if (result.success) {
        statistics_.successfulExposures++;
        statistics_.totalExposureTime += result.actualDuration;
        statistics_.averageExposureTime = statistics_.totalExposureTime / 
                                         statistics_.successfulExposures;
    } else {
        statistics_.failedExposures++;
    }
}

bool ExposureManager::waitForImageReady(double timeoutSec) {
    auto start = std::chrono::steady_clock::now();
    
    while (!isImageReady()) {
        if (timeoutSec > 0) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeoutSec) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return true;
}

std::shared_ptr<AtomCameraFrame> ExposureManager::createFrameFromImageData(
    const std::vector<uint16_t>& imageData) {
    // This would need actual implementation based on AtomCameraFrame requirements
    // For now, return nullptr as placeholder
    LOG_F(WARNING, "createFrameFromImageData not implemented");
    return nullptr;
}

double ExposureManager::calculateTimeout(double exposureDuration) const {
    if (autoTimeoutEnabled_) {
        return exposureDuration * timeoutMultiplier_;
    }
    return 0.0; // No timeout
}

bool ExposureManager::configureExposureParameters() {
    if (!hardware_) {
        return false;
    }
    
    // Set binning
    if (!hardware_->setBinning(currentSettings_.binning, currentSettings_.binning)) {
        LOG_F(ERROR, "Failed to set binning to {}", currentSettings_.binning);
        return false;
    }
    
    // Set ROI if specified
    if (currentSettings_.width > 0 && currentSettings_.height > 0) {
        if (!hardware_->setROI(currentSettings_.startX, currentSettings_.startY,
                               currentSettings_.width, currentSettings_.height)) {
            LOG_F(ERROR, "Failed to set ROI: {}x{} at ({},{})", 
                  currentSettings_.width, currentSettings_.height,
                  currentSettings_.startX, currentSettings_.startY);
            return false;
        }
    }
    
    return true;
}

void ExposureManager::startMonitoring() {
    stopMonitoring(); // Ensure any existing monitor is stopped
    
    monitorRunning_ = true;
    monitorThread_ = std::make_unique<std::thread>([this]() {
        monitorExposure();
    });
}

void ExposureManager::stopMonitoring() {
    monitorRunning_ = false;
    if (monitorThread_ && monitorThread_->joinable()) {
        monitorThread_->join();
    }
}

ExposureManager::ExposureResult ExposureManager::createAbortedResult() {
    ExposureResult result;
    result.success = false;
    result.errorMessage = "Exposure aborted";
    result.settings = currentSettings_;
    result.startTime = exposureStartTime_;
    result.endTime = std::chrono::steady_clock::now();
    return result;
}

} // namespace lithium::device::ascom::camera::components
