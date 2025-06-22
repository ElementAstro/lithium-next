/*
 * exposure_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Exposure Manager Component Implementation

*************************************************/

#include "exposure_manager.hpp"
#include "hardware_interface.hpp"
#include "atom/log/loguru.hpp"
#include <algorithm>
#include <cmath>

namespace lithium::device::asi::camera::components {

ExposureManager::ExposureManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }
    LOG_F(INFO, "ASI Camera ExposureManager initialized");
}

ExposureManager::~ExposureManager() {
    if (isExposing()) {
        abortExposure();
    }
    
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    
    LOG_F(INFO, "ASI Camera ExposureManager destroyed");
}

bool ExposureManager::startExposure(const ExposureSettings& settings) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != ExposureState::IDLE) {
        LOG_F(ERROR, "Cannot start exposure: camera is not idle (state: {})", 
              static_cast<int>(state_));
        return false;
    }
    
    if (!validateExposureSettings(settings)) {
        LOG_F(ERROR, "Invalid exposure settings");
        return false;
    }
    
    if (!hardware_->isConnected()) {
        LOG_F(ERROR, "Cannot start exposure: hardware not connected");
        return false;
    }
    
    // Store settings and reset state
    currentSettings_ = settings;
    abortRequested_ = false;
    lastResult_ = ExposureResult{};
    currentProgress_ = 0.0;
    
    // Join previous thread if exists
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    
    // Start new exposure thread
    updateState(ExposureState::PREPARING);
    exposureThread_ = std::thread(&ExposureManager::exposureWorker, this);
    
    LOG_F(INFO, "Started exposure: duration={:.3f}s, size={}x{}, bin={}, format={}", 
          settings.duration, settings.width, settings.height, settings.binning, settings.format);
    
    return true;
}

bool ExposureManager::abortExposure() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ == ExposureState::IDLE || state_ == ExposureState::COMPLETE || 
        state_ == ExposureState::ABORTED || state_ == ExposureState::ERROR) {
        return true;
    }
    
    LOG_F(INFO, "Aborting exposure");
    abortRequested_ = true;
    
    // Try to stop hardware exposure
    if (state_ == ExposureState::EXPOSING || state_ == ExposureState::DOWNLOADING) {
        hardware_->stopExposure();
    }
    
    stateCondition_.notify_all();
    
    // Wait for thread to finish with timeout
    if (exposureThread_.joinable()) {
        lock.~lock_guard();
        exposureThread_.join();
        std::lock_guard<std::mutex> newLock(stateMutex_);
    }
    
    updateState(ExposureState::ABORTED);
    abortedExposures_++;
    
    LOG_F(INFO, "Exposure aborted");
    return true;
}

std::string ExposureManager::getStateString() const {
    switch (state_) {
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
    if (state_ == ExposureState::IDLE || state_ == ExposureState::PREPARING) {
        return 0.0;
    } else if (state_ == ExposureState::COMPLETE || state_ == ExposureState::ABORTED) {
        return 100.0;
    } else if (state_ == ExposureState::DOWNLOADING) {
        return 95.0; // Assume download is quick
    }
    
    return currentProgress_;
}

double ExposureManager::getRemainingTime() const {
    if (state_ != ExposureState::EXPOSING) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    double remaining = std::max(0.0, currentSettings_.duration - elapsed);
    
    return remaining;
}

double ExposureManager::getElapsedTime() const {
    if (state_ == ExposureState::IDLE || state_ == ExposureState::PREPARING) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - exposureStartTime_).count();
}

ExposureManager::ExposureResult ExposureManager::getLastResult() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return lastResult_;
}

void ExposureManager::clearResult() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastResult_ = ExposureResult{};
}

void ExposureManager::setExposureCallback(ExposureCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    exposureCallback_ = std::move(callback);
}

void ExposureManager::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    progressCallback_ = std::move(callback);
}

void ExposureManager::resetStatistics() {
    completedExposures_ = 0;
    abortedExposures_ = 0;
    failedExposures_ = 0;
    totalExposureTime_ = 0.0;
    LOG_F(INFO, "Exposure statistics reset");
}

void ExposureManager::exposureWorker() {
    ExposureResult result;
    result.startTime = std::chrono::steady_clock::now();
    
    try {
        // Execute the exposure with retries
        int retryCount = 0;
        bool success = false;
        
        while (retryCount <= maxRetries_ && !abortRequested_) {
            if (retryCount > 0) {
                LOG_F(INFO, "Retrying exposure (attempt {}/{})", retryCount + 1, maxRetries_ + 1);
                std::this_thread::sleep_for(retryDelay_);
            }
            
            success = executeExposure(currentSettings_, result);
            if (success || abortRequested_) {
                break;
            }
            
            retryCount++;
        }
        
        result.endTime = std::chrono::steady_clock::now();
        result.actualDuration = std::chrono::duration<double>(result.endTime - result.startTime).count();
        
        if (abortRequested_) {
            result.success = false;
            result.errorMessage = "Exposure aborted by user";
            updateState(ExposureState::ABORTED);
            abortedExposures_++;
        } else if (success) {
            result.success = true;
            updateState(ExposureState::COMPLETE);
            completedExposures_++;
            totalExposureTime_ += result.actualDuration;
        } else {
            result.success = false;
            if (result.errorMessage.empty()) {
                result.errorMessage = "Exposure failed after " + std::to_string(maxRetries_ + 1) + " attempts";
            }
            updateState(ExposureState::ERROR);
            failedExposures_++;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Exception during exposure: " + std::string(e.what());
        result.endTime = std::chrono::steady_clock::now();
        result.actualDuration = std::chrono::duration<double>(result.endTime - result.startTime).count();
        updateState(ExposureState::ERROR);
        failedExposures_++;
        LOG_F(ERROR, "Exception in exposure worker: {}", e.what());
    }
    
    // Store result and notify
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        lastResult_ = result;
    }
    
    notifyExposureComplete(result);
    
    LOG_F(INFO, "Exposure worker completed: success={}, duration={:.3f}s", 
          result.success, result.actualDuration);
}

bool ExposureManager::executeExposure(const ExposureSettings& settings, ExposureResult& result) {
    try {
        // Prepare exposure
        updateState(ExposureState::PREPARING);
        if (!prepareExposure(settings)) {
            result.errorMessage = formatExposureError("prepare", hardware_->getLastSDKError());
            return false;
        }
        
        if (abortRequested_) return false;
        
        // Start exposure
        updateState(ExposureState::EXPOSING);
        exposureStartTime_ = std::chrono::steady_clock::now();
        
        ASI_IMG_TYPE imageType = ASI_IMG_RAW16; // Default
        if (settings.format == "RAW8") imageType = ASI_IMG_RAW8;
        else if (settings.format == "RGB24") imageType = ASI_IMG_RGB24;
        
        if (!hardware_->startExposure(settings.width, settings.height, settings.binning, imageType)) {
            result.errorMessage = formatExposureError("start", hardware_->getLastSDKError());
            return false;
        }
        
        // Wait for exposure to complete
        if (!waitForExposureComplete(settings.duration)) {
            result.errorMessage = "Exposure timeout or abort";
            return false;
        }
        
        if (abortRequested_) return false;
        
        // Download image
        updateState(ExposureState::DOWNLOADING);
        if (!downloadImage(result)) {
            if (result.errorMessage.empty()) {
                result.errorMessage = formatExposureError("download", hardware_->getLastSDKError());
            }
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during exposure execution: " + std::string(e.what());
        LOG_F(ERROR, "Exception in executeExposure: {}", e.what());
        return false;
    }
}

bool ExposureManager::prepareExposure(const ExposureSettings& settings) {
    // Set exposure time control
    if (!hardware_->setControlValue(ASI_EXPOSURE, static_cast<long>(settings.duration * 1000000), false)) {
        return false;
    }
    
    // Set ROI if specified
    if (settings.startX != 0 || settings.startY != 0) {
        // This would be implemented if the hardware interface supported ROI positioning
        LOG_F(INFO, "ROI positioning not implemented in this version");
    }
    
    return true;
}

bool ExposureManager::waitForExposureComplete(double duration) {
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = startTime + std::chrono::seconds(static_cast<long>(duration + 30)); // Add 30s buffer
    
    while (!abortRequested_) {
        auto now = std::chrono::steady_clock::now();
        
        // Check timeout
        if (now > timeout) {
            LOG_F(ERROR, "Exposure timeout after {:.1f} seconds", 
                  std::chrono::duration<double>(now - startTime).count());
            return false;
        }
        
        // Check exposure status
        auto status = hardware_->getExposureStatus();
        
        if (status == ASI_EXPOSURE_SUCCESS) {
            LOG_F(INFO, "Exposure completed successfully");
            return true;
        } else if (status == ASI_EXPOSURE_FAILED) {
            LOG_F(ERROR, "Exposure failed");
            return false;
        }
        
        // Update progress
        updateProgress();
        
        // Brief sleep to avoid busy waiting
        std::this_thread::sleep_for(progressUpdateInterval_);
    }
    
    return false;
}

bool ExposureManager::downloadImage(ExposureResult& result) {
    try {
        size_t bufferSize = calculateBufferSize(currentSettings_);
        auto buffer = std::make_unique<unsigned char[]>(bufferSize);
        
        if (!hardware_->getImageData(buffer.get(), static_cast<long>(bufferSize))) {
            return false;
        }
        
        // Create camera frame
        result.frame = createFrameFromBuffer(buffer.get(), currentSettings_);
        if (!result.frame) {
            result.errorMessage = "Failed to create camera frame from buffer";
            return false;
        }
        
        LOG_F(INFO, "Successfully downloaded image data ({} bytes)", bufferSize);
        return true;
        
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during image download: " + std::string(e.what());
        LOG_F(ERROR, "Exception in downloadImage: {}", e.what());
        return false;
    }
}

void ExposureManager::updateProgress() {
    if (state_ != ExposureState::EXPOSING) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    double progress = std::min(100.0, (elapsed / currentSettings_.duration) * 95.0); // Max 95% during exposure
    
    currentProgress_ = progress;
    
    double remaining = std::max(0.0, currentSettings_.duration - elapsed);
    notifyProgress(progress, remaining);
}

void ExposureManager::notifyExposureComplete(const ExposureResult& result) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (exposureCallback_) {
        try {
            exposureCallback_(result);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in exposure callback: {}", e.what());
        }
    }
}

void ExposureManager::notifyProgress(double progress, double remainingTime) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (progressCallback_) {
        try {
            progressCallback_(progress, remainingTime);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in progress callback: {}", e.what());
        }
    }
}

void ExposureManager::updateState(ExposureState newState) {
    state_ = newState;
    stateCondition_.notify_all();
}

std::shared_ptr<AtomCameraFrame> ExposureManager::createFrameFromBuffer(
    const unsigned char* buffer, const ExposureSettings& settings) {
    
    // This is a simplified implementation
    // In a real implementation, this would create a proper AtomCameraFrame
    // with metadata, timestamp, and proper data formatting
    
    auto frame = std::make_shared<AtomCameraFrame>();
    
    // Set basic properties
    frame->width = settings.width > 0 ? settings.width : 1920; // Default width
    frame->height = settings.height > 0 ? settings.height : 1080; // Default height
    frame->channels = (settings.format == "RGB24") ? 3 : 1;
    frame->bitDepth = (settings.format == "RAW16") ? 16 : 8;
    
    // Calculate buffer size and copy data
    size_t dataSize = calculateBufferSize(settings);
    frame->data.resize(dataSize);
    std::memcpy(frame->data.data(), buffer, dataSize);
    
    // Set metadata
    frame->timestamp = std::chrono::steady_clock::now();
    frame->exposureTime = settings.duration;
    frame->binning = settings.binning;
    
    return frame;
}

size_t ExposureManager::calculateBufferSize(const ExposureSettings& settings) {
    int width = settings.width > 0 ? settings.width : 1920;
    int height = settings.height > 0 ? settings.height : 1080;
    int bytesPerPixel = 1;
    
    if (settings.format == "RAW16") {
        bytesPerPixel = 2;
    } else if (settings.format == "RGB24") {
        bytesPerPixel = 3;
    }
    
    return static_cast<size_t>(width * height * bytesPerPixel);
}

bool ExposureManager::validateExposureSettings(const ExposureSettings& settings) {
    if (settings.duration <= 0.0 || settings.duration > 3600.0) {
        LOG_F(ERROR, "Invalid exposure duration: {:.3f}s (must be 0-3600s)", settings.duration);
        return false;
    }
    
    if (settings.binning < 1 || settings.binning > 8) {
        LOG_F(ERROR, "Invalid binning: {} (must be 1-8)", settings.binning);
        return false;
    }
    
    if (settings.width < 0 || settings.height < 0) {
        LOG_F(ERROR, "Invalid image dimensions: {}x{}", settings.width, settings.height);
        return false;
    }
    
    if (settings.format != "RAW8" && settings.format != "RAW16" && settings.format != "RGB24") {
        LOG_F(ERROR, "Invalid image format: {} (must be RAW8, RAW16, or RGB24)", settings.format);
        return false;
    }
    
    return true;
}

std::string ExposureManager::formatExposureError(const std::string& operation, const std::string& error) {
    return "Failed to " + operation + " exposure: " + error;
}

} // namespace lithium::device::asi::camera::components
