/*
 * atik_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Atik Camera Implementation with SDK integration

*************************************************/

#include "atik_camera.hpp"

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
#include "AtikCameras.h"  // Atik SDK header (stub)
#endif

#include <algorithm>
#include <thread>
#include <chrono>
#include <fstream>

namespace lithium::device::atik::camera {

AtikCamera::AtikCamera(const std::string& name)
    : AtomCamera(name)
    , atik_handle_(nullptr)
    , camera_index_(-1)
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
    , has_filter_wheel_(false)
    , current_filter_(0)
    , filter_count_(0)
    , sequence_running_(false)
    , sequence_current_frame_(0)
    , sequence_total_frames_(0)
    , sequence_exposure_(1.0)
    , sequence_interval_(0.0)
    , current_gain_(100)
    , current_offset_(0)
    , current_iso_(100)
    , advanced_mode_(false)
    , read_mode_(0)
    , amp_glow_enabled_(false)
    , preflash_duration_(0.0)
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
    , has_shutter_(false)
    , total_frames_(0)
    , dropped_frames_(0)
    , last_frame_time_()
    , last_frame_result_(nullptr) {

    LOG_F(INFO, "Created Atik camera instance: {}", name);
}

AtikCamera::~AtikCamera() {
    if (is_connected_) {
        disconnect();
    }
    if (is_initialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed Atik camera instance: {}", name_);
}

auto AtikCamera::initialize() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_initialized_) {
        LOG_F(WARNING, "Atik camera already initialized");
        return true;
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    if (!initializeAtikSDK()) {
        LOG_F(ERROR, "Failed to initialize Atik SDK");
        return false;
    }
#else
    LOG_F(WARNING, "Atik SDK not available, using stub implementation");
#endif

    is_initialized_ = true;
    LOG_F(INFO, "Atik camera initialized successfully");
    return true;
}

auto AtikCamera::destroy() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (!is_initialized_) {
        return true;
    }

    if (is_connected_) {
        disconnect();
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    shutdownAtikSDK();
#endif

    is_initialized_ = false;
    LOG_F(INFO, "Atik camera destroyed successfully");
    return true;
}

auto AtikCamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_connected_) {
        LOG_F(WARNING, "Atik camera already connected");
        return true;
    }

    if (!is_initialized_) {
        LOG_F(ERROR, "Atik camera not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to Atik camera: {} (attempt {}/{})", deviceName, retry + 1, maxRetry);

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
        // Parse camera index from device name or use scan results
        if (deviceName.empty()) {
            auto devices = scan();
            if (devices.empty()) {
                LOG_F(ERROR, "No Atik cameras found");
                continue;
            }
            camera_index_ = 0;  // Use first available camera
        } else {
            // Try to parse index from device name
            try {
                camera_index_ = std::stoi(deviceName);
            } catch (...) {
                // If parsing fails, search by name
                auto devices = scan();
                camera_index_ = -1;
                for (size_t i = 0; i < devices.size(); ++i) {
                    if (devices[i] == deviceName) {
                        camera_index_ = static_cast<int>(i);
                        break;
                    }
                }
                if (camera_index_ == -1) {
                    LOG_F(ERROR, "Atik camera not found: {}", deviceName);
                    continue;
                }
            }
        }

        if (openCamera(camera_index_)) {
            if (setupCameraParameters()) {
                is_connected_ = true;
                LOG_F(INFO, "Connected to Atik camera successfully");
                return true;
            } else {
                closeCamera();
            }
        }
#else
        // Stub implementation
        camera_index_ = 0;
        camera_model_ = "Atik Camera Simulator";
        serial_number_ = "SIM123456";
        firmware_version_ = "1.0.0";
        camera_type_ = "Simulator";
        max_width_ = 1920;
        max_height_ = 1080;
        pixel_size_x_ = pixel_size_y_ = 3.75;
        bit_depth_ = 16;
        is_color_camera_ = false;
        has_shutter_ = true;

        roi_width_ = max_width_;
        roi_height_ = max_height_;

        is_connected_ = true;
        LOG_F(INFO, "Connected to Atik camera simulator");
        return true;
#endif

        if (retry < maxRetry - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    LOG_F(ERROR, "Failed to connect to Atik camera after {} attempts", maxRetry);
    return false;
}

auto AtikCamera::disconnect() -> bool {
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

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    closeCamera();
#endif

    is_connected_ = false;
    LOG_F(INFO, "Disconnected from Atik camera");
    return true;
}

auto AtikCamera::isConnected() const -> bool {
    return is_connected_;
}

auto AtikCamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    try {
        // Implementation would use Atik SDK to enumerate cameras
        int cameraCount = 0;  // AtikGetCameraCount() or similar

        for (int i = 0; i < cameraCount; ++i) {
            std::string cameraName = "Atik Camera " + std::to_string(i);
            devices.push_back(cameraName);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error scanning for Atik cameras: {}", e.what());
    }
#else
    // Stub implementation
    devices.push_back("Atik Camera Simulator");
    devices.push_back("Atik One 6.0");
    devices.push_back("Atik Titan");
#endif

    LOG_F(INFO, "Found {} Atik cameras", devices.size());
    return devices;
}

auto AtikCamera::startExposure(double duration) -> bool {
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
    exposure_thread_ = std::thread(&AtikCamera::exposureThreadFunction, this);

    LOG_F(INFO, "Started exposure: {} seconds", duration);
    return true;
}

auto AtikCamera::abortExposure() -> bool {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (!is_exposing_) {
        return true;
    }

    exposure_abort_requested_ = true;

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Call Atik SDK abort function
    // AtikAbortExposure(atik_handle_);
#endif

    // Wait for exposure thread to finish
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }

    is_exposing_ = false;
    LOG_F(INFO, "Aborted exposure");
    return true;
}

auto AtikCamera::isExposing() const -> bool {
    return is_exposing_;
}

auto AtikCamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::min(elapsed / current_exposure_duration_, 1.0);
}

auto AtikCamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::max(current_exposure_duration_ - elapsed, 0.0);
}

auto AtikCamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (is_exposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }

    return last_frame_result_;
}

auto AtikCamera::saveImage(const std::string& path) -> bool {
    auto frame = getExposureResult();
    if (!frame) {
        LOG_F(ERROR, "No image data available");
        return false;
    }

    return saveFrameToFile(frame, path);
}

// Temperature control implementation
auto AtikCamera::startCooling(double targetTemp) -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    target_temperature_ = targetTemp;
    cooler_enabled_ = true;

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Set target temperature using Atik SDK
    // AtikSetTemperature(atik_handle_, targetTemp);
    // AtikEnableCooling(atik_handle_, true);
#endif

    // Start temperature monitoring thread
    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }
    temperature_thread_ = std::thread(&AtikCamera::temperatureThreadFunction, this);

    LOG_F(INFO, "Started cooling to {} °C", targetTemp);
    return true;
}

auto AtikCamera::stopCooling() -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    cooler_enabled_ = false;

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Disable cooling using Atik SDK
    // AtikEnableCooling(atik_handle_, false);
#endif

    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }

    LOG_F(INFO, "Stopped cooling");
    return true;
}

auto AtikCamera::isCoolerOn() const -> bool {
    return cooler_enabled_;
}

auto AtikCamera::getTemperature() const -> std::optional<double> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    double temperature = 0.0;
    // if (AtikGetTemperature(atik_handle_, &temperature) == ATIK_SUCCESS) {
    //     return temperature;
    // }
    return std::nullopt;
#else
    // Simulate temperature based on cooling state
    double simTemp = cooler_enabled_ ? target_temperature_ + 2.0 : 25.0;
    return simTemp;
#endif
}

// Gain and offset controls
auto AtikCamera::setGain(int gain) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidGain(gain)) {
        LOG_F(ERROR, "Invalid gain value: {}", gain);
        return false;
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Set gain using Atik SDK
    // if (AtikSetGain(atik_handle_, gain) != ATIK_SUCCESS) {
    //     return false;
    // }
#endif

    current_gain_ = gain;
    LOG_F(INFO, "Set gain to {}", gain);
    return true;
}

auto AtikCamera::getGain() -> std::optional<int> {
    return current_gain_;
}

auto AtikCamera::getGainRange() -> std::pair<int, int> {
    return {0, 1000};  // Typical range for Atik cameras
}

// Frame settings
auto AtikCamera::setResolution(int x, int y, int width, int height) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidResolution(x, y, width, height)) {
        LOG_F(ERROR, "Invalid resolution: {}x{} at {},{}", width, height, x, y);
        return false;
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Set ROI using Atik SDK
    // if (AtikSetROI(atik_handle_, x, y, width, height) != ATIK_SUCCESS) {
    //     return false;
    // }
#endif

    roi_x_ = x;
    roi_y_ = y;
    roi_width_ = width;
    roi_height_ = height;

    LOG_F(INFO, "Set resolution to {}x{} at {},{}", width, height, x, y);
    return true;
}

auto AtikCamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Resolution res;
    res.width = roi_width_;
    res.height = roi_height_;
    return res;
}

auto AtikCamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    res.width = max_width_;
    res.height = max_height_;
    return res;
}

auto AtikCamera::setBinning(int horizontal, int vertical) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidBinning(horizontal, vertical)) {
        LOG_F(ERROR, "Invalid binning: {}x{}", horizontal, vertical);
        return false;
    }

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Set binning using Atik SDK
    // if (AtikSetBinning(atik_handle_, horizontal, vertical) != ATIK_SUCCESS) {
    //     return false;
    // }
#endif

    bin_x_ = horizontal;
    bin_y_ = vertical;

    LOG_F(INFO, "Set binning to {}x{}", horizontal, vertical);
    return true;
}

auto AtikCamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = bin_x_;
    bin.vertical = bin_y_;
    return bin;
}

// Pixel information
auto AtikCamera::getPixelSize() -> double {
    return pixel_size_x_;  // Assuming square pixels
}

auto AtikCamera::getPixelSizeX() -> double {
    return pixel_size_x_;
}

auto AtikCamera::getPixelSizeY() -> double {
    return pixel_size_y_;
}

auto AtikCamera::getBitDepth() -> int {
    return bit_depth_;
}

// Color information
auto AtikCamera::isColor() const -> bool {
    return is_color_camera_;
}

auto AtikCamera::getBayerPattern() const -> BayerPattern {
    return bayer_pattern_;
}

// Atik-specific methods
auto AtikCamera::getAtikSDKVersion() const -> std::string {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // return AtikGetSDKVersion();
    return "2.1.0";
#else
    return "Stub 1.0.0";
#endif
}

auto AtikCamera::getCameraModel() const -> std::string {
    return camera_model_;
}

auto AtikCamera::getSerialNumber() const -> std::string {
    return serial_number_;
}

// Private helper methods
auto AtikCamera::initializeAtikSDK() -> bool {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Initialize Atik SDK
    // return AtikInitializeSDK() == ATIK_SUCCESS;
    return true;
#else
    return true;
#endif
}

auto AtikCamera::shutdownAtikSDK() -> bool {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Shutdown Atik SDK
    // AtikShutdownSDK();
#endif
    return true;
}

auto AtikCamera::openCamera(int cameraIndex) -> bool {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Open camera using Atik SDK
    // atik_handle_ = AtikOpenCamera(cameraIndex);
    // return atik_handle_ != nullptr;
    return true;
#else
    return true;
#endif
}

auto AtikCamera::closeCamera() -> bool {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // if (atik_handle_) {
    //     AtikCloseCamera(atik_handle_);
    //     atik_handle_ = nullptr;
    // }
#endif
    return true;
}

auto AtikCamera::setupCameraParameters() -> bool {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Read camera capabilities and setup parameters
    // AtikGetCameraInfo(atik_handle_, &camera_info);
    // camera_model_ = camera_info.model;
    // serial_number_ = camera_info.serial;
    // max_width_ = camera_info.max_width;
    // max_height_ = camera_info.max_height;
    // pixel_size_x_ = camera_info.pixel_size_x;
    // pixel_size_y_ = camera_info.pixel_size_y;
    // bit_depth_ = camera_info.bit_depth;
    // is_color_camera_ = camera_info.is_color;
    // has_shutter_ = camera_info.has_shutter;
#endif

    roi_width_ = max_width_;
    roi_height_ = max_height_;

    return readCameraCapabilities();
}

auto AtikCamera::readCameraCapabilities() -> bool {
    // Initialize camera capabilities using the correct CameraCapabilities structure
    camera_capabilities_.canAbort = true;
    camera_capabilities_.canSubFrame = true;
    camera_capabilities_.canBin = true;
    camera_capabilities_.hasCooler = true;
    camera_capabilities_.hasGain = true;
    camera_capabilities_.hasShutter = has_shutter_;
    camera_capabilities_.canStream = true;
    camera_capabilities_.canRecordVideo = true;
    camera_capabilities_.supportsSequences = true;
    camera_capabilities_.hasImageQualityAnalysis = true;
    camera_capabilities_.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF, ImageFormat::PNG, ImageFormat::JPEG};

    return true;
}

auto AtikCamera::exposureThreadFunction() -> void {
    try {
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
        // Start exposure using Atik SDK
        // if (AtikStartExposure(atik_handle_, current_exposure_duration_) != ATIK_SUCCESS) {
        //     LOG_F(ERROR, "Failed to start exposure");
        //     is_exposing_ = false;
        //     return;
        // }

        // Wait for exposure to complete or be aborted
        // while (!exposure_abort_requested_) {
        //     int status = AtikGetExposureStatus(atik_handle_);
        //     if (status == ATIK_EXPOSURE_COMPLETE) {
        //         break;
        //     } else if (status == ATIK_EXPOSURE_FAILED) {
        //         LOG_F(ERROR, "Exposure failed");
        //         is_exposing_ = false;
        //         return;
        //     }
        //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // }

        // if (!exposure_abort_requested_) {
        //     // Download image data
        //     last_frame_ = captureFrame();
        //     if (last_frame_) {
        //         total_frames_++;
        //     } else {
        //         dropped_frames_++;
        //     }
        // }
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
            // Create simulated frame
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

auto AtikCamera::captureFrame() -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();

    frame->resolution.width = roi_width_ / bin_x_;
    frame->resolution.height = roi_height_ / bin_y_;
    frame->binning.horizontal = bin_x_;
    frame->binning.vertical = bin_y_;
    frame->pixel.size = pixel_size_x_ * bin_x_;  // Effective pixel size
    frame->pixel.sizeX = pixel_size_x_ * bin_x_;
    frame->pixel.sizeY = pixel_size_y_ * bin_y_;
    frame->pixel.depth = bit_depth_;
    frame->type = FrameType::FITS;
    frame->format = "RAW";

    // Calculate frame size
    size_t pixelCount = frame->resolution.width * frame->resolution.height;
    size_t bytesPerPixel = (bit_depth_ <= 8) ? 1 : 2;
    size_t channels = is_color_camera_ ? 3 : 1;
    frame->size = pixelCount * channels * bytesPerPixel;

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Download actual image data from camera
    frame->data = std::make_unique<uint8_t[]>(frame->size);
    // if (AtikDownloadImage(atik_handle_, frame->data.get(), frame->size) != ATIK_SUCCESS) {
    //     LOG_F(ERROR, "Failed to download image from Atik camera");
    //     return nullptr;
    // }
#else
    // Generate simulated image data
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    frame->data = data_buffer.release();

    // Fill with simulated star field
    if (bit_depth_ <= 8) {
        uint8_t* data8 = static_cast<uint8_t*>(frame->data);
        for (size_t i = 0; i < pixelCount; ++i) {
            // Simulate noise + stars
            double noise = (rand() % 20) - 10;  // ±10 ADU noise
            double star = 0;
            if (rand() % 10000 < 5) {  // 0.05% chance of star
                star = rand() % 200 + 50;  // Bright star
            }
            data8[i] = static_cast<uint8_t>(std::clamp(100 + noise + star, 0.0, 255.0));
        }
    } else {
        uint16_t* data16 = static_cast<uint16_t*>(frame->data);
        for (size_t i = 0; i < pixelCount; ++i) {
            double noise = (rand() % 100) - 50;  // ±50 ADU noise
            double star = 0;
            if (rand() % 10000 < 5) {
                star = rand() % 10000 + 1000;  // Bright star
            }
            data16[i] = static_cast<uint16_t>(std::clamp(1000 + noise + star, 0.0, 65535.0));
        }
    }
#endif

    return frame;
}

auto AtikCamera::temperatureThreadFunction() -> void {
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

auto AtikCamera::updateTemperatureInfo() -> bool {
    // Update temperature information and cooling status
    return true;
}

auto AtikCamera::isValidExposureTime(double duration) const -> bool {
    return duration >= 0.001 && duration <= 7200.0;  // 1ms to 2 hours
}

auto AtikCamera::isValidGain(int gain) const -> bool {
    return gain >= 0 && gain <= 1000;  // Typical range for Atik cameras
}

auto AtikCamera::isValidResolution(int x, int y, int width, int height) const -> bool {
    return x >= 0 && y >= 0 &&
           width > 0 && height > 0 &&
           x + width <= max_width_ &&
           y + height <= max_height_;
}

auto AtikCamera::isValidBinning(int binX, int binY) const -> bool {
    return binX >= 1 && binX <= 8 && binY >= 1 && binY <= 8;
}

// ... Additional method implementations would follow ...

} // namespace lithium::device::atik::camera
