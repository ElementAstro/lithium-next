/*
 * asi_camera_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Controller Implementation

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "../../../template/camera.hpp"
#include "../../../template/camera_frame.hpp"

// Forward declarations
namespace lithium::device::asi::camera {
class ASICamera;
struct CameraSequence;

// ROI (Region of Interest) structure
struct ROI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Binning mode structure
struct BinningMode {
    int binX = 1;
    int binY = 1;
    std::string description = "1x1";
};

}

namespace lithium::device::asi::camera::controller {

/**
 * @brief ASI Camera Hardware Controller
 * 
 * This class handles all low-level communication with ASI camera hardware,
 * managing device connection, exposure control, video streaming, cooling,
 * and all advanced camera features.
 */
class ASICameraController {
public:
    explicit ASICameraController(ASICamera* parent);
    ~ASICameraController();

    // Non-copyable and non-movable
    ASICameraController(const ASICameraController&) = delete;
    ASICameraController& operator=(const ASICameraController&) = delete;
    ASICameraController(ASICameraController&&) = delete;
    ASICameraController& operator=(ASICameraController&&) = delete;
    
    // Device management
    bool initialize();
    bool destroy();
    bool connect(const std::string& deviceName, int timeout, int maxRetry);
    bool disconnect();
    bool scan(std::vector<std::string>& devices);
    
    // Exposure control
    bool startExposure(double duration);
    bool abortExposure();
    bool isExposing() const;
    double getExposureProgress() const;
    double getExposureRemaining() const;
    std::shared_ptr<AtomCameraFrame> getExposureResult();
    bool saveImage(const std::string& path);
    
    // Exposure history and statistics
    double getLastExposureDuration() const { return lastExposureDuration_; }
    uint32_t getExposureCount() const { return exposureCount_; }
    bool resetExposureCount();
    
    // Video streaming
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
    double getVideoExposure() const { return videoExposure_; }
    bool setVideoGain(int gain);
    int getVideoGain() const { return videoGain_; }
    
    // Temperature control
    bool startCooling(double targetTemp);
    bool stopCooling();
    bool isCoolerOn() const;
    std::optional<double> getTemperature() const;
    TemperatureInfo getTemperatureInfo() const;
    std::optional<double> getCoolingPower() const;
    bool hasCooler() const { return hasCooler_; }
    
    // Camera properties
    bool setGain(int gain);
    int getGain() const { return currentGain_; }
    std::pair<int, int> getGainRange() const;
    bool setOffset(int offset);
    int getOffset() const { return currentOffset_; }
    std::pair<int, int> getOffsetRange() const;
    bool setExposureTime(double exposure);
    double getExposureTime() const { return currentExposure_; }
    std::pair<double, double> getExposureRange() const;
    
    // ISO and advanced controls
    bool setISO(int iso);
    int getISO() const { return currentISO_; }
    std::vector<int> getISOValues() const;
    bool setUSBBandwidth(int bandwidth);
    int getUSBBandwidth() const { return usbBandwidth_; }
    std::pair<int, int> getUSBBandwidthRange() const;
    
    // Auto controls
    bool setAutoExposure(bool enable);
    bool isAutoExposureEnabled() const { return autoExposureEnabled_; }
    bool setAutoGain(bool enable);
    bool isAutoGainEnabled() const { return autoGainEnabled_; }
    bool setAutoWhiteBalance(bool enable);
    bool isAutoWhiteBalanceEnabled() const { return autoWBEnabled_; }
    
    // Image format and quality
    bool setImageFormat(const std::string& format);
    std::string getImageFormat() const { return currentImageFormat_; }
    std::vector<std::string> getImageFormats() const;
    bool setQuality(int quality);
    int getQuality() const { return imageQuality_; }
    
    // ROI and binning
    bool setROI(int x, int y, int width, int height);
    ROI getROI() const;
    bool setBinning(int binX, int binY);
    BinningMode getBinning() const;
    std::vector<BinningMode> getSupportedBinning() const;
    int getMaxWidth() const { return maxWidth_; }
    int getMaxHeight() const { return maxHeight_; }
    
    // Camera modes
    bool setHighSpeedMode(bool enable);
    bool isHighSpeedMode() const { return highSpeedMode_; }
    bool setFlipMode(int mode);
    int getFlipMode() const { return flipMode_; }
    bool setCameraMode(const std::string& mode);
    std::string getCameraMode() const { return currentMode_; }
    std::vector<std::string> getCameraModes() const;
    
    // Sequence control
    bool startSequence(const CameraSequence& sequence);
    bool stopSequence();
    bool isSequenceRunning() const;
    std::pair<int, int> getSequenceProgress() const;
    bool pauseSequence();
    bool resumeSequence();
    
    // Frame statistics and analysis
    double getFrameRate() const;
    double getDataRate() const;
    uint64_t getTotalDataTransferred() const { return totalDataTransferred_; }
    uint32_t getDroppedFrames() const { return droppedFrames_; }
    
    // Calibration frames
    bool takeDarkFrame(double exposure, int count = 1);
    bool takeFlatFrame(double exposure, int count = 1);
    bool takeBiasFrame(int count = 1);
    
    // Hardware information
    std::string getFirmwareVersion() const { return firmwareVersion_; }
    std::string getSerialNumber() const { return serialNumber_; }
    std::string getModelName() const { return modelName_; }
    std::string getDriverVersion() const;
    double getPixelSize() const { return pixelSize_; }
    int getBitDepth() const { return bitDepth_; }
    
    // Status and diagnostics
    std::string getLastError() const { return lastError_; }
    std::vector<std::string> getOperationHistory() const { return operationHistory_; }
    bool performSelfTest();
    
    // Connection state queries
    bool isInitialized() const { return initialized_; }
    bool isConnected() const { return connected_; }
    
    // Callbacks
    void setExposureCompleteCallback(std::function<void(bool, std::shared_ptr<AtomCameraFrame>)> callback);
    void setVideoFrameCallback(std::function<void(std::shared_ptr<AtomCameraFrame>)> callback);
    void setTemperatureCallback(std::function<void(double)> callback);
    void setCoolerCallback(std::function<void(bool, double)> callback);
    void setSequenceProgressCallback(std::function<void(int, int)> callback);

private:
    // Parent reference
    ASICamera* parent_;
    
    // Connection state
    bool initialized_ = false;
    bool connected_ = false;
    int cameraId_ = -1;
    
    // Camera information
    std::string modelName_ = "ASI Camera";
    std::string serialNumber_ = "ASI12345";
    std::string firmwareVersion_ = "Unknown";
    double pixelSize_ = 3.75; // microns
    int bitDepth_ = 16;
    int maxWidth_ = 0;
    int maxHeight_ = 0;
    bool hasCooler_ = false;
    
    // Exposure state
    std::atomic<bool> exposing_ = false;
    std::atomic<bool> exposureAbortRequested_ = false;
    double currentExposure_ = 1.0;
    double lastExposureDuration_ = 0.0;
    std::atomic<uint32_t> exposureCount_ = 0;
    std::chrono::steady_clock::time_point exposureStartTime_;
    std::thread exposureThread_;
    
    // Video state
    std::atomic<bool> videoRunning_ = false;
    std::atomic<bool> videoRecording_ = false;
    std::string videoRecordingFile_;
    double videoExposure_ = 0.033;
    int videoGain_ = 0;
    std::string videoFormat_ = "RAW16";
    std::thread videoThread_;
    
    // Camera properties
    int currentGain_ = 0;
    int currentOffset_ = 0;
    int currentISO_ = 100;
    int usbBandwidth_ = 40;
    std::string currentImageFormat_ = "FITS";
    int imageQuality_ = 95;
    
    // Auto controls
    bool autoExposureEnabled_ = false;
    bool autoGainEnabled_ = false;
    bool autoWBEnabled_ = false;
    
    // ROI and binning
    int roiX_ = 0;
    int roiY_ = 0;
    int roiWidth_ = 0;
    int roiHeight_ = 0;
    int binX_ = 1;
    int binY_ = 1;
    
    // Camera modes
    bool highSpeedMode_ = false;
    int flipMode_ = 0;
    std::string currentMode_ = "NORMAL";
    
    // Temperature control
    std::atomic<bool> coolerEnabled_ = false;
    double targetTemperature_ = -10.0;
    double currentTemperature_ = 25.0;
    double coolingPower_ = 0.0;
    std::thread temperatureThread_;
    
    // Sequence state
    std::atomic<bool> sequenceRunning_ = false;
    std::atomic<bool> sequencePaused_ = false;
    std::atomic<int> sequenceCurrentFrame_ = 0;
    int sequenceTotalFrames_ = 0;
    double sequenceExposure_ = 1.0;
    double sequenceInterval_ = 0.0;
    std::thread sequenceThread_;
    
    // Statistics
    uint64_t totalDataTransferred_ = 0;
    uint32_t droppedFrames_ = 0;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::vector<std::chrono::steady_clock::time_point> frameTimestamps_;
    
    // Error handling and history
    std::string lastError_;
    std::vector<std::string> operationHistory_;
    
    // Callbacks
    std::function<void(bool, std::shared_ptr<AtomCameraFrame>)> exposureCompleteCallback_;
    std::function<void(std::shared_ptr<AtomCameraFrame>)> videoFrameCallback_;
    std::function<void(double)> temperatureCallback_;
    std::function<void(bool, double)> coolerCallback_;
    std::function<void(int, int)> sequenceProgressCallback_;
    
    // Thread safety
    mutable std::mutex deviceMutex_;
    mutable std::mutex exposureMutex_;
    mutable std::mutex videoMutex_;
    mutable std::mutex temperatureMutex_;
    mutable std::mutex sequenceMutex_;
    std::condition_variable exposureCondition_;
    std::condition_variable sequenceCondition_;
    
    // Background monitoring
    std::thread monitoringThread_;
    std::atomic<bool> monitoringActive_ = false;
    
    // Helper methods
    bool validateExposureTime(double exposure) const;
    bool validateGain(int gain) const;
    bool validateOffset(int offset) const;
    bool validateROI(int x, int y, int width, int height) const;
    bool validateBinning(int binX, int binY) const;
    void updateOperationHistory(const std::string& operation);
    void notifyExposureComplete(bool success, std::shared_ptr<AtomCameraFrame> frame);
    void notifyVideoFrame(std::shared_ptr<AtomCameraFrame> frame);
    void notifyTemperatureChange(double temperature);
    void notifyCoolerChange(bool enabled, double power);
    void notifySequenceProgress(int current, int total);
    
    // Worker threads
    void exposureWorker(double duration);
    void videoWorker();
    void temperatureWorker();
    void sequenceWorker();
    void monitoringWorker();
    
    // SDK wrapper methods
    bool initializeSDK();
    bool cleanupSDK();
    bool openCamera(int cameraId);
    bool closeCamera();
    bool getCameraInfo();
    bool setControlValue(int controlType, long value, bool isAuto = false);
    bool getControlValue(int controlType, long* value, bool* isAuto = nullptr) const;
    std::shared_ptr<AtomCameraFrame> captureFrame(double exposure);
    bool startVideoCapture();
    bool stopVideoCapture();
    std::shared_ptr<AtomCameraFrame> getVideoFrameData();
    void updateFrameStatistics();
};

} // namespace lithium::device::asi::camera::controller
