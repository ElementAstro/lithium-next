/*
 * image_processor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Image Processor Component Implementation

This component handles image processing, format conversion, quality analysis,
and post-processing operations for captured images.

*************************************************/

#include "image_processor.hpp"
#include "hardware_interface.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::device::ascom::camera::components {

ImageProcessor::ImageProcessor(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    LOG_F(INFO, "ASCOM Camera ImageProcessor initialized");
}

bool ImageProcessor::initialize() {
    LOG_F(INFO, "Initializing image processor");
    
    if (!hardware_) {
        LOG_F(ERROR, "Hardware interface not available");
        return false;
    }
    
    // Initialize default settings
    settings_.mode = ProcessingMode::NONE;
    settings_.enableCompression = false;
    settings_.compressionFormat = "AUTO";
    settings_.compressionQuality = 95;
    
    currentFormat_ = "FITS";
    compressionEnabled_ = false;
    processingEnabled_ = true;
    
    // Reset statistics
    processedImages_ = 0;
    failedProcessing_ = 0;
    avgProcessingTime_ = 0.0;
    
    LOG_F(INFO, "Image processor initialized successfully");
    return true;
}

bool ImageProcessor::setImageFormat(const std::string& format) {
    if (!validateFormat(format)) {
        LOG_F(ERROR, "Invalid image format: {}", format);
        return false;
    }
    
    currentFormat_ = format;
    LOG_F(INFO, "Image format set to: {}", format);
    return true;
}

std::string ImageProcessor::getImageFormat() const {
    return currentFormat_;
}

std::vector<std::string> ImageProcessor::getSupportedImageFormats() const {
    return {"FITS", "TIFF", "JPEG", "PNG", "RAW", "XISF"};
}

bool ImageProcessor::enableImageCompression(bool enable) {
    compressionEnabled_ = enable;
    LOG_F(INFO, "Image compression {}", enable ? "enabled" : "disabled");
    return true;
}

bool ImageProcessor::isImageCompressionEnabled() const {
    return compressionEnabled_.load();
}

bool ImageProcessor::setProcessingSettings(const ProcessingSettings& settings) {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    settings_ = settings;
    
    LOG_F(INFO, "Processing settings updated: mode={}, compression={}", 
          static_cast<int>(settings.mode), settings.enableCompression);
    return true;
}

ImageProcessor::ProcessingSettings ImageProcessor::getProcessingSettings() const {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    return settings_;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::processImage(std::shared_ptr<AtomCameraFrame> frame) {
    if (!frame) {
        LOG_F(ERROR, "Invalid input frame");
        failedProcessing_++;
        return nullptr;
    }
    
    if (!processingEnabled_) {
        return frame; // Pass through without processing
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        // Apply format conversion if needed
        auto processedFrame = convertFormat(frame, currentFormat_);
        if (!processedFrame) {
            LOG_F(ERROR, "Format conversion failed");
            failedProcessing_++;
            return nullptr;
        }
        
        // Apply compression if enabled
        if (compressionEnabled_) {
            processedFrame = applyCompression(processedFrame);
            if (!processedFrame) {
                LOG_F(WARNING, "Compression failed, using uncompressed image");
                processedFrame = frame;
            }
        }
        
        // Update statistics
        auto endTime = std::chrono::steady_clock::now();
        auto processingTime = std::chrono::duration<double>(endTime - startTime).count();
        
        processedImages_++;
        avgProcessingTime_ = (avgProcessingTime_ * (processedImages_ - 1) + processingTime) / processedImages_;
        
        LOG_F(INFO, "Image processed successfully in {:.3f}s", processingTime);
        return processedFrame;
        
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Image processing failed: {}", e.what());
        failedProcessing_++;
        return nullptr;
    }
}

ImageProcessor::ImageQuality ImageProcessor::analyzeImageQuality(std::shared_ptr<AtomCameraFrame> frame) {
    if (!frame) {
        return ImageQuality{};
    }
    
    ImageQuality quality = performQualityAnalysis(frame);
    
    // Store as last analysis result
    {
        std::lock_guard<std::mutex> lock(qualityMutex_);
        lastQuality_ = quality;
    }
    
    return quality;
}

std::map<std::string, double> ImageProcessor::getProcessingStatistics() const {
    std::map<std::string, double> stats;
    
    stats["processed_images"] = processedImages_.load();
    stats["failed_processing"] = failedProcessing_.load();
    stats["average_processing_time"] = avgProcessingTime_.load();
    stats["success_rate"] = processedImages_ > 0 ? 
        (static_cast<double>(processedImages_ - failedProcessing_) / processedImages_) : 0.0;
    
    return stats;
}

ImageProcessor::ImageQuality ImageProcessor::getLastImageQuality() const {
    std::lock_guard<std::mutex> lock(qualityMutex_);
    return lastQuality_;
}

std::map<std::string, double> ImageProcessor::getPerformanceMetrics() const {
    auto stats = getProcessingStatistics();
    
    // Add performance-specific metrics
    stats["compression_enabled"] = compressionEnabled_.load() ? 1.0 : 0.0;
    stats["processing_enabled"] = processingEnabled_.load() ? 1.0 : 0.0;
    
    return stats;
}

void ImageProcessor::setProcessingCallback(const ProcessingCallback& callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    processingCallback_ = callback;
}

// Private helper methods

bool ImageProcessor::validateFormat(const std::string& format) const {
    auto supportedFormats = getSupportedImageFormats();
    return std::find(supportedFormats.begin(), supportedFormats.end(), format) != supportedFormats.end();
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::convertFormat(std::shared_ptr<AtomCameraFrame> frame, const std::string& targetFormat) {
    // For now, just update the format string in the frame
    // In a full implementation, this would perform actual format conversion
    if (frame) {
        frame->format = targetFormat;
    }
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::applyCompression(std::shared_ptr<AtomCameraFrame> frame) {
    // Stub implementation - in a real implementation, this would apply compression
    // For now, just return the frame unchanged
    return frame;
}

ImageProcessor::ImageQuality ImageProcessor::performQualityAnalysis(std::shared_ptr<AtomCameraFrame> frame) {
    // Stub implementation - in a real implementation, this would analyze the image
    ImageQuality quality;
    
    // Return some dummy values for now
    quality.snr = 25.0;
    quality.fwhm = 2.5;
    quality.brightness = 128.0;
    quality.contrast = 0.3;
    quality.noise = 10.0;
    quality.stars = 150;
    
    return quality;
}

} // namespace lithium::device::ascom::camera::components
