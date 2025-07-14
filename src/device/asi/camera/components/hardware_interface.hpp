/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Hardware Interface Component

This component provides a clean interface to the ASI Camera SDK,
handling low-level hardware communication, device management,
and SDK integration.

*************************************************/

#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <libasi/ASICamera2.h>

namespace lithium::device::asi::camera::components {

/**
 * @brief Hardware Interface for ASI Camera SDK communication
 *
 * This component encapsulates all direct interaction with the ASI Camera SDK,
 * providing a clean C++ interface for hardware operations while managing
 * SDK lifecycle, device enumeration, connection management, and low-level
 * parameter control.
 */
class HardwareInterface {
public:
    struct CameraInfo {
        int cameraId = -1;
        std::string name;
        std::string serialNumber;
        int maxWidth = 0;
        int maxHeight = 0;
        bool isColorCamera = false;
        int bitDepth = 16;
        double pixelSize = 0.0;
        bool hasMechanicalShutter = false;
        bool hasST4Port = false;
        bool hasCooler = false;
        bool isUSB3Host = false;
        bool isUSB3Camera = false;
        double electronMultiplyGain = 0.0;
        std::vector<int> supportedBins;
        std::vector<ASI_IMG_TYPE> supportedVideoFormats;
        std::string triggerCaps;
    };

    struct ControlCaps {
        std::string name;
        std::string description;
        long maxValue = 0;
        long minValue = 0;
        long defaultValue = 0;
        bool isAutoSupported = false;
        bool isWritable = false;
        ASI_CONTROL_TYPE controlType;
    };

public:
    HardwareInterface();
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // SDK Lifecycle Management
    bool initializeSDK();
    bool shutdownSDK();
    bool isSDKInitialized() const { return sdkInitialized_; }

    // Device Discovery and Management
    std::vector<std::string> enumerateDevices();
    std::vector<CameraInfo> getAvailableCameras();
    bool openCamera(const std::string& deviceName);
    bool openCamera(int cameraId);
    bool closeCamera();
    bool isConnected() const { return connected_; }

    // Camera Information
    std::optional<CameraInfo> getCameraInfo() const;
    int getCurrentCameraId() const { return currentCameraId_; }
    std::string getCurrentDeviceName() const { return currentDeviceName_; }

    // Control Management
    std::vector<ControlCaps> getControlCapabilities();
    bool setControlValue(ASI_CONTROL_TYPE controlType, long value,
                         bool isAuto = false);
    bool getControlValue(ASI_CONTROL_TYPE controlType, long& value,
                         bool& isAuto);
    bool hasControl(ASI_CONTROL_TYPE controlType);

    // Image Capture Operations
    bool startExposure(int width, int height, int binning,
                       ASI_IMG_TYPE imageType);
    bool startExposure(int width, int height, int binning,
                       ASI_IMG_TYPE imageType, bool isDarkFrame);
    bool stopExposure();
    ASI_EXPOSURE_STATUS getExposureStatus();
    bool getImageData(unsigned char* buffer, long bufferSize);

    // Video Capture Operations
    bool startVideoCapture();
    bool stopVideoCapture();
    bool getVideoData(unsigned char* buffer, long bufferSize,
                      int waitMs = 1000);

    // ROI and Binning
    bool setROI(int startX, int startY, int width, int height, int binning);
    bool getROI(int& startX, int& startY, int& width, int& height,
                int& binning);

    // Image Format
    bool setImageFormat(int width, int height, int binning,
                        ASI_IMG_TYPE imageType);
    ASI_IMG_TYPE getImageFormat();

    // Camera Modes
    bool setCameraMode(ASI_CAMERA_MODE mode);
    ASI_CAMERA_MODE getCameraMode();

    // Utility Functions
    std::string getSDKVersion();
    std::string getDriverVersion();
    std::string getLastSDKError() const { return lastError_; }

    // Guiding Support (ST4)
    bool pulseGuide(ASI_GUIDE_DIRECTION direction, int durationMs);
    bool stopGuide();

    // Advanced Features
    bool setFlipStatus(ASI_FLIP_STATUS flipStatus);
    ASI_FLIP_STATUS getFlipStatus();

    // GPS Support
    bool getGPSData(ASI_GPS_DATA& startLineGPS, ASI_GPS_DATA& endLineGPS);
    bool getVideoDataWithGPS(unsigned char* buffer, long bufferSize, int waitMs,
                             ASI_GPS_DATA& gpsData);
    bool getImageDataWithGPS(unsigned char* buffer, long bufferSize,
                             ASI_GPS_DATA& gpsData);

    // Serial Number Support
    std::string getSerialNumber();

    // Trigger Camera Support
    bool getSupportedCameraModes(std::vector<ASI_CAMERA_MODE>& modes);
    bool sendSoftTrigger(bool start);
    bool setTriggerOutputConfig(ASI_TRIG_OUTPUT_PIN pin, bool pinHigh,
                                long delayUs, long durationUs);
    bool getTriggerOutputConfig(ASI_TRIG_OUTPUT_PIN pin, bool& pinHigh,
                                long& delayUs, long& durationUs);

private:
    // Connection state
    std::atomic<bool> sdkInitialized_{false};
    std::atomic<bool> connected_{false};
    int currentCameraId_{-1};
    std::string currentDeviceName_;

    // Camera information cache
    std::optional<CameraInfo> currentCameraInfo_;
    std::vector<ControlCaps> controlCapabilities_;

    // Error handling
    std::string lastError_;

    // Thread safety
    mutable std::mutex sdkMutex_;
    mutable std::mutex connectionMutex_;
    mutable std::mutex controlMutex_;

    // Helper methods
    bool loadCameraInfo(int cameraId);
    bool loadControlCapabilities();
    std::string asiErrorToString(ASI_ERROR_CODE error);
    void updateLastError(const std::string& operation, ASI_ERROR_CODE result);
    bool validateCameraId(int cameraId);
    bool validateControlType(ASI_CONTROL_TYPE controlType);
    int findCameraByName(const std::string& name);
};

}  // namespace lithium::device::asi::camera::components
