#include "indi_camera.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi::camera {

INDICamera::INDICamera(std::string deviceName)
    : AtomCamera(deviceName) {
    spdlog::info("Creating component-based INDI camera for device: {}", deviceName);
    
    // Create core component first
    core_ = std::make_shared<INDICameraCore>(deviceName);
    
    // Create all other components
    exposureController_ = std::make_shared<ExposureController>(core_);
    videoController_ = std::make_shared<VideoController>(core_);
    temperatureController_ = std::make_shared<TemperatureController>(core_);
    hardwareController_ = std::make_shared<HardwareController>(core_);
    imageProcessor_ = std::make_shared<ImageProcessor>(core_);
    sequenceManager_ = std::make_shared<SequenceManager>(core_);
    propertyHandler_ = std::make_shared<PropertyHandler>(core_);
    
    initializeComponents();
}

auto INDICamera::initialize() -> bool {
    spdlog::info("Initializing component-based INDI camera");
    
    // Initialize core first
    if (!core_->initialize()) {
        spdlog::error("Failed to initialize core component");
        return false;
    }
    
    // Initialize all components
    if (!exposureController_->initialize() ||
        !videoController_->initialize() ||
        !temperatureController_->initialize() ||
        !hardwareController_->initialize() ||
        !imageProcessor_->initialize() ||
        !sequenceManager_->initialize() ||
        !propertyHandler_->initialize()) {
        spdlog::error("Failed to initialize one or more camera components");
        return false;
    }
    
    setupComponentCommunication();
    registerPropertyHandlers();
    
    spdlog::info("All camera components initialized successfully");
    return true;
}

auto INDICamera::destroy() -> bool {
    spdlog::info("Destroying component-based INDI camera");
    
    // Destroy components in reverse order
    propertyHandler_->destroy();
    sequenceManager_->destroy();
    imageProcessor_->destroy();
    hardwareController_->destroy();
    temperatureController_->destroy();
    videoController_->destroy();
    exposureController_->destroy();
    core_->destroy();
    
    return true;
}

auto INDICamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    return core_->connect(deviceName, timeout, maxRetry);
}

auto INDICamera::disconnect() -> bool {
    return core_->disconnect();
}

auto INDICamera::isConnected() const -> bool {
    return core_->isConnected();
}

auto INDICamera::scan() -> std::vector<std::string> {
    return core_->scan();
}

// Exposure control delegation
auto INDICamera::startExposure(double duration) -> bool {
    return exposureController_->startExposure(duration);
}

auto INDICamera::abortExposure() -> bool {
    return exposureController_->abortExposure();
}

auto INDICamera::isExposing() const -> bool {
    return exposureController_->isExposing();
}

auto INDICamera::getExposureProgress() const -> double {
    return exposureController_->getExposureProgress();
}

auto INDICamera::getExposureRemaining() const -> double {
    return exposureController_->getExposureRemaining();
}

auto INDICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    return exposureController_->getExposureResult();
}

auto INDICamera::saveImage(const std::string& path) -> bool {
    return exposureController_->saveImage(path);
}

auto INDICamera::getLastExposureDuration() const -> double {
    return exposureController_->getLastExposureDuration();
}

auto INDICamera::getExposureCount() const -> uint32_t {
    return exposureController_->getExposureCount();
}

auto INDICamera::resetExposureCount() -> bool {
    return exposureController_->resetExposureCount();
}

// Video control delegation
auto INDICamera::startVideo() -> bool {
    return videoController_->startVideo();
}

auto INDICamera::stopVideo() -> bool {
    return videoController_->stopVideo();
}

auto INDICamera::isVideoRunning() const -> bool {
    return videoController_->isVideoRunning();
}

auto INDICamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    return videoController_->getVideoFrame();
}

auto INDICamera::setVideoFormat(const std::string& format) -> bool {
    return videoController_->setVideoFormat(format);
}

auto INDICamera::getVideoFormats() -> std::vector<std::string> {
    return videoController_->getVideoFormats();
}

auto INDICamera::startVideoRecording(const std::string& filename) -> bool {
    return videoController_->startVideoRecording(filename);
}

auto INDICamera::stopVideoRecording() -> bool {
    return videoController_->stopVideoRecording();
}

auto INDICamera::isVideoRecording() const -> bool {
    return videoController_->isVideoRecording();
}

auto INDICamera::setVideoExposure(double exposure) -> bool {
    return videoController_->setVideoExposure(exposure);
}

auto INDICamera::getVideoExposure() const -> double {
    return videoController_->getVideoExposure();
}

auto INDICamera::setVideoGain(int gain) -> bool {
    return videoController_->setVideoGain(gain);
}

auto INDICamera::getVideoGain() const -> int {
    return videoController_->getVideoGain();
}

// Temperature control delegation
auto INDICamera::startCooling(double targetTemp) -> bool {
    return temperatureController_->startCooling(targetTemp);
}

auto INDICamera::stopCooling() -> bool {
    return temperatureController_->stopCooling();
}

auto INDICamera::isCoolerOn() const -> bool {
    return temperatureController_->isCoolerOn();
}

auto INDICamera::getTemperature() const -> std::optional<double> {
    return temperatureController_->getTemperature();
}

auto INDICamera::getTemperatureInfo() const -> ::TemperatureInfo {
    return temperatureController_->getTemperatureInfo();
}

auto INDICamera::getCoolingPower() const -> std::optional<double> {
    return temperatureController_->getCoolingPower();
}

auto INDICamera::hasCooler() const -> bool {
    return temperatureController_->hasCooler();
}

auto INDICamera::setTemperature(double temperature) -> bool {
    return temperatureController_->setTemperature(temperature);
}

// Hardware control delegation
auto INDICamera::isColor() const -> bool {
    return hardwareController_->isColor();
}

auto INDICamera::getBayerPattern() const -> BayerPattern {
    return hardwareController_->getBayerPattern();
}

auto INDICamera::setBayerPattern(BayerPattern pattern) -> bool {
    return hardwareController_->setBayerPattern(pattern);
}

auto INDICamera::setGain(int gain) -> bool {
    return hardwareController_->setGain(gain);
}

auto INDICamera::getGain() -> std::optional<int> {
    return hardwareController_->getGain();
}

auto INDICamera::getGainRange() -> std::pair<int, int> {
    return hardwareController_->getGainRange();
}

auto INDICamera::setOffset(int offset) -> bool {
    return hardwareController_->setOffset(offset);
}

auto INDICamera::getOffset() -> std::optional<int> {
    return hardwareController_->getOffset();
}

auto INDICamera::getOffsetRange() -> std::pair<int, int> {
    return hardwareController_->getOffsetRange();
}

auto INDICamera::setISO(int iso) -> bool {
    return hardwareController_->setISO(iso);
}

auto INDICamera::getISO() -> std::optional<int> {
    return hardwareController_->getISO();
}

auto INDICamera::getISOList() -> std::vector<int> {
    return hardwareController_->getISOList();
}

auto INDICamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    return hardwareController_->getResolution();
}

auto INDICamera::setResolution(int x, int y, int width, int height) -> bool {
    return hardwareController_->setResolution(x, y, width, height);
}

auto INDICamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    return hardwareController_->getMaxResolution();
}

auto INDICamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    return hardwareController_->getBinning();
}

auto INDICamera::setBinning(int horizontal, int vertical) -> bool {
    return hardwareController_->setBinning(horizontal, vertical);
}

auto INDICamera::getMaxBinning() -> AtomCameraFrame::Binning {
    return hardwareController_->getMaxBinning();
}

auto INDICamera::setFrameType(FrameType type) -> bool {
    return hardwareController_->setFrameType(type);
}

auto INDICamera::getFrameType() -> FrameType {
    return hardwareController_->getFrameType();
}

auto INDICamera::setUploadMode(UploadMode mode) -> bool {
    return hardwareController_->setUploadMode(mode);
}

auto INDICamera::getUploadMode() -> UploadMode {
    return hardwareController_->getUploadMode();
}

auto INDICamera::getPixelSize() -> double {
    return hardwareController_->getPixelSize();
}

auto INDICamera::getPixelSizeX() -> double {
    return hardwareController_->getPixelSizeX();
}

auto INDICamera::getPixelSizeY() -> double {
    return hardwareController_->getPixelSizeY();
}

auto INDICamera::getBitDepth() -> int {
    return hardwareController_->getBitDepth();
}

auto INDICamera::hasShutter() -> bool {
    return hardwareController_->hasShutter();
}

auto INDICamera::setShutter(bool open) -> bool {
    return hardwareController_->setShutter(open);
}

auto INDICamera::getShutterStatus() -> bool {
    return hardwareController_->getShutterStatus();
}

auto INDICamera::hasFan() -> bool {
    return hardwareController_->hasFan();
}

auto INDICamera::setFanSpeed(int speed) -> bool {
    return hardwareController_->setFanSpeed(speed);
}

auto INDICamera::getFanSpeed() -> int {
    return hardwareController_->getFanSpeed();
}

auto INDICamera::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    return hardwareController_->getFrameInfo();
}

// Sequence management delegation
auto INDICamera::startSequence(int count, double exposure, double interval) -> bool {
    return sequenceManager_->startSequence(count, exposure, interval);
}

auto INDICamera::stopSequence() -> bool {
    return sequenceManager_->stopSequence();
}

auto INDICamera::isSequenceRunning() const -> bool {
    return sequenceManager_->isSequenceRunning();
}

auto INDICamera::getSequenceProgress() const -> std::pair<int, int> {
    return sequenceManager_->getSequenceProgress();
}

// Image processing delegation
auto INDICamera::setImageFormat(const std::string& format) -> bool {
    return imageProcessor_->setImageFormat(format);
}

auto INDICamera::getImageFormat() const -> std::string {
    return imageProcessor_->getImageFormat();
}

auto INDICamera::enableImageCompression(bool enable) -> bool {
    return imageProcessor_->enableImageCompression(enable);
}

auto INDICamera::isImageCompressionEnabled() const -> bool {
    return imageProcessor_->isImageCompressionEnabled();
}

auto INDICamera::getSupportedImageFormats() const -> std::vector<std::string> {
    return imageProcessor_->getSupportedImageFormats();
}

auto INDICamera::getFrameStatistics() const -> std::map<std::string, double> {
    return imageProcessor_->getFrameStatistics();
}

auto INDICamera::getTotalFramesReceived() const -> uint64_t {
    return videoController_->getTotalFramesReceived();
}

auto INDICamera::getDroppedFrames() const -> uint64_t {
    return videoController_->getDroppedFrames();
}

auto INDICamera::getAverageFrameRate() const -> double {
    return videoController_->getAverageFrameRate();
}

auto INDICamera::getLastImageQuality() const -> std::map<std::string, double> {
    return imageProcessor_->getLastImageQuality();
}

// Private helper methods
void INDICamera::initializeComponents() {
    spdlog::debug("Initializing component relationships");
    
    // Register all components with the core
    core_->registerComponent(exposureController_);
    core_->registerComponent(videoController_);
    core_->registerComponent(temperatureController_);
    core_->registerComponent(hardwareController_);
    core_->registerComponent(imageProcessor_);
    core_->registerComponent(sequenceManager_);
    core_->registerComponent(propertyHandler_);
}

void INDICamera::registerPropertyHandlers() {
    spdlog::debug("Registering property handlers");
    
    // Register exposure controller properties
    propertyHandler_->registerPropertyHandler("CCD_EXPOSURE", exposureController_.get());
    propertyHandler_->registerPropertyHandler("CCD1", exposureController_.get());
    
    // Register video controller properties
    propertyHandler_->registerPropertyHandler("CCD_VIDEO_STREAM", videoController_.get());
    propertyHandler_->registerPropertyHandler("CCD_VIDEO_FORMAT", videoController_.get());
    
    // Register temperature controller properties
    propertyHandler_->registerPropertyHandler("CCD_TEMPERATURE", temperatureController_.get());
    propertyHandler_->registerPropertyHandler("CCD_COOLER", temperatureController_.get());
    propertyHandler_->registerPropertyHandler("CCD_COOLER_POWER", temperatureController_.get());
    
    // Register hardware controller properties
    propertyHandler_->registerPropertyHandler("CCD_GAIN", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_OFFSET", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_FRAME", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_BINNING", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_INFO", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_FRAME_TYPE", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_SHUTTER", hardwareController_.get());
    propertyHandler_->registerPropertyHandler("CCD_FAN", hardwareController_.get());
    
    // Register image processor properties
    propertyHandler_->registerPropertyHandler("CCD1", imageProcessor_.get());
}

void INDICamera::setupComponentCommunication() {
    spdlog::debug("Setting up component communication");
    
    // Set exposure controller reference in sequence manager
    sequenceManager_->setExposureController(exposureController_.get());
    
    // Setup any other inter-component communication as needed
    // For example, callbacks between components
}

} // namespace lithium::device::indi::camera
