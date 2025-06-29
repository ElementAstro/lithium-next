/*
 * image_processor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Image Processor Component Implementation

*************************************************/

#include "image_processor.hpp"
#include <spdlog/spdlog.h>

#include <chrono>
#include <future>

namespace lithium::device::asi::camera::components {

ImageProcessor::ImageProcessor() {
    // Initialize default settings
    currentSettings_.mode = ProcessingMode::REALTIME;
    currentSettings_.enableDarkSubtraction = false;
    currentSettings_.enableFlatCorrection = false;
    currentSettings_.enableBiasSubtraction = false;
    currentSettings_.enableHotPixelRemoval = false;
    currentSettings_.enableNoiseReduction = false;
    currentSettings_.enableSharpening = false;
    currentSettings_.enableColorBalance = false;
    currentSettings_.enableGammaCorrection = false;
    currentSettings_.preserveOriginal = true;
}

ImageProcessor::~ImageProcessor() = default;

// =========================================================================
// Processing Control
// =========================================================================

std::future<ImageProcessor::ProcessingResult> ImageProcessor::processImage(
    std::shared_ptr<AtomCameraFrame> frame,
    const ProcessingSettings& settings) {
    return std::async(std::launch::async, [this, frame, settings]() {
        return processImageInternal(frame, settings);
    });
}

std::vector<std::future<ImageProcessor::ProcessingResult>>
ImageProcessor::processImageBatch(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& frames,
    const ProcessingSettings& settings) {
    std::vector<std::future<ProcessingResult>> results;
    results.reserve(frames.size());

    for (const auto& frame : frames) {
        results.emplace_back(processImage(frame, settings));
    }

    return results;
}

// =========================================================================
// Calibration Management
// =========================================================================

bool ImageProcessor::setCalibrationFrames(const CalibrationFrames& frames) {
    std::lock_guard<std::mutex> lock(processingMutex_);
    calibrationFrames_ = frames;
    spdlog::info("Calibration frames updated");
    return true;
}

auto ImageProcessor::getCalibrationFrames() const -> CalibrationFrames {
    std::lock_guard<std::mutex> lock(processingMutex_);
    return calibrationFrames_;
}

bool ImageProcessor::createMasterDark(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& darkFrames) {
    if (darkFrames.empty()) {
        spdlog::error("No dark frames provided for master dark creation");
        return false;
    }

    spdlog::info("Creating master dark from {} frames", darkFrames.size());

    // For now, just use the first frame as master
    // TODO: Implement proper median stacking
    std::lock_guard<std::mutex> lock(processingMutex_);
    calibrationFrames_.masterDark = darkFrames[0];

    spdlog::info("Master dark created successfully");
    return true;
}

bool ImageProcessor::createMasterFlat(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& flatFrames) {
    if (flatFrames.empty()) {
        spdlog::error("No flat frames provided for master flat creation");
        return false;
    }

    spdlog::info("Creating master flat from {} frames", flatFrames.size());

    // For now, just use the first frame as master
    // TODO: Implement proper median stacking
    std::lock_guard<std::mutex> lock(processingMutex_);
    calibrationFrames_.masterFlat = flatFrames[0];

    spdlog::info("Master flat created successfully");
    return true;
}

bool ImageProcessor::createMasterBias(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& biasFrames) {
    if (biasFrames.empty()) {
        spdlog::error("No bias frames provided for master bias creation");
        return false;
    }

    spdlog::info("Creating master bias from {} frames", biasFrames.size());

    // For now, just use the first frame as master
    // TODO: Implement proper median stacking
    std::lock_guard<std::mutex> lock(processingMutex_);
    calibrationFrames_.masterBias = biasFrames[0];

    spdlog::info("Master bias created successfully");
    return true;
}

bool ImageProcessor::loadCalibrationFrames(const std::string& directory) {
    spdlog::info("Loading calibration frames from: {}", directory);
    // TODO: Implement calibration frame loading
    return true;
}

bool ImageProcessor::saveCalibrationFrames(const std::string& directory) {
    spdlog::info("Saving calibration frames to: {}", directory);
    // TODO: Implement calibration frame saving
    return true;
}

// =========================================================================
// Format Conversion (Placeholder implementations)
// =========================================================================

std::shared_ptr<AtomCameraFrame> ImageProcessor::convertFormat(
    std::shared_ptr<AtomCameraFrame> frame, const std::string& targetFormat) {
    spdlog::info("Converting frame to format: {}", targetFormat);
    // TODO: Implement format conversion
    return frame;
}

bool ImageProcessor::convertToFITS(std::shared_ptr<AtomCameraFrame> frame,
                                   const std::string& filename) {
    spdlog::info("Converting to FITS: {}", filename);
    // TODO: Implement FITS conversion
    return true;
}

bool ImageProcessor::convertToTIFF(std::shared_ptr<AtomCameraFrame> frame,
                                   const std::string& filename) {
    spdlog::info("Converting to TIFF: {}", filename);
    // TODO: Implement TIFF conversion
    return true;
}

bool ImageProcessor::convertToJPEG(std::shared_ptr<AtomCameraFrame> frame,
                                   const std::string& filename, int quality) {
    spdlog::info("Converting to JPEG: {} (quality: {})", filename, quality);
    // TODO: Implement JPEG conversion
    return true;
}

bool ImageProcessor::convertToPNG(std::shared_ptr<AtomCameraFrame> frame,
                                  const std::string& filename) {
    spdlog::info("Converting to PNG: {}", filename);
    // TODO: Implement PNG conversion
    return true;
}

// =========================================================================
// Image Analysis (Placeholder implementations)
// =========================================================================

auto ImageProcessor::analyzeImage(std::shared_ptr<AtomCameraFrame> frame)
    -> ImageStatistics {
    spdlog::info("Analyzing image");
    ImageStatistics stats;
    // TODO: Implement actual image analysis
    return stats;
}

std::vector<ImageProcessor::ImageStatistics> ImageProcessor::analyzeImageBatch(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& frames) {
    std::vector<ImageStatistics> results;
    results.reserve(frames.size());

    for (const auto& frame : frames) {
        results.emplace_back(analyzeImage(frame));
    }

    return results;
}

double ImageProcessor::calculateFWHM(std::shared_ptr<AtomCameraFrame> frame) {
    spdlog::info("Calculating FWHM");
    // TODO: Implement FWHM calculation
    return 2.5;  // Placeholder value
}

double ImageProcessor::calculateSNR(std::shared_ptr<AtomCameraFrame> frame) {
    spdlog::info("Calculating SNR");
    // TODO: Implement SNR calculation
    return 10.0;  // Placeholder value
}

int ImageProcessor::countStars(std::shared_ptr<AtomCameraFrame> frame,
                               double threshold) {
    spdlog::info("Counting stars with threshold: {:.2f}", threshold);
    // TODO: Implement star counting
    return 50;  // Placeholder value
}

// =========================================================================
// Image Enhancement (Placeholder implementations)
// =========================================================================

std::shared_ptr<AtomCameraFrame> ImageProcessor::removeHotPixels(
    std::shared_ptr<AtomCameraFrame> frame, double threshold) {
    spdlog::info("Removing hot pixels with threshold: {:.2f}", threshold);
    // TODO: Implement hot pixel removal
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::reduceNoise(
    std::shared_ptr<AtomCameraFrame> frame, int strength) {
    spdlog::info("Reducing noise with strength: {}", strength);
    // TODO: Implement noise reduction
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::sharpenImage(
    std::shared_ptr<AtomCameraFrame> frame, int strength) {
    spdlog::info("Sharpening image with strength: {}", strength);
    // TODO: Implement image sharpening
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::adjustLevels(
    std::shared_ptr<AtomCameraFrame> frame, double brightness, double contrast,
    double gamma) {
    spdlog::info(
        "Adjusting levels: brightness={:.2f}, contrast={:.2f}, gamma={:.2f}",
        brightness, contrast, gamma);
    // TODO: Implement level adjustment
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::stretchHistogram(
    std::shared_ptr<AtomCameraFrame> frame, double blackPoint,
    double whitePoint) {
    spdlog::info("Stretching histogram: black={:.2f}, white={:.2f}", blackPoint,
                 whitePoint);
    // TODO: Implement histogram stretching
    return frame;
}

// =========================================================================
// Color Processing (Placeholder implementations)
// =========================================================================

std::shared_ptr<AtomCameraFrame> ImageProcessor::debayerImage(
    std::shared_ptr<AtomCameraFrame> frame, const std::string& pattern) {
    spdlog::info("Debayering image with pattern: {}", pattern);
    // TODO: Implement debayering
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::balanceColors(
    std::shared_ptr<AtomCameraFrame> frame, double redGain, double greenGain,
    double blueGain) {
    spdlog::info("Balancing colors: R={:.2f}, G={:.2f}, B={:.2f}", redGain,
                 greenGain, blueGain);
    // TODO: Implement color balancing
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::adjustSaturation(
    std::shared_ptr<AtomCameraFrame> frame, double saturation) {
    spdlog::info("Adjusting saturation: {:.2f}", saturation);
    // TODO: Implement saturation adjustment
    return frame;
}

// =========================================================================
// Geometric Operations (Placeholder implementations)
// =========================================================================

std::shared_ptr<AtomCameraFrame> ImageProcessor::cropImage(
    std::shared_ptr<AtomCameraFrame> frame, int x, int y, int width,
    int height) {
    spdlog::info("Cropping image: ({}, {}) {}x{}", x, y, width, height);
    // TODO: Implement image cropping
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::resizeImage(
    std::shared_ptr<AtomCameraFrame> frame, int newWidth, int newHeight) {
    spdlog::info("Resizing image to: {}x{}", newWidth, newHeight);
    // TODO: Implement image resizing
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::rotateImage(
    std::shared_ptr<AtomCameraFrame> frame, double angle) {
    spdlog::info("Rotating image by: {:.2f} degrees", angle);
    // TODO: Implement image rotation
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::flipImage(
    std::shared_ptr<AtomCameraFrame> frame, bool horizontal, bool vertical) {
    spdlog::info("Flipping image: H={}, V={}", horizontal ? "true" : "false",
                 vertical ? "true" : "false");
    // TODO: Implement image flipping
    return frame;
}

// =========================================================================
// Stacking Operations (Placeholder implementations)
// =========================================================================

std::shared_ptr<AtomCameraFrame> ImageProcessor::stackImages(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& frames,
    const std::string& method) {
    spdlog::info("Stacking {} images using method: {}", frames.size(), method);
    // TODO: Implement image stacking
    return frames.empty() ? nullptr : frames[0];
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::alignAndStack(
    const std::vector<std::shared_ptr<AtomCameraFrame>>& frames) {
    spdlog::info("Aligning and stacking {} images", frames.size());
    // TODO: Implement alignment and stacking
    return frames.empty() ? nullptr : frames[0];
}

// =========================================================================
// Callback Management
// =========================================================================

void ImageProcessor::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    progressCallback_ = std::move(callback);
}

void ImageProcessor::setCompletionCallback(CompletionCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    completionCallback_ = std::move(callback);
}

// =========================================================================
// Presets (Placeholder implementations)
// =========================================================================

bool ImageProcessor::saveProcessingPreset(const std::string& name,
                                          const ProcessingSettings& settings) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    processingPresets_[name] = settings;
    spdlog::info("Saved processing preset: {}", name);
    return true;
}

bool ImageProcessor::loadProcessingPreset(const std::string& name,
                                          ProcessingSettings& settings) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    auto it = processingPresets_.find(name);
    if (it != processingPresets_.end()) {
        settings = it->second;
        spdlog::info("Loaded processing preset: {}", name);
        return true;
    }
    spdlog::warn("Processing preset not found: {}", name);
    return false;
}

std::vector<std::string> ImageProcessor::getAvailablePresets() const {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    std::vector<std::string> names;
    names.reserve(processingPresets_.size());
    for (const auto& pair : processingPresets_) {
        names.emplace_back(pair.first);
    }
    return names;
}

bool ImageProcessor::deleteProcessingPreset(const std::string& name) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    auto it = processingPresets_.find(name);
    if (it != processingPresets_.end()) {
        processingPresets_.erase(it);
        spdlog::info("Deleted processing preset: {}", name);
        return true;
    }
    spdlog::warn("Processing preset not found for deletion: {}", name);
    return false;
}

// =========================================================================
// Private Helper Methods
// =========================================================================

auto ImageProcessor::processImageInternal(
    std::shared_ptr<AtomCameraFrame> frame, const ProcessingSettings& settings)
    -> ProcessingResult {
    auto start_time = std::chrono::high_resolution_clock::now();

    ProcessingResult result;
    result.originalFrame = frame;

    try {
        if (!validateFrame(frame)) {
            result.errorMessage = "Invalid frame provided";
            result.success = false;
            return result;
        }

        notifyProgress(0, "Starting image processing");

        // Clone frame for processing if preserveOriginal is true
        auto workingFrame =
            settings.preserveOriginal ? cloneFrame(frame) : frame;

        if (!workingFrame) {
            result.errorMessage = "Failed to create working frame";
            result.success = false;
            return result;
        }

        // Apply calibration if enabled
        if (settings.enableDarkSubtraction || settings.enableFlatCorrection ||
            settings.enableBiasSubtraction) {
            notifyProgress(20, "Applying calibration");
            workingFrame = applyCalibration(workingFrame);
        }

        // Apply various processing steps based on settings
        if (settings.enableHotPixelRemoval) {
            notifyProgress(40, "Removing hot pixels");
            workingFrame = removeHotPixels(workingFrame);
        }

        if (settings.enableNoiseReduction) {
            notifyProgress(60, "Reducing noise");
            workingFrame =
                reduceNoise(workingFrame, settings.noiseReductionStrength);
        }

        if (settings.enableSharpening) {
            notifyProgress(80, "Sharpening image");
            workingFrame =
                sharpenImage(workingFrame, settings.sharpeningStrength);
        }

        notifyProgress(100, "Processing complete");

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        result.processedFrame = workingFrame;
        result.processingTime = duration;
        result.success = true;
        result.statistics = analyzeImage(workingFrame);

        notifyCompletion(result);

    } catch (const std::exception& e) {
        result.errorMessage = "Processing exception: " + std::string(e.what());
        result.success = false;
        spdlog::error("Image processing failed: {}", e.what());
    }

    return result;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::applyCalibration(
    std::shared_ptr<AtomCameraFrame> frame) {
    spdlog::info("Applying calibration to frame");

    auto calibratedFrame = frame;

    // Apply bias subtraction first
    if (currentSettings_.enableBiasSubtraction &&
        calibrationFrames_.masterBias) {
        calibratedFrame = applyBiasSubtraction(calibratedFrame,
                                               calibrationFrames_.masterBias);
    }

    // Apply dark subtraction
    if (currentSettings_.enableDarkSubtraction &&
        calibrationFrames_.masterDark) {
        calibratedFrame = applyDarkSubtraction(calibratedFrame,
                                               calibrationFrames_.masterDark);
    }

    // Apply flat correction
    if (currentSettings_.enableFlatCorrection &&
        calibrationFrames_.masterFlat) {
        calibratedFrame =
            applyFlatCorrection(calibratedFrame, calibrationFrames_.masterFlat);
    }

    return calibratedFrame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::applyDarkSubtraction(
    std::shared_ptr<AtomCameraFrame> frame,
    std::shared_ptr<AtomCameraFrame> dark) {
    spdlog::info("Applying dark subtraction");
    // TODO: Implement dark subtraction
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::applyFlatCorrection(
    std::shared_ptr<AtomCameraFrame> frame,
    std::shared_ptr<AtomCameraFrame> flat) {
    spdlog::info("Applying flat correction");
    // TODO: Implement flat correction
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::applyBiasSubtraction(
    std::shared_ptr<AtomCameraFrame> frame,
    std::shared_ptr<AtomCameraFrame> bias) {
    spdlog::info("Applying bias subtraction");
    // TODO: Implement bias subtraction
    return frame;
}

std::shared_ptr<AtomCameraFrame> ImageProcessor::cloneFrame(
    std::shared_ptr<AtomCameraFrame> frame) {
    // TODO: Implement frame cloning
    return frame;
}

bool ImageProcessor::validateFrame(std::shared_ptr<AtomCameraFrame> frame) {
    return frame != nullptr;
}

bool ImageProcessor::isFrameCompatible(
    std::shared_ptr<AtomCameraFrame> frame1,
    std::shared_ptr<AtomCameraFrame> frame2) {
    // TODO: Implement frame compatibility check
    return frame1 && frame2;
}

void ImageProcessor::notifyProgress(int progress,
                                    const std::string& operation) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (progressCallback_) {
        progressCallback_(progress, operation);
    }
}

void ImageProcessor::notifyCompletion(const ProcessingResult& result) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (completionCallback_) {
        completionCallback_(result);
    }
}

}  // namespace lithium::device::asi::camera::components
