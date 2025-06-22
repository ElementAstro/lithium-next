/*
 * exposure_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera exposure controller implementation

*************************************************/

#include "exposure_controller.hpp"
#include "../core/asi_camera_core.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <random>
#include <cstring>

namespace lithium::device::asi::camera {

ExposureController::ExposureController(ASICameraCore* core)
    : ComponentBase(core) {
    LOG_F(INFO, "Created ASI exposure controller");
}

ExposureController::~ExposureController() {
    if (isExposing_) {
        abortExposure();
    }
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    LOG_F(INFO, "Destroyed ASI exposure controller");
}

auto ExposureController::initialize() -> bool {
    LOG_F(INFO, "Initializing ASI exposure controller");
    
    // Reset statistics
    exposureCount_ = 0;
    lastExposureDuration_ = 0.0;
    
    // Initialize exposure mode settings
    exposureMode_ = 0;
    autoExposureEnabled_ = false;
    autoExposureTarget_ = 50;
    
    return true;
}

auto ExposureController::destroy() -> bool {
    LOG_F(INFO, "Destroying ASI exposure controller");
    
    if (isExposing_) {
        abortExposure();
    }
    
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    
    return true;
}

auto ExposureController::getComponentName() const -> std::string {
    return "ASI Exposure Controller";
}

auto ExposureController::onCameraStateChanged(CameraState state) -> void {
    LOG_F(INFO, "ASI exposure controller: Camera state changed to {}", static_cast<int>(state));
    
    if (state == CameraState::ERROR && isExposing_) {
        abortExposure();
    }
}

auto ExposureController::startExposure(double duration) -> bool {
    std::lock_guard<std::mutex> lock(exposureMutex_);
    
    if (isExposing_) {
        LOG_F(WARNING, "Exposure already in progress");
        return false;
    }
    
    if (!isValidExposureTime(duration)) {
        LOG_F(ERROR, "Invalid exposure duration: {}", duration);
        return false;
    }
    
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
    if (!setupExposureParameters(duration)) {
        LOG_F(ERROR, "Failed to setup exposure parameters");
        return false;
    }
    
    currentExposureDuration_ = duration;
    exposureAbortRequested_ = false;
    exposureStartTime_ = std::chrono::system_clock::now();
    isExposing_ = true;
    
    // Start exposure in separate thread
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    exposureThread_ = std::thread(&ExposureController::exposureThreadFunction, this);
    
    getCore()->updateCameraState(CameraState::EXPOSING);
    LOG_F(INFO, "Started ASI exposure: {} seconds", duration);
    return true;
}

auto ExposureController::abortExposure() -> bool {
    std::lock_guard<std::mutex> lock(exposureMutex_);
    
    if (!isExposing_) {
        return true;
    }
    
    exposureAbortRequested_ = true;
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // Stop the exposure
    ASIStopExposure(getCore()->getCameraId());
#endif
    
    // Wait for exposure thread to finish
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    
    isExposing_ = false;
    getCore()->updateCameraState(CameraState::ABORTED);
    LOG_F(INFO, "Aborted ASI exposure");
    return true;
}

auto ExposureController::isExposing() const -> bool {
    return isExposing_;
}

auto ExposureController::getExposureProgress() const -> double {
    if (!isExposing_) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    return std::min(elapsed / currentExposureDuration_, 1.0);
}

auto ExposureController::getExposureRemaining() const -> double {
    if (!isExposing_) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposureStartTime_).count();
    return std::max(currentExposureDuration_ - elapsed, 0.0);
}

auto ExposureController::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    if (isExposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }
    
    return lastFrameResult_;
}

auto ExposureController::getLastExposureDuration() const -> double {
    return lastExposureDuration_;
}

auto ExposureController::getExposureCount() const -> uint32_t {
    return exposureCount_;
}

auto ExposureController::resetExposureCount() -> bool {
    exposureCount_ = 0;
    LOG_F(INFO, "Reset ASI exposure count");
    return true;
}

auto ExposureController::saveImage(const std::string& path) -> bool {
    auto frame = getExposureResult();
    if (!frame) {
        LOG_F(ERROR, "No image data available");
        return false;
    }
    
    // TODO: Implement actual image saving
    LOG_F(INFO, "Saving ASI image to: {}", path);
    return true;
}

auto ExposureController::setExposureMode(int mode) -> bool {
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
    exposureMode_ = mode;
    LOG_F(INFO, "Set ASI exposure mode to {}", mode);
    return true;
}

auto ExposureController::getExposureMode() -> int {
    return exposureMode_;
}

auto ExposureController::enableAutoExposure(bool enable) -> bool {
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    ASI_BOOL autoExp = enable ? ASI_TRUE : ASI_FALSE;
    if (getCore()->setControlValue(ASI_EXPOSURE, 0, autoExp)) {
        autoExposureEnabled_ = enable;
        LOG_F(INFO, "{} ASI auto exposure", enable ? "Enabled" : "Disabled");
        return true;
    }
    return false;
#else
    autoExposureEnabled_ = enable;
    LOG_F(INFO, "{} ASI auto exposure [STUB]", enable ? "Enabled" : "Disabled");
    return true;
#endif
}

auto ExposureController::isAutoExposureEnabled() const -> bool {
    return autoExposureEnabled_;
}

auto ExposureController::setAutoExposureTarget(int target) -> bool {
    if (target < 1 || target > 99) {
        LOG_F(ERROR, "Invalid auto exposure target: {}", target);
        return false;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (getCore()->setControlValue(ASI_AUTO_TARGET_BRIGHTNESS, target)) {
        autoExposureTarget_ = target;
        LOG_F(INFO, "Set ASI auto exposure target to {}", target);
        return true;
    }
    return false;
#else
    autoExposureTarget_ = target;
    LOG_F(INFO, "Set ASI auto exposure target to {} [STUB]", target);
    return true;
#endif
}

auto ExposureController::getAutoExposureTarget() -> int {
    return autoExposureTarget_;
}

// Private helper methods
auto ExposureController::exposureThreadFunction() -> void {
    try {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
        int cameraId = getCore()->getCameraId();
        
        // Start exposure
        ASI_ERROR_CODE result = ASIStartExposure(cameraId, ASI_FALSE);
        if (result != ASI_SUCCESS) {
            LOG_F(ERROR, "Failed to start ASI exposure: {}", result);
            isExposing_ = false;
            getCore()->updateCameraState(CameraState::ERROR);
            return;
        }
        
        // Wait for exposure to complete
        ASI_EXPOSURE_STATUS status;
        do {
            if (exposureAbortRequested_) {
                break;
            }
            
            result = ASIGetExpStatus(cameraId, &status);
            if (result != ASI_SUCCESS) {
                LOG_F(ERROR, "Failed to get ASI exposure status: {}", result);
                isExposing_ = false;
                getCore()->updateCameraState(CameraState::ERROR);
                return;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (status == ASI_EXP_WORKING);
        
        if (!exposureAbortRequested_ && status == ASI_EXP_SUCCESS) {
            getCore()->updateCameraState(CameraState::DOWNLOADING);
            
            // Download image data
            lastFrameResult_ = captureFrame();
            if (lastFrameResult_) {
                updateExposureStatistics();
                getCore()->setCurrentFrame(lastFrameResult_);
                getCore()->updateCameraState(CameraState::IDLE);
            } else {
                getCore()->updateCameraState(CameraState::ERROR);
            }
        }
#else
        // Simulate exposure
        auto start = std::chrono::steady_clock::now();
        while (!exposureAbortRequested_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= currentExposureDuration_) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        if (!exposureAbortRequested_) {
            getCore()->updateCameraState(CameraState::DOWNLOADING);
            
            lastFrameResult_ = captureFrame();
            if (lastFrameResult_) {
                updateExposureStatistics();
                getCore()->setCurrentFrame(lastFrameResult_);
                getCore()->updateCameraState(CameraState::IDLE);
            } else {
                getCore()->updateCameraState(CameraState::ERROR);
            }
        }
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in ASI exposure thread: {}", e.what());
        getCore()->updateCameraState(CameraState::ERROR);
    }
    
    isExposing_ = false;
}

auto ExposureController::captureFrame() -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();
    
    // Get camera info for frame metadata
    const ASI_CAMERA_INFO* info = getCore()->getCameraInfo();
    if (!info) {
        LOG_F(ERROR, "No camera info available");
        return nullptr;
    }
    
    frame->resolution.width = info->MaxWidth;
    frame->resolution.height = info->MaxHeight;
    frame->pixel.sizeX = info->PixelSize;
    frame->pixel.sizeY = info->PixelSize;
    frame->pixel.size = info->PixelSize;
    frame->pixel.depth = 16;  // ASI cameras are typically 16-bit
    frame->binning.horizontal = 1;
    frame->binning.vertical = 1;
    frame->type = FrameType::FITS;
    frame->format = info->IsColorCam ? "RGB" : "MONO";
    
    // Calculate frame size
    size_t pixelCount = frame->resolution.width * frame->resolution.height;
    size_t bytesPerPixel = 2;  // 16-bit
    frame->size = pixelCount * bytesPerPixel;
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // Download actual image data from camera
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    
    ASI_ERROR_CODE result = ASIGetDataAfterExp(getCore()->getCameraId(), 
                                              data_buffer.get(), 
                                              frame->size);
    if (result != ASI_SUCCESS) {
        LOG_F(ERROR, "Failed to download ASI image data: {}", result);
        return nullptr;
    }
    
    frame->data = data_buffer.release();
#else
    // Generate simulated image data
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    frame->data = data_buffer.release();
    
    // Fill with simulated star field (16-bit)
    uint16_t* data16 = static_cast<uint16_t*>(frame->data);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> noise_dist(0, 50);
    
    for (size_t i = 0; i < pixelCount; ++i) {
        int noise = noise_dist(gen) - 25;  // ±25 ADU noise
        int star = 0;
        if (gen() % 100000 < 5) {  // 0.005% chance of star
            star = gen() % 30000 + 10000;  // Bright star
        }
        data16[i] = static_cast<uint16_t>(std::clamp(500 + noise + star, 0, 65535));
    }
#endif
    
    return frame;
}

auto ExposureController::setupExposureParameters(double duration) -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // Set exposure time (in microseconds)
    long exposureUs = static_cast<long>(duration * 1000000);
    if (!getCore()->setControlValue(ASI_EXPOSURE, exposureUs, ASI_FALSE)) {
        LOG_F(ERROR, "Failed to set ASI exposure time");
        return false;
    }
    
    // Set image format to RAW16
    ASI_ERROR_CODE result = ASISetImageType(getCore()->getCameraId(), ASI_IMG_RAW16);
    if (result != ASI_SUCCESS) {
        LOG_F(ERROR, "Failed to set ASI image type: {}", result);
        return false;
    }
    
    // Set ROI to full frame
    const ASI_CAMERA_INFO* info = getCore()->getCameraInfo();
    if (info) {
        result = ASISetROIFormat(getCore()->getCameraId(), 
                                info->MaxWidth, 
                                info->MaxHeight, 
                                1, ASI_IMG_RAW16);
        if (result != ASI_SUCCESS) {
            LOG_F(ERROR, "Failed to set ASI ROI format: {}", result);
            return false;
        }
    }
#endif
    
    return true;
}

auto ExposureController::downloadImageData() -> std::unique_ptr<uint8_t[]> {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    const ASI_CAMERA_INFO* info = getCore()->getCameraInfo();
    if (!info) {
        return nullptr;
    }
    
    size_t imageSize = info->MaxWidth * info->MaxHeight * 2;  // 16-bit
    auto buffer = std::make_unique<uint8_t[]>(imageSize);
    
    ASI_ERROR_CODE result = ASIGetDataAfterExp(getCore()->getCameraId(), 
                                              buffer.get(), 
                                              imageSize);
    if (result != ASI_SUCCESS) {
        LOG_F(ERROR, "Failed to download ASI image: {}", result);
        return nullptr;
    }
    
    return buffer;
#else
    // Stub implementation
    size_t imageSize = 6248 * 4176 * 2;  // Simulated size
    return std::make_unique<uint8_t[]>(imageSize);
#endif
}

auto ExposureController::createFrameFromData(std::unique_ptr<uint8_t[]> data, size_t size) -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();
    frame->data = data.release();
    frame->size = size;
    return frame;
}

auto ExposureController::isValidExposureTime(double duration) const -> bool {
    return duration >= 0.000001 && duration <= 3600.0;  // 1μs to 1 hour
}

auto ExposureController::updateExposureStatistics() -> void {
    exposureCount_++;
    lastExposureDuration_ = currentExposureDuration_;
    lastExposureTime_ = std::chrono::system_clock::now();
    
    LOG_F(INFO, "ASI exposure completed #{}: {} seconds", 
          exposureCount_, lastExposureDuration_);
}

} // namespace lithium::device::asi::camera
