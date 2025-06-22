/*
 * image_processor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Image Processor Component

This component handles image processing operations including
format conversion, calibration, enhancement, and analysis.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>
#include <map>

#include "../../../template/camera_frame.hpp"

namespace lithium::device::asi::camera::components {

/**
 * @brief Image Processor for ASI Camera
 * 
 * Provides comprehensive image processing capabilities including
 * format conversion, calibration, enhancement, and analysis operations.
 */
class ImageProcessor {
public:
    enum class ProcessingMode {
        REALTIME,       // Real-time processing with minimal latency
        QUALITY,        // High-quality processing with longer processing time
        BATCH           // Batch processing mode
    };

    struct ProcessingSettings {
        ProcessingMode mode = ProcessingMode::REALTIME;
        bool enableDarkSubtraction = false;
        bool enableFlatCorrection = false;
        bool enableBiasSubtraction = false;
        bool enableHotPixelRemoval = false;
        bool enableNoiseReduction = false;
        bool enableSharpening = false;
        bool enableColorBalance = false;
        bool enableGammaCorrection = false;
        double gamma = 1.0;
        double brightness = 0.0;
        double contrast = 1.0;
        double saturation = 1.0;
        int noiseReductionStrength = 50;    // 0-100
        int sharpeningStrength = 0;         // 0-100
        bool preserveOriginal = true;       // Keep original data
    };

    struct CalibrationFrames {
        std::shared_ptr<AtomCameraFrame> masterDark;
        std::shared_ptr<AtomCameraFrame> masterFlat;
        std::shared_ptr<AtomCameraFrame> masterBias;
        std::map<double, std::shared_ptr<AtomCameraFrame>> darkLibrary; // exposure -> dark frame
        bool isValid() const {
            return masterDark || masterFlat || masterBias || !darkLibrary.empty();
        }
    };

    struct ImageStatistics {
        double mean = 0.0;
        double median = 0.0;
        double stdDev = 0.0;
        double min = 0.0;
        double max = 0.0;
        uint32_t histogram[256] = {0};      // Histogram for 8-bit representation
        double snr = 0.0;                   // Signal-to-noise ratio
        int hotPixels = 0;                  // Number of hot pixels detected
        int coldPixels = 0;                 // Number of cold pixels detected
        double starCount = 0;               // Estimated number of stars
        double fwhm = 0.0;                  // Full Width Half Maximum (focus metric)
        double eccentricity = 0.0;          // Star eccentricity (tracking metric)
    };

    struct ProcessingResult {
        bool success = false;
        std::shared_ptr<AtomCameraFrame> processedFrame;
        std::shared_ptr<AtomCameraFrame> originalFrame;
        ImageStatistics statistics;
        std::chrono::milliseconds processingTime{0};
        std::vector<std::string> appliedOperations;
        std::string errorMessage;
    };

    using ProgressCallback = std::function<void(int progress, const std::string& operation)>;
    using CompletionCallback = std::function<void(const ProcessingResult&)>;

public:
    ImageProcessor();
    ~ImageProcessor();

    // Non-copyable and non-movable
    ImageProcessor(const ImageProcessor&) = delete;
    ImageProcessor& operator=(const ImageProcessor&) = delete;
    ImageProcessor(ImageProcessor&&) = delete;
    ImageProcessor& operator=(ImageProcessor&&) = delete;

    // Processing Control
    std::future<ProcessingResult> processImage(std::shared_ptr<AtomCameraFrame> frame,
                                              const ProcessingSettings& settings);
    std::vector<std::future<ProcessingResult>> processImageBatch(
        const std::vector<std::shared_ptr<AtomCameraFrame>>& frames,
        const ProcessingSettings& settings);
    
    // Calibration Management
    bool setCalibrationFrames(const CalibrationFrames& frames);
    CalibrationFrames getCalibrationFrames() const;
    bool createMasterDark(const std::vector<std::shared_ptr<AtomCameraFrame>>& darkFrames);
    bool createMasterFlat(const std::vector<std::shared_ptr<AtomCameraFrame>>& flatFrames);
    bool createMasterBias(const std::vector<std::shared_ptr<AtomCameraFrame>>& biasFrames);
    bool loadCalibrationFrames(const std::string& directory);
    bool saveCalibrationFrames(const std::string& directory);
    
    // Format Conversion
    std::shared_ptr<AtomCameraFrame> convertFormat(std::shared_ptr<AtomCameraFrame> frame,
                                                   const std::string& targetFormat);
    bool convertToFITS(std::shared_ptr<AtomCameraFrame> frame, const std::string& filename);
    bool convertToTIFF(std::shared_ptr<AtomCameraFrame> frame, const std::string& filename);
    bool convertToJPEG(std::shared_ptr<AtomCameraFrame> frame, const std::string& filename, int quality = 95);
    bool convertToPNG(std::shared_ptr<AtomCameraFrame> frame, const std::string& filename);
    
    // Image Analysis
    ImageStatistics analyzeImage(std::shared_ptr<AtomCameraFrame> frame);
    std::vector<ImageStatistics> analyzeImageBatch(const std::vector<std::shared_ptr<AtomCameraFrame>>& frames);
    double calculateFWHM(std::shared_ptr<AtomCameraFrame> frame);
    double calculateSNR(std::shared_ptr<AtomCameraFrame> frame);
    int countStars(std::shared_ptr<AtomCameraFrame> frame, double threshold = 3.0);
    
    // Image Enhancement
    std::shared_ptr<AtomCameraFrame> removeHotPixels(std::shared_ptr<AtomCameraFrame> frame, double threshold = 3.0);
    std::shared_ptr<AtomCameraFrame> reduceNoise(std::shared_ptr<AtomCameraFrame> frame, int strength = 50);
    std::shared_ptr<AtomCameraFrame> sharpenImage(std::shared_ptr<AtomCameraFrame> frame, int strength = 50);
    std::shared_ptr<AtomCameraFrame> adjustLevels(std::shared_ptr<AtomCameraFrame> frame,
                                                  double brightness, double contrast, double gamma);
    std::shared_ptr<AtomCameraFrame> stretchHistogram(std::shared_ptr<AtomCameraFrame> frame,
                                                      double blackPoint = 0.0, double whitePoint = 100.0);
    
    // Color Processing (for color cameras)
    std::shared_ptr<AtomCameraFrame> debayerImage(std::shared_ptr<AtomCameraFrame> frame, const std::string& pattern);
    std::shared_ptr<AtomCameraFrame> balanceColors(std::shared_ptr<AtomCameraFrame> frame,
                                                   double redGain = 1.0, double greenGain = 1.0, double blueGain = 1.0);
    std::shared_ptr<AtomCameraFrame> adjustSaturation(std::shared_ptr<AtomCameraFrame> frame, double saturation);
    
    // Geometric Operations
    std::shared_ptr<AtomCameraFrame> cropImage(std::shared_ptr<AtomCameraFrame> frame,
                                              int x, int y, int width, int height);
    std::shared_ptr<AtomCameraFrame> resizeImage(std::shared_ptr<AtomCameraFrame> frame,
                                                 int newWidth, int newHeight);
    std::shared_ptr<AtomCameraFrame> rotateImage(std::shared_ptr<AtomCameraFrame> frame, double angle);
    std::shared_ptr<AtomCameraFrame> flipImage(std::shared_ptr<AtomCameraFrame> frame, bool horizontal, bool vertical);
    
    // Stacking Operations
    std::shared_ptr<AtomCameraFrame> stackImages(const std::vector<std::shared_ptr<AtomCameraFrame>>& frames,
                                                 const std::string& method = "average");
    std::shared_ptr<AtomCameraFrame> alignAndStack(const std::vector<std::shared_ptr<AtomCameraFrame>>& frames);
    
    // Settings and Configuration
    void setProcessingSettings(const ProcessingSettings& settings) { currentSettings_ = settings; }
    ProcessingSettings getProcessingSettings() const { return currentSettings_; }
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    void setMaxConcurrentProcessing(int max) { maxConcurrentTasks_ = max; }
    
    // Presets
    bool saveProcessingPreset(const std::string& name, const ProcessingSettings& settings);
    bool loadProcessingPreset(const std::string& name, ProcessingSettings& settings);
    std::vector<std::string> getAvailablePresets() const;
    bool deleteProcessingPreset(const std::string& name);

private:
    // Current settings
    ProcessingSettings currentSettings_;
    CalibrationFrames calibrationFrames_;
    
    // Threading and processing
    std::atomic<int> activeTasks_{0};
    int maxConcurrentTasks_ = 4;
    mutable std::mutex processingMutex_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    std::mutex callbackMutex_;
    
    // Presets storage
    std::map<std::string, ProcessingSettings> processingPresets_;
    mutable std::mutex presetsMutex_;
    
    // Core processing methods
    ProcessingResult processImageInternal(std::shared_ptr<AtomCameraFrame> frame,
                                         const ProcessingSettings& settings);
    std::shared_ptr<AtomCameraFrame> applyCalibration(std::shared_ptr<AtomCameraFrame> frame);
    std::shared_ptr<AtomCameraFrame> applyDarkSubtraction(std::shared_ptr<AtomCameraFrame> frame,
                                                          std::shared_ptr<AtomCameraFrame> dark);
    std::shared_ptr<AtomCameraFrame> applyFlatCorrection(std::shared_ptr<AtomCameraFrame> frame,
                                                         std::shared_ptr<AtomCameraFrame> flat);
    std::shared_ptr<AtomCameraFrame> applyBiasSubtraction(std::shared_ptr<AtomCameraFrame> frame,
                                                          std::shared_ptr<AtomCameraFrame> bias);
    
    // Image analysis helpers
    void calculateHistogram(std::shared_ptr<AtomCameraFrame> frame, uint32_t* histogram);
    double calculateMean(std::shared_ptr<AtomCameraFrame> frame);
    double calculateMedian(std::shared_ptr<AtomCameraFrame> frame);
    double calculateStdDev(std::shared_ptr<AtomCameraFrame> frame, double mean);
    std::pair<double, double> calculateMinMax(std::shared_ptr<AtomCameraFrame> frame);
    
    // Utility methods
    std::shared_ptr<AtomCameraFrame> cloneFrame(std::shared_ptr<AtomCameraFrame> frame);
    bool validateFrame(std::shared_ptr<AtomCameraFrame> frame);
    bool isFrameCompatible(std::shared_ptr<AtomCameraFrame> frame1, std::shared_ptr<AtomCameraFrame> frame2);
    void notifyProgress(int progress, const std::string& operation);
    void notifyCompletion(const ProcessingResult& result);
    
    // Preset management
    bool savePresetToFile(const std::string& name, const ProcessingSettings& settings);
    bool loadPresetFromFile(const std::string& name, ProcessingSettings& settings);
    std::string getPresetFilename(const std::string& name) const;
    
    // Math utilities
    template<typename T>
    T clamp(T value, T min, T max) {
        return std::max(min, std::min(value, max));
    }
    
    double bilinearInterpolate(double x, double y, const std::vector<std::vector<double>>& data);
};

} // namespace lithium::device::asi::camera::components
