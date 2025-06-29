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

#include "atom/log/loguru.hpp"

#include <algorithm>
#include <thread>
#include <cstring>

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

    // Stop hardware exposure
    if (hardware_) {
        hardware_->stopExposure();
    }

    setState(ExposureState::ABORTED);

    return true;
}

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

bool ExposureManager::isImageReady() const {
    if (!hardware_) {
        return false;
    }
    return hardware_->isImageReady();
}

std::shared_ptr<AtomCameraFrame> ExposureManager::downloadImage() {
    if (!hardware_) {
        return nullptr;
    }

    setState(ExposureState::DOWNLOADING);

    // Get raw image data from hardware
    auto imageData = hardware_->getImageArray();
    if (!imageData) {
        setState(ExposureState::ERROR);
        return nullptr;
    }

    // Create frame from image data
    auto frame = createFrameFromImageData(*imageData);

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
            if (hardware_ && hardware_->isImageReady()) {
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
    auto frame = std::make_shared<AtomCameraFrame>();

    // Get image dimensions from hardware
    if (hardware_) {
        auto dimensions = hardware_->getImageDimensions();
        frame->resolution.width = dimensions.first;
        frame->resolution.height = dimensions.second;

        auto binning = hardware_->getBinning();
        frame->binning.horizontal = binning.first;
        frame->binning.vertical = binning.second;
    }

    // Set frame type based on settings
    frame->type = currentSettings_.frameType;

    // Copy image data
    frame->size = imageData.size() * sizeof(uint16_t);
    frame->data = malloc(frame->size);
    if (frame->data) {
        std::memcpy(frame->data, imageData.data(), frame->size);
    } else {
        LOG_F(ERROR, "Failed to allocate memory for image data");
        return nullptr;
    }

    return frame;
}

double ExposureManager::calculateTimeout(double exposureDuration) const {
    if (autoTimeoutEnabled_) {
        return exposureDuration * timeoutMultiplier_;
    }
    return 0.0; // No timeout
}



} // namespace lithium::device::ascom::camera::components
