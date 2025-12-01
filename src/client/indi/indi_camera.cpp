/*
 * indi_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Camera Device Implementation

**************************************************/

#include "indi_camera.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDICamera::INDICamera(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDICamera created: {}", name_);
}

INDICamera::~INDICamera() {
    if (isExposing()) {
        abortExposure();
    }
    LOG_DEBUG("INDICamera destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDICamera::connect(const std::string& deviceName, int timeout,
                         int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Camera {} connected", deviceName);
    return true;
}

auto INDICamera::disconnect() -> bool {
    if (isExposing()) {
        abortExposure();
    }
    if (isVideoRunning()) {
        stopVideo();
    }
    return INDIDeviceBase::disconnect();
}

// ==================== Exposure Control ====================

auto INDICamera::startExposure(double duration) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot start exposure: camera not connected");
        return false;
    }

    if (isExposing()) {
        LOG_WARN("Exposure already in progress");
        return false;
    }

    LOG_INFO("Starting exposure: {} seconds", duration);

    currentExposure_.store(duration);
    exposureRemaining_.store(duration);
    cameraState_.store(CameraState::Exposing);
    isExposing_.store(true);

    // Set the exposure property
    if (!setNumberProperty("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", duration)) {
        LOG_ERROR("Failed to set exposure property");
        cameraState_.store(CameraState::Error);
        isExposing_.store(false);
        return false;
    }

    return true;
}

auto INDICamera::abortExposure() -> bool {
    if (!isExposing()) {
        return true;
    }

    LOG_INFO("Aborting exposure");

    if (!setSwitchProperty("CCD_ABORT_EXPOSURE", "ABORT", true)) {
        LOG_ERROR("Failed to abort exposure");
        return false;
    }

    cameraState_.store(CameraState::Aborted);
    isExposing_.store(false);
    exposureCondition_.notify_all();

    return true;
}

auto INDICamera::isExposing() const -> bool { return isExposing_.load(); }

auto INDICamera::getExposureProgress() const -> std::optional<double> {
    if (!isExposing()) {
        return std::nullopt;
    }
    return exposureRemaining_.load();
}

auto INDICamera::getExposureResult() const -> ExposureResult {
    std::lock_guard<std::mutex> lock(exposureMutex_);
    return lastExposureResult_;
}

auto INDICamera::waitForExposure(std::chrono::milliseconds timeout) -> bool {
    if (!isExposing()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(exposureMutex_);
    return exposureCondition_.wait_for(lock, timeout, [this] {
        return !isExposing_.load();
    });
}

// ==================== Temperature Control ====================

auto INDICamera::startCooling(double targetTemp) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot start cooling: camera not connected");
        return false;
    }

    if (!hasCooler()) {
        LOG_ERROR("Camera does not have a cooler");
        return false;
    }

    LOG_INFO("Starting cooling to {} C", targetTemp);

    // Enable cooler
    if (!setSwitchProperty("CCD_COOLER", "COOLER_ON", true)) {
        LOG_ERROR("Failed to enable cooler");
        return false;
    }

    // Set target temperature
    if (!setNumberProperty("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE",
                           targetTemp)) {
        LOG_ERROR("Failed to set target temperature");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(coolerMutex_);
        coolerInfo_.targetTemp = targetTemp;
        coolerInfo_.coolerOn = true;
    }

    return true;
}

auto INDICamera::stopCooling() -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!hasCooler()) {
        return true;
    }

    LOG_INFO("Stopping cooling");

    if (!setSwitchProperty("CCD_COOLER", "COOLER_OFF", true)) {
        LOG_ERROR("Failed to disable cooler");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(coolerMutex_);
        coolerInfo_.coolerOn = false;
    }

    return true;
}

auto INDICamera::isCoolerOn() const -> bool {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    return coolerInfo_.coolerOn;
}

auto INDICamera::getTemperature() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    if (!coolerInfo_.hasCooler) {
        return std::nullopt;
    }
    return coolerInfo_.currentTemp;
}

auto INDICamera::getCoolerPower() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    if (!coolerInfo_.hasCooler || !coolerInfo_.coolerOn) {
        return std::nullopt;
    }
    return coolerInfo_.coolerPower;
}

auto INDICamera::hasCooler() const -> bool {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    return coolerInfo_.hasCooler;
}

auto INDICamera::getCoolerInfo() const -> CoolerInfo {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    return coolerInfo_;
}

// ==================== Gain/Offset Control ====================

auto INDICamera::setGain(int gain) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set gain: camera not connected");
        return false;
    }

    LOG_DEBUG("Setting gain to {}", gain);

    if (!setNumberProperty("CCD_GAIN", "GAIN", static_cast<double>(gain))) {
        LOG_ERROR("Failed to set gain");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(gainOffsetMutex_);
        gainOffsetInfo_.gain = gain;
    }

    return true;
}

auto INDICamera::getGain() const -> std::optional<int> {
    std::lock_guard<std::mutex> lock(gainOffsetMutex_);
    return gainOffsetInfo_.gain;
}

auto INDICamera::setOffset(int offset) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set offset: camera not connected");
        return false;
    }

    LOG_DEBUG("Setting offset to {}", offset);

    if (!setNumberProperty("CCD_OFFSET", "OFFSET", static_cast<double>(offset))) {
        LOG_ERROR("Failed to set offset");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(gainOffsetMutex_);
        gainOffsetInfo_.offset = offset;
    }

    return true;
}

auto INDICamera::getOffset() const -> std::optional<int> {
    std::lock_guard<std::mutex> lock(gainOffsetMutex_);
    return gainOffsetInfo_.offset;
}

auto INDICamera::getGainOffsetInfo() const -> GainOffsetInfo {
    std::lock_guard<std::mutex> lock(gainOffsetMutex_);
    return gainOffsetInfo_;
}

// ==================== Frame Settings ====================

auto INDICamera::setFrame(int x, int y, int width, int height) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set frame: camera not connected");
        return false;
    }

    LOG_DEBUG("Setting frame: x={}, y={}, w={}, h={}", x, y, width, height);

    auto prop = getProperty("CCD_FRAME");
    if (!prop) {
        LOG_ERROR("CCD_FRAME property not found");
        return false;
    }

    // Set all frame parameters
    bool success = true;
    success &= setNumberProperty("CCD_FRAME", "X", static_cast<double>(x));
    success &= setNumberProperty("CCD_FRAME", "Y", static_cast<double>(y));
    success &= setNumberProperty("CCD_FRAME", "WIDTH", static_cast<double>(width));
    success &= setNumberProperty("CCD_FRAME", "HEIGHT", static_cast<double>(height));

    if (success) {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_.x = x;
        currentFrame_.y = y;
        currentFrame_.width = width;
        currentFrame_.height = height;
    }

    return success;
}

auto INDICamera::resetFrame() -> bool {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return setFrame(0, 0, sensorInfo_.maxWidth, sensorInfo_.maxHeight);
}

auto INDICamera::getFrame() const -> CameraFrame {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_;
}

auto INDICamera::setBinning(int binX, int binY) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set binning: camera not connected");
        return false;
    }

    LOG_DEBUG("Setting binning: {}x{}", binX, binY);

    bool success = true;
    success &= setNumberProperty("CCD_BINNING", "HOR_BIN", static_cast<double>(binX));
    success &= setNumberProperty("CCD_BINNING", "VER_BIN", static_cast<double>(binY));

    if (success) {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_.binX = binX;
        currentFrame_.binY = binY;
    }

    return success;
}

auto INDICamera::getBinning() const -> std::pair<int, int> {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return {currentFrame_.binX, currentFrame_.binY};
}

auto INDICamera::setFrameType(FrameType type) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string typeName;
    switch (type) {
        case FrameType::Light:
            typeName = "FRAME_LIGHT";
            break;
        case FrameType::Bias:
            typeName = "FRAME_BIAS";
            break;
        case FrameType::Dark:
            typeName = "FRAME_DARK";
            break;
        case FrameType::Flat:
            typeName = "FRAME_FLAT";
            break;
    }

    if (!setSwitchProperty("CCD_FRAME_TYPE", typeName, true)) {
        LOG_ERROR("Failed to set frame type");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_.frameType = type;
    }

    return true;
}

auto INDICamera::getFrameType() const -> FrameType {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.frameType;
}

auto INDICamera::setUploadMode(UploadMode mode) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string modeName;
    switch (mode) {
        case UploadMode::Client:
            modeName = "UPLOAD_CLIENT";
            break;
        case UploadMode::Local:
            modeName = "UPLOAD_LOCAL";
            break;
        case UploadMode::Both:
            modeName = "UPLOAD_BOTH";
            break;
    }

    if (!setSwitchProperty("UPLOAD_MODE", modeName, true)) {
        LOG_ERROR("Failed to set upload mode");
        return false;
    }

    uploadMode_.store(mode);
    return true;
}

auto INDICamera::setUploadDirectory(const std::string& directory) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setTextProperty("UPLOAD_SETTINGS", "UPLOAD_DIR", directory)) {
        LOG_ERROR("Failed to set upload directory");
        return false;
    }

    uploadDirectory_ = directory;
    return true;
}

auto INDICamera::setUploadPrefix(const std::string& prefix) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setTextProperty("UPLOAD_SETTINGS", "UPLOAD_PREFIX", prefix)) {
        LOG_ERROR("Failed to set upload prefix");
        return false;
    }

    uploadPrefix_ = prefix;
    return true;
}

// ==================== Sensor Information ====================

auto INDICamera::getSensorInfo() const -> SensorInfo {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return sensorInfo_;
}

auto INDICamera::isColor() const -> bool {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return sensorInfo_.isColor;
}

// ==================== Video Streaming ====================

auto INDICamera::startVideo() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot start video: camera not connected");
        return false;
    }

    if (isVideoRunning()) {
        return true;
    }

    LOG_INFO("Starting video streaming");

    if (!setSwitchProperty("CCD_VIDEO_STREAM", "STREAM_ON", true)) {
        LOG_ERROR("Failed to start video stream");
        return false;
    }

    isVideoRunning_.store(true);
    return true;
}

auto INDICamera::stopVideo() -> bool {
    if (!isVideoRunning()) {
        return true;
    }

    LOG_INFO("Stopping video streaming");

    if (!setSwitchProperty("CCD_VIDEO_STREAM", "STREAM_OFF", true)) {
        LOG_ERROR("Failed to stop video stream");
        return false;
    }

    isVideoRunning_.store(false);
    return true;
}

auto INDICamera::isVideoRunning() const -> bool { return isVideoRunning_.load(); }

auto INDICamera::setVideoFrameRate(double fps) -> bool {
    if (!isConnected()) {
        return false;
    }

    return setNumberProperty("STREAMING_FRAME_RATE", "FRAME_RATE", fps);
}

// ==================== Image Format ====================

auto INDICamera::setImageFormat(ImageFormat format) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string formatName;
    switch (format) {
        case ImageFormat::FITS:
            formatName = "FORMAT_FITS";
            break;
        case ImageFormat::Native:
            formatName = "FORMAT_NATIVE";
            break;
        case ImageFormat::XISF:
            formatName = "FORMAT_XISF";
            break;
        default:
            return false;
    }

    if (!setSwitchProperty("CCD_TRANSFER_FORMAT", formatName, true)) {
        LOG_ERROR("Failed to set image format");
        return false;
    }

    imageFormat_.store(format);
    return true;
}

auto INDICamera::getImageFormat() const -> ImageFormat {
    return imageFormat_.load();
}

// ==================== Status ====================

auto INDICamera::getCameraState() const -> CameraState {
    return cameraState_.load();
}

auto INDICamera::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["cameraState"] = static_cast<int>(cameraState_.load());
    status["isExposing"] = isExposing();
    status["isVideoRunning"] = isVideoRunning();
    status["exposureRemaining"] = exposureRemaining_.load();
    status["imageFormat"] = static_cast<int>(imageFormat_.load());

    status["cooler"] = getCoolerInfo().toJson();
    status["gainOffset"] = getGainOffsetInfo().toJson();
    status["frame"] = getFrame().toJson();
    status["sensor"] = getSensorInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDICamera::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "CCD_INFO") {
        handleCCDInfoProperty(property);
    } else if (property.name == "CCD_TEMPERATURE") {
        handleTemperatureProperty(property);
    } else if (property.name == "CCD_COOLER") {
        handleCoolerProperty(property);
    } else if (property.name == "CCD_GAIN") {
        handleGainProperty(property);
    } else if (property.name == "CCD_OFFSET") {
        handleOffsetProperty(property);
    } else if (property.name == "CCD_FRAME") {
        handleFrameProperty(property);
    } else if (property.name == "CCD_BINNING") {
        handleBinningProperty(property);
    }
}

void INDICamera::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "CCD_EXPOSURE") {
        handleExposureProperty(property);
    } else if (property.name == "CCD_TEMPERATURE") {
        handleTemperatureProperty(property);
    } else if (property.name == "CCD_COOLER_POWER") {
        std::lock_guard<std::mutex> lock(coolerMutex_);
        if (auto power = property.getNumber("CCD_COOLER_VALUE")) {
            coolerInfo_.coolerPower = *power;
        }
    } else if (property.name == "CCD_GAIN") {
        handleGainProperty(property);
    } else if (property.name == "CCD_OFFSET") {
        handleOffsetProperty(property);
    } else if (property.name == "CCD_FRAME") {
        handleFrameProperty(property);
    } else if (property.name == "CCD_BINNING") {
        handleBinningProperty(property);
    }
}

void INDICamera::onBlobReceived(const INDIProperty& property) {
    INDIDeviceBase::onBlobReceived(property);

    if (property.name == "CCD1" || property.name == "CCD2") {
        handleBlobProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDICamera::handleExposureProperty(const INDIProperty& property) {
    if (auto remaining = property.getNumber("CCD_EXPOSURE_VALUE")) {
        exposureRemaining_.store(*remaining);

        if (*remaining <= 0.0 && isExposing()) {
            // Exposure complete, waiting for image
            cameraState_.store(CameraState::Downloading);
        }
    }

    if (property.state == PropertyState::Ok && isExposing()) {
        // Exposure completed successfully
        cameraState_.store(CameraState::Idle);
        isExposing_.store(false);
        exposureCondition_.notify_all();
    } else if (property.state == PropertyState::Alert) {
        // Exposure failed
        cameraState_.store(CameraState::Error);
        isExposing_.store(false);
        exposureCondition_.notify_all();
    }
}

void INDICamera::handleTemperatureProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    coolerInfo_.hasCooler = true;

    if (auto temp = property.getNumber("CCD_TEMPERATURE_VALUE")) {
        coolerInfo_.currentTemp = *temp;
    }
}

void INDICamera::handleCoolerProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(coolerMutex_);
    coolerInfo_.hasCooler = true;

    if (auto on = property.getSwitch("COOLER_ON")) {
        coolerInfo_.coolerOn = *on;
    }
}

void INDICamera::handleGainProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(gainOffsetMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "GAIN") {
            gainOffsetInfo_.gain = static_cast<int>(elem.value);
            gainOffsetInfo_.minGain = static_cast<int>(elem.min);
            gainOffsetInfo_.maxGain = static_cast<int>(elem.max);
        }
    }
}

void INDICamera::handleOffsetProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(gainOffsetMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "OFFSET") {
            gainOffsetInfo_.offset = static_cast<int>(elem.value);
            gainOffsetInfo_.minOffset = static_cast<int>(elem.min);
            gainOffsetInfo_.maxOffset = static_cast<int>(elem.max);
        }
    }
}

void INDICamera::handleFrameProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(frameMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "X") {
            currentFrame_.x = static_cast<int>(elem.value);
        } else if (elem.name == "Y") {
            currentFrame_.y = static_cast<int>(elem.value);
        } else if (elem.name == "WIDTH") {
            currentFrame_.width = static_cast<int>(elem.value);
        } else if (elem.name == "HEIGHT") {
            currentFrame_.height = static_cast<int>(elem.value);
        }
    }
}

void INDICamera::handleBinningProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(frameMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "HOR_BIN") {
            currentFrame_.binX = static_cast<int>(elem.value);
            sensorInfo_.maxBinX = static_cast<int>(elem.max);
        } else if (elem.name == "VER_BIN") {
            currentFrame_.binY = static_cast<int>(elem.value);
            sensorInfo_.maxBinY = static_cast<int>(elem.max);
        }
    }
}

void INDICamera::handleCCDInfoProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(frameMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "CCD_MAX_X") {
            sensorInfo_.maxWidth = static_cast<int>(elem.value);
        } else if (elem.name == "CCD_MAX_Y") {
            sensorInfo_.maxHeight = static_cast<int>(elem.value);
        } else if (elem.name == "CCD_PIXEL_SIZE") {
            sensorInfo_.pixelSizeX = elem.value;
            sensorInfo_.pixelSizeY = elem.value;
        } else if (elem.name == "CCD_PIXEL_SIZE_X") {
            sensorInfo_.pixelSizeX = elem.value;
        } else if (elem.name == "CCD_PIXEL_SIZE_Y") {
            sensorInfo_.pixelSizeY = elem.value;
        } else if (elem.name == "CCD_BITSPERPIXEL") {
            sensorInfo_.bitDepth = static_cast<int>(elem.value);
        }
    }

    // Initialize frame to full sensor
    if (currentFrame_.width == 0 && sensorInfo_.maxWidth > 0) {
        currentFrame_.width = sensorInfo_.maxWidth;
        currentFrame_.height = sensorInfo_.maxHeight;
    }
}

void INDICamera::handleBlobProperty(const INDIProperty& property) {
    if (property.blobs.empty()) {
        return;
    }

    const auto& blob = property.blobs[0];

    {
        std::lock_guard<std::mutex> lock(exposureMutex_);
        lastExposureResult_.success = true;
        lastExposureResult_.format = blob.format;
        lastExposureResult_.data = blob.data;
        lastExposureResult_.size = blob.size;
        lastExposureResult_.duration = currentExposure_.load();
        lastExposureResult_.timestamp = std::chrono::system_clock::now();
        lastExposureResult_.frame = getFrame();
    }

    LOG_INFO("Received image: {} bytes, format: {}", blob.size, blob.format);

    cameraState_.store(CameraState::Idle);
    isExposing_.store(false);
    exposureCondition_.notify_all();
}

void INDICamera::setupPropertyWatchers() {
    // Watch exposure property
    watchProperty("CCD_EXPOSURE", [this](const INDIProperty& prop) {
        handleExposureProperty(prop);
    });

    // Watch temperature property
    watchProperty("CCD_TEMPERATURE", [this](const INDIProperty& prop) {
        handleTemperatureProperty(prop);
    });

    // Watch cooler property
    watchProperty("CCD_COOLER", [this](const INDIProperty& prop) {
        handleCoolerProperty(prop);
    });
}

}  // namespace lithium::client::indi
