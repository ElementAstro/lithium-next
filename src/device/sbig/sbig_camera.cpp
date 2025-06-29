/*
 * sbig_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: SBIG Camera Implementation with dual-chip support and professional features

*************************************************/

#include "sbig_camera.hpp"

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
#include "sbigudrv.h"  // SBIG SDK header (stub)
#endif

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>

namespace lithium::device::sbig::camera {

SBIGCamera::SBIGCamera(const std::string& name)
    : AtomCamera(name)
    , device_handle_(INVALID_HANDLE_VALUE)
    , device_index_(-1)
    , is_connected_(false)
    , is_initialized_(false)
    , is_exposing_(false)
    , exposure_abort_requested_(false)
    , current_exposure_duration_(0.0)
    , is_video_running_(false)
    , is_video_recording_(false)
    , video_exposure_(0.01)
    , video_gain_(100)
    , cooler_enabled_(false)
    , target_temperature_(-10.0)
    , current_temperature_(25.0)
    , cooling_power_(0.0)
    , has_dual_chip_(false)
    , current_chip_(ChipType::IMAGING)
    , guide_chip_width_(0)
    , guide_chip_height_(0)
    , guide_chip_pixel_size_(0.0)
    , has_cfw_(false)
    , cfw_position_(0)
    , cfw_filter_count_(0)
    , cfw_homed_(false)
    , has_ao_(false)
    , ao_x_position_(0)
    , ao_y_position_(0)
    , ao_max_displacement_(0)
    , sequence_running_(false)
    , sequence_current_frame_(0)
    , sequence_total_frames_(0)
    , sequence_exposure_(1.0)
    , sequence_interval_(0.0)
    , current_gain_(100)
    , current_offset_(0)
    , readout_mode_(0)
    , abg_enabled_(false)
    , roi_x_(0)
    , roi_y_(0)
    , roi_width_(0)
    , roi_height_(0)
    , bin_x_(1)
    , bin_y_(1)
    , max_width_(0)
    , max_height_(0)
    , pixel_size_x_(0.0)
    , pixel_size_y_(0.0)
    , bit_depth_(16)
    , bayer_pattern_(BayerPattern::MONO)
    , is_color_camera_(false)
    , has_shutter_(true)
    , has_mechanical_shutter_(true)
    , total_frames_(0)
    , dropped_frames_(0)
    , last_frame_result_(nullptr) {

    LOG_F(INFO, "Created SBIG camera instance: {}", name);
}

SBIGCamera::~SBIGCamera() {
    if (is_connected_) {
        disconnect();
    }
    if (is_initialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed SBIG camera instance: {}", name_);
}

auto SBIGCamera::initialize() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_initialized_) {
        LOG_F(WARNING, "SBIG camera already initialized");
        return true;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    if (!initializeSBIGSDK()) {
        LOG_F(ERROR, "Failed to initialize SBIG SDK");
        return false;
    }
#else
    LOG_F(WARNING, "SBIG SDK not available, using stub implementation");
#endif

    is_initialized_ = true;
    LOG_F(INFO, "SBIG camera initialized successfully");
    return true;
}

auto SBIGCamera::destroy() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (!is_initialized_) {
        return true;
    }

    if (is_connected_) {
        disconnect();
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    shutdownSBIGSDK();
#endif

    is_initialized_ = false;
    LOG_F(INFO, "SBIG camera destroyed successfully");
    return true;
}

auto SBIGCamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_connected_) {
        LOG_F(WARNING, "SBIG camera already connected");
        return true;
    }

    if (!is_initialized_) {
        LOG_F(ERROR, "SBIG camera not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to SBIG camera: {} (attempt {}/{})", deviceName, retry + 1, maxRetry);

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
        auto devices = scan();
        device_index_ = -1;

        if (deviceName.empty()) {
            if (!devices.empty()) {
                device_index_ = 0;
            }
        } else {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i] == deviceName) {
                    device_index_ = static_cast<int>(i);
                    break;
                }
            }
        }

        if (device_index_ == -1) {
            LOG_F(ERROR, "SBIG camera not found: {}", deviceName);
            continue;
        }

        if (openCamera(device_index_)) {
            if (establishLink() && setupCameraParameters()) {
                is_connected_ = true;
                LOG_F(INFO, "Connected to SBIG camera successfully");
                return true;
            } else {
                closeCamera();
            }
        }
#else
        // Stub implementation
        device_index_ = 0;
        device_handle_ = reinterpret_cast<HANDLE>(1);  // Fake handle
        camera_model_ = "SBIG ST-402ME Simulator";
        serial_number_ = "SIM123789";
        firmware_version_ = "1.12";
        camera_type_ = "ST-402ME";
        max_width_ = 765;
        max_height_ = 510;
        pixel_size_x_ = pixel_size_y_ = 9.0;
        bit_depth_ = 16;
        is_color_camera_ = false;
        has_dual_chip_ = true;
        has_cfw_ = true;
        has_mechanical_shutter_ = true;

        // Setup guide chip
        guide_chip_width_ = 192;
        guide_chip_height_ = 165;
        guide_chip_pixel_size_ = 9.0;

        // Setup CFW
        cfw_filter_count_ = 5;

        roi_width_ = max_width_;
        roi_height_ = max_height_;

        is_connected_ = true;
        LOG_F(INFO, "Connected to SBIG camera simulator");
        return true;
#endif

        if (retry < maxRetry - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    LOG_F(ERROR, "Failed to connect to SBIG camera after {} attempts", maxRetry);
    return false;
}

auto SBIGCamera::disconnect() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (!is_connected_) {
        return true;
    }

    // Stop any ongoing operations
    if (is_exposing_) {
        abortExposure();
    }
    if (is_video_running_) {
        stopVideo();
    }
    if (sequence_running_) {
        stopSequence();
    }
    if (cooler_enabled_) {
        stopCooling();
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    closeCamera();
#endif

    is_connected_ = false;
    LOG_F(INFO, "Disconnected from SBIG camera");
    return true;
}

auto SBIGCamera::isConnected() const -> bool {
    return is_connected_;
}

auto SBIGCamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    try {
        QueryUSBResults queryResults;
        if (SBIGUnivDrvCommand(CC_QUERY_USB, nullptr, &queryResults) == CE_NO_ERROR) {
            for (int i = 0; i < queryResults.camerasFound; ++i) {
                devices.push_back(std::string(queryResults.usbInfo[i].name));
            }
        }

        // Also check for Ethernet cameras
        QueryEthernetResults ethResults;
        if (SBIGUnivDrvCommand(CC_QUERY_ETHERNET, nullptr, &ethResults) == CE_NO_ERROR) {
            for (int i = 0; i < ethResults.camerasFound; ++i) {
                devices.push_back(std::string(ethResults.ethernetInfo[i].name));
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error scanning for SBIG cameras: {}", e.what());
    }
#else
    // Stub implementation
    devices.push_back("SBIG ST-402ME Simulator");
    devices.push_back("SBIG STF-8300M");
    devices.push_back("SBIG STX-16803");
#endif

    LOG_F(INFO, "Found {} SBIG cameras", devices.size());
    return devices;
}

auto SBIGCamera::startExposure(double duration) -> bool {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (is_exposing_) {
        LOG_F(WARNING, "Exposure already in progress");
        return false;
    }

    if (!isValidExposureTime(duration)) {
        LOG_F(ERROR, "Invalid exposure duration: {}", duration);
        return false;
    }

    current_exposure_duration_ = duration;
    exposure_abort_requested_ = false;
    exposure_start_time_ = std::chrono::system_clock::now();
    is_exposing_ = true;

    // Start exposure in separate thread
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }
    exposure_thread_ = std::thread(&SBIGCamera::exposureThreadFunction, this);

    LOG_F(INFO, "Started exposure: {} seconds on {} chip", duration,
          (current_chip_ == ChipType::IMAGING) ? "imaging" : "guide");
    return true;
}

auto SBIGCamera::abortExposure() -> bool {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (!is_exposing_) {
        return true;
    }

    exposure_abort_requested_ = true;

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    SBIGUnivDrvCommand(CC_END_EXPOSURE, nullptr, nullptr);
#endif

    // Wait for exposure thread to finish
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }

    is_exposing_ = false;
    LOG_F(INFO, "Aborted exposure");
    return true;
}

auto SBIGCamera::isExposing() const -> bool {
    return is_exposing_;
}

auto SBIGCamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::min(elapsed / current_exposure_duration_, 1.0);
}

auto SBIGCamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::max(current_exposure_duration_ - elapsed, 0.0);
}

auto SBIGCamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (is_exposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }

    return last_frame_result_;
}

auto SBIGCamera::saveImage(const std::string& path) -> bool {
    auto frame = getExposureResult();
    if (!frame) {
        LOG_F(ERROR, "No image data available");
        return false;
    }

    return saveFrameToFile(frame, path);
}

// Temperature control (excellent on SBIG cameras)
auto SBIGCamera::startCooling(double targetTemp) -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    target_temperature_ = targetTemp;
    cooler_enabled_ = true;

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    SetTemperatureRegulationParams params;
    params.regulation = REGULATION_ON;
    params.ccdSetpoint = static_cast<unsigned short>(targetTemp * 100 + 27315);  // Convert to SBIG format
    SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, &params, nullptr);
#endif

    // Start temperature monitoring thread
    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }
    temperature_thread_ = std::thread(&SBIGCamera::temperatureThreadFunction, this);

    LOG_F(INFO, "Started cooling to {} °C", targetTemp);
    return true;
}

auto SBIGCamera::stopCooling() -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    cooler_enabled_ = false;

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    SetTemperatureRegulationParams params;
    params.regulation = REGULATION_OFF;
    SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, &params, nullptr);
#endif

    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }

    LOG_F(INFO, "Stopped cooling");
    return true;
}

auto SBIGCamera::isCoolerOn() const -> bool {
    return cooler_enabled_;
}

auto SBIGCamera::getTemperature() const -> std::optional<double> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    QueryTemperatureStatusResults tempResults;
    if (SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, nullptr, &tempResults) == CE_NO_ERROR) {
        // Convert from SBIG format (1/100 degree K above absolute zero) to Celsius
        return (tempResults.imagingCCDTemperature / 100.0) - 273.15;
    }
    return std::nullopt;
#else
    // Simulate temperature based on cooling state
    double simTemp = cooler_enabled_ ? target_temperature_ + 1.0 : 25.0;
    return simTemp;
#endif
}

// Dual-chip control (SBIG specialty)
auto SBIGCamera::setActiveChip(ChipType chip) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!has_dual_chip_ && chip == ChipType::GUIDE) {
        LOG_F(ERROR, "Camera does not have a guide chip");
        return false;
    }

    current_chip_ = chip;
    LOG_F(INFO, "Set active chip to {}", (chip == ChipType::IMAGING) ? "imaging" : "guide");
    return true;
}

auto SBIGCamera::getActiveChip() const -> ChipType {
    return current_chip_;
}

auto SBIGCamera::hasDualChip() const -> bool {
    return has_dual_chip_;
}

auto SBIGCamera::getGuideChipResolution() -> std::pair<int, int> {
    if (!has_dual_chip_) {
        return {0, 0};
    }
    return {guide_chip_width_, guide_chip_height_};
}

auto SBIGCamera::getGuideChipPixelSize() -> double {
    return guide_chip_pixel_size_;
}

// CFW (Color Filter Wheel) control
auto SBIGCamera::hasCFW() const -> bool {
    return has_cfw_;
}

auto SBIGCamera::getCFWPosition() -> int {
    if (!has_cfw_) {
        return -1;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    CFWResults cfwResults;
    CFWParams cfwParams;
    cfwParams.cfwModel = CFWSEL_CFW5;
    cfwParams.cfwCommand = CFWC_QUERY;

    if (SBIGUnivDrvCommand(CC_CFW, &cfwParams, &cfwResults) == CE_NO_ERROR) {
        return cfwResults.cfwPosition;
    }
#endif

    return cfw_position_;
}

auto SBIGCamera::setCFWPosition(int position) -> bool {
    if (!has_cfw_) {
        LOG_F(ERROR, "Camera does not have CFW");
        return false;
    }

    if (position < 1 || position > cfw_filter_count_) {
        LOG_F(ERROR, "Invalid CFW position: {}", position);
        return false;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    CFWParams cfwParams;
    cfwParams.cfwModel = CFWSEL_CFW5;
    cfwParams.cfwCommand = CFWC_GOTO;
    cfwParams.cfwParam1 = position;

    if (SBIGUnivDrvCommand(CC_CFW, &cfwParams, nullptr) != CE_NO_ERROR) {
        return false;
    }
#endif

    cfw_position_ = position;
    LOG_F(INFO, "Set CFW position to {}", position);
    return true;
}

auto SBIGCamera::getCFWFilterCount() -> int {
    return cfw_filter_count_;
}

auto SBIGCamera::homeCFW() -> bool {
    if (!has_cfw_) {
        LOG_F(ERROR, "Camera does not have CFW");
        return false;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    CFWParams cfwParams;
    cfwParams.cfwModel = CFWSEL_CFW5;
    cfwParams.cfwCommand = CFWC_INIT;

    if (SBIGUnivDrvCommand(CC_CFW, &cfwParams, nullptr) != CE_NO_ERROR) {
        return false;
    }
#endif

    cfw_homed_ = true;
    cfw_position_ = 1;
    LOG_F(INFO, "CFW homed successfully");
    return true;
}

// AO (Adaptive Optics) control
auto SBIGCamera::hasAO() const -> bool {
    return has_ao_;
}

auto SBIGCamera::setAOPosition(int x, int y) -> bool {
    if (!has_ao_) {
        LOG_F(ERROR, "Camera does not have AO");
        return false;
    }

    if (std::abs(x) > ao_max_displacement_ || std::abs(y) > ao_max_displacement_) {
        LOG_F(ERROR, "AO displacement too large: {},{}", x, y);
        return false;
    }

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    AOTipTiltParams aoParams;
    aoParams.xDeflection = x;
    aoParams.yDeflection = y;

    if (SBIGUnivDrvCommand(CC_AO_TIP_TILT, &aoParams, nullptr) != CE_NO_ERROR) {
        return false;
    }
#endif

    ao_x_position_ = x;
    ao_y_position_ = y;
    LOG_F(INFO, "Set AO position to {},{}", x, y);
    return true;
}

auto SBIGCamera::getAOPosition() -> std::pair<int, int> {
    return {ao_x_position_, ao_y_position_};
}

auto SBIGCamera::centerAO() -> bool {
    return setAOPosition(0, 0);
}

// ABG (Anti-Blooming Gate) control
auto SBIGCamera::enableABG(bool enable) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    abg_enabled_ = enable;
    LOG_F(INFO, "{} Anti-Blooming Gate", enable ? "Enabled" : "Disabled");
    return true;
}

auto SBIGCamera::isABGEnabled() const -> bool {
    return abg_enabled_;
}

// Readout mode control
auto SBIGCamera::setReadoutMode(int mode) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    readout_mode_ = mode;
    LOG_F(INFO, "Set readout mode to {}", mode);
    return true;
}

auto SBIGCamera::getReadoutMode() -> int {
    return readout_mode_;
}

auto SBIGCamera::getReadoutModes() -> std::vector<std::string> {
    return {"High Quality", "Fast", "Low Noise"};
}

// Frame settings
auto SBIGCamera::setResolution(int x, int y, int width, int height) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidResolution(x, y, width, height)) {
        LOG_F(ERROR, "Invalid resolution: {}x{} at {},{}", width, height, x, y);
        return false;
    }

    roi_x_ = x;
    roi_y_ = y;
    roi_width_ = width;
    roi_height_ = height;

    LOG_F(INFO, "Set resolution to {}x{} at {},{}", width, height, x, y);
    return true;
}

auto SBIGCamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Resolution res;
    if (current_chip_ == ChipType::IMAGING) {
        res.width = roi_width_;
        res.height = roi_height_;
    } else {
        res.width = guide_chip_width_;
        res.height = guide_chip_height_;
    }
    return res;
}

auto SBIGCamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    if (current_chip_ == ChipType::IMAGING) {
        res.width = max_width_;
        res.height = max_height_;
    } else {
        res.width = guide_chip_width_;
        res.height = guide_chip_height_;
    }
    return res;
}

auto SBIGCamera::setBinning(int horizontal, int vertical) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidBinning(horizontal, vertical)) {
        LOG_F(ERROR, "Invalid binning: {}x{}", horizontal, vertical);
        return false;
    }

    bin_x_ = horizontal;
    bin_y_ = vertical;

    LOG_F(INFO, "Set binning to {}x{}", horizontal, vertical);
    return true;
}

auto SBIGCamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = bin_x_;
    bin.vertical = bin_y_;
    return bin;
}

// SBIG-specific methods
auto SBIGCamera::getSBIGSDKVersion() const -> std::string {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    GetDriverInfoResults driverInfo;
    if (SBIGUnivDrvCommand(CC_GET_DRIVER_INFO, nullptr, &driverInfo) == CE_NO_ERROR) {
        return std::string(driverInfo.version);
    }
    return "Unknown";
#else
    return "Stub 4.99";
#endif
}

auto SBIGCamera::getCameraModel() const -> std::string {
    return camera_model_;
}

auto SBIGCamera::getSerialNumber() const -> std::string {
    return serial_number_;
}

auto SBIGCamera::getCameraType() const -> std::string {
    return camera_type_;
}

// Private helper methods
auto SBIGCamera::initializeSBIGSDK() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    GetDriverInfoParams driverParams;
    driverParams.request = DRIVER_STD;

    GetDriverInfoResults driverResults;
    return (SBIGUnivDrvCommand(CC_GET_DRIVER_INFO, &driverParams, &driverResults) == CE_NO_ERROR);
#else
    return true;
#endif
}

auto SBIGCamera::shutdownSBIGSDK() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    // SBIG driver doesn't require explicit shutdown
#endif
    return true;
}

auto SBIGCamera::openCamera(int cameraIndex) -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    OpenDeviceParams openParams;
    openParams.deviceType = DEV_USB1;  // or DEV_USB2, DEV_ETH, etc.
    openParams.lptBaseAddress = 0;
    openParams.ipAddress = 0;

    return (SBIGUnivDrvCommand(CC_OPEN_DEVICE, &openParams, nullptr) == CE_NO_ERROR);
#else
    return true;
#endif
}

auto SBIGCamera::closeCamera() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    SBIGUnivDrvCommand(CC_CLOSE_DEVICE, nullptr, nullptr);
#endif
    return true;
}

auto SBIGCamera::establishLink() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    EstablishLinkParams linkParams;
    linkParams.sbigUseOnly = 0;

    EstablishLinkResults linkResults;
    return (SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &linkParams, &linkResults) == CE_NO_ERROR);
#else
    return true;
#endif
}

auto SBIGCamera::setupCameraParameters() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    // Get camera information
    GetCCDInfoParams infoParams;
    infoParams.request = CCD_INFO_IMAGING;

    GetCCDInfoResults0 infoResults;
    if (SBIGUnivDrvCommand(CC_GET_CCD_INFO, &infoParams, &infoResults) == CE_NO_ERROR) {
        max_width_ = infoResults.readoutInfo[0].width;
        max_height_ = infoResults.readoutInfo[0].height;
        pixel_size_x_ = infoResults.readoutInfo[0].pixelWidth / 100.0;  // Convert from 1/100 microns
        pixel_size_y_ = infoResults.readoutInfo[0].pixelHeight / 100.0;
        camera_model_ = std::string(infoResults.name);
    }

    // Check for guide chip
    infoParams.request = CCD_INFO_TRACKING;
    GetCCDInfoResults0 guideInfo;
    if (SBIGUnivDrvCommand(CC_GET_CCD_INFO, &infoParams, &guideInfo) == CE_NO_ERROR) {
        has_dual_chip_ = true;
        guide_chip_width_ = guideInfo.readoutInfo[0].width;
        guide_chip_height_ = guideInfo.readoutInfo[0].height;
        guide_chip_pixel_size_ = guideInfo.readoutInfo[0].pixelWidth / 100.0;
    }

    // Check for CFW
    CFWParams cfwParams;
    cfwParams.cfwModel = CFWSEL_CFW5;
    cfwParams.cfwCommand = CFWC_QUERY;

    CFWResults cfwResults;
    if (SBIGUnivDrvCommand(CC_CFW, &cfwParams, &cfwResults) == CE_NO_ERROR) {
        has_cfw_ = true;
        cfw_filter_count_ = 5;  // Standard CFW-5
    }
#endif

    roi_width_ = max_width_;
    roi_height_ = max_height_;

    return readCameraCapabilities();
}

auto SBIGCamera::readCameraCapabilities() -> bool {
    // Initialize camera capabilities using the correct CameraCapabilities structure
    camera_capabilities_.canAbort = true;
    camera_capabilities_.canSubFrame = true;
    camera_capabilities_.canBin = true;
    camera_capabilities_.hasCooler = true;
    camera_capabilities_.hasGain = false;  // SBIG cameras typically don't have gain control
    camera_capabilities_.hasShutter = has_mechanical_shutter_;
    camera_capabilities_.canStream = false;  // SBIG cameras are primarily for imaging
    camera_capabilities_.canRecordVideo = false;
    camera_capabilities_.supportsSequences = true;
    camera_capabilities_.hasImageQualityAnalysis = true;
    camera_capabilities_.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF};

    return true;
}

auto SBIGCamera::exposureThreadFunction() -> void {
    try {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
        // Start exposure
        StartExposureParams2 expParams;
        expParams.ccd = (current_chip_ == ChipType::IMAGING) ? CCD_IMAGING : CCD_TRACKING;
        expParams.exposureTime = static_cast<unsigned long>(current_exposure_duration_ * 100);  // 1/100 second units
        expParams.abgState = abg_enabled_ ? ABG_LOW7 : ABG_CLK_LOW;
        expParams.openShutter = SC_OPEN_SHUTTER;
        expParams.readoutMode = readout_mode_;
        expParams.top = roi_y_;
        expParams.left = roi_x_;
        expParams.height = roi_height_;
        expParams.width = roi_width_;

        if (SBIGUnivDrvCommand(CC_START_EXPOSURE2, &expParams, nullptr) != CE_NO_ERROR) {
            LOG_F(ERROR, "Failed to start exposure");
            is_exposing_ = false;
            return;
        }

        // Wait for exposure to complete
        QueryCommandStatusParams statusParams;
        statusParams.command = CC_START_EXPOSURE2;

        QueryCommandStatusResults statusResults;
        do {
            if (exposure_abort_requested_) {
                break;
            }

            if (SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS, &statusParams, &statusResults) != CE_NO_ERROR) {
                LOG_F(ERROR, "Failed to query exposure status");
                is_exposing_ = false;
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (statusResults.status != CS_IDLE);

        if (!exposure_abort_requested_) {
            // End exposure and download image
            EndExposureParams endParams;
            endParams.ccd = (current_chip_ == ChipType::IMAGING) ? CCD_IMAGING : CCD_TRACKING;
            SBIGUnivDrvCommand(CC_END_EXPOSURE, &endParams, nullptr);

            // Download image data
            last_frame_result_ = captureFrame();
            if (last_frame_result_) {
                total_frames_++;
            } else {
                dropped_frames_++;
            }
        }
#else
        // Simulate exposure
        auto start = std::chrono::steady_clock::now();
        while (!exposure_abort_requested_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= current_exposure_duration_) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!exposure_abort_requested_) {
            last_frame_result_ = captureFrame();
            if (last_frame_result_) {
                total_frames_++;
            } else {
                dropped_frames_++;
            }
        }
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in exposure thread: {}", e.what());
        dropped_frames_++;
    }

    is_exposing_ = false;
    last_frame_time_ = std::chrono::system_clock::now();
}

auto SBIGCamera::captureFrame() -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();

    if (current_chip_ == ChipType::IMAGING) {
        frame->resolution.width = roi_width_ / bin_x_;
        frame->resolution.height = roi_height_ / bin_y_;
        frame->pixel.sizeX = pixel_size_x_ * bin_x_;
        frame->pixel.sizeY = pixel_size_y_ * bin_y_;
    } else {
        frame->resolution.width = guide_chip_width_ / bin_x_;
        frame->resolution.height = guide_chip_height_ / bin_y_;
        frame->pixel.sizeX = guide_chip_pixel_size_ * bin_x_;
        frame->pixel.sizeY = guide_chip_pixel_size_ * bin_y_;
    }

    frame->binning.horizontal = bin_x_;
    frame->binning.vertical = bin_y_;
    frame->pixel.size = frame->pixel.sizeX;  // Assuming square pixels
    frame->pixel.depth = bit_depth_;
    frame->type = FrameType::FITS;
    frame->format = "RAW";

    // Calculate frame size
    size_t pixelCount = frame->resolution.width * frame->resolution.height;
    size_t bytesPerPixel = 2;  // SBIG cameras are typically 16-bit
    frame->size = pixelCount * bytesPerPixel;

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    // Download actual image data from camera
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);

    ReadoutLineParams readParams;
    readParams.ccd = (current_chip_ == ChipType::IMAGING) ? CCD_IMAGING : CCD_TRACKING;
    readParams.readoutMode = readout_mode_;
    readParams.pixelStart = 0;
    readParams.pixelLength = frame->resolution.width;

    uint16_t* data16 = reinterpret_cast<uint16_t*>(data_buffer.get());

    for (int row = 0; row < frame->resolution.height; ++row) {
        if (SBIGUnivDrvCommand(CC_READOUT_LINE, &readParams, &data16[row * frame->resolution.width]) != CE_NO_ERROR) {
            LOG_F(ERROR, "Failed to download image row {}", row);
            return nullptr;
        }
    }

    frame->data = data_buffer.release();
#else
    // Generate simulated image data
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    frame->data = data_buffer.release();

    // Fill with simulated star field (16-bit)
    uint16_t* data16 = static_cast<uint16_t*>(frame->data);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> noise_dist(0, 30);

    for (size_t i = 0; i < pixelCount; ++i) {
        int noise = noise_dist(gen) - 15;  // ±15 ADU noise
        int star = 0;
        if (gen() % 50000 < 3) {  // 0.006% chance of star
            star = gen() % 20000 + 5000;  // Very bright star
        }
        data16[i] = static_cast<uint16_t>(std::clamp(800 + noise + star, 0, 65535));
    }
#endif

    return frame;
}

auto SBIGCamera::temperatureThreadFunction() -> void {
    while (cooler_enabled_) {
        try {
            updateTemperatureInfo();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in temperature thread: {}", e.what());
            break;
        }
    }
}

auto SBIGCamera::updateTemperatureInfo() -> bool {
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    QueryTemperatureStatusResults tempResults;
    if (SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, nullptr, &tempResults) == CE_NO_ERROR) {
        current_temperature_ = (tempResults.imagingCCDTemperature / 100.0) - 273.15;
        cooling_power_ = tempResults.coolerPower;
    }
#else
    // Simulate temperature convergence
    double temp_diff = target_temperature_ - current_temperature_;
    current_temperature_ += temp_diff * 0.02;  // Very gradual convergence (realistic for SBIG)
    cooling_power_ = std::min(std::abs(temp_diff) * 2.0, 100.0);
#endif
    return true;
}

auto SBIGCamera::isValidExposureTime(double duration) const -> bool {
    return duration >= 0.01 && duration <= 3600.0;  // 10ms to 1 hour
}

auto SBIGCamera::isValidResolution(int x, int y, int width, int height) const -> bool {
    int maxW = (current_chip_ == ChipType::IMAGING) ? max_width_ : guide_chip_width_;
    int maxH = (current_chip_ == ChipType::IMAGING) ? max_height_ : guide_chip_height_;

    return x >= 0 && y >= 0 &&
           width > 0 && height > 0 &&
           x + width <= maxW &&
           y + height <= maxH;
}

auto SBIGCamera::isValidBinning(int binX, int binY) const -> bool {
    return binX >= 1 && binX <= 9 && binY >= 1 && binY <= 9;
}

} // namespace lithium::device::sbig::camera
