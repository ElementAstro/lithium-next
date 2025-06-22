/*
 * fli_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: FLI Camera Implementation with SDK integration

*************************************************/

#include "fli_camera.hpp"

#ifdef LITHIUM_FLI_CAMERA_ENABLED
#include "libfli.h"  // FLI SDK header (stub)
#endif

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>

namespace lithium::device::fli::camera {

FLICamera::FLICamera(const std::string& name)
    : AtomCamera(name)
    , fli_device_(0)  // Will use proper invalid value with SDK
    , device_name_("")
    , camera_model_("")
    , serial_number_("")
    , firmware_version_("")
    , camera_type_("")
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
    , base_temperature_(25.0)
    , has_filter_wheel_(false)
    , filter_device_(0)
    , current_filter_(0)
    , filter_count_(0)
    , filter_wheel_homed_(false)
    , has_focuser_(false)
    , focuser_device_(0)
    , focuser_position_(0)
    , focuser_min_(0)
    , focuser_max_(10000)
    , step_size_(1.0)
    , focuser_homed_(false)
    , sequence_running_(false)
    , sequence_current_frame_(0)
    , sequence_total_frames_(0)
    , sequence_exposure_(1.0)
    , sequence_interval_(0.0)
    , current_gain_(100)
    , current_offset_(0)
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
    , total_frames_(0)
    , dropped_frames_(0)
    , last_frame_result_(nullptr) {
    
    LOG_F(INFO, "Created FLI camera instance: {}", name);
}

FLICamera::~FLICamera() {
    if (is_connected_) {
        disconnect();
    }
    if (is_initialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed FLI camera instance: {}", name_);
}

auto FLICamera::initialize() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);
    
    if (is_initialized_) {
        LOG_F(WARNING, "FLI camera already initialized");
        return true;
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    if (!initializeFLISDK()) {
        LOG_F(ERROR, "Failed to initialize FLI SDK");
        return false;
    }
#else
    LOG_F(WARNING, "FLI SDK not available, using stub implementation");
#endif

    is_initialized_ = true;
    LOG_F(INFO, "FLI camera initialized successfully");
    return true;
}

auto FLICamera::destroy() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);
    
    if (!is_initialized_) {
        return true;
    }

    if (is_connected_) {
        disconnect();
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    shutdownFLISDK();
#endif

    is_initialized_ = false;
    LOG_F(INFO, "FLI camera destroyed successfully");
    return true;
}

auto FLICamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);
    
    if (is_connected_) {
        LOG_F(WARNING, "FLI camera already connected");
        return true;
    }

    if (!is_initialized_) {
        LOG_F(ERROR, "FLI camera not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to FLI camera: {} (attempt {}/{})", deviceName, retry + 1, maxRetry);

#ifdef LITHIUM_FLI_CAMERA_ENABLED
        if (deviceName.empty()) {
            auto devices = scan();
            if (devices.empty()) {
                LOG_F(ERROR, "No FLI cameras found");
                continue;
            }
            camera_index_ = 0;
        } else {
            auto devices = scan();
            camera_index_ = -1;
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i] == deviceName) {
                    camera_index_ = static_cast<int>(i);
                    break;
                }
            }
            if (camera_index_ == -1) {
                LOG_F(ERROR, "FLI camera not found: {}", deviceName);
                continue;
            }
        }

        if (openCamera(camera_index_)) {
            if (setupCameraParameters()) {
                is_connected_ = true;
                LOG_F(INFO, "Connected to FLI camera successfully");
                return true;
            } else {
                closeCamera();
            }
        }
#else
        // Stub implementation
        camera_index_ = 0;
        camera_model_ = "FLI Camera Simulator";
        serial_number_ = "SIM789012";
        firmware_version_ = "1.5.0";
        camera_type_ = "ProLine";
        max_width_ = 2048;
        max_height_ = 2048;
        pixel_size_x_ = pixel_size_y_ = 13.5;
        bit_depth_ = 16;
        is_color_camera_ = false;
        has_shutter_ = true;
        has_focuser_ = true;
        
        roi_width_ = max_width_;
        roi_height_ = max_height_;
        
        is_connected_ = true;
        LOG_F(INFO, "Connected to FLI camera simulator");
        return true;
#endif

        if (retry < maxRetry - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    LOG_F(ERROR, "Failed to connect to FLI camera after {} attempts", maxRetry);
    return false;
}

auto FLICamera::disconnect() -> bool {
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

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    closeCamera();
#endif

    is_connected_ = false;
    LOG_F(INFO, "Disconnected from FLI camera");
    return true;
}

auto FLICamera::isConnected() const -> bool {
    return is_connected_;
}

auto FLICamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    try {
        char **names;
        long domain = FLIDOMAIN_USB | FLIDEVICE_CAMERA;
        
        if (FLIList(domain, &names) == 0) {
            for (int i = 0; names[i] != nullptr; ++i) {
                devices.push_back(std::string(names[i]));
                delete[] names[i];
            }
            delete[] names;
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error scanning for FLI cameras: {}", e.what());
    }
#else
    // Stub implementation
    devices.push_back("FLI Camera Simulator");
    devices.push_back("FLI ProLine 16801");
    devices.push_back("FLI MicroLine 8300");
#endif

    LOG_F(INFO, "Found {} FLI cameras", devices.size());
    return devices;
}

auto FLICamera::startExposure(double duration) -> bool {
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
    exposure_thread_ = std::thread(&FLICamera::exposureThreadFunction, this);

    LOG_F(INFO, "Started exposure: {} seconds", duration);
    return true;
}

auto FLICamera::abortExposure() -> bool {
    std::lock_guard<std::mutex> lock(exposure_mutex_);
    
    if (!is_exposing_) {
        return true;
    }

    exposure_abort_requested_ = true;
    
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    FLICancelExposure(fli_device_);
#endif

    // Wait for exposure thread to finish
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }

    is_exposing_ = false;
    LOG_F(INFO, "Aborted exposure");
    return true;
}

auto FLICamera::isExposing() const -> bool {
    return is_exposing_;
}

auto FLICamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::min(elapsed / current_exposure_duration_, 1.0);
}

auto FLICamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::max(current_exposure_duration_ - elapsed, 0.0);
}

auto FLICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(exposure_mutex_);
    
    if (is_exposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }

    return last_frame_result_;
}

auto FLICamera::saveImage(const std::string& path) -> bool {
    auto frame = getExposureResult();
    if (!frame) {
        LOG_F(ERROR, "No image data available");
        return false;
    }

    return saveFrameToFile(frame, path);
}

// Temperature control implementation
auto FLICamera::startCooling(double targetTemp) -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    target_temperature_ = targetTemp;
    cooler_enabled_ = true;

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    FLISetTemperature(fli_device_, targetTemp);
#endif

    // Start temperature monitoring thread
    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }
    temperature_thread_ = std::thread(&FLICamera::temperatureThreadFunction, this);

    LOG_F(INFO, "Started cooling to {} °C", targetTemp);
    return true;
}

auto FLICamera::stopCooling() -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    cooler_enabled_ = false;

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // FLI cameras automatically control cooling
#endif

    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }

    LOG_F(INFO, "Stopped cooling");
    return true;
}

auto FLICamera::isCoolerOn() const -> bool {
    return cooler_enabled_;
}

auto FLICamera::getTemperature() const -> std::optional<double> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    double temperature = 0.0;
    if (FLIGetTemperature(fli_device_, &temperature) == 0) {
        return temperature;
    }
    return std::nullopt;
#else
    // Simulate temperature based on cooling state
    double simTemp = cooler_enabled_ ? target_temperature_ + 1.0 : 25.0;
    return simTemp;
#endif
}

// FLI-specific focuser controls
auto FLICamera::setFocuserPosition(int position) -> bool {
    if (!is_connected_ || !has_focuser_) {
        LOG_F(ERROR, "Focuser not available");
        return false;
    }

    if (position < 0 || position > focuser_max_position_) {
        LOG_F(ERROR, "Invalid focuser position: {}", position);
        return false;
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    if (FLIStepMotorAsync(fli_device_, position - focuser_position_) != 0) {
        return false;
    }
#endif

    focuser_position_ = position;
    LOG_F(INFO, "Set focuser position to {}", position);
    return true;
}

auto FLICamera::getFocuserPosition() const -> int {
    return focuser_position_;
}

auto FLICamera::getFocuserMaxPosition() const -> int {
    return focuser_max_position_;
}

auto FLICamera::isFocuserMoving() const -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    long status;
    if (FLIGetStepperPosition(fli_device_, &status) == 0) {
        return status != focuser_position_;
    }
#endif
    return false;
}

// Gain and offset controls
auto FLICamera::setGain(int gain) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidGain(gain)) {
        LOG_F(ERROR, "Invalid gain value: {}", gain);
        return false;
    }

    // FLI cameras typically use readout mode instead of direct gain
    current_gain_ = gain;
    LOG_F(INFO, "Set gain to {}", gain);
    return true;
}

auto FLICamera::getGain() -> std::optional<int> {
    return current_gain_;
}

auto FLICamera::getGainRange() -> std::pair<int, int> {
    return {0, 100};  // FLI cameras typically have limited gain control
}

// Frame settings
auto FLICamera::setResolution(int x, int y, int width, int height) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidResolution(x, y, width, height)) {
        LOG_F(ERROR, "Invalid resolution: {}x{} at {},{}", width, height, x, y);
        return false;
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    if (FLISetImageArea(fli_device_, x, y, x + width, y + height) != 0) {
        return false;
    }
#endif

    roi_x_ = x;
    roi_y_ = y;
    roi_width_ = width;
    roi_height_ = height;

    LOG_F(INFO, "Set resolution to {}x{} at {},{}", width, height, x, y);
    return true;
}

auto FLICamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Resolution res;
    res.width = roi_width_;
    res.height = roi_height_;
    return res;
}

auto FLICamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    res.width = max_width_;
    res.height = max_height_;
    return res;
}

auto FLICamera::setBinning(int horizontal, int vertical) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidBinning(horizontal, vertical)) {
        LOG_F(ERROR, "Invalid binning: {}x{}", horizontal, vertical);
        return false;
    }

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    if (FLISetHBin(fli_device_, horizontal) != 0 || FLISetVBin(fli_device_, vertical) != 0) {
        return false;
    }
#endif

    bin_x_ = horizontal;
    bin_y_ = vertical;

    LOG_F(INFO, "Set binning to {}x{}", horizontal, vertical);
    return true;
}

auto FLICamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = bin_x_;
    bin.vertical = bin_y_;
    return bin;
}

// Pixel information
auto FLICamera::getPixelSize() -> double {
    return pixel_size_x_;  // Assuming square pixels
}

auto FLICamera::getPixelSizeX() -> double {
    return pixel_size_x_;
}

auto FLICamera::getPixelSizeY() -> double {
    return pixel_size_y_;
}

auto FLICamera::getBitDepth() -> int {
    return bit_depth_;
}

// Color information
auto FLICamera::isColor() const -> bool {
    return is_color_camera_;
}

auto FLICamera::getBayerPattern() const -> BayerPattern {
    return bayer_pattern_;
}

// FLI-specific methods
auto FLICamera::getFLISDKVersion() const -> std::string {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    char version[256];
    if (FLIGetLibVersion(version, sizeof(version)) == 0) {
        return std::string(version);
    }
    return "Unknown";
#else
    return "Stub 1.0.0";
#endif
}

auto FLICamera::getCameraModel() const -> std::string {
    return camera_model_;
}

auto FLICamera::getSerialNumber() const -> std::string {
    return serial_number_;
}

// Private helper methods
auto FLICamera::initializeFLISDK() -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // FLI SDK initializes automatically
    return true;
#else
    return true;
#endif
}

auto FLICamera::shutdownFLISDK() -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // FLI SDK cleans up automatically
#endif
    return true;
}

auto FLICamera::openCamera(int cameraIndex) -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    char **names;
    long domain = FLIDOMAIN_USB | FLIDEVICE_CAMERA;
    
    if (FLIList(domain, &names) == 0) {
        if (cameraIndex >= 0 && names[cameraIndex] != nullptr) {
            if (FLIOpen(&fli_device_, names[cameraIndex], domain) == 0) {
                // Cleanup names
                for (int i = 0; names[i] != nullptr; ++i) {
                    delete[] names[i];
                }
                delete[] names;
                return true;
            }
        }
        
        // Cleanup on failure
        for (int i = 0; names[i] != nullptr; ++i) {
            delete[] names[i];
        }
        delete[] names;
    }
    return false;
#else
    return true;
#endif
}

auto FLICamera::closeCamera() -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    if (fli_device_ != INVALID_DEVICE) {
        FLIClose(fli_device_);
        fli_device_ = INVALID_DEVICE;
    }
#endif
    return true;
}

auto FLICamera::setupCameraParameters() -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // Get camera information
    long ul_x, ul_y, lr_x, lr_y;
    if (FLIGetArrayArea(fli_device_, &ul_x, &ul_y, &lr_x, &lr_y) == 0) {
        max_width_ = lr_x - ul_x;
        max_height_ = lr_y - ul_y;
    }
    
    double pixel_x, pixel_y;
    if (FLIGetPixelSize(fli_device_, &pixel_x, &pixel_y) == 0) {
        pixel_size_x_ = pixel_x;
        pixel_size_y_ = pixel_y;
    }
    
    char model[256];
    if (FLIGetModel(fli_device_, model, sizeof(model)) == 0) {
        camera_model_ = std::string(model);
    }
    
    // Check for focuser
    long focuser_extent;
    if (FLIGetFocuserExtent(fli_device_, &focuser_extent) == 0) {
        has_focuser_ = true;
        focuser_max_position_ = static_cast<int>(focuser_extent);
    }
#endif

    roi_width_ = max_width_;
    roi_height_ = max_height_;
    
    return readCameraCapabilities();
}

auto FLICamera::readCameraCapabilities() -> bool {
    // Initialize camera capabilities using the correct CameraCapabilities structure
    camera_capabilities_.canAbort = true;
    camera_capabilities_.canSubFrame = true;
    camera_capabilities_.canBin = true;
    camera_capabilities_.hasCooler = true;
    camera_capabilities_.hasShutter = has_shutter_;
    camera_capabilities_.canStream = false; // FLI cameras are primarily for imaging
    camera_capabilities_.canRecordVideo = false;
    camera_capabilities_.supportsSequences = true;
    camera_capabilities_.hasImageQualityAnalysis = true;
    camera_capabilities_.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF};

    return true;
}

auto FLICamera::exposureThreadFunction() -> void {
    try {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
        // Start exposure
        long duration_ms = static_cast<long>(current_exposure_duration_ * 1000);
        if (FLIExposeFrame(fli_device_) != 0) {
            LOG_F(ERROR, "Failed to start exposure");
            is_exposing_ = false;
            return;
        }
        
        // Set exposure time
        if (FLISetExposureTime(fli_device_, duration_ms) != 0) {
            LOG_F(ERROR, "Failed to set exposure time");
            is_exposing_ = false;
            return;
        }

        // Wait for exposure to complete
        long time_left;
        do {
            if (exposure_abort_requested_) {
                break;
            }
            
            if (FLIGetExposureStatus(fli_device_, &time_left) != 0) {
                LOG_F(ERROR, "Failed to get exposure status");
                is_exposing_ = false;
                return;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (time_left > 0);

        if (!exposure_abort_requested_) {
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

auto FLICamera::captureFrame() -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();
    
    frame->resolution.width = roi_width_ / bin_x_;
    frame->resolution.height = roi_height_ / bin_y_;
    frame->binning.horizontal = bin_x_;
    frame->binning.vertical = bin_y_;
    frame->pixel.size = pixel_size_x_ * bin_x_;
    frame->pixel.sizeX = pixel_size_x_ * bin_x_;
    frame->pixel.sizeY = pixel_size_y_ * bin_y_;
    frame->pixel.depth = bit_depth_;
    frame->type = FrameType::FITS;
    frame->format = "RAW";

    // Calculate frame size
    size_t pixelCount = frame->resolution.width * frame->resolution.height;
    size_t bytesPerPixel = (bit_depth_ <= 8) ? 1 : 2;
    frame->size = pixelCount * bytesPerPixel;

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // Download actual image data from camera
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    
    if (FLIGrabRow(fli_device_, data_buffer.get(), frame->resolution.width) == 0) {
        frame->data = data_buffer.release();
    } else {
        LOG_F(ERROR, "Failed to download image from FLI camera");
        return nullptr;
    }
#else
    // Generate simulated image data
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    frame->data = data_buffer.release();
    
    // Fill with simulated star field (16-bit)
    uint16_t* data16 = static_cast<uint16_t*>(frame->data);
    for (size_t i = 0; i < pixelCount; ++i) {
        double noise = (rand() % 50) - 25;  // ±25 ADU noise
        double star = 0;
        if (rand() % 20000 < 3) {  // 0.015% chance of star
            star = rand() % 15000 + 2000;  // Bright star
        }
        data16[i] = static_cast<uint16_t>(std::clamp(500 + noise + star, 0.0, 65535.0));
    }
#endif

    return frame;
}

auto FLICamera::temperatureThreadFunction() -> void {
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

auto FLICamera::updateTemperatureInfo() -> bool {
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    double temp;
    if (FLIGetTemperature(fli_device_, &temp) == 0) {
        current_temperature_ = temp;
        
        // Calculate cooling power (estimation)
        double temp_diff = std::abs(target_temperature_ - current_temperature_);
        cooling_power_ = std::min(temp_diff * 10.0, 100.0);
    }
#else
    // Simulate temperature convergence
    double temp_diff = target_temperature_ - current_temperature_;
    current_temperature_ += temp_diff * 0.1;  // Gradual convergence
    cooling_power_ = std::abs(temp_diff) * 5.0;
#endif
    return true;
}

auto FLICamera::isValidExposureTime(double duration) const -> bool {
    return duration >= 0.001 && duration <= 3600.0;  // 1ms to 1 hour
}

auto FLICamera::isValidGain(int gain) const -> bool {
    return gain >= 0 && gain <= 100;
}

auto FLICamera::isValidResolution(int x, int y, int width, int height) const -> bool {
    return x >= 0 && y >= 0 && 
           width > 0 && height > 0 &&
           x + width <= max_width_ && 
           y + height <= max_height_;
}

auto FLICamera::isValidBinning(int binX, int binY) const -> bool {
    return binX >= 1 && binX <= 8 && binY >= 1 && binY <= 8;
}

} // namespace lithium::device::fli::camera
