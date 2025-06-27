/*
 * image_processor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Image Processor Component

This component handles image processing, format conversion, quality analysis,
and post-processing operations for captured images.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <vector>
#include <functional>

#include "device/template/camera.hpp"

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Image Processor for ASCOM Camera
 * 
 * Handles image processing tasks including format conversion,
 * compression, quality analysis, and post-processing operations.
 */
class ImageProcessor {
public:
    enum class ProcessingMode {
        NONE,           // No processing
        BASIC,          // Basic level correction
        ADVANCED,       // Advanced processing with noise reduction
        CUSTOM          // Custom processing pipeline
    };

    struct ProcessingSettings {
        ProcessingMode mode = ProcessingMode::NONE;
        bool enableCompression = false;
        std::string compressionFormat = "AUTO";
        int compressionQuality = 95;
        bool enableNoiseReduction = false;
        bool enableSharpening = false;
        bool enableColorCorrection = false;
        bool enableHistogramStretching = false;
    };

    struct ImageQuality {
        double snr = 0.0;           // Signal-to-noise ratio
        double fwhm = 0.0;          // Full width at half maximum
        double brightness = 0.0;    // Average brightness
        double contrast = 0.0;      // RMS contrast
        double noise = 0.0;         // Noise level
        int stars = 0;              // Detected stars count
    };

    using ProcessingCallback = std::function<void(bool success, const std::string& message)>;

public:
    explicit ImageProcessor(std::shared_ptr<HardwareInterface> hardware);
    ~ImageProcessor() = default;

    // Non-copyable and non-movable
    ImageProcessor(const ImageProcessor&) = delete;
    ImageProcessor& operator=(const ImageProcessor&) = delete;
    ImageProcessor(ImageProcessor&&) = delete;
    ImageProcessor& operator=(ImageProcessor&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize image processor
     * @return true if initialization successful
     */
    bool initialize();

    // =========================================================================
    // Format and Compression
    // =========================================================================

    /**
     * @brief Set image format
     * @param format Image format string (FITS, TIFF, JPEG, PNG, etc.)
     * @return true if format set successfully
     */
    bool setImageFormat(const std::string& format);

    /**
     * @brief Get current image format
     * @return Current image format
     */
    std::string getImageFormat() const;

    /**
     * @brief Get supported image formats
     * @return Vector of supported format strings
     */
    std::vector<std::string> getSupportedImageFormats() const;

    /**
     * @brief Enable/disable image compression
     * @param enable True to enable compression
     * @return true if setting applied successfully
     */
    bool enableImageCompression(bool enable);

    /**
     * @brief Check if image compression is enabled
     * @return true if compression is enabled
     */
    bool isImageCompressionEnabled() const;

    // =========================================================================
    // Processing Control
    // =========================================================================

    /**
     * @brief Set processing settings
     * @param settings Processing configuration
     * @return true if settings applied successfully
     */
    bool setProcessingSettings(const ProcessingSettings& settings);

    /**
     * @brief Get current processing settings
     * @return Current processing configuration
     */
    ProcessingSettings getProcessingSettings() const;

    /**
     * @brief Process image frame
     * @param frame Input image frame
     * @return Processed image frame or nullptr on failure
     */
    std::shared_ptr<AtomCameraFrame> processImage(std::shared_ptr<AtomCameraFrame> frame);

    /**
     * @brief Analyze image quality
     * @param frame Image frame to analyze
     * @return Image quality metrics
     */
    ImageQuality analyzeImageQuality(std::shared_ptr<AtomCameraFrame> frame);

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get processing statistics
     * @return Map of processing statistics
     */
    std::map<std::string, double> getProcessingStatistics() const;

    /**
     * @brief Get last image quality analysis
     * @return Quality metrics of last processed image
     */
    ImageQuality getLastImageQuality() const;

    /**
     * @brief Get processing performance metrics
     * @return Map of performance metrics
     */
    std::map<std::string, double> getPerformanceMetrics() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set processing completion callback
     * @param callback Function to call when processing completes
     */
    void setProcessingCallback(const ProcessingCallback& callback);

private:
    std::shared_ptr<HardwareInterface> hardware_;
    
    // Processing settings
    ProcessingSettings settings_;
    mutable std::mutex settingsMutex_;
    
    // State
    std::atomic<bool> processingEnabled_{false};
    std::string currentFormat_{"FITS"};
    std::atomic<bool> compressionEnabled_{false};
    
    // Statistics
    std::atomic<uint64_t> processedImages_{0};
    std::atomic<uint64_t> failedProcessing_{0};
    std::atomic<double> avgProcessingTime_{0.0};
    
    // Last analysis results
    ImageQuality lastQuality_;
    mutable std::mutex qualityMutex_;
    
    // Callback
    ProcessingCallback processingCallback_;
    std::mutex callbackMutex_;
    
    // Helper methods
    bool validateFormat(const std::string& format) const;
    std::shared_ptr<AtomCameraFrame> convertFormat(std::shared_ptr<AtomCameraFrame> frame, const std::string& targetFormat);
    std::shared_ptr<AtomCameraFrame> applyCompression(std::shared_ptr<AtomCameraFrame> frame);
    ImageQuality performQualityAnalysis(std::shared_ptr<AtomCameraFrame> frame);
};

} // namespace lithium::device::ascom::camera::components
