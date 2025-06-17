#include "camera.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "atom/components/component.hpp"
#include "atom/components/module_macro.hpp"
#include "atom/components/registry.hpp"
#include "atom/error/exception.hpp"
#include "device/template/camera.hpp"

INDICamera::INDICamera(std::string deviceName)
    : AtomCamera(deviceName), name_(std::move(deviceName)) {
    // 初始化默认视频格式
    videoFormats_ = {"MJPEG", "RAW8", "RAW16"};
    currentVideoFormat_ = "MJPEG";
    
    // 初始化连接状态
    isConnected_.store(false);
    serverConnected_.store(false);
    isExposing_.store(false);
    isVideoRunning_.store(false);
    isCooling_.store(false);
    shutterOpen_.store(true);
    fanSpeed_.store(0);
    
    // 初始化增强功能状态
    isVideoRecording_.store(false);
    videoExposure_.store(0.033); // 30 FPS default
    videoGain_.store(0);
    
    isSequenceRunning_.store(false);
    sequenceCount_.store(0);
    sequenceTotal_.store(0);
    sequenceExposure_.store(1.0);
    sequenceInterval_.store(0.0);
    
    imageCompressionEnabled_.store(false);
    supportedImageFormats_ = {"FITS", "NATIVE", "XISF", "JPEG", "PNG", "TIFF"};
    currentImageFormat_ = "FITS";
    
    totalFramesReceived_.store(0);
    droppedFrames_.store(0);
    averageFrameRate_.store(0.0);
    
    lastImageMean_.store(0.0);
    lastImageStdDev_.store(0.0);
    lastImageMin_.store(0);
    lastImageMax_.store(0);
    
    // Initialize enhanced capability flags
    camera_capabilities_.canRecordVideo = true;
    camera_capabilities_.supportsSequences = true;
    camera_capabilities_.hasImageQualityAnalysis = true;
    camera_capabilities_.supportsCompression = true;
    camera_capabilities_.hasAdvancedControls = true;
    camera_capabilities_.supportsBurstMode = true;
    
    camera_capabilities_.supportedFormats = {
        ImageFormat::FITS, ImageFormat::JPEG, ImageFormat::PNG,
        ImageFormat::TIFF, ImageFormat::XISF, ImageFormat::NATIVE
    };
    
    camera_capabilities_.supportedVideoFormats = {"MJPEG", "RAW8", "RAW16", "H264"};
}

auto INDICamera::getDeviceInstance() -> INDI::BaseDevice & {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        THROW_NOT_FOUND("Device is not connected.");
    }
    return device_;
}

auto INDICamera::initialize() -> bool { return true; }

auto INDICamera::destroy() -> bool { return true; }

auto INDICamera::connect(const std::string &deviceName, int timeout,
                         int maxRetry) -> bool {
    ATOM_UNREF_PARAM(timeout);
    ATOM_UNREF_PARAM(maxRetry);
    if (isConnected_.load()) {
        spdlog::error("{} is already connected.", deviceName_);
        return false;
    }

    deviceName_ = deviceName;
    spdlog::info("Connecting to INDI server and watching for device {}...", deviceName_);

    // Set server host and port (default is localhost:7624)
    setServer("localhost", 7624);
    
    // Connect to INDI server
    if (!connectServer()) {
        spdlog::error("Failed to connect to INDI server");
        return false;
    }
    
    // Setup device watching with callbacks
    watchDevice(deviceName_.c_str(), [this](INDI::BaseDevice device) {
        device_ = device;
        spdlog::info("Device {} found, setting up property monitoring", deviceName_);
        
        // Enable BLOB reception for images
        setBLOBMode(B_ALSO, deviceName_.c_str(), nullptr);
        
        // Setup enhanced image and video features
        setupImageFormats();
        setupVideoStreamOptions();
        
        // Watch for CONNECTION property and auto-connect
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property property) {
                if (property.getType() == INDI_SWITCH) {
                    spdlog::info("CONNECTION property available for {}", deviceName_);
                    // Auto-connect to device
                    connectDevice(deviceName_.c_str());
                }
            },
            INDI::BaseDevice::WATCH_NEW);

        // The property monitoring is now handled by the callback system
        // through newProperty() and updateProperty() overrides
    });
    
    return true;
}

auto INDICamera::disconnect() -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }
    spdlog::info("Disconnecting from {}...", deviceName_);
    
    // Disconnect the specific device first
    if (!deviceName_.empty()) {
        disconnectDevice(deviceName_.c_str());
    }
    
    // Disconnect from INDI server
    disconnectServer();
    
    isConnected_.store(false);
    serverConnected_.store(false);
    updateCameraState(CameraState::IDLE);
    return true;
}

auto INDICamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;
    for (auto &device : getDevices()) {
        devices.emplace_back(device.getDeviceName());
    }
    return devices;
}

auto INDICamera::isConnected() const -> bool { return isConnected_.load(); }

// 曝光控制实现
auto INDICamera::startExposure(double duration) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    if (isExposing_.load()) {
        spdlog::error("Camera is already exposing.");
        return false;
    }

    INDI::PropertyNumber exposureProperty = device_.getProperty("CCD_EXPOSURE");
    if (!exposureProperty.isValid()) {
        spdlog::error("Error: unable to find CCD_EXPOSURE property...");
        return false;
    }

    spdlog::info("Starting exposure of {} seconds...", duration);
    current_exposure_duration_ = duration;
    exposureProperty[0].setValue(duration);
    sendNewProperty(exposureProperty);
    return true;
}

auto INDICamera::abortExposure() -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch ccdAbort = device_.getProperty("CCD_ABORT_EXPOSURE");
    if (!ccdAbort.isValid()) {
        spdlog::error("Error: unable to find CCD_ABORT_EXPOSURE property...");
        return false;
    }

    ccdAbort[0].setState(ISS_ON);
    sendNewProperty(ccdAbort);
    updateCameraState(CameraState::ABORTED);
    isExposing_.store(false);
    return true;
}

auto INDICamera::isExposing() const -> bool { return isExposing_.load(); }

auto INDICamera::getExposureProgress() const -> double {
    if (!isExposing_.load()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - exposure_start_time_)
                       .count() /
                   1000.0;

    if (current_exposure_duration_ <= 0) {
        return 0.0;
    }

    return std::min(1.0, elapsed / current_exposure_duration_);
}

auto INDICamera::getExposureRemaining() const -> double {
    if (!isExposing_.load()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - exposure_start_time_)
                       .count() /
                   1000.0;

    return std::max(0.0, current_exposure_duration_ - elapsed);
}

auto INDICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    return current_frame_;
}

auto INDICamera::saveImage(const std::string &path) -> bool {
    if (!current_frame_ || !current_frame_->data) {
        spdlog::error("No image data available to save.");
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open file for writing: {}", path);
        return false;
    }

    file.write(static_cast<const char *>(current_frame_->data),
               current_frame_->size);
    file.close();

    spdlog::info("Image saved to: {}", path);
    return true;
}

// 曝光历史和统计
auto INDICamera::getLastExposureDuration() const -> double {
    return lastExposureDuration_.load();
}

auto INDICamera::getExposureCount() const -> uint32_t {
    return exposureCount_.load();
}

auto INDICamera::resetExposureCount() -> bool {
    exposureCount_.store(0);
    return true;
}

// 视频控制实现
auto INDICamera::startVideo() -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch ccdVideo = device_.getProperty("CCD_VIDEO_STREAM");
    if (!ccdVideo.isValid()) {
        spdlog::error("Error: unable to find CCD_VIDEO_STREAM property...");
        return false;
    }

    ccdVideo[0].setState(ISS_ON);
    sendNewProperty(ccdVideo);
    isVideoRunning_.store(true);
    return true;
}

auto INDICamera::stopVideo() -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch ccdVideo = device_.getProperty("CCD_VIDEO_STREAM");
    if (!ccdVideo.isValid()) {
        spdlog::error("Error: unable to find CCD_VIDEO_STREAM property...");
        return false;
    }

    ccdVideo[0].setState(ISS_OFF);
    sendNewProperty(ccdVideo);
    isVideoRunning_.store(false);
    return true;
}

auto INDICamera::isVideoRunning() const -> bool {
    return isVideoRunning_.load();
}

auto INDICamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    // 返回当前帧，视频模式下会持续更新
    return current_frame_;
}

auto INDICamera::setVideoFormat(const std::string &format) -> bool {
    // 检查格式是否支持
    auto it = std::find(videoFormats_.begin(), videoFormats_.end(), format);
    if (it == videoFormats_.end()) {
        spdlog::error("Unsupported video format: {}", format);
        return false;
    }

    currentVideoFormat_ = format;
    // 这里可以设置INDI属性，如果驱动支持的话
    return true;
}

auto INDICamera::getVideoFormats() -> std::vector<std::string> {
    return videoFormats_;
}

// 温度控制实现
auto INDICamera::startCooling(double targetTemp) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    // 设置目标温度
    if (!setTemperature(targetTemp)) {
        return false;
    }

    // 启动制冷器
    INDI::PropertySwitch ccdCooler = device_.getProperty("CCD_COOLER");
    if (!ccdCooler.isValid()) {
        spdlog::error("Error: unable to find CCD_COOLER property...");
        return false;
    }

    ccdCooler[0].setState(ISS_ON);
    sendNewProperty(ccdCooler);
    targetTemperature_ = targetTemp;
    temperature_info_.target = targetTemp;
    return true;
}

auto INDICamera::stopCooling() -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch ccdCooler = device_.getProperty("CCD_COOLER");
    if (!ccdCooler.isValid()) {
        spdlog::error("Error: unable to find CCD_COOLER property...");
        return false;
    }

    ccdCooler[0].setState(ISS_OFF);
    sendNewProperty(ccdCooler);
    return true;
}

auto INDICamera::isCoolerOn() const -> bool { return isCooling_.load(); }

auto INDICamera::getTemperature() const -> std::optional<double> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }
    return currentTemperature_.load();
}

auto INDICamera::getTemperatureInfo() const -> TemperatureInfo {
    return temperature_info_;
}

auto INDICamera::getCoolingPower() const -> std::optional<double> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }
    return coolingPower_.load();
}

auto INDICamera::hasCooler() const -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    INDI::PropertySwitch ccdCooler = device_.getProperty("CCD_COOLER");
    return ccdCooler.isValid();
}

auto INDICamera::setTemperature(double temperature) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber ccdTemperature =
        device_.getProperty("CCD_TEMPERATURE");
    if (!ccdTemperature.isValid()) {
        spdlog::error("Error: unable to find CCD_TEMPERATURE property...");
        return false;
    }

    spdlog::info("Setting temperature to {} C...", temperature);
    ccdTemperature[0].setValue(temperature);
    sendNewProperty(ccdTemperature);
    targetTemperature_ = temperature;
    temperature_info_.target = temperature;
    return true;
}

// 色彩信息实现
auto INDICamera::isColor() const -> bool {
    return bayerPattern_ != BayerPattern::MONO;
}

auto INDICamera::getBayerPattern() const -> BayerPattern {
    return bayerPattern_;
}

auto INDICamera::setBayerPattern(BayerPattern pattern) -> bool {
    bayerPattern_ = pattern;
    return true;
}

// 参数控制实现
auto INDICamera::setGain(int gain) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber ccdGain = device_.getProperty("CCD_GAIN");
    if (!ccdGain.isValid()) {
        spdlog::error("Error: unable to find CCD_GAIN property...");
        return false;
    }

    if (gain < minGain_ || gain > maxGain_) {
        spdlog::error("Gain {} is out of range [{}, {}]", gain, minGain_,
                      maxGain_);
        return false;
    }

    spdlog::info("Setting gain to {}...", gain);
    ccdGain[0].setValue(gain);
    sendNewProperty(ccdGain);
    return true;
}

auto INDICamera::getGain() -> std::optional<int> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }
    return static_cast<int>(currentGain_.load());
}

auto INDICamera::getGainRange() -> std::pair<int, int> {
    return {static_cast<int>(minGain_), static_cast<int>(maxGain_)};
}

auto INDICamera::setOffset(int offset) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber ccdOffset = device_.getProperty("CCD_OFFSET");
    if (!ccdOffset.isValid()) {
        spdlog::error("Error: unable to find CCD_OFFSET property...");
        return false;
    }

    if (offset < minOffset_ || offset > maxOffset_) {
        spdlog::error("Offset {} is out of range [{}, {}]", offset, minOffset_,
                      maxOffset_);
        return false;
    }

    spdlog::info("Setting offset to {}...", offset);
    ccdOffset[0].setValue(offset);
    sendNewProperty(ccdOffset);
    return true;
}

auto INDICamera::getOffset() -> std::optional<int> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }
    return static_cast<int>(currentOffset_.load());
}

auto INDICamera::getOffsetRange() -> std::pair<int, int> {
    return {static_cast<int>(minOffset_), static_cast<int>(maxOffset_)};
}

auto INDICamera::setISO(int iso) -> bool {
    // INDI通常不直接支持ISO设置，这里返回false
    spdlog::warn("ISO setting not supported in INDI cameras");
    return false;
}

auto INDICamera::getISO() -> std::optional<int> {
    // INDI通常不直接支持ISO获取
    return std::nullopt;
}

auto INDICamera::getISOList() -> std::vector<int> {
    // INDI通常不支持ISO列表
    return {};
}

// 帧设置实现
auto INDICamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }

    AtomCameraFrame::Resolution res;
    // res.x = frameX_;
    // res.y = frameY_;
    res.width = frameWidth_;
    res.height = frameHeight_;
    res.maxWidth = maxFrameX_;
    res.maxHeight = maxFrameY_;
    return res;
}

auto INDICamera::setResolution(int x, int y, int width, int height) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber ccdFrame = device_.getProperty("CCD_FRAME");
    if (!ccdFrame.isValid()) {
        spdlog::error("Error: unable to find CCD_FRAME property...");
        return false;
    }

    ccdFrame[0].setValue(x);       // X
    ccdFrame[1].setValue(y);       // Y
    ccdFrame[2].setValue(width);   // Width
    ccdFrame[3].setValue(height);  // Height
    sendNewProperty(ccdFrame);
    return true;
}

auto INDICamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    res.maxWidth = maxFrameX_;
    res.maxHeight = maxFrameY_;
    res.width = maxFrameX_;
    res.height = maxFrameY_;
    // res.x = 0;
    // res.y = 0;
    return res;
}

auto INDICamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!isConnected_.load()) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = binHor_;
    bin.vertical = binVer_;
    // bin.max_horizontal = maxBinHor_;
    // bin.max_vertical = maxBinVer_;
    return bin;
}

auto INDICamera::setBinning(int horizontal, int vertical) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber ccdBinning = device_.getProperty("CCD_BINNING");
    if (!ccdBinning.isValid()) {
        spdlog::error("Error: unable to find CCD_BINNING property...");
        return false;
    }

    if (horizontal > maxBinHor_ || vertical > maxBinVer_) {
        spdlog::error("Binning values out of range");
        return false;
    }

    ccdBinning[0].setValue(horizontal);
    ccdBinning[1].setValue(vertical);
    sendNewProperty(ccdBinning);
    return true;
}

auto INDICamera::getMaxBinning() -> AtomCameraFrame::Binning {
    AtomCameraFrame::Binning bin;
    // bin.max_horizontal = maxBinHor_;
    // bin.max_vertical = maxBinVer_;
    bin.horizontal = maxBinHor_;
    bin.vertical = maxBinVer_;
    return bin;
}

auto INDICamera::setFrameType(FrameType type) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch ccdFrameType = device_.getProperty("CCD_FRAME_TYPE");
    if (!ccdFrameType.isValid()) {
        spdlog::error("Error: unable to find CCD_FRAME_TYPE property...");
        return false;
    }

    // 重置所有开关
    for (int i = 0; i < ccdFrameType->nsp; i++) {
        ccdFrameType[i].setState(ISS_OFF);
    }

    // 根据类型设置对应开关
    switch (type) {
        case FrameType::FITS:
            ccdFrameType[0].setState(ISS_ON);
            break;
        case FrameType::NATIVE:
            ccdFrameType[1].setState(ISS_ON);
            break;
        case FrameType::XISF:
            ccdFrameType[2].setState(ISS_ON);
            break;
        case FrameType::JPG:
            ccdFrameType[3].setState(ISS_ON);
            break;
        case FrameType::PNG:
            ccdFrameType[4].setState(ISS_ON);
            break;
        case FrameType::TIFF:
            ccdFrameType[5].setState(ISS_ON);
            break;
    }

    sendNewProperty(ccdFrameType);
    currentFrameType_ = type;
    return true;
}

auto INDICamera::getFrameType() -> FrameType { return currentFrameType_; }

auto INDICamera::setUploadMode(UploadMode mode) -> bool {
    currentUploadMode_ = mode;
    // INDI的上传模式通常通过UPLOAD_MODE属性控制
    return true;
}

auto INDICamera::getUploadMode() -> UploadMode { return currentUploadMode_; }

auto INDICamera::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();

    // frame->resolution.x = frameX_;
    // frame->resolution.y = frameY_;
    frame->resolution.width = frameWidth_;
    frame->resolution.height = frameHeight_;
    frame->resolution.maxWidth = maxFrameX_;
    frame->resolution.maxHeight = maxFrameY_;

    frame->binning.horizontal = binHor_;
    frame->binning.vertical = binVer_;
    // frame->binning.max_horizontal = maxBinHor_;
    // frame->binning.max_vertical = maxBinVer_;

    frame->pixel.size = framePixel_;
    frame->pixel.sizeX = framePixelX_;
    frame->pixel.sizeY = framePixelY_;
    frame->pixel.depth = frameDepth_;

    return frame;
}

// 像素信息实现
auto INDICamera::getPixelSize() -> double { return framePixel_; }

auto INDICamera::getPixelSizeX() -> double { return framePixelX_; }

auto INDICamera::getPixelSizeY() -> double { return framePixelY_; }

auto INDICamera::getBitDepth() -> int { return static_cast<int>(frameDepth_); }

// 快门控制实现
auto INDICamera::hasShutter() -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    // 检查是否有快门控制属性
    INDI::PropertySwitch shutterControl = device_.getProperty("CCD_SHUTTER");
    return shutterControl.isValid();
}

auto INDICamera::setShutter(bool open) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertySwitch shutterControl = device_.getProperty("CCD_SHUTTER");
    if (!shutterControl.isValid()) {
        spdlog::warn("No shutter control available");
        return false;
    }

    if (open) {
        shutterControl[0].setState(ISS_ON);
        shutterControl[1].setState(ISS_OFF);
    } else {
        shutterControl[0].setState(ISS_OFF);
        shutterControl[1].setState(ISS_ON);
    }

    sendNewProperty(shutterControl);
    shutterOpen_.store(open);
    return true;
}

auto INDICamera::getShutterStatus() -> bool { return shutterOpen_.load(); }

// 风扇控制实现
auto INDICamera::hasFan() -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    INDI::PropertyNumber fanControl = device_.getProperty("CCD_FAN");
    return fanControl.isValid();
}

auto INDICamera::setFanSpeed(int speed) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return false;
    }

    INDI::PropertyNumber fanControl = device_.getProperty("CCD_FAN");
    if (!fanControl.isValid()) {
        spdlog::warn("No fan control available");
        return false;
    }

    fanControl[0].setValue(speed);
    sendNewProperty(fanControl);
    fanSpeed_.store(speed);
    return true;
}

auto INDICamera::getFanSpeed() -> int { return fanSpeed_.load(); }

// 辅助方法实现
auto INDICamera::watchAdditionalProperty() -> bool { return true; }

/* 重复定义，已在前面实现
auto INDICamera::getDeviceInstance() -> INDI::BaseDevice & { return device_; }
*/

void INDICamera::setPropertyNumber(std::string_view propertyName,
                                   double value) {
    if (!isConnected_.load()) {
        spdlog::error("{} is not connected.", deviceName_);
        return;
    }

    INDI::PropertyNumber property = device_.getProperty(propertyName.data());
    if (property.isValid()) {
        property[0].setValue(value);
        sendNewProperty(property);
    } else {
        spdlog::error("Error: Unable to find property {}", propertyName);
    }
}

void INDICamera::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    spdlog::info("New message from {}.{}", baseDevice.getDeviceName(),
                 messageID);
}

// 私有辅助方法
/* 未声明，注释掉
void INDICamera::setupAdditionalProperties() {
    // ...
}
*/

// INDI BaseClient methods implementation
void INDICamera::watchDevice(const char *deviceName, const std::function<void(INDI::BaseDevice)> &callback) {
    if (!deviceName) {
        spdlog::error("Device name cannot be null");
        return;
    }
    
    std::string name(deviceName);
    deviceCallbacks_[name] = callback;
    
    // Check if device already exists
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (const auto& device : devices_) {
        if (device.getDeviceName() == name) {
            callback(device);
            return;
        }
    }
    
    spdlog::info("Watching for device: {}", name);
}

void INDICamera::connectDevice(const char *deviceName) {
    if (!deviceName) {
        spdlog::error("Device name cannot be null");
        return;
    }
    
    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }
    
    // Find device
    INDI::BaseDevice device;
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (const auto& dev : devices_) {
            if (dev.getDeviceName() == deviceName) {
                device = dev;
                break;
            }
        }
    }
    
    if (!device.isValid()) {
        spdlog::error("Device {} not found", deviceName);
        return;
    }
    
    // Get CONNECTION property
    INDI::PropertySwitch connectProperty = device.getProperty("CONNECTION");
    if (!connectProperty.isValid()) {
        spdlog::error("Device {} has no CONNECTION property", deviceName);
        return;
    }
    
    // Set CONNECT switch to ON
    connectProperty.reset();
    connectProperty[0].setState(ISS_ON);  // CONNECT
    connectProperty[1].setState(ISS_OFF); // DISCONNECT
    
    sendNewProperty(connectProperty);
    spdlog::info("Connecting to device: {}", deviceName);
}

void INDICamera::disconnectDevice(const char *deviceName) {
    if (!deviceName) {
        spdlog::error("Device name cannot be null");
        return;
    }
    
    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }
    
    // Find device
    INDI::BaseDevice device;
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (const auto& dev : devices_) {
            if (dev.getDeviceName() == deviceName) {
                device = dev;
                break;
            }
        }
    }
    
    if (!device.isValid()) {
        spdlog::error("Device {} not found", deviceName);
        return;
    }
    
    // Get CONNECTION property
    INDI::PropertySwitch connectProperty = device.getProperty("CONNECTION");
    if (!connectProperty.isValid()) {
        spdlog::error("Device {} has no CONNECTION property", deviceName);
        return;
    }
    
    // Set DISCONNECT switch to ON
    connectProperty.reset();
    connectProperty[0].setState(ISS_OFF);  // CONNECT
    connectProperty[1].setState(ISS_ON);   // DISCONNECT
    
    sendNewProperty(connectProperty);
    spdlog::info("Disconnecting from device: {}", deviceName);
}

void INDICamera::sendNewProperty(INDI::Property property) {
    if (!property.isValid()) {
        spdlog::error("Invalid property");
        return;
    }
    
    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }
    
    // Send property to server using base client functionality
    INDI::BaseClient::sendNewProperty(property);
}

std::vector<INDI::BaseDevice> INDICamera::getDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return devices_;
}

// INDI BaseClient callback methods
void INDICamera::newDevice(INDI::BaseDevice device) {
    if (!device.isValid()) {
        return;
    }
    
    std::string deviceName = device.getDeviceName();
    spdlog::info("New device discovered: {}", deviceName);
    
    // Add to devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.push_back(device);
    }
    
    // Check if we have a callback for this device
    auto it = deviceCallbacks_.find(deviceName);
    if (it != deviceCallbacks_.end()) {
        it->second(device);
    }
}

void INDICamera::removeDevice(INDI::BaseDevice device) {
    if (!device.isValid()) {
        return;
    }
    
    std::string deviceName = device.getDeviceName();
    spdlog::info("Device removed: {}", deviceName);
    
    // Remove from devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.erase(
            std::remove_if(devices_.begin(), devices_.end(),
                [&deviceName](const INDI::BaseDevice& dev) {
                    return dev.getDeviceName() == deviceName;
                }),
            devices_.end()
        );
    }
    
    // If this was our target device, mark as disconnected
    if (deviceName == deviceName_) {
        isConnected_.store(false);
        updateCameraState(CameraState::ERROR);
    }
}

void INDICamera::newProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }
    
    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();
    
    spdlog::debug("New property: {}.{}", deviceName, propertyName);
    
    // Handle device-specific properties
    if (deviceName == deviceName_) {
        handleDeviceProperty(property);
    }
}

void INDICamera::updateProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }
    
    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();
    
    spdlog::debug("Property updated: {}.{}", deviceName, propertyName);
    
    // Handle device-specific properties
    if (deviceName == deviceName_) {
        handleDeviceProperty(property);
    }
}

void INDICamera::removeProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }
    
    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();
    
    spdlog::debug("Property removed: {}.{}", deviceName, propertyName);
}

void INDICamera::serverConnected() {
    serverConnected_.store(true);
    spdlog::info("Connected to INDI server");
}

void INDICamera::serverDisconnected(int exit_code) {
    serverConnected_.store(false);
    isConnected_.store(false);
    updateCameraState(CameraState::ERROR);
    
    // Clear devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.clear();
    }
    
    spdlog::warn("Disconnected from INDI server (exit code: {})", exit_code);
}

// Property handler method
void INDICamera::handleDeviceProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }
    
    std::string propertyName = property.getName();
    
    if (propertyName == "CONNECTION") {
        handleConnectionProperty(property);
    } else if (propertyName == "CCD_EXPOSURE") {
        handleExposureProperty(property);
    } else if (propertyName == "CCD_TEMPERATURE") {
        handleTemperatureProperty(property);
    } else if (propertyName == "CCD_COOLER") {
        handleCoolerProperty(property);
    } else if (propertyName == "CCD_COOLER_POWER") {
        handleCoolerPowerProperty(property);
    } else if (propertyName == "CCD_GAIN") {
        handleGainProperty(property);
    } else if (propertyName == "CCD_OFFSET") {
        handleOffsetProperty(property);
    } else if (propertyName == "CCD_FRAME") {
        handleFrameProperty(property);
    } else if (propertyName == "CCD_BINNING") {
        handleBinningProperty(property);
    } else if (propertyName == "CCD_INFO") {
        handleInfoProperty(property);
    } else if (propertyName == "CCD1") {
        handleBlobProperty(property);
    } else if (propertyName == "CCD_VIDEO_STREAM") {
        handleVideoStreamProperty(property);
    }
}

// Individual property handlers
void INDICamera::handleConnectionProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch connectProperty = property;
    if (connectProperty[0].getState() == ISS_ON) {
        spdlog::info("{} is connected.", deviceName_);
        isConnected_.store(true);
        updateCameraState(CameraState::IDLE);
    } else {
        spdlog::info("{} is disconnected.", deviceName_);
        isConnected_.store(false);
        updateCameraState(CameraState::ERROR);
    }
}

void INDICamera::handleExposureProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber exposureProperty = property;
    if (exposureProperty.isValid()) {
        auto exposure = exposureProperty[0].getValue();
        currentExposure_ = exposure;

        // Check exposure state
        if (property.getState() == IPS_BUSY) {
            isExposing_.store(true);
            updateCameraState(CameraState::EXPOSING);
            exposureStartTime_ = std::chrono::system_clock::now();
        } else if (property.getState() == IPS_OK) {
            isExposing_.store(false);
            updateCameraState(CameraState::IDLE);
            lastExposureDuration_ = exposure;
            exposureCount_++;
            notifyExposureComplete(true, "Exposure completed successfully");
        } else if (property.getState() == IPS_ALERT) {
            isExposing_.store(false);
            updateCameraState(CameraState::ERROR);
            notifyExposureComplete(false, "Exposure failed");
        }
    }
}

void INDICamera::handleTemperatureProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber tempProperty = property;
    if (tempProperty.isValid()) {
        auto temp = tempProperty[0].getValue();
        currentTemperature_ = temp;
        temperature_info_.current = temp;
        notifyTemperatureChange();
    }
}

void INDICamera::handleCoolerProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch coolerProperty = property;
    if (coolerProperty.isValid()) {
        auto coolerState = coolerProperty[0].getState();
        bool coolerOn = (coolerState == ISS_ON);
        isCooling_.store(coolerOn);
        temperature_info_.coolerOn = coolerOn;
    }
}

void INDICamera::handleCoolerPowerProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber powerProperty = property;
    if (powerProperty.isValid()) {
        auto power = powerProperty[0].getValue();
        coolingPower_ = power;
        temperature_info_.coolingPower = power;
    }
}

void INDICamera::handleGainProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber gainProperty = property;
    if (gainProperty.isValid()) {
        currentGain_ = gainProperty[0].getValue();
        maxGain_ = gainProperty[0].getMax();
        minGain_ = gainProperty[0].getMin();
    }
}

void INDICamera::handleOffsetProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber offsetProperty = property;
    if (offsetProperty.isValid()) {
        currentOffset_ = offsetProperty[0].getValue();
        maxOffset_ = offsetProperty[0].getMax();
        minOffset_ = offsetProperty[0].getMin();
    }
}

void INDICamera::handleFrameProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber frameProperty = property;
    if (frameProperty.isValid()) {
        frameX_ = frameProperty[0].getValue();
        frameY_ = frameProperty[1].getValue();
        frameWidth_ = frameProperty[2].getValue();
        frameHeight_ = frameProperty[3].getValue();
    }
}

void INDICamera::handleBinningProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber binProperty = property;
    if (binProperty.isValid()) {
        binHor_ = binProperty[0].getValue();
        binVer_ = binProperty[1].getValue();
        maxBinHor_ = binProperty[0].getMax();
        maxBinVer_ = binProperty[1].getMax();
    }
}

void INDICamera::handleInfoProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber infoProperty = property;
    if (infoProperty.isValid()) {
        maxFrameX_ = infoProperty[0].getValue();
        maxFrameY_ = infoProperty[1].getValue();
        framePixel_ = infoProperty[2].getValue();
        framePixelX_ = infoProperty[3].getValue();
        framePixelY_ = infoProperty[4].getValue();
        frameDepth_ = infoProperty[5].getValue();
    }
}

void INDICamera::handleBlobProperty(INDI::Property property) {
    if (property.getType() != INDI_BLOB) {
        return;
    }
    
    INDI::PropertyBlob blobProperty = property;
    if (blobProperty.isValid() && blobProperty[0].getBlobLen() > 0) {
        // Use enhanced image processing
        processReceivedImage(blobProperty);
    }
}

void INDICamera::handleVideoStreamProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch videoProperty = property;
    if (videoProperty.isValid()) {
        bool videoRunning = (videoProperty[0].getState() == ISS_ON);
        isVideoRunning_.store(videoRunning);
    }
}

// Enhanced image and video processing implementation
void INDICamera::processReceivedImage(const INDI::PropertyBlob &property) {
    if (!property.isValid() || property[0].getBlobLen() == 0) {
        spdlog::warn("Invalid image data received");
        droppedFrames_++;
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    size_t imageSize = property[0].getBlobLen();
    const void* imageData = property[0].getBlob();
    const char* format = property[0].getFormat();
    
    spdlog::info("Processing image: size={}, format={}", imageSize, format ? format : "unknown");
    
    // Validate image data
    if (!validateImageData(imageData, imageSize)) {
        spdlog::error("Image data validation failed");
        droppedFrames_++;
        return;
    }
    
    updateCameraState(CameraState::DOWNLOADING);
    
    // Create enhanced AtomCameraFrame
    current_frame_ = std::make_shared<AtomCameraFrame>();
    current_frame_->size = imageSize;
    current_frame_->data = malloc(current_frame_->size);
    
    if (!current_frame_->data) {
        spdlog::error("Failed to allocate memory for image data");
        droppedFrames_++;
        return;
    }
    
    memcpy(current_frame_->data, imageData, current_frame_->size);
    
    // Set comprehensive frame information
    current_frame_->resolution.width = frameWidth_;
    current_frame_->resolution.height = frameHeight_;
    current_frame_->resolution.maxWidth = maxFrameX_;
    current_frame_->resolution.maxHeight = maxFrameY_;
    
    current_frame_->binning.horizontal = binHor_;
    current_frame_->binning.vertical = binVer_;
    
    current_frame_->pixel.size = framePixel_;
    current_frame_->pixel.sizeX = framePixelX_;
    current_frame_->pixel.sizeY = framePixelY_;
    current_frame_->pixel.depth = frameDepth_;
    
    // Calculate frame rate
    totalFramesReceived_++;
    if (lastFrameTime_.time_since_epoch().count() != 0) {
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastFrameTime_).count();
        if (frameDuration > 0) {
            double frameRate = 1000.0 / frameDuration;
            averageFrameRate_ = (averageFrameRate_.load() * 0.9) + (frameRate * 0.1);
        }
    }
    lastFrameTime_ = now;
    
    // Basic image quality analysis (for 16-bit images)
    if (frameDepth_ == 16 && imageSize >= frameWidth_ * frameHeight_ * 2) {
        analyzeImageQuality(static_cast<const uint16_t*>(imageData), 
                          frameWidth_ * frameHeight_);
    }
    
    // Handle video recording
    if (isVideoRecording_.load()) {
        recordVideoFrame(current_frame_);
    }
    
    // Handle sequence capture
    if (isSequenceRunning_.load()) {
        handleSequenceCapture();
    }
    
    updateCameraState(CameraState::IDLE);
    
    // Notify video frame callback if available
    if (video_callback_) {
        video_callback_(current_frame_);
    }
    
    spdlog::debug("Image processed successfully. Total frames: {}, Frame rate: {:.2f} fps", 
                  totalFramesReceived_.load(), averageFrameRate_.load());
}

void INDICamera::setupImageFormats() {
    supportedImageFormats_ = {"FITS", "NATIVE", "XISF", "JPEG", "PNG", "TIFF"};
    currentImageFormat_ = "FITS"; // Default format
    
    // Query device for supported formats if available
    if (device_.isValid()) {
        INDI::PropertySwitch formatProperty = device_.getProperty("CCD_CAPTURE_FORMAT");
        if (formatProperty.isValid()) {
            supportedImageFormats_.clear();
            for (int i = 0; i < formatProperty.count(); i++) {
                supportedImageFormats_.push_back(formatProperty[i].getName());
            }
        }
    }
}

void INDICamera::setupVideoStreamOptions() {
    if (!device_.isValid()) {
        return;
    }
    
    // Setup video stream format
    INDI::PropertySwitch streamFormat = device_.getProperty("CCD_STREAM_FORMAT");
    if (streamFormat.isValid()) {
        // Set to preferred format (MJPEG for performance)
        for (int i = 0; i < streamFormat.count(); i++) {
            streamFormat[i].setState(ISS_OFF);
        }
        
        // Find and enable MJPEG if available
        for (int i = 0; i < streamFormat.count(); i++) {
            if (std::string(streamFormat[i].getName()).find("MJPEG") != std::string::npos ||
                std::string(streamFormat[i].getName()).find("JPEG") != std::string::npos) {
                streamFormat[i].setState(ISS_ON);
                currentVideoFormat_ = streamFormat[i].getName();
                break;
            }
        }
        sendNewProperty(streamFormat);
    }
    
    // Setup video recorder
    INDI::PropertySwitch recorder = device_.getProperty("RECORD_STREAM");
    if (recorder.isValid()) {
        spdlog::info("Video recording capability detected");
    }
}

auto INDICamera::getImageFormat(const std::string& extension) -> std::string {
    if (extension == ".fits" || extension == ".fit") return "FITS";
    if (extension == ".jpg" || extension == ".jpeg") return "JPEG";
    if (extension == ".png") return "PNG";
    if (extension == ".tiff" || extension == ".tif") return "TIFF";
    if (extension == ".xisf") return "XISF";
    return "FITS"; // Default
}

auto INDICamera::validateImageData(const void* data, size_t size) -> bool {
    if (!data || size == 0) {
        return false;
    }
    
    // Check minimum size for a valid image
    size_t expectedMinSize = frameWidth_ * frameHeight_ * (frameDepth_ / 8);
    if (size < expectedMinSize) {
        spdlog::warn("Image size {} smaller than expected minimum {}", size, expectedMinSize);
        // Don't reject, as some formats may be compressed
    }
    
    // Basic FITS header validation
    if (size >= 2880) // FITS minimum header size
    {
        const char* header = static_cast<const char*>(data);
        if (strncmp(header, "SIMPLE  ", 8) == 0) {
            spdlog::debug("FITS format detected");
            return true;
        }
    }
    
    // For other formats, assume valid for now
    return true;
}

// Advanced video features implementation
auto INDICamera::startVideoRecording(const std::string& filename) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("Camera not connected");
        return false;
    }
    
    if (isVideoRecording_.load()) {
        spdlog::warn("Video recording already in progress");
        return false;
    }
    
    // Check if device supports video recording
    INDI::PropertySwitch recorder = device_.getProperty("RECORD_STREAM");
    if (!recorder.isValid()) {
        spdlog::error("Device does not support video recording");
        return false;
    }
    
    // Set recording filename
    INDI::PropertyText filename_prop = device_.getProperty("RECORD_FILE");
    if (filename_prop.isValid()) {
        filename_prop[0].setText(filename.c_str());
        sendNewProperty(filename_prop);
    }
    
    // Start recording
    recorder.reset();
    recorder[0].setState(ISS_ON); // Record ON
    sendNewProperty(recorder);
    
    isVideoRecording_.store(true);
    videoRecordingFile_ = filename;
    
    spdlog::info("Started video recording to: {}", filename);
    return true;
}

auto INDICamera::stopVideoRecording() -> bool {
    if (!isVideoRecording_.load()) {
        spdlog::warn("No video recording in progress");
        return false;
    }
    
    INDI::PropertySwitch recorder = device_.getProperty("RECORD_STREAM");
    if (recorder.isValid()) {
        recorder.reset();
        recorder[1].setState(ISS_ON); // Record OFF
        sendNewProperty(recorder);
    }
    
    isVideoRecording_.store(false);
    spdlog::info("Stopped video recording");
    return true;
}

auto INDICamera::isVideoRecording() const -> bool {
    return isVideoRecording_.load();
}

auto INDICamera::setVideoExposure(double exposure) -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    
    INDI::PropertyNumber streamExp = device_.getProperty("STREAMING_EXPOSURE");
    if (!streamExp.isValid()) {
        // Fallback to regular exposure for video
        return startExposure(exposure);
    }
    
    streamExp[0].setValue(exposure);
    sendNewProperty(streamExp);
    videoExposure_.store(exposure);
    
    spdlog::debug("Set video exposure to {} seconds", exposure);
    return true;
}

auto INDICamera::getVideoExposure() const -> double {
    return videoExposure_.load();
}

auto INDICamera::setVideoGain(int gain) -> bool {
    videoGain_.store(gain);
    return setGain(gain); // Use existing gain implementation
}

auto INDICamera::getVideoGain() const -> int {
    return videoGain_.load();
}

// Image sequence capabilities
auto INDICamera::startSequence(int count, double exposure, double interval) -> bool {
    if (!isConnected_.load()) {
        spdlog::error("Camera not connected");
        return false;
    }
    
    if (isSequenceRunning_.load()) {
        spdlog::warn("Sequence already running");
        return false;
    }
    
    if (count <= 0 || exposure <= 0) {
        spdlog::error("Invalid sequence parameters");
        return false;
    }
    
    sequenceTotal_.store(count);
    sequenceCount_.store(0);
    sequenceExposure_.store(exposure);
    sequenceInterval_.store(interval);
    sequenceStartTime_ = std::chrono::system_clock::now();
    lastSequenceCapture_ = std::chrono::system_clock::time_point{};
    
    isSequenceRunning_.store(true);
    
    spdlog::info("Starting sequence: {} frames, {} sec exposure, {} sec interval", 
                 count, exposure, interval);
    
    // Start first exposure
    return startExposure(exposure);
}

auto INDICamera::stopSequence() -> bool {
    if (!isSequenceRunning_.load()) {
        return false;
    }
    
    isSequenceRunning_.store(false);
    abortExposure(); // Stop current exposure if any
    
    spdlog::info("Sequence stopped. Captured {}/{} frames", 
                 sequenceCount_.load(), sequenceTotal_.load());
    return true;
}

auto INDICamera::isSequenceRunning() const -> bool {
    return isSequenceRunning_.load();
}

auto INDICamera::getSequenceProgress() const -> std::pair<int, int> {
    return {sequenceCount_.load(), sequenceTotal_.load()};
}

void INDICamera::handleSequenceCapture() {
    if (!isSequenceRunning_.load()) {
        return;
    }
    
    int current = sequenceCount_.load();
    int total = sequenceTotal_.load();
    
    current++;
    sequenceCount_.store(current);
    
    spdlog::info("Sequence progress: {}/{}", current, total);
    
    // Update sequence info structure
    sequence_info_.currentFrame = current;
    sequence_info_.totalFrames = total;
    sequence_info_.state = SequenceState::RUNNING;
    
    // Notify sequence progress
    if (sequence_callback_) {
        sequence_callback_(SequenceState::RUNNING, current, total);
    }
    
    if (current >= total) {
        // Sequence complete
        isSequenceRunning_.store(false);
        sequence_info_.state = SequenceState::COMPLETED;
        
        if (sequence_callback_) {
            sequence_callback_(SequenceState::COMPLETED, current, total);
        }
        
        spdlog::info("Sequence completed successfully");
        return;
    }
    
    // Schedule next exposure considering interval
    auto now = std::chrono::system_clock::now();
    auto intervalMs = static_cast<long>(sequenceInterval_.load() * 1000);
    
    if (lastSequenceCapture_.time_since_epoch().count() != 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastSequenceCapture_).count();
        
        if (elapsed < intervalMs) {
            // Wait for interval
            auto waitTime = intervalMs - elapsed;
            spdlog::debug("Waiting {} ms before next exposure", waitTime);
            
            // Use a timer or thread to schedule next exposure
            std::thread([this, waitTime]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
                if (isSequenceRunning_.load()) {
                    startExposure(sequenceExposure_.load());
                }
            }).detach();
        } else {
            // Start immediately
            startExposure(sequenceExposure_.load());
        }
    } else {
        // First frame, start immediately
        startExposure(sequenceExposure_.load());
    }
    
    lastSequenceCapture_ = now;
}

// Advanced image processing
auto INDICamera::setImageFormat(const std::string& format) -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    
    // Check if format is supported
    auto it = std::find(supportedImageFormats_.begin(), supportedImageFormats_.end(), format);
    if (it == supportedImageFormats_.end()) {
        spdlog::error("Image format {} not supported", format);
        return false;
    }
    
    // Set format via INDI property if available
    INDI::PropertySwitch formatProperty = device_.getProperty("CCD_CAPTURE_FORMAT");
    if (formatProperty.isValid()) {
        formatProperty.reset();
        for (int i = 0; i < formatProperty.count(); i++) {
            if (std::string(formatProperty[i].getName()) == format) {
                formatProperty[i].setState(ISS_ON);
                break;
            }
        }
        sendNewProperty(formatProperty);
    }
    
    currentImageFormat_ = format;
    spdlog::info("Image format set to: {}", format);
    return true;
}

auto INDICamera::getImageFormat() const -> std::string {
    return currentImageFormat_;
}

auto INDICamera::enableImageCompression(bool enable) -> bool {
    if (!isConnected_.load()) {
        return false;
    }
    
    INDI::PropertySwitch compression = device_.getProperty("CCD_COMPRESSION");
    if (compression.isValid()) {
        compression.reset();
        compression[0].setState(enable ? ISS_ON : ISS_OFF);
        sendNewProperty(compression);
        
        imageCompressionEnabled_.store(enable);
        spdlog::info("Image compression {}", enable ? "enabled" : "disabled");
        return true;
    }
    
    return false;
}

auto INDICamera::isImageCompressionEnabled() const -> bool {
    return imageCompressionEnabled_.load();
}

// Helper methods
void INDICamera::recordVideoFrame(std::shared_ptr<AtomCameraFrame> frame) {
    // This would integrate with video encoding libraries
    // For now, just log the frame recording
    spdlog::debug("Recording video frame to: {}", videoRecordingFile_);
}

void INDICamera::analyzeImageQuality(const uint16_t* data, size_t pixelCount) {
    if (!data || pixelCount == 0) {
        return;
    }
    
    uint64_t sum = 0;
    uint16_t minVal = 65535;
    uint16_t maxVal = 0;
    
    // Calculate basic statistics
    for (size_t i = 0; i < pixelCount; i++) {
        uint16_t pixel = data[i];
        sum += pixel;
        minVal = std::min(minVal, pixel);
        maxVal = std::max(maxVal, pixel);
    }
    
    double mean = static_cast<double>(sum) / pixelCount;
    
    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = 0; i < pixelCount; i++) {
        double diff = data[i] - mean;
        variance += diff * diff;
    }
    double stdDev = std::sqrt(variance / pixelCount);
    
    // Store results in atomic variables
    lastImageMean_.store(mean);
    lastImageStdDev_.store(stdDev);
    lastImageMin_.store(minVal);
    lastImageMax_.store(maxVal);
    
    // Update enhanced image quality structure
    last_image_quality_.mean = mean;
    last_image_quality_.standardDeviation = stdDev;
    last_image_quality_.minimum = minVal;
    last_image_quality_.maximum = maxVal;
    last_image_quality_.signal = mean;
    last_image_quality_.noise = stdDev;
    last_image_quality_.snr = stdDev > 0 ? mean / stdDev : 0.0;
    
    // Notify image quality callback
    if (image_quality_callback_) {
        image_quality_callback_(last_image_quality_);
    }
    
    spdlog::debug("Image quality: mean={:.1f}, std={:.1f}, min={}, max={}, SNR={:.2f}", 
                  mean, stdDev, minVal, maxVal, last_image_quality_.snr);
}

ATOM_MODULE(camera_indi, [](Component &component) {
    LOG_F(INFO, "Registering camera_indi module...");

    // 基础设备控制
    component.def("initialize", &INDICamera::initialize, "device",
                  "Initialize camera device.");
    component.def("destroy", &INDICamera::destroy, "device",
                  "Destroy camera device.");
    component.def("connect", &INDICamera::connect, "device",
                  "Connect to a camera device.");
    component.def("disconnect", &INDICamera::disconnect, "device",
                  "Disconnect from a camera device.");
    component.def("scan", &INDICamera::scan, "Scan for camera devices.");
    component.def("is_connected", &INDICamera::isConnected,
                  "Check if a camera device is connected.");

    // 曝光控制
    component.def("start_exposure", &INDICamera::startExposure, "device",
                  "Start exposure.");
    component.def("abort_exposure", &INDICamera::abortExposure, "device",
                  "Abort exposure.");
    component.def("is_exposing", &INDICamera::isExposing,
                  "Check if camera is exposing.");
    component.def("get_exposure_progress", &INDICamera::getExposureProgress,
                  "Get exposure progress.");
    component.def("get_exposure_remaining", &INDICamera::getExposureRemaining,
                  "Get remaining exposure time.");
    component.def("save_image", &INDICamera::saveImage,
                  "Save captured image to file.");

    // 温度控制
    component.def("start_cooling", &INDICamera::startCooling, "device",
                  "Start cooling.");
    component.def("stop_cooling", &INDICamera::stopCooling, "device",
                  "Stop cooling.");
    component.def("get_temperature", &INDICamera::getTemperature,
                  "Get the current temperature of a camera device.");
    component.def("set_temperature", &INDICamera::setTemperature,
                  "Set the temperature of a camera device.");
    component.def("is_cooler_on", &INDICamera::isCoolerOn,
                  "Check if cooler is on.");
    component.def("has_cooler", &INDICamera::hasCooler,
                  "Check if camera has cooler.");

    // 参数控制
    component.def("get_gain", &INDICamera::getGain,
                  "Get the current gain of a camera device.");
    component.def("set_gain", &INDICamera::setGain,
                  "Set the gain of a camera device.");
    component.def("get_offset", &INDICamera::getOffset,
                  "Get the current offset of a camera device.");
    component.def("set_offset", &INDICamera::setOffset,
                  "Set the offset of a camera device.");

    // 帧设置
    component.def("get_binning", &INDICamera::getBinning,
                  "Get the current binning of a camera device.");
    component.def("set_binning", &INDICamera::setBinning,
                  "Set the binning of a camera device.");
    component.def("set_resolution", &INDICamera::setResolution,
                  "Set camera resolution.");
    component.def("get_frame_type", &INDICamera::getFrameType, "device",
                  "Get the current frame type of a camera device.");
    component.def("set_frame_type", &INDICamera::setFrameType, "device",
                  "Set the frame type of a camera device.");

    // 视频控制
    component.def("start_video", &INDICamera::startVideo,
                  "Start video streaming.");
    component.def("stop_video", &INDICamera::stopVideo,
                  "Stop video streaming.");
    component.def("is_video_running", &INDICamera::isVideoRunning,
                  "Check if video is running.");
    
    // 增强视频功能
    component.def("start_video_recording", &INDICamera::startVideoRecording,
                  "Start video recording to file.");
    component.def("stop_video_recording", &INDICamera::stopVideoRecording,
                  "Stop video recording.");
    component.def("is_video_recording", &INDICamera::isVideoRecording,
                  "Check if video recording is active.");
    component.def("set_video_exposure", &INDICamera::setVideoExposure,
                  "Set video exposure time.");
    component.def("get_video_exposure", &INDICamera::getVideoExposure,
                  "Get video exposure time.");
    component.def("set_video_gain", &INDICamera::setVideoGain,
                  "Set video gain.");
    component.def("get_video_gain", &INDICamera::getVideoGain,
                  "Get video gain.");
    
    // 图像序列功能
    component.def("start_sequence", &INDICamera::startSequence,
                  "Start image sequence capture.");
    component.def("stop_sequence", &INDICamera::stopSequence,
                  "Stop image sequence capture.");
    component.def("is_sequence_running", &INDICamera::isSequenceRunning,
                  "Check if sequence is running.");
    component.def("get_sequence_progress", &INDICamera::getSequenceProgress,
                  "Get sequence progress.");
    
    // 图像格式和压缩
    component.def("set_image_format", 
                  static_cast<bool(INDICamera::*)(const std::string&)>(&INDICamera::setImageFormat),
                  "Set image format.");
    component.def("get_current_image_format", 
                  static_cast<std::string(INDICamera::*)() const>(&INDICamera::getImageFormat),
                  "Get current image format.");
    component.def("enable_image_compression", &INDICamera::enableImageCompression,
                  "Enable/disable image compression.");
    component.def("is_image_compression_enabled", &INDICamera::isImageCompressionEnabled,
                  "Check if image compression is enabled.");
                  
    // 统计和质量信息
    component.def("get_supported_image_formats", &INDICamera::getSupportedImageFormats,
                  "Get list of supported image formats.");
    component.def("get_frame_statistics", &INDICamera::getFrameStatistics,
                  "Get frame statistics.");
    component.def("get_total_frames", &INDICamera::getTotalFramesReceived,
                  "Get total frames received.");
    component.def("get_dropped_frames", &INDICamera::getDroppedFrames,
                  "Get number of dropped frames.");
    component.def("get_average_frame_rate", &INDICamera::getAverageFrameRate,
                  "Get average frame rate.");
    component.def("get_image_quality", &INDICamera::getLastImageQuality,
                  "Get last image quality metrics.");

    // 工厂方法
    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomCamera> instance =
                std::make_shared<INDICamera>(name);
            return instance;
        },
        "device", "Create a new camera instance.");

    component.defType<INDICamera>("camera_indi", "device",
                                  "Define a new camera instance.");

    LOG_F(INFO, "Registered camera_indi module.");
});

