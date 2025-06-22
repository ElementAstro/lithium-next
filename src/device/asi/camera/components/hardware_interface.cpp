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
#include "atom/log/loguru.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>

#ifdef LITHIUM_ASI_CAMERA_ENABLED
// Stub implementations for SDK functions when not available
extern "C" {
    int ASIGetNumOfConnectedCameras() { return 0; }
    ASI_ERROR_CODE ASIGetCameraProperty(ASI_CAMERA_INFO*, int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetCameraPropertyByID(int, ASI_CAMERA_INFO*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIOpenCamera(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIInitCamera(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASICloseCamera(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetNumOfControls(int, int*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetControlCaps(int, int, ASI_CONTROL_CAPS*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetControlValue(int, ASI_CONTROL_TYPE, long*, ASI_BOOL*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetControlValue(int, ASI_CONTROL_TYPE, long, ASI_BOOL) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetROIFormat(int, int, int, int, ASI_IMG_TYPE) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetROIFormat(int, int*, int*, int*, ASI_IMG_TYPE*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetStartPos(int, int, int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetStartPos(int, int*, int*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetDroppedFrames(int, int*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStartExposure(int, ASI_BOOL) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStopExposure(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetExpStatus(int, ASI_EXPOSURE_STATUS*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetDataAfterExp(int, unsigned char*, long) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetID(int, ASI_ID*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetID(int, ASI_ID) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetGainOffset(int, int*, int*, int*, int*) { return ASI_ERROR_GENERAL_ERROR; }
    const char* ASIGetSDKVersion() { return "Stub 1.0.0"; }
    ASI_ERROR_CODE ASIGetCameraSupportMode(int, ASI_CAMERA_MODE*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetCameraMode(int, ASI_CAMERA_MODE*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetCameraMode(int, ASI_CAMERA_MODE) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISendSoftTrigger(int, ASI_BOOL) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStartVideoCapture(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStopVideoCapture(int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetVideoData(int, unsigned char*, long, int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIPulseGuideOn(int, ASI_GUIDE_DIRECTION) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIPulseGuideOff(int, ASI_GUIDE_DIRECTION) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStartGuide(int, ASI_GUIDE_DIRECTION, int) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIStopGuide(int, ASI_GUIDE_DIRECTION) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetSerialNumber(int, ASI_ID*) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASISetTriggerOutputIOConf(int, int, ASI_BOOL, long, long) { return ASI_ERROR_GENERAL_ERROR; }
    ASI_ERROR_CODE ASIGetTriggerOutputIOConf(int, int, ASI_BOOL*, long*, long*) { return ASI_ERROR_GENERAL_ERROR; }
}
#endif

namespace lithium::device::asi::camera::components {

HardwareInterface::HardwareInterface() {
    LOG_F(INFO, "ASI Camera HardwareInterface initialized");
}

HardwareInterface::~HardwareInterface() {
    if (connected_) {
        closeCamera();
    }
    if (sdkInitialized_) {
        shutdownSDK();
    }
    LOG_F(INFO, "ASI Camera HardwareInterface destroyed");
}

bool HardwareInterface::initializeSDK() {
    std::lock_guard<std::mutex> lock(sdkMutex_);
    
    if (sdkInitialized_) {
        LOG_F(WARNING, "ASI SDK already initialized");
        return true;
    }

    LOG_F(INFO, "Initializing ASI Camera SDK");
    
    // In a real implementation, this would initialize the ASI SDK
    // For now, we simulate successful initialization
    sdkInitialized_ = true;
    
    LOG_F(INFO, "ASI Camera SDK initialized successfully");
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

    LOG_F(INFO, "Shutting down ASI Camera SDK");
    
    // In a real implementation, this would cleanup the ASI SDK
    sdkInitialized_ = false;
    
    LOG_F(INFO, "ASI Camera SDK shutdown complete");
    return true;
}

std::vector<std::string> HardwareInterface::enumerateDevices() {
    std::lock_guard<std::mutex> lock(sdkMutex_);
    
    if (!sdkInitialized_) {
        LOG_F(ERROR, "SDK not initialized");
        return {};
    }

    std::vector<std::string> deviceNames;
    
    int numCameras = ASIGetNumOfConnectedCameras();
    LOG_F(INFO, "Found {} ASI cameras", numCameras);
    
    for (int i = 0; i < numCameras; ++i) {
        ASI_CAMERA_INFO cameraInfo;
        ASI_ERROR_CODE result = ASIGetCameraProperty(&cameraInfo, i);
        
        if (result == ASI_SUCCESS) {
            deviceNames.emplace_back(cameraInfo.Name);
            LOG_F(INFO, "Found camera: {} (ID: {})", cameraInfo.Name, cameraInfo.CameraID);
        } else {
            updateLastError("ASIGetCameraProperty", result);
            LOG_F(ERROR, "Failed to get camera property for index {}: {}", i, lastError_);
        }
    }
    
    return deviceNames;
}

std::vector<HardwareInterface::CameraInfo> HardwareInterface::getAvailableCameras() {
    std::lock_guard<std::mutex> lock(sdkMutex_);
    
    if (!sdkInitialized_) {
        LOG_F(ERROR, "SDK not initialized");
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
            camera.hasMechanicalShutter = (asiInfo.MechanicalShutter == ASI_TRUE);
            camera.hasST4Port = (asiInfo.ST4Port == ASI_TRUE);
            camera.hasCooler = (asiInfo.IsCoolerCam == ASI_TRUE);
            camera.isUSB3Host = (asiInfo.IsUSB3HOST == ASI_TRUE);
            camera.isUSB3Camera = (asiInfo.IsUSB3Camera == ASI_TRUE);
            camera.electronMultiplyGain = asiInfo.ElecPerADU;
            
            // Parse supported binning modes
            for (int j = 0; j < 16 && asiInfo.SupportedBins[j] != 0; ++j) {
                camera.supportedBins.push_back(asiInfo.SupportedBins[j]);
            }
            
            // Parse supported video formats
            for (int j = 0; j < 8 && asiInfo.SupportedVideoFormat[j] != ASI_IMG_END; ++j) {
                camera.supportedVideoFormats.push_back(asiInfo.SupportedVideoFormat[j]);
            }
            
            cameras.push_back(camera);
        } else {
            updateLastError("ASIGetCameraProperty", result);
            LOG_F(ERROR, "Failed to get camera info for index {}: {}", i, lastError_);
        }
    }
    
    return cameras;
}

bool HardwareInterface::openCamera(const std::string& deviceName) {
    int cameraId = findCameraByName(deviceName);
    if (cameraId < 0) {
        lastError_ = "Camera not found: " + deviceName;
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    return openCamera(cameraId);
}

bool HardwareInterface::openCamera(int cameraId) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!sdkInitialized_) {
        lastError_ = "SDK not initialized";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    if (connected_) {
        if (currentCameraId_ == cameraId) {
            LOG_F(INFO, "Camera {} already connected", cameraId);
            return true;
        }
        closeCamera();
    }
    
    if (!validateCameraId(cameraId)) {
        lastError_ = "Invalid camera ID: " + std::to_string(cameraId);
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    LOG_F(INFO, "Opening ASI camera with ID: {}", cameraId);
    
    ASI_ERROR_CODE result = ASIOpenCamera(cameraId);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIOpenCamera", result);
        LOG_F(ERROR, "Failed to open camera {}: {}", cameraId, lastError_);
        return false;
    }
    
    result = ASIInitCamera(cameraId);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIInitCamera", result);
        LOG_F(ERROR, "Failed to initialize camera {}: {}", cameraId, lastError_);
        ASICloseCamera(cameraId);
        return false;
    }
    
    currentCameraId_ = cameraId;
    connected_ = true;
    
    // Load camera information and capabilities
    if (!loadCameraInfo(cameraId) || !loadControlCapabilities()) {
        LOG_F(WARNING, "Failed to load complete camera information");
    }
    
    LOG_F(INFO, "Successfully opened and initialized camera {}", cameraId);
    return true;
}

bool HardwareInterface::closeCamera() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!connected_) {
        return true;
    }
    
    LOG_F(INFO, "Closing ASI camera with ID: {}", currentCameraId_);
    
    ASI_ERROR_CODE result = ASICloseCamera(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASICloseCamera", result);
        LOG_F(ERROR, "Failed to close camera {}: {}", currentCameraId_, lastError_);
        // Continue with cleanup even if close failed
    }
    
    connected_ = false;
    currentCameraId_ = -1;
    currentDeviceName_.clear();
    currentCameraInfo_.reset();
    controlCapabilities_.clear();
    
    LOG_F(INFO, "Camera closed successfully");
    return true;
}

std::optional<HardwareInterface::CameraInfo> HardwareInterface::getCameraInfo() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return currentCameraInfo_;
}

std::vector<HardwareInterface::ControlCaps> HardwareInterface::getControlCapabilities() {
    std::lock_guard<std::mutex> lock(controlMutex_);
    return controlCapabilities_;
}

bool HardwareInterface::setControlValue(ASI_CONTROL_TYPE controlType, long value, bool isAuto) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    
    if (!connected_) {
        lastError_ = "Camera not connected";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    if (!validateControlType(controlType)) {
        lastError_ = "Invalid control type: " + std::to_string(static_cast<int>(controlType));
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    ASI_BOOL autoMode = isAuto ? ASI_TRUE : ASI_FALSE;
    ASI_ERROR_CODE result = ASISetControlValue(currentCameraId_, controlType, value, autoMode);
    
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetControlValue", result);
        LOG_F(ERROR, "Failed to set control value (type: {}, value: {}, auto: {}): {}", 
              static_cast<int>(controlType), value, isAuto, lastError_);
        return false;
    }
    
    LOG_F(INFO, "Set control value (type: {}, value: {}, auto: {})", 
          static_cast<int>(controlType), value, isAuto);
    return true;
}

bool HardwareInterface::getControlValue(ASI_CONTROL_TYPE controlType, long& value, bool& isAuto) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    
    if (!connected_) {
        lastError_ = "Camera not connected";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    if (!validateControlType(controlType)) {
        lastError_ = "Invalid control type: " + std::to_string(static_cast<int>(controlType));
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    ASI_BOOL autoMode;
    ASI_ERROR_CODE result = ASIGetControlValue(currentCameraId_, controlType, &value, &autoMode);
    
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetControlValue", result);
        LOG_F(ERROR, "Failed to get control value (type: {}): {}", 
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

bool HardwareInterface::startExposure(int width, int height, int binning, ASI_IMG_TYPE imageType) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!connected_) {
        lastError_ = "Camera not connected";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    // Set ROI format
    ASI_ERROR_CODE result = ASISetROIFormat(currentCameraId_, width, height, binning, imageType);
    if (result != ASI_SUCCESS) {
        updateLastError("ASISetROIFormat", result);
        LOG_F(ERROR, "Failed to set ROI format: {}", lastError_);
        return false;
    }
    
    // Start exposure
    result = ASIStartExposure(currentCameraId_, ASI_FALSE);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStartExposure", result);
        LOG_F(ERROR, "Failed to start exposure: {}", lastError_);
        return false;
    }
    
    LOG_F(INFO, "Started exposure ({}x{}, bin: {}, type: {})", width, height, binning, static_cast<int>(imageType));
    return true;
}

bool HardwareInterface::stopExposure() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!connected_) {
        lastError_ = "Camera not connected";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    ASI_ERROR_CODE result = ASIStopExposure(currentCameraId_);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIStopExposure", result);
        LOG_F(ERROR, "Failed to stop exposure: {}", lastError_);
        return false;
    }
    
    LOG_F(INFO, "Stopped exposure");
    return true;
}

ASI_EXPOSURE_STATUS HardwareInterface::getExposureStatus() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!connected_) {
        return ASI_EXPOSURE_FAILED;
    }
    
    ASI_EXPOSURE_STATUS status;
    ASI_ERROR_CODE result = ASIGetExpStatus(currentCameraId_, &status);
    
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetExpStatus", result);
        LOG_F(ERROR, "Failed to get exposure status: {}", lastError_);
        return ASI_EXPOSURE_FAILED;
    }
    
    return status;
}

bool HardwareInterface::getImageData(unsigned char* buffer, long bufferSize) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    if (!connected_) {
        lastError_ = "Camera not connected";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    if (!buffer) {
        lastError_ = "Invalid buffer pointer";
        LOG_F(ERROR, "{}", lastError_);
        return false;
    }
    
    ASI_ERROR_CODE result = ASIGetDataAfterExp(currentCameraId_, buffer, bufferSize);
    if (result != ASI_SUCCESS) {
        updateLastError("ASIGetDataAfterExp", result);
        LOG_F(ERROR, "Failed to get image data: {}", lastError_);
        return false;
    }
    
    LOG_F(INFO, "Retrieved image data ({} bytes)", bufferSize);
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
    camera.isUSB3Host = (asiInfo.IsUSB3HOST == ASI_TRUE);
    camera.isUSB3Camera = (asiInfo.IsUSB3Camera == ASI_TRUE);
    camera.electronMultiplyGain = asiInfo.ElecPerADU;
    
    // Parse supported binning modes
    for (int j = 0; j < 16 && asiInfo.SupportedBins[j] != 0; ++j) {
        camera.supportedBins.push_back(asiInfo.SupportedBins[j]);
    }
    
    // Parse supported video formats
    for (int j = 0; j < 8 && asiInfo.SupportedVideoFormat[j] != ASI_IMG_END; ++j) {
        camera.supportedVideoFormats.push_back(asiInfo.SupportedVideoFormat[j]);
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
        case ASI_SUCCESS: return "Success";
        case ASI_ERROR_INVALID_INDEX: return "Invalid index";
        case ASI_ERROR_INVALID_ID: return "Invalid ID";
        case ASI_ERROR_INVALID_CONTROL_TYPE: return "Invalid control type";
        case ASI_ERROR_CAMERA_CLOSED: return "Camera closed";
        case ASI_ERROR_CAMERA_REMOVED: return "Camera removed";
        case ASI_ERROR_INVALID_PATH: return "Invalid path";
        case ASI_ERROR_INVALID_FILEFORMAT: return "Invalid file format";
        case ASI_ERROR_INVALID_SIZE: return "Invalid size";
        case ASI_ERROR_INVALID_IMGTYPE: return "Invalid image type";
        case ASI_ERROR_OUTOF_BOUNDARY: return "Out of boundary";
        case ASI_ERROR_TIMEOUT: return "Timeout";
        case ASI_ERROR_INVALID_SEQUENCE: return "Invalid sequence";
        case ASI_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case ASI_ERROR_VIDEO_MODE_ACTIVE: return "Video mode active";
        case ASI_ERROR_EXPOSURE_IN_PROGRESS: return "Exposure in progress";
        case ASI_ERROR_GENERAL_ERROR: return "General error";
        case ASI_ERROR_INVALID_MODE: return "Invalid mode";
        default: return "Unknown error";
    }
}

void HardwareInterface::updateLastError(const std::string& operation, ASI_ERROR_CODE result) {
    std::ostringstream oss;
    oss << operation << " failed: " << asiErrorToString(result) << " (" << static_cast<int>(result) << ")";
    lastError_ = oss.str();
}

bool HardwareInterface::validateCameraId(int cameraId) {
    return cameraId >= 0 && cameraId < ASIGetNumOfConnectedCameras();
}

bool HardwareInterface::validateControlType(ASI_CONTROL_TYPE controlType) {
    return controlType >= ASI_GAIN && controlType < ASI_CONTROL_TYPE_END;
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

} // namespace lithium::device::asi::camera::components
