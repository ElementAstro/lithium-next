/*
 * asi_camera_controller_v2.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Controller V2 - Modular Implementation

This is the modular version of the ASI Camera Controller that orchestrates
all the individual components to provide a unified camera interface.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>
#include <map>

#include "../../../template/camera.hpp"
#include "../../../template/camera_frame.hpp"
#include "../components/hardware_interface.hpp"
#include "../components/exposure_manager.hpp"
#include "../components/video_manager.hpp"
#include "../components/temperature_controller.hpp"
#include "../components/property_manager.hpp"
#include "../components/sequence_manager.hpp"
#include "../components/image_processor.hpp"

namespace lithium::device::asi::camera::controller {

/**
 * @brief Modular ASI Camera Controller V2
 * 
 * This controller orchestrates all the modular camera components to provide
 * a unified interface for ASI camera operations while maintaining the same
 * API as the original monolithic controller.
 */
class ASICameraControllerV2 {
public:
    // Component type aliases for convenience
    using HardwareInterface = lithium::device::asi::camera::components::HardwareInterface;
    using ExposureManager = lithium::device::asi::camera::components::ExposureManager;
    using VideoManager = lithium::device::asi::camera::components::VideoManager;
    using TemperatureController = lithium::device::asi::camera::components::TemperatureController;
    using PropertyManager = lithium::device::asi::camera::components::PropertyManager;
    using SequenceManager = lithium::device::asi::camera::components::SequenceManager;
    using ImageProcessor = lithium::device::asi::camera::components::ImageProcessor;

    // Forward declarations for callback types
    using ExposureCompleteCallback = std::function<void(bool, std::shared_ptr<AtomCameraFrame>)>;
    using VideoFrameCallback = std::function<void(std::shared_ptr<AtomCameraFrame>)>;
    using TemperatureCallback = std::function<void(double)>;
    using CoolerCallback = std::function<void(bool, double)>;
    using SequenceProgressCallback = std::function<void(int, int)>;

public:
    ASICameraControllerV2();
    ~ASICameraControllerV2();

    // Non-copyable and non-movable
    ASICameraControllerV2(const ASICameraControllerV2&) = delete;
    ASICameraControllerV2& operator=(const ASICameraControllerV2&) = delete;
    ASICameraControllerV2(ASICameraControllerV2&&) = delete;
    ASICameraControllerV2& operator=(ASICameraControllerV2&&) = delete;
    
    // ================================
    // Device Management
    // ================================
    bool initialize();
    bool destroy();
    bool connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3);
    bool disconnect();
    bool scan(std::vector<std::string>& devices);
    
    // ================================
    // Exposure Control
    // ================================
    bool startExposure(double duration);
    bool abortExposure();
    bool isExposing() const;
    double getExposureProgress() const;
    double getExposureRemaining() const;
    std::shared_ptr<AtomCameraFrame> getExposureResult();
    bool saveImage(const std::string& path);
    
    // Exposure history and statistics
    double getLastExposureDuration() const;
    uint32_t getExposureCount() const;
    bool resetExposureCount();
    
    // ================================
    // Video Streaming
    // ================================
    bool startVideo();
    bool stopVideo();
    bool isVideoRunning() const;
    std::shared_ptr<AtomCameraFrame> getVideoFrame();
    bool setVideoFormat(const std::string& format);
    std::vector<std::string> getVideoFormats() const;
    
    // Advanced video features
    bool startVideoRecording(const std::string& filename);
    bool stopVideoRecording();
    bool isVideoRecording() const;
    bool setVideoExposure(double exposure);
    double getVideoExposure() const;
    bool setVideoGain(int gain);
    int getVideoGain() const;
    
    // ================================
    // Temperature Control
    // ================================
    bool startCooling(double targetTemp);
    bool stopCooling();
    bool isCoolerOn() const;
    std::optional<double> getTemperature() const;
    TemperatureInfo getTemperatureInfo() const;
    std::optional<double> getCoolingPower() const;
    bool hasCooler() const;
    
    // ================================
    // Camera Properties
    // ================================
    bool setGain(int gain);
    int getGain() const;
    std::pair<int, int> getGainRange() const;
    bool setOffset(int offset);
    int getOffset() const;
    std::pair<int, int> getOffsetRange() const;
    bool setExposureTime(double exposure);
    double getExposureTime() const;
    std::pair<double, double> getExposureRange() const;
    
    // ISO and advanced controls
    bool setISO(int iso);
    int getISO() const;
    std::vector<int> getISOValues() const;
    bool setUSBBandwidth(int bandwidth);
    int getUSBBandwidth() const;
    std::pair<int, int> getUSBBandwidthRange() const;
    
    // Auto controls
    bool setAutoExposure(bool enable);
    bool isAutoExposureEnabled() const;
    bool setAutoGain(bool enable);
    bool isAutoGainEnabled() const;
    bool setAutoWhiteBalance(bool enable);
    bool isAutoWhiteBalanceEnabled() const;
    
    // Image format and quality
    bool setImageFormat(const std::string& format);
    std::string getImageFormat() const;
    std::vector<std::string> getImageFormats() const;
    bool setQuality(int quality);
    int getQuality() const;
    
    // ================================
    // ROI and Binning
    // ================================
    bool setROI(int x, int y, int width, int height);
    PropertyManager::ROI getROI() const;
    bool setBinning(int binX, int binY);
    PropertyManager::BinningMode getBinning() const;
    std::vector<PropertyManager::BinningMode> getSupportedBinning() const;
    int getMaxWidth() const;
    int getMaxHeight() const;
    
    // ================================
    // Camera Modes
    // ================================
    bool setHighSpeedMode(bool enable);
    bool isHighSpeedMode() const;
    bool setFlipMode(int mode);
    int getFlipMode() const;
    bool setCameraMode(const std::string& mode);
    std::string getCameraMode() const;
    std::vector<std::string> getCameraModes() const;
    
    // ================================
    // Sequence Control
    // ================================
    bool startSequence(const SequenceManager::SequenceSettings& sequence);
    bool stopSequence();
    bool isSequenceRunning() const;
    std::pair<int, int> getSequenceProgress() const;
    bool pauseSequence();
    bool resumeSequence();
    
    // ================================
    // Frame Statistics and Analysis
    // ================================
    double getFrameRate() const;
    double getDataRate() const;
    uint64_t getTotalDataTransferred() const;
    uint32_t getDroppedFrames() const;
    
    // ================================
    // Calibration Frames
    // ================================
    bool takeDarkFrame(double exposure, int count = 1);
    bool takeFlatFrame(double exposure, int count = 1);
    bool takeBiasFrame(int count = 1);
    
    // ================================
    // Hardware Information
    // ================================
    std::string getFirmwareVersion() const;
    std::string getSerialNumber() const;
    std::string getModelName() const;
    std::string getDriverVersion() const;
    double getPixelSize() const;
    int getBitDepth() const;
    
    // ================================
    // Status and Diagnostics
    // ================================
    std::string getLastError() const;
    std::vector<std::string> getOperationHistory() const;
    bool performSelfTest();
    
    // Connection state queries
    bool isInitialized() const;
    bool isConnected() const;
    
    // ================================
    // Callbacks
    // ================================
    void setExposureCompleteCallback(ExposureCompleteCallback callback);
    void setVideoFrameCallback(VideoFrameCallback callback);
    void setTemperatureCallback(TemperatureCallback callback);
    void setCoolerCallback(CoolerCallback callback);
    void setSequenceProgressCallback(SequenceProgressCallback callback);
    
    // ================================
    // Component Access (for advanced users)
    // ================================
    std::shared_ptr<HardwareInterface> getHardwareInterface() const { return hardware_; }
    std::shared_ptr<ExposureManager> getExposureManager() const { return exposureManager_; }
    std::shared_ptr<VideoManager> getVideoManager() const { return videoManager_; }
    std::shared_ptr<TemperatureController> getTemperatureController() const { return temperatureController_; }
    std::shared_ptr<PropertyManager> getPropertyManager() const { return propertyManager_; }
    std::shared_ptr<SequenceManager> getSequenceManager() const { return sequenceManager_; }
    std::shared_ptr<ImageProcessor> getImageProcessor() const { return imageProcessor_; }
    
    // ================================
    // Advanced Features
    // ================================
    bool processImage(std::shared_ptr<AtomCameraFrame> frame, const ImageProcessor::ProcessingSettings& settings);
    ImageProcessor::ImageStatistics analyzeImage(std::shared_ptr<AtomCameraFrame> frame);
    bool saveCalibrationFrames(const std::string& directory);
    bool loadCalibrationFrames(const std::string& directory);

private:
    // Component instances
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<ExposureManager> exposureManager_;
    std::shared_ptr<VideoManager> videoManager_;
    std::shared_ptr<TemperatureController> temperatureController_;
    std::shared_ptr<PropertyManager> propertyManager_;
    std::shared_ptr<SequenceManager> sequenceManager_;
    std::shared_ptr<ImageProcessor> imageProcessor_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    mutable std::mutex stateMutex_;
    
    // Callbacks
    ExposureCompleteCallback exposureCompleteCallback_;
    VideoFrameCallback videoFrameCallback_;
    TemperatureCallback temperatureCallback_;
    CoolerCallback coolerCallback_;
    SequenceProgressCallback sequenceProgressCallback_;
    std::mutex callbackMutex_;
    
    // Error handling and history
    mutable std::string lastError_;
    std::vector<std::string> operationHistory_;
    mutable std::mutex errorMutex_;
    
    // Cache for frequently accessed data
    mutable std::map<std::string, std::pair<std::chrono::steady_clock::time_point, std::string>> stringCache_;
    mutable std::map<std::string, std::pair<std::chrono::steady_clock::time_point, double>> doubleCache_;
    mutable std::map<std::string, std::pair<std::chrono::steady_clock::time_point, int>> intCache_;
    static constexpr std::chrono::seconds CACHE_DURATION{1}; // 1 second cache
    
    // Helper methods
    bool initializeComponents();
    void setupCallbacks();
    void cleanupComponents();
    void updateOperationHistory(const std::string& operation);
    void setLastError(const std::string& error);
    
    // Cache management
    template<typename T>
    bool getCachedValue(const std::string& key, T& value, const std::map<std::string, std::pair<std::chrono::steady_clock::time_point, T>>& cache) const;
    template<typename T>
    void setCachedValue(const std::string& key, const T& value, std::map<std::string, std::pair<std::chrono::steady_clock::time_point, T>>& cache) const;
    void clearCache();
    
    // Conversion helpers
    std::string flipStatusToString(ASI_FLIP_STATUS flip) const;
    ASI_FLIP_STATUS stringToFlipStatus(const std::string& flip) const;
    std::string cameraModeToString(ASI_CAMERA_MODE mode) const;
    ASI_CAMERA_MODE stringToCameraMode(const std::string& mode) const;
    std::string imageTypeToString(ASI_IMG_TYPE type) const;
    ASI_IMG_TYPE stringToImageType(const std::string& type) const;
    
    // Validation helpers
    bool validateExposureTime(double exposure) const;
    bool validateGain(int gain) const;
    bool validateOffset(int offset) const;
    bool validateROI(int x, int y, int width, int height) const;
    bool validateBinning(int binX, int binY) const;
    
    // Component callback handlers
    void handleExposureComplete(const ExposureManager::ExposureResult& result);
    void handleVideoFrame(std::shared_ptr<AtomCameraFrame> frame);
    void handleTemperatureChange(const TemperatureController::TemperatureInfo& info);
    void handleSequenceProgress(const SequenceManager::SequenceProgress& progress);
};

} // namespace lithium::device::asi::camera::controller
