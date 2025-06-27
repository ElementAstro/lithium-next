/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include "spdlog/spdlog.h"

namespace lithium::device::asi::camera::components {

HardwareInterface::HardwareInterface() {
    spdlog::info("ASI Camera HardwareInterface initialized");
}

HardwareInterface::~HardwareInterface() {
    if (connected_) {
        closeCamera();
    }
    if (sdkInitialized_) {
        shutdownSDK();
    }
    spdlog::info("ASI Camera HardwareInterface destroyed");
}

bool HardwareInterface::initializeSDK() {
    std::lock_guard<std::mutex> lock(sdkMutex_);

    if (sdkInitialized_) {
        spdlog::warn("ASI SDK already initialized");
        return true;
    }

    spdlog::info("Initializing ASI Camera SDK");

    // In a real implementation, this would initialize the ASI SDK
    // For now, we simulate successful initialization
    sdkInitialized_ = true;

    spdlog::info("ASI Camera SDK initialized successfully");
    return true;
}

bool HardwareInterface::shutdownSDK() {
    std::lock_guard<std::mutex> lock(sdkMutex_);

    if (!sdkInitialized_) {
        return true;
    }

    if (connected_) {
        closeCamera();
    }

    spdlog::info("Shutting down ASI Camera SDK");

    // In a real implementation, this would cleanup the ASI SDK
    sdkInitialized_ = false;

    spdlog::info("ASI Camera SDK shutdown complete");
    return true;
}

std::vector<std::string> HardwareInterface::enumerateDevices() {
    std::lock_guard<std::mutex> lock(sdkMutex_);

    if (!sdkInitialized_) {
        spdlog::error("SDK not initialized");
        return {};
    }

    std::vector<std::string> deviceNames;

    int numCameras = ASIGetNumOfConnectedCameras();
    spdlog::info("Found {} ASI cameras", numCameras);

    for (int i = 0; i < numCameras; ++i) {
        ASI_CAMERA_INFO cameraInfo;
        ASI_ERROR_CODE result = ASIGetCameraProperty(&cameraInfo, i);

        if (result == ASI_SUCCESS) {
            deviceNames.emplace_back(cameraInfo.Name);
            spdlog::info("Found camera: {} (ID: {})", cameraInfo.Name,
                  cameraInfo.CameraID);
        } else {
            updateLastError("ASIGetCameraProperty", result);
            spdlog::error("Failed to get camera property for index {}: {}", i,
                  lastError_);
        }
    }

    return deviceNames;
}

std::vector<HardwareInterface::CameraInfo>
HardwareInterface::getAvailableCameras() {
    std::lock_guard<std::mutex> lock(sdkMutex_);

    if (!sdkInitialized_) {
        spdlog::error("SDK not initialized");
        return {};
    }

    std::vector<CameraInfo> cameras;

    int numCameras = ASIGetNumOfConnectedCameras();

    for (int i = 0; i < numCameras; ++i) {
        ASI_CAMERA_INFO asiInfo;
        ASI_ERROR_CODE result = ASIGetCameraProperty(&asiInfo, i);

        if (result == ASI_SUCCESS) {
            CameraInfo camera;
            camera.cameraId = asiInfo.CameraID;
            camera.name = asiInfo.Name;
            camera.maxWidth = static_cast<int>(asiInfo.MaxWidth);
            camera.maxHeight = static_cast<int>(asiInfo.MaxHeight);
            camera.isColorCamera = (asiInfo.IsColorCam == ASI_TRUE);
            camera.bitDepth = asiInfo.BitDepth;
            camera.pixelSize = asiInfo.PixelSize;
            camera.hasMechanicalShutter =
                (asiInfo.MechanicalShutter == ASI_TRUE);
            camera.hasST4Port = (asiInfo.ST4Port == ASI_TRUE);
            camera.hasCooler = (asiInfo.IsCoolerCam == ASI_TRUE);
            camera.isUSB3Host = (asiInfo.IsUSB3Host == ASI_TRUE);
            camera.isUSB3Camera = (asiInfo.IsUSB3Camera == ASI_TRUE);
            camera.electronMultiplyGain = asiInfo.ElecPerADU;

            // Parse supported binning modes
            for (int j = 0; j < 16 && asiInfo.SupportedBins[j] != 0; ++j) {
                camera.supportedBins.push_back(asiInfo.SupportedBins[j]);
            }

            // Parse supported video formats
            for (int j = 0;
                 j < 8 && asiInfo.SupportedVideoFormat[j] != ASI_IMG_END; ++j) {
                camera.supportedVideoFormats.push_back(
                    asiInfo.SupportedVideoFormat[j]);
            }

            cameras.push_back(camera);
        } else {
            updateLastError("ASIGetCameraProperty", result);
            spdlog::error( "Failed to get camera info for index {}: {}", i,
                  lastError_);
        }
    }

    return cameras;
}

bool HardwareInterface::openCamera(const std::string& deviceName) {
    int cameraId = findCameraByName(deviceName);
    if (cameraId < 0) {
        lastError_ = "Camera not found: " + deviceName;
        spdlog::error( "{}", lastError_);
        return false;
    }

    return openCamera(cameraId);
}

bool HardwareInterface::openCamera(int cameraId) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!sdkInitialized_) {
        lastError_ = "SDK not initialized";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (connected_) {
        if (currentCameraId_ == cameraId) {
            spdlog::info( "Camera {} already connected", cameraId);
            return true;
        }
        closeCamera();
    }

    if (!validateCameraId(cameraId)) {
        lastError_ = "Invalid camera ID: " + std::to_string(cameraId);
        spdlog::error( "{}", lastError_);
        return false;
    }

    spdlog::info( "Opening ASI camera with ID: {}", cameraId);

    ASI_ERROR_CODE result = ASIOpenCamera(cameraId);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIOpenCamera", result);
        spdlog::error( "Failed to open camera {}: {}", cameraId, lastError_);
        return false;
    }

    result = ASIInitCamera(cameraId);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIInitCamera", result);
        spdlog::error( "Failed to initialize camera {}: {}", cameraId,
              lastError_);
        ASICloseCamera(cameraId);
        return false;
    }

    currentCameraId_ = cameraId;
    connected_ = true;

    // Load camera information and capabilities
    if (!loadCameraInfo(cameraId) || !loadControlCapabilities()) {
        spdlog::warn( "Failed to load complete camera information");
    }

    spdlog::info( "Successfully opened and initialized camera {}", cameraId);
    return true;
}

bool HardwareInterface::closeCamera() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        return true;
    }

    spdlog::info( "Closing ASI camera with ID: {}", currentCameraId_);

    ASI_ERROR_CODE result = ASICloseCamera(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASICloseCamera", result);
        spdlog::error( "Failed to close camera {}: {}", currentCameraId_,
              lastError_);
        // Continue with cleanup even if close failed
    }

    connected_ = false;
    currentCameraId_ = -1;
    currentDeviceName_.clear();
    currentCameraInfo_.reset();
    controlCapabilities_.clear();

    spdlog::info( "Camera closed successfully");
    return true;
}

std::optional<HardwareInterface::CameraInfo> HardwareInterface::getCameraInfo()
    const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return currentCameraInfo_;
}

std::vector<HardwareInterface::ControlCaps>
HardwareInterface::getControlCapabilities() {
    std::lock_guard<std::mutex> lock(controlMutex_);
    return controlCapabilities_;
}

bool HardwareInterface::setControlValue(ASI_CONTROL_TYPE controlType,
                                        long value, bool isAuto) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!validateControlType(controlType)) {
        lastError_ = "Invalid control type: " +
                     std::to_string(static_cast<int>(controlType));
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_BOOL autoMode = isAuto ? ASI_TRUE : ASI_FALSE;
    ASI_ERROR_CODE result =
        ASISetControlValue(currentCameraId_, controlType, value, autoMode);

    if (result != ASI_SUCCESS) {
        updateLastError("ASISetControlValue", result);
        spdlog::error(
              "Failed to set control value (type: {}, value: {}, auto: {}): {}",
              static_cast<int>(controlType), value, isAuto, lastError_);
        return false;
    }

    spdlog::info( "Set control value (type: {}, value: {}, auto: {})",
          static_cast<int>(controlType), value, isAuto);
    return true;
}

bool HardwareInterface::getControlValue(ASI_CONTROL_TYPE controlType,
                                        long& value, bool& isAuto) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!validateControlType(controlType)) {
        lastError_ = "Invalid control type: " +
                     std::to_string(static_cast<int>(controlType));
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_BOOL autoMode;
    ASI_ERROR_CODE result =
        ASIGetControlValue(currentCameraId_, controlType, &value, &autoMode);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetControlValue", result);
        spdlog::error( "Failed to get control value (type: {}): {}",
              static_cast<int>(controlType), lastError_);
        return false;
    }

    isAuto = (autoMode == ASI_TRUE);
    return true;
}

bool HardwareInterface::hasControl(ASI_CONTROL_TYPE controlType) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    return std::any_of(controlCapabilities_.begin(), controlCapabilities_.end(),
                       [controlType](const ControlCaps& caps) {
                           return caps.controlType == controlType;
                       });
}

bool HardwareInterface::startExposure(int width, int height, int binning,
                                      ASI_IMG_TYPE imageType) {
    return startExposure(width, height, binning, imageType, false);
}

bool HardwareInterface::startExposure(int width, int height, int binning,
                                      ASI_IMG_TYPE imageType,
                                      bool isDarkFrame) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Set ROI format
    ASI_ERROR_CODE result =
        ASISetROIFormat(currentCameraId_, width, height, binning, imageType);
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetROIFormat", result);
        spdlog::error( "Failed to set ROI format: {}", lastError_);
        return false;
    }

    // Start exposure
    ASI_BOOL darkFrame = isDarkFrame ? ASI_TRUE : ASI_FALSE;
    result = ASIStartExposure(currentCameraId_, darkFrame);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStartExposure", result);
        spdlog::error( "Failed to start exposure: {}", lastError_);
        return false;
    }

    spdlog::info( "Started exposure ({}x{}, bin: {}, type: {}, dark: {})", width,
          height, binning, static_cast<int>(imageType), isDarkFrame);
    return true;
}

bool HardwareInterface::stopExposure() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result = ASIStopExposure(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStopExposure", result);
        spdlog::error( "Failed to stop exposure: {}", lastError_);
        return false;
    }

    spdlog::info( "Stopped exposure");
    return true;
}

ASI_EXPOSURE_STATUS HardwareInterface::getExposureStatus() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        return ASI_EXP_FAILED;
    }

    ASI_EXPOSURE_STATUS status;
    ASI_ERROR_CODE result = ASIGetExpStatus(currentCameraId_, &status);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetExpStatus", result);
        spdlog::error( "Failed to get exposure status: {}", lastError_);
        return ASI_EXP_FAILED;
    }

    return status;
}

bool HardwareInterface::getImageData(unsigned char* buffer, long bufferSize) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!buffer) {
        lastError_ = "Invalid buffer pointer";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result =
        ASIGetDataAfterExp(currentCameraId_, buffer, bufferSize);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetDataAfterExp", result);
        spdlog::error( "Failed to get image data: {}", lastError_);
        return false;
    }

    spdlog::info( "Retrieved image data ({} bytes)", bufferSize);
    return true;
}

std::string HardwareInterface::getSDKVersion() {
    const char* version = ASIGetSDKVersion();
    return version ? std::string(version) : "Unknown";
}

std::string HardwareInterface::getDriverVersion() {
    // This would typically be retrieved from the SDK or driver
    return "ASI Driver 1.0.0";
}

// Helper methods implementation

bool HardwareInterface::loadCameraInfo(int cameraId) {
    ASI_CAMERA_INFO asiInfo;
    ASI_ERROR_CODE result = ASIGetCameraPropertyByID(cameraId, &asiInfo);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetCameraPropertyByID", result);
        return false;
    }

    CameraInfo camera;
    camera.cameraId = asiInfo.CameraID;
    camera.name = asiInfo.Name;
    camera.maxWidth = static_cast<int>(asiInfo.MaxWidth);
    camera.maxHeight = static_cast<int>(asiInfo.MaxHeight);
    camera.isColorCamera = (asiInfo.IsColorCam == ASI_TRUE);
    camera.bitDepth = asiInfo.BitDepth;
    camera.pixelSize = asiInfo.PixelSize;
    camera.hasMechanicalShutter = (asiInfo.MechanicalShutter == ASI_TRUE);
    camera.hasST4Port = (asiInfo.ST4Port == ASI_TRUE);
    camera.hasCooler = (asiInfo.IsCoolerCam == ASI_TRUE);
    camera.isUSB3Host = (asiInfo.IsUSB3Host == ASI_TRUE);
    camera.isUSB3Camera = (asiInfo.IsUSB3Camera == ASI_TRUE);
    camera.electronMultiplyGain = asiInfo.ElecPerADU;

    // Parse supported binning modes
    for (int j = 0; j < 16 && asiInfo.SupportedBins[j] != 0; ++j) {
        camera.supportedBins.push_back(asiInfo.SupportedBins[j]);
    }

    // Parse supported video formats
    for (int j = 0; j < 8 && asiInfo.SupportedVideoFormat[j] != ASI_IMG_END;
         ++j) {
        camera.supportedVideoFormats.push_back(asiInfo.SupportedVideoFormat[j]);
    }

    // Get serial number (if available)
    ASI_SN serialNumber;
    if (ASIGetSerialNumber(cameraId, &serialNumber) == ASI_SUCCESS) {
        std::ostringstream oss;
        for (int i = 0; i < 8; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(serialNumber.id[i]);
        }
        camera.serialNumber = oss.str();
    }

    // Set trigger capabilities
    if (asiInfo.IsTriggerCam == ASI_TRUE) {
        camera.triggerCaps =
            "Trigger camera with software and hardware trigger support";
    }

    currentCameraInfo_ = camera;
    currentDeviceName_ = camera.name;

    return true;
}

bool HardwareInterface::loadControlCapabilities() {
    controlCapabilities_.clear();

    int numControls = 0;
    ASI_ERROR_CODE result = ASIGetNumOfControls(currentCameraId_, &numControls);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetNumOfControls", result);
        return false;
    }

    for (int i = 0; i < numControls; ++i) {
        ASI_CONTROL_CAPS asiCaps;
        result = ASIGetControlCaps(currentCameraId_, i, &asiCaps);

        if (result == ASI_SUCCESS) {
            ControlCaps caps;
            caps.name = asiCaps.Name;
            caps.description = asiCaps.Description;
            caps.maxValue = asiCaps.MaxValue;
            caps.minValue = asiCaps.MinValue;
            caps.defaultValue = asiCaps.DefaultValue;
            caps.isAutoSupported = (asiCaps.IsAutoSupported == ASI_TRUE);
            caps.isWritable = (asiCaps.IsWritable == ASI_TRUE);
            caps.controlType = asiCaps.ControlType;

            controlCapabilities_.push_back(caps);
        }
    }

    return true;
}

std::string HardwareInterface::asiErrorToString(ASI_ERROR_CODE error) {
    switch (error) {
        case ASI_SUCCESS:
            return "Success";
        case ASI_ERROR_INVALID_INDEX:
            return "Invalid index";
        case ASI_ERROR_INVALID_ID:
            return "Invalid ID";
        case ASI_ERROR_INVALID_CONTROL_TYPE:
            return "Invalid control type";
        case ASI_ERROR_CAMERA_CLOSED:
            return "Camera closed";
        case ASI_ERROR_CAMERA_REMOVED:
            return "Camera removed";
        case ASI_ERROR_INVALID_PATH:
            return "Invalid path";
        case ASI_ERROR_INVALID_FILEFORMAT:
            return "Invalid file format";
        case ASI_ERROR_INVALID_SIZE:
            return "Invalid size";
        case ASI_ERROR_INVALID_IMGTYPE:
            return "Invalid image type";
        case ASI_ERROR_OUTOF_BOUNDARY:
            return "Out of boundary";
        case ASI_ERROR_TIMEOUT:
            return "Timeout";
        case ASI_ERROR_INVALID_SEQUENCE:
            return "Invalid sequence";
        case ASI_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case ASI_ERROR_VIDEO_MODE_ACTIVE:
            return "Video mode active";
        case ASI_ERROR_EXPOSURE_IN_PROGRESS:
            return "Exposure in progress";
        case ASI_ERROR_GENERAL_ERROR:
            return "General error";
        case ASI_ERROR_INVALID_MODE:
            return "Invalid mode";
        default:
            return "Unknown error";
    }
}

void HardwareInterface::updateLastError(const std::string& operation,
                                        ASI_ERROR_CODE result) {
    std::ostringstream oss;
    oss << operation << " failed: " << asiErrorToString(result) << " ("
        << static_cast<int>(result) << ")";
    lastError_ = oss.str();
}

bool HardwareInterface::validateCameraId(int cameraId) {
    return cameraId >= 0 && cameraId < ASIGetNumOfConnectedCameras();
}

bool HardwareInterface::validateControlType(ASI_CONTROL_TYPE controlType) {
    return controlType >= ASI_GAIN && controlType <= ASI_ROLLING_INTERVAL;
}

int HardwareInterface::findCameraByName(const std::string& name) {
    int numCameras = ASIGetNumOfConnectedCameras();

    for (int i = 0; i < numCameras; ++i) {
        ASI_CAMERA_INFO cameraInfo;
        ASI_ERROR_CODE result = ASIGetCameraProperty(&cameraInfo, i);

        if (result == ASI_SUCCESS && std::string(cameraInfo.Name) == name) {
            return cameraInfo.CameraID;
        }
    }

    return -1;
}

// Video Capture Operations
bool HardwareInterface::startVideoCapture() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    spdlog::info( "Starting video capture for camera {}", currentCameraId_);

    ASI_ERROR_CODE result = ASIStartVideoCapture(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStartVideoCapture", result);
        spdlog::error( "Failed to start video capture: {}", lastError_);
        return false;
    }

    spdlog::info( "Video capture started successfully");
    return true;
}

bool HardwareInterface::stopVideoCapture() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    spdlog::info( "Stopping video capture for camera {}", currentCameraId_);

    ASI_ERROR_CODE result = ASIStopVideoCapture(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStopVideoCapture", result);
        spdlog::error( "Failed to stop video capture: {}", lastError_);
        return false;
    }

    spdlog::info( "Video capture stopped successfully");
    return true;
}

bool HardwareInterface::getVideoData(unsigned char* buffer, long bufferSize,
                                     int waitMs) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!buffer) {
        lastError_ = "Invalid buffer pointer";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result =
        ASIGetVideoData(currentCameraId_, buffer, bufferSize, waitMs);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetVideoData", result);
        spdlog::error( "Failed to get video data: {}", lastError_);
        return false;
    }

    spdlog::info( "Retrieved video data ({} bytes)", bufferSize);
    return true;
}

// ROI and Binning
bool HardwareInterface::setROI(int startX, int startY, int width, int height,
                               int binning) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Set ROI start position
    ASI_ERROR_CODE result = ASISetStartPos(currentCameraId_, startX, startY);
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetStartPos", result);
        spdlog::error( "Failed to set ROI start position: {}", lastError_);
        return false;
    }

    spdlog::info( "Set ROI: start({}, {}), size({}x{}), binning: {}", startX,
          startY, width, height, binning);
    return true;
}

bool HardwareInterface::getROI(int& startX, int& startY, int& width,
                               int& height, int& binning) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Get ROI start position
    ASI_ERROR_CODE result = ASIGetStartPos(currentCameraId_, &startX, &startY);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetStartPos", result);
        spdlog::error( "Failed to get ROI start position: {}", lastError_);
        return false;
    }

    // Get ROI format
    ASI_IMG_TYPE imageType;
    result = ASIGetROIFormat(currentCameraId_, &width, &height, &binning,
                             &imageType);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetROIFormat", result);
        spdlog::error( "Failed to get ROI format: {}", lastError_);
        return false;
    }

    return true;
}

// Image Format
bool HardwareInterface::setImageFormat(int width, int height, int binning,
                                       ASI_IMG_TYPE imageType) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result =
        ASISetROIFormat(currentCameraId_, width, height, binning, imageType);
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetROIFormat", result);
        spdlog::error( "Failed to set image format: {}", lastError_);
        return false;
    }

    spdlog::info( "Set image format: {}x{}, binning: {}, type: {}", width, height,
          binning, static_cast<int>(imageType));
    return true;
}

ASI_IMG_TYPE HardwareInterface::getImageFormat() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        return ASI_IMG_END;
    }

    int width, height, binning;
    ASI_IMG_TYPE imageType;
    ASI_ERROR_CODE result = ASIGetROIFormat(currentCameraId_, &width, &height,
                                            &binning, &imageType);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetROIFormat", result);
        spdlog::error( "Failed to get image format: {}", lastError_);
        return ASI_IMG_END;
    }

    return imageType;
}

// Camera Modes
bool HardwareInterface::setCameraMode(ASI_CAMERA_MODE mode) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Check if camera supports trigger modes
    if (!currentCameraInfo_ || !currentCameraInfo_->triggerCaps.empty()) {
        ASI_ERROR_CODE result = ASISetCameraMode(currentCameraId_, mode);
        if (result != ASI_SUCCESS) {
            updateLastError("ASISetCameraMode", result);
            spdlog::error( "Failed to set camera mode: {}", lastError_);
            return false;
        }

        spdlog::info( "Set camera mode: {}", static_cast<int>(mode));
        return true;
    } else {
        lastError_ = "Camera does not support trigger modes";
        spdlog::error( "{}", lastError_);
        return false;
    }
}

ASI_CAMERA_MODE HardwareInterface::getCameraMode() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        return ASI_MODE_END;
    }

    ASI_CAMERA_MODE mode;
    ASI_ERROR_CODE result = ASIGetCameraMode(currentCameraId_, &mode);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetCameraMode", result);
        spdlog::error( "Failed to get camera mode: {}", lastError_);
        return ASI_MODE_END;
    }

    return mode;
}

// Guiding Support (ST4)
bool HardwareInterface::pulseGuide(ASI_GUIDE_DIRECTION direction,
                                   int durationMs) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Check if camera has ST4 port
    if (!currentCameraInfo_ || !currentCameraInfo_->hasST4Port) {
        lastError_ = "Camera does not have ST4 port";
        spdlog::error( "{}", lastError_);
        return false;
    }

    spdlog::info( "Starting pulse guide: direction {}, duration {}ms",
          static_cast<int>(direction), durationMs);

    // Start pulse guide
    ASI_ERROR_CODE result = ASIPulseGuideOn(currentCameraId_, direction);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIPulseGuideOn", result);
        spdlog::error( "Failed to start pulse guide: {}", lastError_);
        return false;
    }

    // Wait for the specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));

    // Stop pulse guide
    result = ASIPulseGuideOff(currentCameraId_, direction);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIPulseGuideOff", result);
        spdlog::error( "Failed to stop pulse guide: {}", lastError_);
        return false;
    }

    spdlog::info( "Pulse guide completed successfully");
    return true;
}

bool HardwareInterface::stopGuide() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Check if camera has ST4 port
    if (!currentCameraInfo_ || !currentCameraInfo_->hasST4Port) {
        lastError_ = "Camera does not have ST4 port";
        spdlog::error( "{}", lastError_);
        return false;
    }

    spdlog::info( "Stopping all guide directions");

    // Stop all guide directions
    ASI_ERROR_CODE result;
    bool success = true;

    for (int dir = ASI_GUIDE_NORTH; dir <= ASI_GUIDE_WEST; ++dir) {
        result = ASIPulseGuideOff(currentCameraId_,
                                  static_cast<ASI_GUIDE_DIRECTION>(dir));
        if (result != ASI_SUCCESS) {
            updateLastError("ASIPulseGuideOff", result);
            spdlog::warn( "Failed to stop guide direction {}: {}", dir,
                  lastError_);
            success = false;
        }
    }

    if (success) {
        spdlog::info( "All guide directions stopped successfully");
    }
    return success;
}

// Advanced Features
bool HardwareInterface::setFlipStatus(ASI_FLIP_STATUS flipStatus) {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result = ASISetControlValue(
        currentCameraId_, ASI_FLIP, static_cast<long>(flipStatus), ASI_FALSE);
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetControlValue(ASI_FLIP)", result);
        spdlog::error( "Failed to set flip status: {}", lastError_);
        return false;
    }

    spdlog::info( "Set flip status: {}", static_cast<int>(flipStatus));
    return true;
}

ASI_FLIP_STATUS HardwareInterface::getFlipStatus() {
    std::lock_guard<std::mutex> lock(controlMutex_);

    if (!connected_) {
        return ASI_FLIP_NONE;
    }

    long value;
    ASI_BOOL isAuto;
    ASI_ERROR_CODE result =
        ASIGetControlValue(currentCameraId_, ASI_FLIP, &value, &isAuto);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetControlValue(ASI_FLIP)", result);
        spdlog::error( "Failed to get flip status: {}", lastError_);
        return ASI_FLIP_NONE;
    }

    return static_cast<ASI_FLIP_STATUS>(value);
}

// GPS Support
bool HardwareInterface::getGPSData(ASI_GPS_DATA& startLineGPS,
                                   ASI_GPS_DATA& endLineGPS) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result =
        ASIGPSGetData(currentCameraId_, &startLineGPS, &endLineGPS);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGPSGetData", result);
        spdlog::error( "Failed to get GPS data: {}", lastError_);
        return false;
    }

    spdlog::info( "Retrieved GPS data successfully");
    return true;
}

bool HardwareInterface::getVideoDataWithGPS(unsigned char* buffer,
                                            long bufferSize, int waitMs,
                                            ASI_GPS_DATA& gpsData) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!buffer) {
        lastError_ = "Invalid buffer pointer";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result = ASIGetVideoDataGPS(currentCameraId_, buffer,
                                               bufferSize, waitMs, &gpsData);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetVideoDataGPS", result);
        spdlog::error( "Failed to get video data with GPS: {}", lastError_);
        return false;
    }

    spdlog::info( "Retrieved video data with GPS ({} bytes)", bufferSize);
    return true;
}

bool HardwareInterface::getImageDataWithGPS(unsigned char* buffer,
                                            long bufferSize,
                                            ASI_GPS_DATA& gpsData) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    if (!buffer) {
        lastError_ = "Invalid buffer pointer";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_ERROR_CODE result =
        ASIGetDataAfterExpGPS(currentCameraId_, buffer, bufferSize, &gpsData);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetDataAfterExpGPS", result);
        spdlog::error( "Failed to get image data with GPS: {}", lastError_);
        return false;
    }

    spdlog::info( "Retrieved image data with GPS ({} bytes)", bufferSize);
    return true;
}

// Serial Number Support
std::string HardwareInterface::getSerialNumber() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        return "";
    }

    ASI_SN serialNumber;
    ASI_ERROR_CODE result = ASIGetSerialNumber(currentCameraId_, &serialNumber);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetSerialNumber", result);
        spdlog::warn( "Failed to get serial number: {}", lastError_);
        return "";
    }

    // Convert ASI_SN to hex string
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(serialNumber.id[i]);
    }

    return oss.str();
}

// Trigger Camera Support
bool HardwareInterface::getSupportedCameraModes(
    std::vector<ASI_CAMERA_MODE>& modes) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    // Check if camera supports trigger modes
    if (!currentCameraInfo_ || currentCameraInfo_->triggerCaps.empty()) {
        lastError_ = "Camera does not support trigger modes";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_SUPPORTED_MODE supportedMode;
    ASI_ERROR_CODE result =
        ASIGetCameraSupportMode(currentCameraId_, &supportedMode);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetCameraSupportMode", result);
        spdlog::error( "Failed to get supported camera modes: {}", lastError_);
        return false;
    }

    modes.clear();
    for (int i = 0;
         i < 16 && supportedMode.SupportedCameraMode[i] != ASI_MODE_END; ++i) {
        modes.push_back(supportedMode.SupportedCameraMode[i]);
    }

    return true;
}

bool HardwareInterface::sendSoftTrigger(bool start) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_BOOL triggerState = start ? ASI_TRUE : ASI_FALSE;
    ASI_ERROR_CODE result = ASISendSoftTrigger(currentCameraId_, triggerState);

    if (result != ASI_SUCCESS) {
        updateLastError("ASISendSoftTrigger", result);
        spdlog::error( "Failed to send soft trigger: {}", lastError_);
        return false;
    }

    spdlog::info( "Sent soft trigger: {}", start ? "start" : "stop");
    return true;
}

bool HardwareInterface::setTriggerOutputConfig(ASI_TRIG_OUTPUT_PIN pin,
                                               bool pinHigh, long delayUs,
                                               long durationUs) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_BOOL pinHighState = pinHigh ? ASI_TRUE : ASI_FALSE;
    ASI_ERROR_CODE result = ASISetTriggerOutputIOConf(
        currentCameraId_, pin, pinHighState, delayUs, durationUs);

    if (result != ASI_SUCCESS) {
        updateLastError("ASISetTriggerOutputIOConf", result);
        spdlog::error( "Failed to set trigger output config: {}", lastError_);
        return false;
    }

    spdlog::info(
          "Set trigger output config: pin {}, high: {}, delay: {}us, duration: "
          "{}us",
          static_cast<int>(pin), pinHigh, delayUs, durationUs);
    return true;
}

bool HardwareInterface::getTriggerOutputConfig(ASI_TRIG_OUTPUT_PIN pin,
                                               bool& pinHigh, long& delayUs,
                                               long& durationUs) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!connected_) {
        lastError_ = "Camera not connected";
        spdlog::error( "{}", lastError_);
        return false;
    }

    ASI_BOOL pinHighState;
    ASI_ERROR_CODE result = ASIGetTriggerOutputIOConf(
        currentCameraId_, pin, &pinHighState, &delayUs, &durationUs);

    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetTriggerOutputIOConf", result);
        spdlog::error( "Failed to get trigger output config: {}", lastError_);
        return false;
    }

    pinHigh = (pinHighState == ASI_TRUE);
    return true;
}

}  // namespace lithium::device::asi::camera::components