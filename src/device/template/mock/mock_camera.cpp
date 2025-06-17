/*
 * mock_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mock_camera.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <thread>

MockCamera::MockCamera(const std::string& name) 
    : AtomCamera(name), gen_(rd_()) {
    
    // Set up mock capabilities
    CameraCapabilities caps;
    caps.canAbort = true;
    caps.canSubFrame = true;
    caps.canBin = true;
    caps.hasCooler = true;
    caps.hasShutter = true;
    caps.hasGain = true;
    caps.hasOffset = true;
    caps.hasTemperature = true;
    caps.canStream = true;
    caps.bayerPattern = BayerPattern::MONO;
    setCameraCapabilities(caps);
    
    // Set device info
    DeviceInfo info;
    info.driverName = "Mock Camera Driver";
    info.driverVersion = "1.0.0";
    info.manufacturer = "Lithium Astronomy";
    info.model = "MockCam-2000";
    info.serialNumber = "MOCK123456";
    setDeviceInfo(info);
}

bool MockCamera::initialize() {
    setState(DeviceState::IDLE);
    return true;
}

bool MockCamera::destroy() {
    if (is_exposing_) {
        abortExposure();
    }
    if (is_video_running_) {
        stopVideo();
    }
    setState(DeviceState::UNKNOWN);
    return true;
}

bool MockCamera::connect(const std::string& port, int timeout, int maxRetry) {
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    connected_ = true;
    setState(DeviceState::IDLE);
    updateTimestamp();
    return true;
}

bool MockCamera::disconnect() {
    if (is_exposing_) {
        abortExposure();
    }
    if (is_video_running_) {
        stopVideo();
    }
    
    connected_ = false;
    setState(DeviceState::UNKNOWN);
    return true;
}

std::vector<std::string> MockCamera::scan() {
    return {"MockCamera:USB", "MockCamera:Ethernet"};
}

auto MockCamera::startExposure(double duration) -> bool {
    if (!isConnected() || is_exposing_) {
        return false;
    }
    
    exposure_duration_ = duration;
    exposure_start_ = std::chrono::system_clock::now();
    is_exposing_ = true;
    exposure_count_++;
    last_exposure_duration_ = duration;
    
    updateCameraState(CameraState::EXPOSING);
    
    // Start exposure simulation in background
    std::thread([this]() { simulateExposure(); }).detach();
    
    return true;
}

auto MockCamera::abortExposure() -> bool {
    if (!is_exposing_) {
        return false;
    }
    
    is_exposing_ = false;
    updateCameraState(CameraState::ABORTED);
    notifyExposureComplete(false, "Exposure aborted by user");
    
    return true;
}

auto MockCamera::isExposing() const -> bool {
    return is_exposing_;
}

auto MockCamera::getExposureProgress() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_).count();
    
    return std::min(1.0, elapsed / exposure_duration_);
}

auto MockCamera::getExposureRemaining() const -> double {
    if (!is_exposing_) {
        return 0.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - exposure_start_).count();
    
    return std::max(0.0, exposure_duration_ - elapsed);
}

auto MockCamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    return current_frame_;
}

auto MockCamera::saveImage(const std::string& path) -> bool {
    if (!current_frame_) {
        return false;
    }
    
    // Mock saving - just update the path
    current_frame_->recentImagePath = path;
    return true;
}

auto MockCamera::getLastExposureDuration() const -> double {
    return last_exposure_duration_;
}

auto MockCamera::getExposureCount() const -> uint32_t {
    return exposure_count_;
}

auto MockCamera::resetExposureCount() -> bool {
    exposure_count_ = 0;
    return true;
}

auto MockCamera::startVideo() -> bool {
    if (!isConnected() || is_video_running_) {
        return false;
    }
    
    is_video_running_ = true;
    return true;
}

auto MockCamera::stopVideo() -> bool {
    is_video_running_ = false;
    return true;
}

auto MockCamera::isVideoRunning() const -> bool {
    return is_video_running_;
}

auto MockCamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    if (!is_video_running_) {
        return nullptr;
    }
    
    return generateMockFrame();
}

auto MockCamera::setVideoFormat(const std::string& format) -> bool {
    return format == "RGB24" || format == "MONO8" || format == "MONO16";
}

auto MockCamera::getVideoFormats() -> std::vector<std::string> {
    return {"RGB24", "MONO8", "MONO16"};
}

auto MockCamera::startCooling(double targetTemp) -> bool {
    if (!hasCooler()) {
        return false;
    }
    
    target_temperature_ = targetTemp;
    cooler_on_ = true;
    
    // Start temperature simulation
    std::thread([this]() { simulateTemperatureControl(); }).detach();
    
    return true;
}

auto MockCamera::stopCooling() -> bool {
    cooler_on_ = false;
    cooling_power_ = 0.0;
    return true;
}

auto MockCamera::isCoolerOn() const -> bool {
    return cooler_on_;
}

auto MockCamera::getTemperature() const -> std::optional<double> {
    return current_temperature_;
}

auto MockCamera::getTemperatureInfo() const -> TemperatureInfo {
    TemperatureInfo info;
    info.current = current_temperature_;
    info.target = target_temperature_;
    info.ambient = 20.0;
    info.coolingPower = cooling_power_;
    info.coolerOn = cooler_on_;
    info.canSetTemperature = true;
    return info;
}

auto MockCamera::getCoolingPower() const -> std::optional<double> {
    return cooling_power_;
}

auto MockCamera::hasCooler() const -> bool {
    return camera_capabilities_.hasCooler;
}

auto MockCamera::setTemperature(double temperature) -> bool {
    if (!hasCooler()) {
        return false;
    }
    
    target_temperature_ = temperature;
    if (!cooler_on_) {
        cooler_on_ = true;
        std::thread([this]() { simulateTemperatureControl(); }).detach();
    }
    
    return true;
}

auto MockCamera::isColor() const -> bool {
    return camera_capabilities_.bayerPattern != BayerPattern::MONO;
}

auto MockCamera::getBayerPattern() const -> BayerPattern {
    return camera_capabilities_.bayerPattern;
}

auto MockCamera::setBayerPattern(BayerPattern pattern) -> bool {
    camera_capabilities_.bayerPattern = pattern;
    return true;
}

auto MockCamera::setGain(int gain) -> bool {
    current_gain_ = std::clamp(gain, 0, 100);
    return true;
}

auto MockCamera::getGain() -> std::optional<int> {
    return current_gain_;
}

auto MockCamera::getGainRange() -> std::pair<int, int> {
    return {0, 100};
}

auto MockCamera::setOffset(int offset) -> bool {
    current_offset_ = std::clamp(offset, 0, 50);
    return true;
}

auto MockCamera::getOffset() -> std::optional<int> {
    return current_offset_;
}

auto MockCamera::getOffsetRange() -> std::pair<int, int> {
    return {0, 50};
}

auto MockCamera::setISO(int iso) -> bool {
    static const std::vector<int> valid_isos = {100, 200, 400, 800, 1600, 3200};
    auto it = std::find(valid_isos.begin(), valid_isos.end(), iso);
    if (it != valid_isos.end()) {
        current_iso_ = iso;
        return true;
    }
    return false;
}

auto MockCamera::getISO() -> std::optional<int> {
    return current_iso_;
}

auto MockCamera::getISOList() -> std::vector<int> {
    return {100, 200, 400, 800, 1600, 3200};
}

auto MockCamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    return current_resolution_;
}

auto MockCamera::setResolution(int x, int y, int width, int height) -> bool {
    if (width > 0 && height > 0 && width <= MOCK_WIDTH && height <= MOCK_HEIGHT) {
        current_resolution_.width = width;
        current_resolution_.height = height;
        return true;
    }
    return false;
}

auto MockCamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    return {MOCK_WIDTH, MOCK_HEIGHT, MOCK_WIDTH, MOCK_HEIGHT};
}

auto MockCamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    return current_binning_;
}

auto MockCamera::setBinning(int horizontal, int vertical) -> bool {
    if (horizontal >= 1 && horizontal <= 4 && vertical >= 1 && vertical <= 4) {
        current_binning_.horizontal = horizontal;
        current_binning_.vertical = vertical;
        return true;
    }
    return false;
}

auto MockCamera::getMaxBinning() -> AtomCameraFrame::Binning {
    return {4, 4};
}

auto MockCamera::setFrameType(FrameType type) -> bool {
    current_frame_type_ = type;
    return true;
}

auto MockCamera::getFrameType() -> FrameType {
    return current_frame_type_;
}

auto MockCamera::setUploadMode(UploadMode mode) -> bool {
    current_upload_mode_ = mode;
    return true;
}

auto MockCamera::getUploadMode() -> UploadMode {
    return current_upload_mode_;
}

auto MockCamera::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();
    frame->resolution = current_resolution_;
    frame->binning = current_binning_;
    frame->type = current_frame_type_;
    frame->uploadMode = current_upload_mode_;
    frame->pixel.size = MOCK_PIXEL_SIZE;
    frame->pixel.sizeX = MOCK_PIXEL_SIZE;
    frame->pixel.sizeY = MOCK_PIXEL_SIZE;
    frame->pixel.depth = MOCK_BIT_DEPTH;
    return frame;
}

auto MockCamera::getPixelSize() -> double {
    return MOCK_PIXEL_SIZE;
}

auto MockCamera::getPixelSizeX() -> double {
    return MOCK_PIXEL_SIZE;
}

auto MockCamera::getPixelSizeY() -> double {
    return MOCK_PIXEL_SIZE;
}

auto MockCamera::getBitDepth() -> int {
    return MOCK_BIT_DEPTH;
}

auto MockCamera::hasShutter() -> bool {
    return camera_capabilities_.hasShutter;
}

auto MockCamera::setShutter(bool open) -> bool {
    shutter_open_ = open;
    return true;
}

auto MockCamera::getShutterStatus() -> bool {
    return shutter_open_;
}

auto MockCamera::hasFan() -> bool {
    return true; // Mock camera has fan
}

auto MockCamera::setFanSpeed(int speed) -> bool {
    fan_speed_ = std::clamp(speed, 0, 100);
    return true;
}

auto MockCamera::getFanSpeed() -> int {
    return fan_speed_;
}

void MockCamera::simulateExposure() {
    std::this_thread::sleep_for(std::chrono::duration<double>(exposure_duration_));
    
    if (is_exposing_) {
        // Generate mock frame
        current_frame_ = generateMockFrame();
        is_exposing_ = false;
        updateCameraState(CameraState::IDLE);
        notifyExposureComplete(true, "Exposure completed successfully");
    }
}

void MockCamera::simulateTemperatureControl() {
    while (cooler_on_) {
        double temp_diff = target_temperature_ - current_temperature_;
        
        if (std::abs(temp_diff) > 0.1) {
            // Simulate cooling/warming
            double cooling_rate = 0.1; // degrees per second
            double step = std::copysign(cooling_rate, temp_diff);
            
            current_temperature_ += step;
            cooling_power_ = std::abs(temp_diff) / 40.0 * 100.0; // 0-100%
            cooling_power_ = std::clamp(cooling_power_, 0.0, 100.0);
            
            notifyTemperatureChange();
        } else {
            cooling_power_ = 10.0; // Maintenance power
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::shared_ptr<AtomCameraFrame> MockCamera::generateMockFrame() {
    auto frame = getFrameInfo();
    // Generate mock image data would go here
    // For now, just return the frame structure
    return frame;
}

std::vector<uint16_t> MockCamera::generateMockImageData() {
    int width = current_resolution_.width / current_binning_.horizontal;
    int height = current_resolution_.height / current_binning_.vertical;
    
    std::vector<uint16_t> data(width * height);
    
    // Generate some mock star field
    std::uniform_int_distribution<uint16_t> noise_dist(100, 200);
    std::uniform_real_distribution<double> star_prob(0.0, 1.0);
    std::uniform_int_distribution<uint16_t> star_brightness(1000, 60000);
    
    for (int i = 0; i < width * height; ++i) {
        // Base noise level
        data[i] = noise_dist(gen_);
        
        // Add random stars
        if (star_prob(gen_) < 0.001) { // 0.1% chance of star
            data[i] = star_brightness(gen_);
        }
    }
    
    return data;
}
