/*
 * sequence_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Sequence Manager Component

This component manages automated imaging sequences including exposure
series, time-lapse, bracketing, and complex multi-step sequences.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <queue>

#include "../../../template/camera_frame.hpp"

namespace lithium::device::asi::camera::components {

class ExposureManager;
class PropertyManager;

/**
 * @brief Sequence Manager for ASI Camera
 * 
 * Manages automated imaging sequences with support for various
 * sequence types, progress tracking, and result collection.
 */
class SequenceManager {
public:
    enum class SequenceType {
        SIMPLE,         // Simple exposure series
        BRACKETING,     // Exposure bracketing
        TIME_LAPSE,     // Time-lapse photography
        CUSTOM,         // Custom sequence with scripts
        CALIBRATION     // Calibration frame sequences
    };

    enum class SequenceState {
        IDLE,
        PREPARING,
        RUNNING,
        PAUSED,
        STOPPING,
        COMPLETE,
        ABORTED,
        ERROR
    };

    struct ExposureStep {
        double duration = 1.0;              // Exposure duration in seconds
        int gain = 0;                       // Gain setting
        int offset = 0;                     // Offset setting
        std::string filter = "";            // Filter name (if applicable)
        std::string filename = "";          // Output filename pattern
        bool isDark = false;                // Dark frame flag
        std::map<std::string, double> customSettings; // Custom property settings
    };

    struct SequenceSettings {
        SequenceType type = SequenceType::SIMPLE;
        std::string name = "Sequence";
        std::vector<ExposureStep> steps;
        int repeatCount = 1;                // Number of sequence repetitions
        std::chrono::seconds intervalDelay{0}; // Delay between exposures
        std::chrono::seconds sequenceDelay{0}; // Delay between sequence repetitions
        bool saveImages = true;             // Save images to disk
        std::string outputDirectory = "";   // Output directory
        std::string filenameTemplate = "";  // Filename template
        bool enableDithering = false;       // Enable dithering between exposures
        int ditherPixels = 5;               // Dither amount in pixels
        bool enableAutoFocus = false;       // Enable autofocus before sequence
        int autoFocusInterval = 10;         // Autofocus every N exposures
        bool enableTemperatureStabilization = false; // Wait for temperature stability
        double targetTemperature = -10.0;   // Target temperature for stabilization
    };

    struct SequenceProgress {
        int currentStep = 0;
        int totalSteps = 0;
        int currentRepeat = 0;
        int totalRepeats = 0;
        int completedExposures = 0;
        int totalExposures = 0;
        double progress = 0.0;              // Overall progress percentage
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point estimatedEndTime;
        std::chrono::seconds remainingTime{0};
        std::string currentOperation = "";
    };

    struct SequenceResult {
        bool success = false;
        std::string sequenceName;
        std::vector<std::shared_ptr<AtomCameraFrame>> frames;
        std::vector<std::string> savedFilenames;
        int completedExposures = 0;
        int failedExposures = 0;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        std::chrono::seconds totalDuration{0};
        std::string errorMessage;
        std::map<std::string, std::string> metadata;
    };

    using ProgressCallback = std::function<void(const SequenceProgress&)>;
    using StepCallback = std::function<void(int step, const ExposureStep&)>;
    using CompletionCallback = std::function<void(const SequenceResult&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

public:
    SequenceManager(std::shared_ptr<ExposureManager> exposureManager,
                   std::shared_ptr<PropertyManager> propertyManager);
    ~SequenceManager();

    // Non-copyable and non-movable
    SequenceManager(const SequenceManager&) = delete;
    SequenceManager& operator=(const SequenceManager&) = delete;
    SequenceManager(SequenceManager&&) = delete;
    SequenceManager& operator=(SequenceManager&&) = delete;

    // Sequence Control
    bool startSequence(const SequenceSettings& settings);
    bool pauseSequence();
    bool resumeSequence();
    bool stopSequence();
    bool abortSequence();
    
    // State and Progress
    SequenceState getState() const { return state_; }
    std::string getStateString() const;
    SequenceProgress getProgress() const;
    bool isRunning() const { return state_ == SequenceState::RUNNING; }
    bool isPaused() const { return state_ == SequenceState::PAUSED; }
    
    // Results
    SequenceResult getLastResult() const;
    std::vector<SequenceResult> getAllResults() const;
    bool hasResult() const;
    void clearResults();
    
    // Sequence Templates
    SequenceSettings createSimpleSequence(double exposure, int count, 
                                         std::chrono::seconds interval = std::chrono::seconds{0});
    SequenceSettings createBracketingSequence(double baseExposure, 
                                             const std::vector<double>& exposureMultipliers,
                                             int repeatCount = 1);
    SequenceSettings createTimeLapseSequence(double exposure, int count, 
                                            std::chrono::seconds interval);
    SequenceSettings createCalibrationSequence(const std::string& frameType,
                                              double exposure, int count);
    
    // Custom Sequences
    bool addExposureStep(SequenceSettings& settings, const ExposureStep& step);
    bool removeExposureStep(SequenceSettings& settings, int index);
    bool updateExposureStep(SequenceSettings& settings, int index, const ExposureStep& step);
    
    // Sequence Validation
    bool validateSequence(const SequenceSettings& settings) const;
    std::chrono::seconds estimateSequenceDuration(const SequenceSettings& settings) const;
    int calculateTotalExposures(const SequenceSettings& settings) const;
    
    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setStepCallback(StepCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Configuration
    void setMaxConcurrentSequences(int max) { maxConcurrentSequences_ = max; }
    void setDefaultOutputDirectory(const std::string& directory) { defaultOutputDirectory_ = directory; }
    void setDefaultFilenameTemplate(const std::string& template_str) { defaultFilenameTemplate_ = template_str; }
    
    // Sequence Management
    std::vector<std::string> getRunningSequences() const;
    bool isSequenceRunning(const std::string& sequenceName) const;
    
    // Presets
    bool saveSequencePreset(const std::string& name, const SequenceSettings& settings);
    bool loadSequencePreset(const std::string& name, SequenceSettings& settings);
    std::vector<std::string> getAvailablePresets() const;
    bool deleteSequencePreset(const std::string& name);

private:
    // Component references
    std::shared_ptr<ExposureManager> exposureManager_;
    std::shared_ptr<PropertyManager> propertyManager_;
    
    // State management
    std::atomic<SequenceState> state_{SequenceState::IDLE};
    SequenceSettings currentSettings_;
    SequenceProgress currentProgress_;
    SequenceResult currentResult_;
    
    // Threading
    std::thread sequenceThread_;
    std::atomic<bool> pauseRequested_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> abortRequested_{false};
    std::mutex stateMutex_;
    std::condition_variable stateCondition_;
    
    // Results storage
    std::vector<SequenceResult> results_;
    mutable std::mutex resultsMutex_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    StepCallback stepCallback_;
    CompletionCallback completionCallback_;
    ErrorCallback errorCallback_;
    std::mutex callbackMutex_;
    
    // Configuration
    int maxConcurrentSequences_ = 1;
    std::string defaultOutputDirectory_;
    std::string defaultFilenameTemplate_ = "{name}_{step:03d}_{timestamp}";
    
    // Sequence presets
    std::map<std::string, SequenceSettings> sequencePresets_;
    mutable std::mutex presetsMutex_;
    
    // Worker methods
    void sequenceWorker();
    bool executeSequence(const SequenceSettings& settings, SequenceResult& result);
    bool executeExposureStep(const ExposureStep& step, int stepIndex, SequenceResult& result);
    bool prepareSequence(const SequenceSettings& settings);
    bool applyStepSettings(const ExposureStep& step);
    bool restoreOriginalSettings();
    void updateProgress();
    void waitForInterval(std::chrono::seconds interval);
    bool performDithering(int pixels);
    bool performAutoFocus();
    bool waitForTemperatureStabilization(double targetTemp);
    
    // File management
    std::string generateFilename(const SequenceSettings& settings, int step, int repeat) const;
    bool saveFrame(std::shared_ptr<AtomCameraFrame> frame, const std::string& filename);
    bool createOutputDirectory(const std::string& directory);
    
    // Progress and notification
    void updateState(SequenceState newState);
    void notifyProgress(const SequenceProgress& progress);
    void notifyStepStart(int step, const ExposureStep& stepSettings);
    void notifyCompletion(const SequenceResult& result);
    void notifyError(const std::string& error);
    
    // Helper methods
    std::string replaceFilenameTokens(const std::string& template_str, 
                                     const SequenceSettings& settings,
                                     int step, int repeat) const;
    std::string getCurrentTimestamp() const;
    bool validateExposureStep(const ExposureStep& step) const;
    void copyOriginalSettings();
    std::string formatSequenceError(const std::string& operation, const std::string& error);
    
    // Preset management
    bool savePresetToFile(const std::string& name, const SequenceSettings& settings);
    bool loadPresetFromFile(const std::string& name, SequenceSettings& settings);
    std::string getPresetFilename(const std::string& name) const;
    
    // Original settings storage (for restoration)
    std::map<std::string, double> originalSettings_;
    bool originalSettingsStored_ = false;
};

} // namespace lithium::device::asi::camera::components
