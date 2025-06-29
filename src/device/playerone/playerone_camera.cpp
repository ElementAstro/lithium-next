/*
 * playerone_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: PlayerOne Camera Implementation with SDK integration

*************************************************/

#include "playerone_camera.hpp"

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
#include "PlayerOneCamera.h"  // PlayerOne SDK header (stub)
#endif

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>

namespace lithium::device::playerone::camera {

PlayerOneCamera::PlayerOneCamera(const std::string& name)
    : AtomCamera(name)
    , camera_handle_(-1)
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
    , auto_exposure_enabled_(false)
    , auto_gain_enabled_(false)
    , cooler_enabled_(false)
    , target_temperature_(-10.0)
    , current_temperature_(25.0)
    , cooling_power_(0.0)
    , sequence_running_(false)
    , sequence_current_frame_(0)
    , sequence_total_frames_(0)
    , sequence_exposure_(1.0)
    , sequence_interval_(0.0)
    , current_gain_(100)
    , current_offset_(0)
    , current_iso_(100)
    , hardware_binning_enabled_(true)
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
    , has_shutter_(false)  // Most PlayerOne cameras don't have mechanical shutters
    , total_frames_(0)
    , dropped_frames_(0)
    , last_frame_result_(nullptr) {

    LOG_F(INFO, "Created PlayerOne camera instance: {}", name);
}

PlayerOneCamera::~PlayerOneCamera() {
    if (is_connected_) {
        disconnect();
    }
    if (is_initialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed PlayerOne camera instance: {}", name_);
}

auto PlayerOneCamera::initialize() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_initialized_) {
        LOG_F(WARNING, "PlayerOne camera already initialized");
        return true;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (!initializePlayerOneSDK()) {
        LOG_F(ERROR, "Failed to initialize PlayerOne SDK");
        return false;
    }
#else
    LOG_F(WARNING, "PlayerOne SDK not available, using stub implementation");
#endif

    is_initialized_ = true;
    LOG_F(INFO, "PlayerOne camera initialized successfully");
    return true;
}

auto PlayerOneCamera::destroy() -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (!is_initialized_) {
        return true;
    }

    if (is_connected_) {
        disconnect();
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    shutdownPlayerOneSDK();
#endif

    is_initialized_ = false;
    LOG_F(INFO, "PlayerOne camera destroyed successfully");
    return true;
}

auto PlayerOneCamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(camera_mutex_);

    if (is_connected_) {
        LOG_F(WARNING, "PlayerOne camera already connected");
        return true;
    }

    if (!is_initialized_) {
        LOG_F(ERROR, "PlayerOne camera not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to PlayerOne camera: {} (attempt {}/{})", deviceName, retry + 1, maxRetry);

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
        auto devices = scan();
        camera_index_ = -1;

        if (deviceName.empty()) {
            if (!devices.empty()) {
                camera_index_ = 0;
            }
        } else {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i] == deviceName) {
                    camera_index_ = static_cast<int>(i);
                    break;
                }
            }
        }

        if (camera_index_ == -1) {
            LOG_F(ERROR, "PlayerOne camera not found: {}", deviceName);
            continue;
        }

        camera_handle_ = POAOpenCamera(camera_index_);
        if (camera_handle_ >= 0) {
            if (POAInitCamera(camera_handle_) == POA_OK) {
                if (setupCameraParameters()) {
                    is_connected_ = true;
                    LOG_F(INFO, "Connected to PlayerOne camera successfully");
                    return true;
                } else {
                    POACloseCamera(camera_handle_);
                    camera_handle_ = -1;
                }
            } else {
                POACloseCamera(camera_handle_);
                camera_handle_ = -1;
            }
        }
#else
        // Stub implementation
        camera_index_ = 0;
        camera_handle_ = 1;  // Fake handle
        camera_model_ = "PlayerOne Apollo Simulator";
        serial_number_ = "SIM555666";
        firmware_version_ = "2.1.0";
        max_width_ = 5496;
        max_height_ = 3672;
        pixel_size_x_ = pixel_size_y_ = 2.315;
        bit_depth_ = 16;
        is_color_camera_ = true;
        bayer_pattern_ = BayerPattern::RGGB;

        roi_width_ = max_width_;
        roi_height_ = max_height_;

        is_connected_ = true;
        LOG_F(INFO, "Connected to PlayerOne camera simulator");
        return true;
#endif

        if (retry < maxRetry - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    LOG_F(ERROR, "Failed to connect to PlayerOne camera after {} attempts", maxRetry);
    return false;
}

auto PlayerOneCamera::disconnect() -> bool {
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

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (camera_handle_ >= 0) {
        POACloseCamera(camera_handle_);
        camera_handle_ = -1;
    }
#endif

    is_connected_ = false;
    LOG_F(INFO, "Disconnected from PlayerOne camera");
    return true;
}

auto PlayerOneCamera::isConnected() const -> bool {
    return is_connected_;
}

auto PlayerOneCamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    int camera_count = POAGetCameraCount();
    for (int i = 0; i < camera_count; ++i) {
        POACameraProperties camera_props;
        if (POAGetCameraProperties(i, &camera_props) == POA_OK) {
            devices.push_back(std::string(camera_props.cameraModelName));
        }
    }
#else
    // Stub implementation
    devices.push_back("PlayerOne Apollo Simulator");
    devices.push_back("PlayerOne Uranus-C Pro");
    devices.push_back("PlayerOne Neptune-M");
#endif

    LOG_F(INFO, "Found {} PlayerOne cameras", devices.size());
    return devices;
}

auto PlayerOneCamera::startExposure(double duration) -> bool {
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
    exposure_thread_ = std::thread(&PlayerOneCamera::exposureThreadFunction, this);

    LOG_F(INFO, "Started exposure: {} seconds", duration);
    return true;
}

auto PlayerOneCamera::abortExposure() -> bool {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (!is_exposing_) {
        return true;
    }

    exposure_abort_requested_ = true;

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POAStopExposure(camera_handle_);
#endif

    // Wait for exposure thread to finish
    if (exposure_thread_.joinable()) {
        exposure_thread_.join();
    }

    is_exposing_ = false;
    LOG_F(INFO, "Aborted exposure");
    return true;
}

auto PlayerOneCamera::isExposing() const -> bool {
    return is_exposing_;
}

auto PlayerOneCamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::min(elapsed / current_exposure_duration_, 1.0);
}

auto PlayerOneCamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_time_).count();
    return std::max(current_exposure_duration_ - elapsed, 0.0);
}

auto PlayerOneCamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(exposure_mutex_);

    if (is_exposing_) {
        LOG_F(WARNING, "Exposure still in progress");
        return nullptr;
    }

    return last_frame_result_;
}

auto PlayerOneCamera::saveImage(const std::string& path) -> bool {
    auto frame = getExposureResult();
    if (!frame) {
        LOG_F(ERROR, "No image data available");
        return false;
    }

    return saveFrameToFile(frame, path);
}

// Video streaming implementation (PlayerOne strength)
auto PlayerOneCamera::startVideo() -> bool {
    std::lock_guard<std::mutex> lock(video_mutex_);

    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (is_video_running_) {
        LOG_F(WARNING, "Video already running");
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (POAStartExposure(camera_handle_, POA_TRUE) != POA_OK) {
        LOG_F(ERROR, "Failed to start video mode");
        return false;
    }
#endif

    is_video_running_ = true;

    // Start video thread
    if (video_thread_.joinable()) {
        video_thread_.join();
    }
    video_thread_ = std::thread(&PlayerOneCamera::videoThreadFunction, this);

    LOG_F(INFO, "Started video streaming");
    return true;
}

auto PlayerOneCamera::stopVideo() -> bool {
    std::lock_guard<std::mutex> lock(video_mutex_);

    if (!is_video_running_) {
        return true;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POAStopExposure(camera_handle_);
#endif

    is_video_running_ = false;

    if (video_thread_.joinable()) {
        video_thread_.join();
    }

    LOG_F(INFO, "Stopped video streaming");
    return true;
}

auto PlayerOneCamera::isVideoRunning() const -> bool {
    return is_video_running_;
}

auto PlayerOneCamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    if (!is_video_running_) {
        return nullptr;
    }

    return captureVideoFrame();
}

// Temperature control (if available)
auto PlayerOneCamera::startCooling(double targetTemp) -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!hasCooler()) {
        LOG_F(WARNING, "Camera does not have cooling capability");
        return false;
    }

    target_temperature_ = targetTemp;
    cooler_enabled_ = true;

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POASetConfig(camera_handle_, POA_COOLER_ON, POA_TRUE, POA_FALSE);
    POASetConfig(camera_handle_, POA_TARGET_TEMP, static_cast<long>(targetTemp), POA_FALSE);
#endif

    // Start temperature monitoring thread
    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }
    temperature_thread_ = std::thread(&PlayerOneCamera::temperatureThreadFunction, this);

    LOG_F(INFO, "Started cooling to {} °C", targetTemp);
    return true;
}

auto PlayerOneCamera::stopCooling() -> bool {
    std::lock_guard<std::mutex> lock(temperature_mutex_);

    cooler_enabled_ = false;

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POASetConfig(camera_handle_, POA_COOLER_ON, POA_FALSE, POA_FALSE);
#endif

    if (temperature_thread_.joinable()) {
        temperature_thread_.join();
    }

    LOG_F(INFO, "Stopped cooling");
    return true;
}

auto PlayerOneCamera::isCoolerOn() const -> bool {
    return cooler_enabled_;
}

auto PlayerOneCamera::getTemperature() const -> std::optional<double> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    long temp_value;
    POA_BOOL is_auto;
    if (POAGetConfig(camera_handle_, POA_TEMPERATURE, &temp_value, &is_auto) == POA_OK) {
        return static_cast<double>(temp_value) / 10.0;  // PlayerOne temperatures are in 0.1°C units
    }
    return std::nullopt;
#else
    // Simulate temperature based on cooling state
    double simTemp = cooler_enabled_ ? target_temperature_ + 1.0 : 25.0;
    return simTemp;
#endif
}

auto PlayerOneCamera::hasCooler() const -> bool {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POAConfigAttributes config_attrib;
    return (POAGetConfigAttributes(camera_handle_, POA_COOLER_ON, &config_attrib) == POA_OK);
#else
    // Some PlayerOne cameras have cooling, simulate based on model
    return camera_model_.find("Pro") != std::string::npos;
#endif
}

// Gain and offset controls (PlayerOne strength)
auto PlayerOneCamera::setGain(int gain) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidGain(gain)) {
        LOG_F(ERROR, "Invalid gain value: {}", gain);
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (POASetConfig(camera_handle_, POA_GAIN, gain, POA_FALSE) != POA_OK) {
        return false;
    }
#endif

    current_gain_ = gain;
    LOG_F(INFO, "Set gain to {}", gain);
    return true;
}

auto PlayerOneCamera::getGain() -> std::optional<int> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    long gain_value;
    POA_BOOL is_auto;
    if (POAGetConfig(camera_handle_, POA_GAIN, &gain_value, &is_auto) == POA_OK) {
        return static_cast<int>(gain_value);
    }
    return std::nullopt;
#else
    return current_gain_;
#endif
}

auto PlayerOneCamera::getGainRange() -> std::pair<int, int> {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POAConfigAttributes config_attrib;
    if (POAGetConfigAttributes(camera_handle_, POA_GAIN, &config_attrib) == POA_OK) {
        return {static_cast<int>(config_attrib.minValue), static_cast<int>(config_attrib.maxValue)};
    }
#endif
    return {0, 600};  // Typical range for PlayerOne cameras
}

auto PlayerOneCamera::setOffset(int offset) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidOffset(offset)) {
        LOG_F(ERROR, "Invalid offset value: {}", offset);
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (POASetConfig(camera_handle_, POA_OFFSET, offset, POA_FALSE) != POA_OK) {
        return false;
    }
#endif

    current_offset_ = offset;
    LOG_F(INFO, "Set offset to {}", offset);
    return true;
}

auto PlayerOneCamera::getOffset() -> std::optional<int> {
    if (!is_connected_) {
        return std::nullopt;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    long offset_value;
    POA_BOOL is_auto;
    if (POAGetConfig(camera_handle_, POA_OFFSET, &offset_value, &is_auto) == POA_OK) {
        return static_cast<int>(offset_value);
    }
    return std::nullopt;
#else
    return current_offset_;
#endif
}

// Hardware binning implementation (PlayerOne feature)
auto PlayerOneCamera::setBinning(int horizontal, int vertical) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    if (!isValidBinning(horizontal, vertical)) {
        LOG_F(ERROR, "Invalid binning: {}x{}", horizontal, vertical);
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (hardware_binning_enabled_) {
        // Use hardware binning
        if (POASetConfig(camera_handle_, POA_HARDWARE_BIN, horizontal, POA_FALSE) != POA_OK) {
            return false;
        }
    } else {
        // Use software binning or pixel combining
        if (POASetImageBin(camera_handle_, horizontal) != POA_OK) {
            return false;
        }
    }
#endif

    bin_x_ = horizontal;
    bin_y_ = vertical;

    // Update ROI size accordingly
    roi_width_ = max_width_ / bin_x_;
    roi_height_ = max_height_ / bin_y_;

    LOG_F(INFO, "Set binning to {}x{} (hardware: {})", horizontal, vertical, hardware_binning_enabled_);
    return true;
}

auto PlayerOneCamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!is_connected_) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = bin_x_;
    bin.vertical = bin_y_;
    return bin;
}

// Auto exposure and gain (PlayerOne feature)
auto PlayerOneCamera::enableAutoExposure(bool enable) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (POASetConfig(camera_handle_, POA_EXPOSURE, 0, enable ? POA_TRUE : POA_FALSE) != POA_OK) {
        return false;
    }
#endif

    auto_exposure_enabled_ = enable;
    LOG_F(INFO, "{} auto exposure", enable ? "Enabled" : "Disabled");
    return true;
}

auto PlayerOneCamera::enableAutoGain(bool enable) -> bool {
    if (!is_connected_) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    if (POASetConfig(camera_handle_, POA_GAIN, 0, enable ? POA_TRUE : POA_FALSE) != POA_OK) {
        return false;
    }
#endif

    auto_gain_enabled_ = enable;
    LOG_F(INFO, "{} auto gain", enable ? "Enabled" : "Disabled");
    return true;
}

// PlayerOne-specific methods
auto PlayerOneCamera::getPlayerOneSDKVersion() const -> std::string {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    char version[64];
    POAGetSDKVersion(version);
    return std::string(version);
#else
    return "Stub 1.0.0";
#endif
}

auto PlayerOneCamera::getCameraModel() const -> std::string {
    return camera_model_;
}

auto PlayerOneCamera::getSerialNumber() const -> std::string {
    return serial_number_;
}

auto PlayerOneCamera::enableHardwareBinning(bool enable) -> bool {
    hardware_binning_enabled_ = enable;
    LOG_F(INFO, "{} hardware binning", enable ? "Enabled" : "Disabled");
    return true;
}

auto PlayerOneCamera::isHardwareBinningEnabled() const -> bool {
    return hardware_binning_enabled_;
}

// Private helper methods
auto PlayerOneCamera::initializePlayerOneSDK() -> bool {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    // PlayerOne SDK initializes automatically
    return true;
#else
    return true;
#endif
}

auto PlayerOneCamera::shutdownPlayerOneSDK() -> bool {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    // PlayerOne SDK cleans up automatically
#endif
    return true;
}

auto PlayerOneCamera::setupCameraParameters() -> bool {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    POACameraProperties camera_props;
    if (POAGetCameraProperties(camera_index_, &camera_props) == POA_OK) {
        camera_model_ = std::string(camera_props.cameraModelName);
        max_width_ = camera_props.maxWidth;
        max_height_ = camera_props.maxHeight;
        pixel_size_x_ = camera_props.pixelSize;
        pixel_size_y_ = camera_props.pixelSize;
        is_color_camera_ = (camera_props.isColorCamera == POA_TRUE);
        bit_depth_ = camera_props.bitDepth;

        // Get serial number and firmware
        char serial[32];
        if (POAGetCameraSN(camera_handle_, serial) == POA_OK) {
            serial_number_ = std::string(serial);
        }

        char firmware[32];
        if (POAGetCameraFirmwareVersion(camera_handle_, firmware) == POA_OK) {
            firmware_version_ = std::string(firmware);
        }

        // Set Bayer pattern for color cameras
        if (is_color_camera_) {
            bayer_pattern_ = convertPlayerOneBayerPattern(camera_props.bayerPattern);
        }
    }
#endif

    roi_width_ = max_width_;
    roi_height_ = max_height_;

    return readCameraCapabilities();
}

auto PlayerOneCamera::readCameraCapabilities() -> bool {
    // Initialize camera capabilities using the correct CameraCapabilities structure
    camera_capabilities_.canAbort = true;
    camera_capabilities_.canSubFrame = true;
    camera_capabilities_.canBin = true;
    camera_capabilities_.hasCooler = hasCooler();
    camera_capabilities_.hasGain = true;
    camera_capabilities_.hasShutter = has_shutter_;
    camera_capabilities_.canStream = true;
    camera_capabilities_.canRecordVideo = true;
    camera_capabilities_.supportsSequences = true;
    camera_capabilities_.hasImageQualityAnalysis = true;
    camera_capabilities_.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF, ImageFormat::PNG, ImageFormat::JPEG};

    return true;
}

auto PlayerOneCamera::exposureThreadFunction() -> void {
    try {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
        // Set exposure time
        long exposure_ms = static_cast<long>(current_exposure_duration_ * 1000000);  // Microseconds
        if (POASetConfig(camera_handle_, POA_EXPOSURE, exposure_ms, POA_FALSE) != POA_OK) {
            LOG_F(ERROR, "Failed to set exposure time");
            is_exposing_ = false;
            return;
        }

        // Start single exposure
        if (POAStartExposure(camera_handle_, POA_FALSE) != POA_OK) {
            LOG_F(ERROR, "Failed to start exposure");
            is_exposing_ = false;
            return;
        }

        // Wait for exposure to complete
        POAImageReady ready_status;
        do {
            if (exposure_abort_requested_) {
                break;
            }

            if (POAImageReady(camera_handle_, &ready_status) != POA_OK) {
                LOG_F(ERROR, "Failed to check exposure status");
                is_exposing_ = false;
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (ready_status != POA_IMAGE_READY);

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

auto PlayerOneCamera::captureFrame() -> std::shared_ptr<AtomCameraFrame> {
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
    frame->format = is_color_camera_ ? "RGB" : "RAW";

    // Calculate frame size
    size_t pixelCount = frame->resolution.width * frame->resolution.height;
    size_t bytesPerPixel = (bit_depth_ <= 8) ? 1 : 2;
    size_t channels = is_color_camera_ ? 3 : 1;
    frame->size = pixelCount * channels * bytesPerPixel;

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    // Download actual image data from camera
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);

    if (POAGetImageData(camera_handle_, data_buffer.get(), frame->size, timeout) == POA_OK) {
        frame->data = data_buffer.release();
    } else {
        LOG_F(ERROR, "Failed to download image from PlayerOne camera");
        return nullptr;
    }
#else
    // Generate simulated image data
    auto data_buffer = std::make_unique<uint8_t[]>(frame->size);
    frame->data = data_buffer.release();

    // Fill with simulated data
    if (bit_depth_ <= 8) {
        uint8_t* data8 = static_cast<uint8_t*>(frame->data);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> noise_dist(0, 20);

        for (size_t i = 0; i < pixelCount * channels; ++i) {
            int noise = noise_dist(gen) - 10;  // ±10 ADU noise
            int star = 0;
            if (gen() % 20000 < 5) {  // 0.025% chance of star
                star = gen() % 150 + 50;  // Bright star
            }
            data8[i] = static_cast<uint8_t>(std::clamp(80 + noise + star, 0, 255));
        }
    } else {
        uint16_t* data16 = static_cast<uint16_t*>(frame->data);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> noise_dist(0, 100);

        for (size_t i = 0; i < pixelCount * channels; ++i) {
            int noise = noise_dist(gen) - 50;  // ±50 ADU noise
            int star = 0;
            if (gen() % 20000 < 5) {
                star = gen() % 8000 + 1000;  // Bright star
            }
            data16[i] = static_cast<uint16_t>(std::clamp(1000 + noise + star, 0, 65535));
        }
    }
#endif

    return frame;
}

auto PlayerOneCamera::captureVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    // Similar to captureFrame but optimized for video
    return captureFrame();
}

auto PlayerOneCamera::videoThreadFunction() -> void {
    while (is_video_running_) {
        try {
            auto frame = captureVideoFrame();
            if (frame) {
                total_frames_++;
                // Store frame for getVideoFrame() calls
            }

            // Video frame rate limiting
            std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 FPS
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in video thread: {}", e.what());
            dropped_frames_++;
        }
    }
}

auto PlayerOneCamera::temperatureThreadFunction() -> void {
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

auto PlayerOneCamera::updateTemperatureInfo() -> bool {
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    long temp_value;
    POA_BOOL is_auto;
    if (POAGetConfig(camera_handle_, POA_TEMPERATURE, &temp_value, &is_auto) == POA_OK) {
        current_temperature_ = static_cast<double>(temp_value) / 10.0;

        // Calculate cooling power
        long cooler_power;
        if (POAGetConfig(camera_handle_, POA_COOLER_POWER, &cooler_power, &is_auto) == POA_OK) {
            cooling_power_ = static_cast<double>(cooler_power);
        }
    }
#else
    // Simulate temperature convergence
    double temp_diff = target_temperature_ - current_temperature_;
    current_temperature_ += temp_diff * 0.05;  // Gradual convergence
    cooling_power_ = std::abs(temp_diff) * 3.0;
#endif
    return true;
}

auto PlayerOneCamera::convertPlayerOneBayerPattern(int pattern) -> BayerPattern {
    // Convert PlayerOne Bayer pattern constants to our enum
    switch (pattern) {
        case 0: return BayerPattern::RGGB;
        case 1: return BayerPattern::BGGR;
        case 2: return BayerPattern::GRBG;
        case 3: return BayerPattern::GBRG;
        default: return BayerPattern::MONO;
    }
}

auto PlayerOneCamera::isValidExposureTime(double duration) const -> bool {
    return duration >= 0.00001 && duration <= 3600.0;  // 10µs to 1 hour
}

auto PlayerOneCamera::isValidGain(int gain) const -> bool {
    auto range = getGainRange();
    return gain >= range.first && gain <= range.second;
}

auto PlayerOneCamera::isValidOffset(int offset) const -> bool {
    return offset >= 0 && offset <= 511;  // Typical range for PlayerOne cameras
}

auto PlayerOneCamera::isValidBinning(int binX, int binY) const -> bool {
    return binX >= 1 && binX <= 4 && binY >= 1 && binY <= 4 && binX == binY;
}

} // namespace lithium::device::playerone::camera
