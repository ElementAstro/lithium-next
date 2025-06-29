/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Camera Controller Implementation

This modular controller orchestrates the camera components to provide
a clean, maintainable, and testable interface for ASCOM camera control.

*************************************************/

#include "controller.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::device::ascom::camera {

ASCOMCameraController::ASCOMCameraController(const std::string& name)
    : AtomCamera(name) {
    LOG_F(INFO, "Creating ASCOM Camera Controller: {}", name);
}

ASCOMCameraController::~ASCOMCameraController() {
    LOG_F(INFO, "Destroying ASCOM Camera Controller");
    if (initialized_) {
        shutdownComponents();
    }
}

// =========================================================================
// AtomDriver Interface Implementation
// =========================================================================

auto ASCOMCameraController::initialize() -> bool {
    LOG_F(INFO, "Initializing ASCOM Camera Controller");

    if (initialized_) {
        LOG_F(WARNING, "Controller already initialized");
        return true;
    }

    if (!initializeComponents()) {
        LOG_F(ERROR, "Failed to initialize components");
        return false;
    }

    initialized_ = true;
    LOG_F(INFO, "ASCOM Camera Controller initialized successfully");
    return true;
}

auto ASCOMCameraController::destroy() -> bool {
    LOG_F(INFO, "Destroying ASCOM Camera Controller");

    if (!initialized_) {
        LOG_F(WARNING, "Controller not initialized");
        return true;
    }

    // Disconnect if connected
    if (connected_) {
        disconnect();
    }

    if (!shutdownComponents()) {
        LOG_F(ERROR, "Failed to shutdown components properly");
        return false;
    }

    initialized_ = false;
    LOG_F(INFO, "ASCOM Camera Controller destroyed successfully");
    return true;
}

auto ASCOMCameraController::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    LOG_F(INFO, "Connecting to ASCOM camera: {} (timeout: {}ms, retries: {})", deviceName, timeout, maxRetry);

    if (!initialized_) {
        LOG_F(ERROR, "Controller not initialized");
        return false;
    }

    if (connected_) {
        LOG_F(WARNING, "Already connected");
        return true;
    }

    if (!validateComponentsReady()) {
        LOG_F(ERROR, "Components not ready for connection");
        return false;
    }

    // Connect hardware interface
    components::HardwareInterface::ConnectionSettings settings;
    settings.deviceName = deviceName;

    if (!hardwareInterface_->connect(settings)) {
        LOG_F(ERROR, "Failed to connect hardware interface");
        return false;
    }

    connected_ = true;
    LOG_F(INFO, "Successfully connected to ASCOM camera: {}", deviceName);
    return true;
}

auto ASCOMCameraController::disconnect() -> bool {
    LOG_F(INFO, "Disconnecting ASCOM camera");

    if (!connected_) {
        LOG_F(WARNING, "Not connected");
        return true;
    }

    // Stop any ongoing operations
    if (exposureManager_ && exposureManager_->isExposing()) {
        exposureManager_->abortExposure();
    }

    if (videoManager_ && videoManager_->isRecording()) {
        videoManager_->stopRecording();
    }

    if (sequenceManager_ && sequenceManager_->isSequenceRunning()) {
        sequenceManager_->stopSequence();
    }

    // Disconnect hardware interface
    if (hardwareInterface_) {
        hardwareInterface_->disconnect();
    }

    connected_ = false;
    LOG_F(INFO, "Disconnected from ASCOM camera");
    return true;
}

auto ASCOMCameraController::scan() -> std::vector<std::string> {
    LOG_F(INFO, "Scanning for ASCOM cameras");

    if (!hardwareInterface_) {
        LOG_F(ERROR, "Hardware interface not available");
        return {};
    }

    // Placeholder implementation
    return {"ASCOM.Simulator.Camera"};
}

auto ASCOMCameraController::isConnected() const -> bool {
    return connected_.load() &&
           hardwareInterface_ &&
           hardwareInterface_->isConnected();
}

// =========================================================================
// AtomCamera Interface Implementation - Exposure Control
// =========================================================================

auto ASCOMCameraController::startExposure(double duration) -> bool {
    if (!exposureManager_) {
        LOG_F(ERROR, "Exposure manager not available");
        return false;
    }

    if (!isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    bool result = exposureManager_->startExposure(duration);
    if (result) {
        exposureCount_++;
        lastExposureDuration_ = duration;
    }

    return result;
}

auto ASCOMCameraController::abortExposure() -> bool {
    if (!exposureManager_) {
        LOG_F(ERROR, "Exposure manager not available");
        return false;
    }

    return exposureManager_->abortExposure();
}

auto ASCOMCameraController::isExposing() const -> bool {
    return exposureManager_ && exposureManager_->isExposing();
}

auto ASCOMCameraController::getExposureProgress() const -> double {
    return exposureManager_ ? exposureManager_->getProgress() : 0.0;
}

auto ASCOMCameraController::getExposureRemaining() const -> double {
    return exposureManager_ ? exposureManager_->getRemainingTime() : 0.0;
}

auto ASCOMCameraController::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    if (!exposureManager_) {
        return nullptr;
    }

    // Use getLastFrame instead of getResult
    auto frame = exposureManager_->getLastFrame();
    if (frame) {
        totalFramesReceived_++;

        // Apply image processing if enabled
        if (imageProcessor_) {
            auto processedFrame = imageProcessor_->processImage(frame);
            if (processedFrame) {
                frame = processedFrame;
            }
        }
    }

    return frame;
}

auto ASCOMCameraController::saveImage(const std::string &path) -> bool {
    // Placeholder implementation
    LOG_F(INFO, "Saving image to: {}", path);
    return true;
}

auto ASCOMCameraController::getLastExposureDuration() const -> double {
    return lastExposureDuration_.load();
}

auto ASCOMCameraController::getExposureCount() const -> uint32_t {
    return exposureCount_.load();
}

auto ASCOMCameraController::resetExposureCount() -> bool {
    exposureCount_ = 0;
    return true;
}

// =========================================================================
// AtomCamera Interface Implementation - Video/streaming control
// =========================================================================

auto ASCOMCameraController::startVideo() -> bool {
    return videoManager_ && videoManager_->startVideo();
}

auto ASCOMCameraController::stopVideo() -> bool {
    return videoManager_ && videoManager_->stopVideo();
}

auto ASCOMCameraController::isVideoRunning() const -> bool {
    return videoManager_ && videoManager_->isVideoActive();
}

auto ASCOMCameraController::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    return videoManager_ ? videoManager_->getLatestFrame() : nullptr;
}

auto ASCOMCameraController::setVideoFormat(const std::string &format) -> bool {
    return videoManager_ && videoManager_->setVideoFormat(format);
}

auto ASCOMCameraController::getVideoFormats() -> std::vector<std::string> {
    return videoManager_ ? videoManager_->getSupportedFormats() : std::vector<std::string>{};
}

// =========================================================================
// AtomCamera Interface Implementation - Temperature control
// =========================================================================

auto ASCOMCameraController::startCooling(double targetTemp) -> bool {
    return temperatureController_ && temperatureController_->startCooling(targetTemp);
}

auto ASCOMCameraController::stopCooling() -> bool {
    return temperatureController_ && temperatureController_->stopCooling();
}

auto ASCOMCameraController::isCoolerOn() const -> bool {
    return temperatureController_ && temperatureController_->isCoolerOn();
}

auto ASCOMCameraController::getTemperature() const -> std::optional<double> {
    if (!temperatureController_) {
        return std::nullopt;
    }

    double temp = temperatureController_->getCurrentTemperature();
    return std::optional<double>(temp);
}

auto ASCOMCameraController::getTemperatureInfo() const -> TemperatureInfo {
    TemperatureInfo info;

    if (temperatureController_) {
        info.current = temperatureController_->getCurrentTemperature();
        info.target = temperatureController_->getTargetTemperature();
        // Note: TemperatureInfo structure may not have power/enabled fields
        // info.power = temperatureController_->getCoolingPower();
        // info.enabled = temperatureController_->isCoolerOn();
    }

    return info;
}

auto ASCOMCameraController::getCoolingPower() const -> std::optional<double> {
    if (!temperatureController_) {
        return std::nullopt;
    }

    // Placeholder - return a dummy value for now
    return std::optional<double>(50.0);
}

auto ASCOMCameraController::hasCooler() const -> bool {
    return temperatureController_ && temperatureController_->hasCooler();
}

auto ASCOMCameraController::setTemperature(double temperature) -> bool {
    return temperatureController_ && temperatureController_->setTargetTemperature(temperature);
}

// =========================================================================
// AtomCamera Interface Implementation - Color information
// =========================================================================

auto ASCOMCameraController::isColor() const -> bool {
    return propertyManager_ ? propertyManager_->isColor() : false;
}

auto ASCOMCameraController::getBayerPattern() const -> BayerPattern {
    return propertyManager_ ? propertyManager_->getBayerPattern() : BayerPattern::MONO;
}

auto ASCOMCameraController::setBayerPattern(BayerPattern pattern) -> bool {
    return propertyManager_ && propertyManager_->setBayerPattern(pattern);
}

// =========================================================================
// AtomCamera Interface Implementation - Parameter control
// =========================================================================

auto ASCOMCameraController::setGain(int gain) -> bool {
    return propertyManager_ && propertyManager_->setGain(gain);
}

auto ASCOMCameraController::getGain() -> std::optional<int> {
    return propertyManager_ ? propertyManager_->getGain() : std::nullopt;
}

auto ASCOMCameraController::getGainRange() -> std::pair<int, int> {
    return propertyManager_ ? propertyManager_->getGainRange() : std::make_pair(0, 100);
}

auto ASCOMCameraController::setOffset(int offset) -> bool {
    return propertyManager_ && propertyManager_->setOffset(offset);
}

auto ASCOMCameraController::getOffset() -> std::optional<int> {
    return propertyManager_ ? propertyManager_->getOffset() : std::nullopt;
}

auto ASCOMCameraController::getOffsetRange() -> std::pair<int, int> {
    return propertyManager_ ? propertyManager_->getOffsetRange() : std::make_pair(0, 100);
}

auto ASCOMCameraController::setISO(int iso) -> bool {
    return propertyManager_ && propertyManager_->setISO(iso);
}

auto ASCOMCameraController::getISO() -> std::optional<int> {
    return propertyManager_ ? propertyManager_->getISO() : std::nullopt;
}

auto ASCOMCameraController::getISOList() -> std::vector<int> {
    return propertyManager_ ? propertyManager_->getISOList() : std::vector<int>{};
}

// =========================================================================
// AtomCamera Interface Implementation - Frame settings
// =========================================================================

auto ASCOMCameraController::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    return propertyManager_ ? propertyManager_->getResolution() : std::nullopt;
}

auto ASCOMCameraController::setResolution(int x, int y, int width, int height) -> bool {
    return propertyManager_ && propertyManager_->setResolution(x, y, width, height);
}

auto ASCOMCameraController::getMaxResolution() -> AtomCameraFrame::Resolution {
    return propertyManager_ ? propertyManager_->getMaxResolution() : AtomCameraFrame::Resolution{};
}

auto ASCOMCameraController::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    return propertyManager_ ? propertyManager_->getBinning() : std::nullopt;
}

auto ASCOMCameraController::setBinning(int horizontal, int vertical) -> bool {
    return propertyManager_ && propertyManager_->setBinning(horizontal, vertical);
}

auto ASCOMCameraController::getMaxBinning() -> AtomCameraFrame::Binning {
    return propertyManager_ ? propertyManager_->getMaxBinning() : AtomCameraFrame::Binning{};
}

auto ASCOMCameraController::setFrameType(FrameType type) -> bool {
    return propertyManager_ && propertyManager_->setFrameType(type);
}

auto ASCOMCameraController::getFrameType() -> FrameType {
    return propertyManager_ ? propertyManager_->getFrameType() : FrameType::FITS;
}

auto ASCOMCameraController::setUploadMode(UploadMode mode) -> bool {
    return propertyManager_ && propertyManager_->setUploadMode(mode);
}

auto ASCOMCameraController::getUploadMode() -> UploadMode {
    return propertyManager_ ? propertyManager_->getUploadMode() : UploadMode::LOCAL;
}

auto ASCOMCameraController::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    return propertyManager_ ? propertyManager_->getFrameInfo() : nullptr;
}

// =========================================================================
// AtomCamera Interface Implementation - Pixel information
// =========================================================================

auto ASCOMCameraController::getPixelSize() -> double {
    return propertyManager_ ? propertyManager_->getPixelSize() : 0.0;
}

auto ASCOMCameraController::getPixelSizeX() -> double {
    return propertyManager_ ? propertyManager_->getPixelSizeX() : 0.0;
}

auto ASCOMCameraController::getPixelSizeY() -> double {
    return propertyManager_ ? propertyManager_->getPixelSizeY() : 0.0;
}

auto ASCOMCameraController::getBitDepth() -> int {
    return propertyManager_ ? propertyManager_->getBitDepth() : 16;
}

// =========================================================================
// AtomCamera Interface Implementation - Advanced features
// =========================================================================

auto ASCOMCameraController::hasShutter() -> bool {
    return propertyManager_ ? propertyManager_->hasShutter() : false;
}

auto ASCOMCameraController::setShutter(bool open) -> bool {
    return propertyManager_ && propertyManager_->setShutter(open);
}

auto ASCOMCameraController::getShutterStatus() -> bool {
    return propertyManager_ ? propertyManager_->getShutterStatus() : false;
}

auto ASCOMCameraController::hasFan() -> bool {
    return propertyManager_ ? propertyManager_->hasFan() : false;
}

auto ASCOMCameraController::setFanSpeed(int speed) -> bool {
    return propertyManager_ && propertyManager_->setFanSpeed(speed);
}

auto ASCOMCameraController::getFanSpeed() -> int {
    return propertyManager_ ? propertyManager_->getFanSpeed() : 0;
}

// Advanced video features
auto ASCOMCameraController::startVideoRecording(const std::string &filename) -> bool {
    if (!videoManager_) {
        return false;
    }

    // Create recording settings
    components::VideoManager::RecordingSettings settings;
    settings.filename = filename;
    settings.format = "AVI";
    settings.maxDuration = std::chrono::seconds(0); // unlimited

    return videoManager_->startRecording(settings);
}

auto ASCOMCameraController::stopVideoRecording() -> bool {
    return videoManager_ && videoManager_->stopRecording();
}

auto ASCOMCameraController::isVideoRecording() const -> bool {
    return videoManager_ && videoManager_->isRecording();
}

auto ASCOMCameraController::setVideoExposure(double exposure) -> bool {
    // Placeholder implementation
    LOG_F(INFO, "Setting video exposure: {}", exposure);
    return true;
}

auto ASCOMCameraController::getVideoExposure() const -> double {
    // Placeholder implementation
    return 1.0;
}

auto ASCOMCameraController::setVideoGain(int gain) -> bool {
    // Placeholder implementation
    LOG_F(INFO, "Setting video gain: {}", gain);
    return true;
}

auto ASCOMCameraController::getVideoGain() const -> int {
    // Placeholder implementation
    return 0;
}

// Image sequence capabilities
auto ASCOMCameraController::startSequence(int count, double exposure, double interval) -> bool {
    return sequenceManager_ && sequenceManager_->startSequence(count, exposure, interval);
}

auto ASCOMCameraController::stopSequence() -> bool {
    return sequenceManager_ && sequenceManager_->stopSequence();
}

auto ASCOMCameraController::isSequenceRunning() const -> bool {
    return sequenceManager_ && sequenceManager_->isSequenceRunning();
}

auto ASCOMCameraController::getSequenceProgress() const -> std::pair<int, int> {
    return sequenceManager_ ? sequenceManager_->getSequenceProgress() : std::make_pair(0, 0);
}

// Advanced image processing
auto ASCOMCameraController::setImageFormat(const std::string &format) -> bool {
    return imageProcessor_ && imageProcessor_->setImageFormat(format);
}

auto ASCOMCameraController::getImageFormat() const -> std::string {
    return imageProcessor_ ? imageProcessor_->getImageFormat() : "FITS";
}

auto ASCOMCameraController::enableImageCompression(bool enable) -> bool {
    return imageProcessor_ && imageProcessor_->enableImageCompression(enable);
}

auto ASCOMCameraController::isImageCompressionEnabled() const -> bool {
    return imageProcessor_ && imageProcessor_->isImageCompressionEnabled();
}

auto ASCOMCameraController::getSupportedImageFormats() const -> std::vector<std::string> {
    return imageProcessor_ ? imageProcessor_->getSupportedImageFormats() : std::vector<std::string>{"FITS"};
}

// Image quality and statistics
auto ASCOMCameraController::getFrameStatistics() const -> std::map<std::string, double> {
    std::map<std::string, double> stats;

    if (exposureManager_) {
        auto expStats = exposureManager_->getStatistics();
        stats["totalExposures"] = static_cast<double>(expStats.totalExposures);
        stats["successfulExposures"] = static_cast<double>(expStats.successfulExposures);
        stats["failedExposures"] = static_cast<double>(expStats.failedExposures);
        stats["abortedExposures"] = static_cast<double>(expStats.abortedExposures);
        stats["totalExposureTime"] = expStats.totalExposureTime;
        stats["averageExposureTime"] = expStats.averageExposureTime;
    }

    return stats;
}

auto ASCOMCameraController::getTotalFramesReceived() const -> uint64_t {
    return totalFramesReceived_.load();
}

auto ASCOMCameraController::getDroppedFrames() const -> uint64_t {
    return droppedFrames_.load();
}

auto ASCOMCameraController::getAverageFrameRate() const -> double {
    // Placeholder implementation
    return 10.0;
}

auto ASCOMCameraController::getLastImageQuality() const -> std::map<std::string, double> {
    if (!imageProcessor_) {
        return {};
    }

    auto quality = imageProcessor_->getLastImageQuality();
    return {
        {"snr", quality.snr},
        {"fwhm", quality.fwhm},
        {"brightness", quality.brightness},
        {"contrast", quality.contrast},
        {"noise", quality.noise},
        {"stars", static_cast<double>(quality.stars)}
    };
}

// =========================================================================
// Component Access
// =========================================================================

auto ASCOMCameraController::getHardwareInterface() -> std::shared_ptr<components::HardwareInterface> {
    return hardwareInterface_;
}

auto ASCOMCameraController::getExposureManager() -> std::shared_ptr<components::ExposureManager> {
    return exposureManager_;
}

auto ASCOMCameraController::getTemperatureController() -> std::shared_ptr<components::TemperatureController> {
    return temperatureController_;
}

auto ASCOMCameraController::getSequenceManager() -> std::shared_ptr<components::SequenceManager> {
    return sequenceManager_;
}

auto ASCOMCameraController::getPropertyManager() -> std::shared_ptr<components::PropertyManager> {
    return propertyManager_;
}

auto ASCOMCameraController::getVideoManager() -> std::shared_ptr<components::VideoManager> {
    return videoManager_;
}

auto ASCOMCameraController::getImageProcessor() -> std::shared_ptr<components::ImageProcessor> {
    return imageProcessor_;
}

// =========================================================================
// ASCOM-specific methods
// =========================================================================

auto ASCOMCameraController::getASCOMDriverInfo() -> std::optional<std::string> {
    if (hardwareInterface_) {
        return hardwareInterface_->getDriverInfo();
    }
    return std::nullopt;
}

auto ASCOMCameraController::getASCOMVersion() -> std::optional<std::string> {
    if (hardwareInterface_) {
        return hardwareInterface_->getDriverVersion();
    }
    return std::nullopt;
}

auto ASCOMCameraController::getASCOMInterfaceVersion() -> std::optional<int> {
    if (hardwareInterface_) {
        return hardwareInterface_->getInterfaceVersion();
    }
    return std::nullopt;
}

auto ASCOMCameraController::setASCOMClientID(const std::string &clientId) -> bool {
    // This functionality is handled internally by the hardware interface
    return hardwareInterface_ != nullptr;
}

auto ASCOMCameraController::getASCOMClientID() -> std::optional<std::string> {
    // Return a default client ID since the hardware interface doesn't expose this
    if (hardwareInterface_) {
        return std::string("Lithium-Next");
    }
    return std::nullopt;
}

// =========================================================================
// Private Helper Methods
// =========================================================================

auto ASCOMCameraController::initializeComponents() -> bool {
    LOG_F(INFO, "Initializing ASCOM camera components");

    try {
        // Create hardware interface first
        hardwareInterface_ = std::make_shared<components::HardwareInterface>();
        if (!hardwareInterface_->initialize()) {
            LOG_F(ERROR, "Failed to initialize hardware interface");
            return false;
        }

        // Create property manager
        propertyManager_ = std::make_shared<components::PropertyManager>(hardwareInterface_);
        if (!propertyManager_->initialize()) {
            LOG_F(ERROR, "Failed to initialize property manager");
            return false;
        }

        // Create exposure manager
        exposureManager_ = std::make_shared<components::ExposureManager>(hardwareInterface_);
        if (!exposureManager_) {
            LOG_F(ERROR, "Failed to create exposure manager");
            return false;
        }

        // Create temperature controller
        temperatureController_ = std::make_shared<components::TemperatureController>(hardwareInterface_);
        if (!temperatureController_) {
            LOG_F(ERROR, "Failed to create temperature controller");
            return false;
        }

        // Create video manager
        videoManager_ = std::make_shared<components::VideoManager>(hardwareInterface_);
        if (!videoManager_) {
            LOG_F(ERROR, "Failed to create video manager");
            return false;
        }

        // Create sequence manager
        sequenceManager_ = std::make_shared<components::SequenceManager>(hardwareInterface_);
        if (!sequenceManager_) {
            LOG_F(ERROR, "Failed to create sequence manager");
            return false;
        }

        // Create image processor
        imageProcessor_ = std::make_shared<components::ImageProcessor>(hardwareInterface_);
        if (!imageProcessor_) {
            LOG_F(ERROR, "Failed to create image processor");
            return false;
        }

        LOG_F(INFO, "All ASCOM camera components initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during component initialization: {}", e.what());
        return false;
    }
}

auto ASCOMCameraController::shutdownComponents() -> bool {
    LOG_F(INFO, "Shutting down ASCOM camera components");

    // Shutdown in reverse order
    imageProcessor_.reset();
    sequenceManager_.reset();
    videoManager_.reset();
    temperatureController_.reset();
    exposureManager_.reset();
    propertyManager_.reset();
    hardwareInterface_.reset();

    LOG_F(INFO, "ASCOM camera components shutdown complete");
    return true;
}

auto ASCOMCameraController::validateComponentsReady() const -> bool {
    return hardwareInterface_ &&
           exposureManager_ &&
           temperatureController_ &&
           propertyManager_ &&
           videoManager_ &&
           sequenceManager_ &&
           imageProcessor_;
}

// =========================================================================
// Factory Implementation
// =========================================================================

auto ControllerFactory::createModularController(const std::string& name)
    -> std::unique_ptr<ASCOMCameraController> {
    return std::make_unique<ASCOMCameraController>(name);
}

auto ControllerFactory::createSharedController(const std::string& name)
    -> std::shared_ptr<ASCOMCameraController> {
    return std::make_shared<ASCOMCameraController>(name);
}

} // namespace lithium::device::ascom::camera
