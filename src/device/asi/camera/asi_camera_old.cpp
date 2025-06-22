/*
 * asi_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ZWO ASI Camera Implementation with full SDK integration

*************************************************/

#include "asi_camera.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

// ASI SDK includes
extern "C" {
    #include "ASICamera2.h"
}

namespace lithium::device::asi::camera {

namespace {
    // ASI SDK error handling
    constexpr int ASI_SUCCESS = ASI_SUCCESS;
    constexpr int ASI_ERROR_INVALID_INDEX = ASI_ERROR_INVALID_INDEX;
    constexpr int ASI_ERROR_INVALID_ID = ASI_ERROR_INVALID_ID;
    
    // Default values
    constexpr double DEFAULT_PIXEL_SIZE = 3.75; // microns
    constexpr int DEFAULT_BIT_DEPTH = 16;
    constexpr double MIN_EXPOSURE_TIME = 0.000032; // 32 microseconds
    constexpr double MAX_EXPOSURE_TIME = 1000.0; // 1000 seconds
    constexpr int DEFAULT_USB_BANDWIDTH = 40;
    constexpr double DEFAULT_TARGET_TEMP = -10.0; // Celsius
    
    // Video formats
    const std::vector<std::string> SUPPORTED_VIDEO_FORMATS = {
        "RAW8", "RAW16", "RGB24", "MONO8", "MONO16"
    };
    
    // Image formats
    const std::vector<std::string> SUPPORTED_IMAGE_FORMATS = {
        "FITS", "TIFF", "PNG", "JPEG", "RAW"
    };
    
    // Camera modes
    const std::vector<std::string> CAMERA_MODES = {
        "NORMAL", "HIGH_SPEED", "SLOW_MODE"
    };
}

ASICamera::ASICamera(const std::string& name)
    : AtomCamera(name)
    , camera_id_(-1)
    , camera_info_(nullptr)
    , camera_model_("")
    , serial_number_("")
    , firmware_version_("")
    , is_connected_(false)
    , is_initialized_(false)
    , is_exposing_(false)
    , exposure_abort_requested_(false)
    , current_exposure_duration_(1.0)
    , is_video_running_(false)
    , is_video_recording_(false)
    , video_recording_file_("")
    , video_exposure_(0.033)
    , video_gain_(0)
    , cooler_enabled_(false)
    , target_temperature_(DEFAULT_TARGET_TEMP)
    , sequence_running_(false)
    , sequence_current_frame_(0)
    , sequence_total_frames_(0)
    , sequence_exposure_(1.0)
    , sequence_interval_(0.0)
    , current_gain_(0)
    , current_offset_(0)
    , current_iso_(100)
    , usb_bandwidth_(DEFAULT_USB_BANDWIDTH)
    , auto_exposure_enabled_(false)
    , auto_gain_enabled_(false)
    , auto_wb_enabled_(false)
    , high_speed_mode_(false)
    , flip_mode_(0)
    , current_mode_("NORMAL")
    , roi_x_(0)
    , roi_y_(0)
    , roi_width_(0)
    , roi_height_(0)
    , bin_x_(1)
    , bin_y_(1)
    , max_width_(0)
    , max_height_(0)
    , pixel_size_x_(DEFAULT_PIXEL_SIZE)
    , pixel_size_y_(DEFAULT_PIXEL_SIZE)
    , bit_depth_(DEFAULT_BIT_DEPTH)
    , bayer_pattern_(BayerPattern::MONO)
    , is_color_camera_(false)
    , total_frames_(0)
    , dropped_frames_(0)
    , has_eaf_focuser_(false)
    , eaf_focuser_connected_(false)
    , eaf_focuser_id_(0)
    , eaf_focuser_position_(0)
    , eaf_focuser_max_position_(10000)
    , eaf_focuser_step_size_(1)
    , eaf_focuser_moving_(false)
    , eaf_backlash_compensation_(false)
    , eaf_backlash_steps_(0)
    , efw_filter_wheel_connected_(false)
    , efw_filter_wheel_id_(0)
    , efw_current_position_(0)
    , efw_filter_count_(0)
    , efw_unidirectional_mode_(false)
{
    LOG_F(INFO, "ASICamera constructor: Creating camera instance '{}'", name);
    
    // Set camera type and capabilities
    setCameraType(CameraType::PRIMARY);
    
    // Initialize capabilities
    CameraCapabilities caps;
    caps.canAbort = true;
    caps.canSubFrame = true;
    caps.canBin = true;
    caps.hasCooler = true;
    caps.hasGuideHead = false;
    caps.hasShutter = false; // Most ASI cameras don't have mechanical shutter
    caps.hasFilters = false;
    caps.hasBayer = true;
    caps.canStream = true;
    caps.hasGain = true;
    caps.hasOffset = true;
    caps.hasTemperature = true;
    caps.canRecordVideo = true;
    caps.supportsSequences = true;
    caps.hasImageQualityAnalysis = true;
    caps.supportsCompression = false;
    caps.hasAdvancedControls = true;
    caps.supportsBurstMode = true;
    caps.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF, ImageFormat::PNG, ImageFormat::JPEG, ImageFormat::RAW};
    caps.supportedVideoFormats = SUPPORTED_VIDEO_FORMATS;
    
    setCameraCapabilities(caps);
    
    // Initialize frame info
    current_frame_ = std::make_shared<AtomCameraFrame>();
}

ASICamera::~ASICamera() {
    LOG_F(INFO, "ASICamera destructor: Destroying camera instance");
    
    if (isConnected()) {
        disconnect();
    }
    
    if (is_initialized_) {
        destroy();
    }
}

auto ASICamera::initialize() -> bool {
    LOG_F(INFO, "ASICamera::initialize: Initializing ASI camera");
    
    if (is_initialized_) {
        LOG_F(WARNING, "ASICamera already initialized");
        return true;
    }
    
    if (!initializeASISDK()) {
        LOG_F(ERROR, "Failed to initialize ASI SDK");
        return false;
    }
    
    is_initialized_ = true;
    setState(DeviceState::IDLE);
    
    LOG_F(INFO, "ASICamera initialization successful");
    return true;
}

auto ASICamera::destroy() -> bool {
    LOG_F(INFO, "ASICamera::destroy: Shutting down ASI camera");
    
    if (!is_initialized_) {
        return true;
    }
    
    // Stop all running operations
    if (is_exposing_) {
        abortExposure();
    }
    
    if (is_video_running_) {
        stopVideo();
    }
    
    if (sequence_running_) {
        stopSequence();
    }
    
    // Disconnect if connected
    if (isConnected()) {
        disconnect();
    }
    
    // Shutdown SDK
    shutdownASISDK();
    
    is_initialized_ = false;
    setState(DeviceState::UNKNOWN);
    
    LOG_F(INFO, "ASICamera shutdown complete");
    return true;
}

auto ASICamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    LOG_F(INFO, "ASICamera::connect: Connecting to camera '{}'", deviceName.empty() ? "auto" : deviceName);
    
    if (!is_initialized_) {
        LOG_F(ERROR, "Camera not initialized");
        return false;
    }
    
    if (isConnected()) {
        LOG_F(WARNING, "Camera already connected");
        return true;
    }
    
    std::lock_guard<std::mutex> lock(camera_mutex_);
    
    int targetCameraId = -1;
    if (deviceName.empty()) {
        // Auto-detect first available camera
        auto cameras = scan();
        if (cameras.empty()) {
            LOG_F(ERROR, "No ASI cameras found");
            return false;
        }
        targetCameraId = 0; // Use first camera
    } else {
        // Find camera by name/ID
        try {
            targetCameraId = std::stoi(deviceName);
        } catch (const std::exception&) {
            LOG_F(ERROR, "Invalid camera ID: {}", deviceName);
            return false;
        }
    }
    
    // Attempt connection with retries
    for (int attempt = 0; attempt < maxRetry; ++attempt) {
        LOG_F(INFO, "Connection attempt {} of {}", attempt + 1, maxRetry);
        
        if (openCamera(targetCameraId)) {
            camera_id_ = targetCameraId;
            
            // Setup camera parameters and read capabilities
            if (setupCameraParameters() && readCameraCapabilities()) {
                is_connected_ = true;
                setState(DeviceState::IDLE);
                
                // Start temperature monitoring thread
                if (hasCooler()) {
                    temperature_thread_ = std::thread(&ASICamera::temperatureThreadFunction, this);
                }
                
                LOG_F(INFO, "Successfully connected to ASI camera ID: {}", camera_id_);
                return true;
            } else {
                closeCamera();
                LOG_F(WARNING, "Failed to setup camera parameters on attempt {}", attempt + 1);
            }
        }
        
        if (attempt < maxRetry - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    LOG_F(ERROR, "Failed to connect to ASI camera after {} attempts", maxRetry);
    return false;
}

auto ASICamera::disconnect() -> bool {
    LOG_F(INFO, "ASICamera::disconnect: Disconnecting camera");
    
    if (!isConnected()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(camera_mutex_);
    
    // Stop all operations
    if (is_exposing_) {
        abortExposure();
    }
    
    if (is_video_running_) {
        stopVideo();
    }
    
    if (sequence_running_) {
        stopSequence();
    }
    
    // Stop temperature thread
    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }
    
    // Close camera
    closeCamera();
    
    is_connected_ = false;
    setState(DeviceState::UNKNOWN);
    
    LOG_F(INFO, "ASI camera disconnected successfully");
    return true;
}

auto ASICamera::isConnected() const -> bool {
    return is_connected_.load();
}

auto ASICamera::scan() -> std::vector<std::string> {
    LOG_F(INFO, "ASICamera::scan: Scanning for available ASI cameras");
    
    std::vector<std::string> cameras;
    
    if (!is_initialized_) {
        LOG_F(ERROR, "Camera not initialized for scanning");
        return cameras;
    }
    
    // Scan for ASI cameras
    int numCameras = ASIGetNumOfConnectedCameras();
    LOG_F(INFO, "Found {} ASI cameras", numCameras);
    
    for (int i = 0; i < numCameras; ++i) {
        ASI_CAMERA_INFO cameraInfo;
        ASI_ERROR_CODE result = ASIGetCameraProperty(&cameraInfo, i);
        
        if (result == ASI_SUCCESS) {
            std::string cameraDesc = std::string(cameraInfo.Name) + " (ID: " + std::to_string(cameraInfo.CameraID) + ")";
            cameras.push_back(std::to_string(cameraInfo.CameraID));
            LOG_F(INFO, "Found ASI camera: {}", cameraDesc);
        } else {
            LOG_F(WARNING, "Failed to get camera property for index {}", i);
        }
    }
    
    return cameras;
}

// Exposure control implementations
auto ASICamera::startExposure(double duration) -> bool {
    LOG_F(INFO, "ASICamera::startExposure: Starting exposure for {} seconds", duration);
    
    if (!isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
    if (is_exposing_) {
        LOG_F(ERROR, "Camera already exposing");
        return false;
    }
    
    if (!isValidExposureTime(duration)) {
        LOG_F(ERROR, "Invalid exposure duration: {}", duration);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(exposure_mutex_);
    
    current_exposure_duration_ = duration;
    exposure_abort_requested_ = false;
    
    // Start exposure in separate thread
    exposure_thread_ = std::thread(&ASICamera::exposureThreadFunction, this);
    
    is_exposing_ = true;
    exposure_start_time_ = std::chrono::system_clock::now();
    updateCameraState(CameraState::EXPOSING);
    
    LOG_F(INFO, "Exposure started successfully");
    return true;
}

auto ASICamera::abortExposure() -> bool {
    LOG_F(INFO, "ASICamera::abortExposure: Aborting current exposure");
    
    if (!is_exposing_) {
        LOG_F(WARNING, "No exposure in progress");
        return true;
    }
    
    exposure_abort_requested_ = true;
    
    // Stop ASI exposure
    ASI_ERROR_CODE result = ASIStopExposure(camera_id_);
    if (result != ASI_SUCCESS) {
        handleASIError(result, "ASIStopExposure");
    }
    
    // Wait for exposure thread to finish
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }
    
    is_exposing_ = false;
    updateCameraState(CameraState::ABORTED);
    
    LOG_F(INFO, "Exposure aborted successfully");
    return true;
}

auto ASICamera::isExposing() const -> bool {
    return is_exposing_.load();
}

auto ASICamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - exposure_start_time_).count() / 1000.0;
    
    return std::min(elapsed / current_exposure_duration_, 1.0);
}

auto ASICamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }
    
    auto progress = getExposureProgress();
    return std::max(0.0, current_exposure_duration_ * (1.0 - progress));
}

auto ASICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    if (is_exposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }
    
    return current_frame_;
}

auto ASICamera::saveImage(const std::string& path) -> bool {
    if (!current_frame_ || !current_frame_->data) {
        LOG_F(ERROR, "No image data to save");
        return false;
    }
    
    return saveFrameToFile(current_frame_, path);
}

// Private helper methods
auto ASICamera::initializeASISDK() -> bool {
    LOG_F(INFO, "Initializing ASI SDK");
    
    // No explicit initialization required for ASI SDK
    // Just check if any cameras are available
    int numCameras = ASIGetNumOfConnectedCameras();
    LOG_F(INFO, "ASI SDK initialized, {} cameras detected", numCameras);
    
    return true;
}

auto ASICamera::shutdownASISDK() -> bool {
    LOG_F(INFO, "Shutting down ASI SDK");
    
    // No explicit shutdown required for ASI SDK
    LOG_F(INFO, "ASI SDK shutdown successfully");
    return true;
}

auto ASICamera::openCamera(int cameraId) -> bool {
    LOG_F(INFO, "Opening ASI camera ID: {}", cameraId);
    
    ASI_ERROR_CODE result = ASIOpenCamera(cameraId);
    if (result != ASI_SUCCESS) {
        handleASIError(result, "ASIOpenCamera");
        return false;
    }
    
    // Initialize camera
    result = ASIInitCamera(cameraId);
    if (result != ASI_SUCCESS) {
        handleASIError(result, "ASIInitCamera");
        ASICloseCamera(cameraId);
        return false;
    }
    
    LOG_F(INFO, "ASI camera opened successfully");
    return true;
}

auto ASICamera::closeCamera() -> bool {
    if (camera_id_ < 0) {
        return true;
    }
    
    LOG_F(INFO, "Closing ASI camera");
    
    ASI_ERROR_CODE result = ASICloseCamera(camera_id_);
    
    if (result != ASI_SUCCESS) {
        handleASIError(result, "ASICloseCamera");
        return false;
    }
    
    camera_id_ = -1;
    LOG_F(INFO, "ASI camera closed successfully");
    return true;
}

auto ASICamera::handleASIError(int errorCode, const std::string& operation) -> void {
    std::string errorMsg = "ASI Error in " + operation + ": ";
    
    switch (errorCode) {
        case ASI_ERROR_INVALID_INDEX:
            errorMsg += "Invalid index";
            break;
        case ASI_ERROR_INVALID_ID:
            errorMsg += "Invalid ID";
            break;
        case ASI_ERROR_INVALID_CONTROL_TYPE:
            errorMsg += "Invalid control type";
            break;
        case ASI_ERROR_CAMERA_CLOSED:
            errorMsg += "Camera closed";
            break;
        case ASI_ERROR_CAMERA_REMOVED:
            errorMsg += "Camera removed";
            break;
        case ASI_ERROR_INVALID_PATH:
            errorMsg += "Invalid path";
            break;
        case ASI_ERROR_INVALID_FILEFORMAT:
            errorMsg += "Invalid file format";
            break;
        case ASI_ERROR_INVALID_SIZE:
            errorMsg += "Invalid size";
            break;
        case ASI_ERROR_INVALID_IMGTYPE:
            errorMsg += "Invalid image type";
            break;
        case ASI_ERROR_OUTOF_BOUNDARY:
            errorMsg += "Out of boundary";
            break;
        case ASI_ERROR_TIMEOUT:
            errorMsg += "Timeout";
            break;
        case ASI_ERROR_INVALID_SEQUENCE:
            errorMsg += "Invalid sequence";
            break;
        case ASI_ERROR_BUFFER_TOO_SMALL:
            errorMsg += "Buffer too small";
            break;
        case ASI_ERROR_VIDEO_MODE_ACTIVE:
            errorMsg += "Video mode active";
            break;
        case ASI_ERROR_EXPOSURE_IN_PROGRESS:
            errorMsg += "Exposure in progress";
            break;
        case ASI_ERROR_GENERAL_ERROR:
            errorMsg += "General error";
            break;
        case ASI_ERROR_INVALID_MODE:
            errorMsg += "Invalid mode";
            break;
        default:
            errorMsg += "Unknown error (" + std::to_string(errorCode) + ")";
            break;
    }
    
    LOG_F(ERROR, "{}", errorMsg);
}

auto ASICamera::isValidExposureTime(double duration) const -> bool {
    return duration >= MIN_EXPOSURE_TIME && duration <= MAX_EXPOSURE_TIME;
}

// ASI-specific methods
auto ASICamera::getASISDKVersion() const -> std::string {
    return ASIGetSDKVersion();
}

auto ASICamera::getCameraModes() -> std::vector<std::string> {
    return CAMERA_MODES;
}

auto ASICamera::setUSBBandwidth(int bandwidth) -> bool {
    if (!isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
    ASI_ERROR_CODE result = ASISetControlValue(camera_id_, ASI_BANDWIDTHOVERLOAD, bandwidth, ASI_FALSE);
    if (result == ASI_SUCCESS) {
        usb_bandwidth_ = bandwidth;
        LOG_F(INFO, "USB bandwidth set to: {}", bandwidth);
        return true;
    }
    
    handleASIError(result, "ASISetControlValue(ASI_BANDWIDTHOVERLOAD)");
    return false;
}

auto ASICamera::getUSBBandwidth() -> int {
    if (!isConnected()) {
        return usb_bandwidth_;
    }
    
    long value;
    ASI_BOOL isAuto;
    ASI_ERROR_CODE result = ASIGetControlValue(camera_id_, ASI_BANDWIDTHOVERLOAD, &value, &isAuto);
    
    if (result == ASI_SUCCESS) {
        usb_bandwidth_ = static_cast<int>(value);
        return usb_bandwidth_;
    }
    
    handleASIError(result, "ASIGetControlValue(ASI_BANDWIDTHOVERLOAD)");
    return usb_bandwidth_;
}

// ASI EAF (Electronic Auto Focuser) implementation
auto ASICamera::hasEAFFocuser() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int eaf_count = EAFGetNum();
    if (eaf_count > 0) {
        EAF_INFO eaf_info;
        if (EAFGetID(0, &eaf_focuser_id_) == EAF_SUCCESS) {
            if (EAFGetProperty(eaf_focuser_id_, &eaf_info) == EAF_SUCCESS) {
                has_eaf_focuser_ = true;
                eaf_focuser_max_position_ = eaf_info.MaxStep;
                return true;
            }
        }
    }
#endif
    return has_eaf_focuser_;
}

auto ASICamera::connectEAFFocuser() -> bool {
    if (!has_eaf_focuser_) {
        LOG_F(ERROR, "No EAF focuser available");
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EAFOpen(eaf_focuser_id_) == EAF_SUCCESS) {
        eaf_focuser_connected_ = true;
        
        // Get initial position
        int position;
        if (EAFGetPosition(eaf_focuser_id_, &position) == EAF_SUCCESS) {
            eaf_focuser_position_ = position;
        }
        
        // Get firmware version
        char firmware[32];
        if (EAFGetFirmwareVersion(eaf_focuser_id_, firmware) == EAF_SUCCESS) {
            eaf_focuser_firmware_ = std::string(firmware);
        }
        
        LOG_F(INFO, "Connected to ASI EAF focuser");
        return true;
    }
#else
    eaf_focuser_connected_ = true;
    eaf_focuser_position_ = 5000;
    eaf_focuser_max_position_ = 10000;
    eaf_focuser_firmware_ = "1.2.0";
    LOG_F(INFO, "Connected to ASI EAF focuser simulator");
    return true;
#endif

    return false;
}

auto ASICamera::disconnectEAFFocuser() -> bool {
    if (!eaf_focuser_connected_) {
        return true;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    EAFClose(eaf_focuser_id_);
#endif

    eaf_focuser_connected_ = false;
    LOG_F(INFO, "Disconnected ASI EAF focuser");
    return true;
}

auto ASICamera::isEAFFocuserConnected() -> bool {
    return eaf_focuser_connected_;
}

auto ASICamera::setEAFFocuserPosition(int position) -> bool {
    if (!eaf_focuser_connected_) {
        LOG_F(ERROR, "EAF focuser not connected");
        return false;
    }

    if (position < 0 || position > eaf_focuser_max_position_) {
        LOG_F(ERROR, "Invalid EAF focuser position: {}", position);
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EAFMove(eaf_focuser_id_, position) == EAF_SUCCESS) {
        eaf_focuser_position_ = position;
        eaf_focuser_moving_ = true;
        LOG_F(INFO, "Moving EAF focuser to position {}", position);
        return true;
    }
#else
    eaf_focuser_position_ = position;
    eaf_focuser_moving_ = true;
    LOG_F(INFO, "Moving EAF focuser to position {}", position);
    
    // Simulate movement completion after delay
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        eaf_focuser_moving_ = false;
    }).detach();
    
    return true;
#endif

    return false;
}

auto ASICamera::getEAFFocuserPosition() -> int {
    if (!eaf_focuser_connected_) {
        return -1;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int position;
    if (EAFGetPosition(eaf_focuser_id_, &position) == EAF_SUCCESS) {
        eaf_focuser_position_ = position;
    }
#endif

    return eaf_focuser_position_;
}

auto ASICamera::getEAFFocuserMaxPosition() -> int {
    return eaf_focuser_max_position_;
}

auto ASICamera::isEAFFocuserMoving() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    bool moving;
    if (EAFIsMoving(eaf_focuser_id_, &moving) == EAF_SUCCESS) {
        eaf_focuser_moving_ = moving;
    }
#endif
    return eaf_focuser_moving_;
}

auto ASICamera::stopEAFFocuser() -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EAFStop(eaf_focuser_id_) == EAF_SUCCESS) {
        eaf_focuser_moving_ = false;
        LOG_F(INFO, "Stopped EAF focuser");
        return true;
    }
#else
    eaf_focuser_moving_ = false;
    LOG_F(INFO, "Stopped EAF focuser");
    return true;
#endif

    return false;
}

auto ASICamera::setEAFFocuserStepSize(int stepSize) -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

    eaf_focuser_step_size_ = stepSize;
    LOG_F(INFO, "Set EAF focuser step size to {}", stepSize);
    return true;
}

auto ASICamera::getEAFFocuserStepSize() -> int {
    return eaf_focuser_step_size_;
}

auto ASICamera::homeEAFFocuser() -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EAFMove(eaf_focuser_id_, 0) == EAF_SUCCESS) {
        eaf_focuser_position_ = 0;
        LOG_F(INFO, "Homing EAF focuser");
        return true;
    }
#else
    eaf_focuser_position_ = 0;
    LOG_F(INFO, "Homing EAF focuser");
    return true;
#endif

    return false;
}

auto ASICamera::calibrateEAFFocuser() -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EAFCalibrate(eaf_focuser_id_) == EAF_SUCCESS) {
        LOG_F(INFO, "Calibrating EAF focuser");
        return true;
    }
#else
    LOG_F(INFO, "Calibrating EAF focuser");
    return true;
#endif

    return false;
}

auto ASICamera::getEAFFocuserTemperature() -> double {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    float temperature;
    if (EAFGetTemp(eaf_focuser_id_, &temperature) == EAF_SUCCESS) {
        eaf_focuser_temperature_ = static_cast<double>(temperature);
    }
#else
    eaf_focuser_temperature_ = 23.5;  // Simulate room temperature
#endif
    return eaf_focuser_temperature_;
}

auto ASICamera::enableEAFFocuserBacklashCompensation(bool enable) -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

    eaf_backlash_compensation_ = enable;
    LOG_F(INFO, "{} EAF focuser backlash compensation", enable ? "Enabled" : "Disabled");
    return true;
}

auto ASICamera::setEAFFocuserBacklashSteps(int steps) -> bool {
    if (!eaf_focuser_connected_) {
        return false;
    }

    eaf_backlash_steps_ = steps;
    LOG_F(INFO, "Set EAF focuser backlash steps to {}", steps);
    return true;
}

// ASI EFW (Electronic Filter Wheel) implementation
auto ASICamera::hasEFWFilterWheel() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int efw_count = EFWGetNum();
    if (efw_count > 0) {
        EFW_INFO efw_info;
        if (EFWGetID(0, &efw_filter_wheel_id_) == EFW_SUCCESS) {
            if (EFWGetProperty(efw_filter_wheel_id_, &efw_info) == EFW_SUCCESS) {
                has_efw_filter_wheel_ = true;
                efw_filter_count_ = efw_info.slotNum;
                return true;
            }
        }
    }
#endif
    return has_efw_filter_wheel_;
}

auto ASICamera::connectEFWFilterWheel() -> bool {
    if (!has_efw_filter_wheel_) {
        LOG_F(ERROR, "No EFW filter wheel available");
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EFWOpen(efw_filter_wheel_id_) == EFW_SUCCESS) {
        efw_filter_wheel_connected_ = true;
        
        // Get initial position
        int position;
        if (EFWGetPosition(efw_filter_wheel_id_, &position) == EFW_SUCCESS) {
            efw_current_position_ = position;
        }
        
        // Get firmware version
        char firmware[32];
        if (EFWGetFirmwareVersion(efw_filter_wheel_id_, firmware) == EFW_SUCCESS) {
            efw_firmware_ = std::string(firmware);
        }
        
        // Initialize filter names
        efw_filter_names_.resize(efw_filter_count_);
        for (int i = 0; i < efw_filter_count_; ++i) {
            efw_filter_names_[i] = "Filter " + std::to_string(i + 1);
        }
        
        LOG_F(INFO, "Connected to ASI EFW filter wheel");
        return true;
    }
#else
    efw_filter_wheel_connected_ = true;
    efw_current_position_ = 1;
    efw_filter_count_ = 7;  // EFW-7 simulator
    efw_firmware_ = "1.3.0";
    
    // Initialize filter names
    efw_filter_names_ = {"Red", "Green", "Blue", "Clear", "H-Alpha", "OIII", "SII"};
    
    LOG_F(INFO, "Connected to ASI EFW filter wheel simulator");
    return true;
#endif

    return false;
}

auto ASICamera::disconnectEFWFilterWheel() -> bool {
    if (!efw_filter_wheel_connected_) {
        return true;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    EFWClose(efw_filter_wheel_id_);
#endif

    efw_filter_wheel_connected_ = false;
    LOG_F(INFO, "Disconnected ASI EFW filter wheel");
    return true;
}

auto ASICamera::isEFWFilterWheelConnected() -> bool {
    return efw_filter_wheel_connected_;
}

auto ASICamera::setEFWFilterPosition(int position) -> bool {
    if (!efw_filter_wheel_connected_) {
        LOG_F(ERROR, "EFW filter wheel not connected");
        return false;
    }

    if (position < 1 || position > efw_filter_count_) {
        LOG_F(ERROR, "Invalid EFW filter position: {}", position);
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EFWSetPosition(efw_filter_wheel_id_, position) == EFW_SUCCESS) {
        efw_current_position_ = position;
        efw_filter_wheel_moving_ = true;
        LOG_F(INFO, "Moving EFW filter wheel to position {}", position);
        return true;
    }
#else
    efw_current_position_ = position;
    efw_filter_wheel_moving_ = true;
    LOG_F(INFO, "Moving EFW filter wheel to position {} ({})", position, 
          position <= efw_filter_names_.size() ? efw_filter_names_[position-1] : "Unknown");
    
    // Simulate movement completion after delay
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        efw_filter_wheel_moving_ = false;
    }).detach();
    
    return true;
#endif

    return false;
}

auto ASICamera::getEFWFilterPosition() -> int {
    if (!efw_filter_wheel_connected_) {
        return -1;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int position;
    if (EFWGetPosition(efw_filter_wheel_id_, &position) == EFW_SUCCESS) {
        efw_current_position_ = position;
    }
#endif

    return efw_current_position_;
}

auto ASICamera::getEFWFilterCount() -> int {
    return efw_filter_count_;
}

auto ASICamera::isEFWFilterWheelMoving() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    bool moving;
    if (EFWGetPosition(efw_filter_wheel_id_, nullptr) == EFW_ERROR_MOVING) {
        efw_filter_wheel_moving_ = true;
    } else {
        efw_filter_wheel_moving_ = false;
    }
#endif
    return efw_filter_wheel_moving_;
}

auto ASICamera::homeEFWFilterWheel() -> bool {
    if (!efw_filter_wheel_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EFWCalibrate(efw_filter_wheel_id_) == EFW_SUCCESS) {
        LOG_F(INFO, "Homing EFW filter wheel");
        return true;
    }
#else
    efw_current_position_ = 1;
    LOG_F(INFO, "Homing EFW filter wheel");
    return true;
#endif

    return false;
}

auto ASICamera::getEFWFilterWheelFirmware() -> std::string {
    return efw_firmware_;
}

auto ASICamera::setEFWFilterNames(const std::vector<std::string>& names) -> bool {
    if (names.size() != static_cast<size_t>(efw_filter_count_)) {
        LOG_F(ERROR, "Filter names count ({}) doesn't match filter wheel slots ({})", 
              names.size(), efw_filter_count_);
        return false;
    }

    efw_filter_names_ = names;
    LOG_F(INFO, "Updated EFW filter names");
    return true;
}

auto ASICamera::getEFWFilterNames() -> std::vector<std::string> {
    return efw_filter_names_;
}

auto ASICamera::getEFWUnidirectionalMode() -> bool {
    return efw_unidirectional_mode_;
}

auto ASICamera::setEFWUnidirectionalMode(bool enable) -> bool {
    if (!efw_filter_wheel_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EFWSetDirection(efw_filter_wheel_id_, enable) == EFW_SUCCESS) {
        efw_unidirectional_mode_ = enable;
        LOG_F(INFO, "{} EFW unidirectional mode", enable ? "Enabled" : "Disabled");
        return true;
    }
#else
    efw_unidirectional_mode_ = enable;
    LOG_F(INFO, "{} EFW unidirectional mode", enable ? "Enabled" : "Disabled");
    return true;
#endif

    return false;
}

auto ASICamera::calibrateEFWFilterWheel() -> bool {
    if (!efw_filter_wheel_connected_) {
        return false;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (EFWCalibrate(efw_filter_wheel_id_) == EFW_SUCCESS) {
        LOG_F(INFO, "Calibrating EFW filter wheel");
        return true;
    }
#else
    LOG_F(INFO, "Calibrating EFW filter wheel");
    return true;
#endif

    return false;
}
