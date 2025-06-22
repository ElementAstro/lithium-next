/*
 * asi_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ZWO ASI Camera Implementation with Controller

*************************************************/

#include "asi_camera.hpp"
#include "controller/asi_camera_controller.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::device::asi::camera {

ASICamera::ASICamera(const std::string& name)
    : AtomCamera(name), controller_(std::make_unique<controller::ASICameraController>(this)) {
    
    // Initialize ASI camera specific capabilities
    CameraCapabilities caps;
    caps.canAbort = true;
    caps.canSubFrame = true;
    caps.canBin = true;
    caps.hasCooler = true;
    caps.hasGuideHead = false;
    caps.hasShutter = false;
    caps.hasFilters = false;
    caps.hasBayer = true;
    caps.canStream = true;
    caps.hasGain = true;
    caps.hasOffset = true;
    caps.hasTemperature = true;
    caps.bayerPattern = BayerPattern::RGGB;
    caps.canRecordVideo = true;
    caps.supportsSequences = true;
    caps.hasImageQualityAnalysis = true;
    caps.supportsCompression = false;
    caps.hasAdvancedControls = true;
    caps.supportsBurstMode = false;
    caps.supportedFormats = {ImageFormat::FITS, ImageFormat::TIFF, ImageFormat::PNG, ImageFormat::JPEG};
    caps.supportedVideoFormats = {"RAW8", "RAW16", "RGB24", "MONO8", "MONO16"};
    
    setCameraCapabilities(caps);
    
    LOG_F(INFO, "Created ASI Camera: {}", name);
}

ASICamera::~ASICamera() {
    if (controller_) {
        controller_->destroy();
    }
    LOG_F(INFO, "Destroyed ASI Camera");
}

// Basic device interface
auto ASICamera::initialize() -> bool {
    return controller_->initialize();
}

auto ASICamera::destroy() -> bool {
    return controller_->destroy();
}

auto ASICamera::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    return controller_->connect(deviceName, timeout, maxRetry);
}

auto ASICamera::disconnect() -> bool {
    return controller_->disconnect();
}

auto ASICamera::isConnected() const -> bool {
    return controller_->isConnected();
}

auto ASICamera::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;
    controller_->scan(devices);
    return devices;
}

// Exposure control
auto ASICamera::startExposure(double duration) -> bool {
    return controller_->startExposure(duration);
}

auto ASICamera::abortExposure() -> bool {
    return controller_->abortExposure();
}

auto ASICamera::isExposing() const -> bool {
    return controller_->isExposing();
}

auto ASICamera::getExposureProgress() const -> double {
    return controller_->getExposureProgress();
}

auto ASICamera::getExposureRemaining() const -> double {
    return controller_->getExposureRemaining();
}

auto ASICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    return controller_->getExposureResult();
}

auto ASICamera::saveImage(const std::string& path) -> bool {
    return controller_->saveImage(path);
}

// Exposure history and statistics
auto ASICamera::getLastExposureDuration() const -> double {
    return controller_->getLastExposureDuration();
}

auto ASICamera::getExposureCount() const -> uint32_t {
    return controller_->getExposureCount();
}

auto ASICamera::resetExposureCount() -> bool {
    return controller_->resetExposureCount();
}

// Video streaming
auto ASICamera::startVideo() -> bool {
    return controller_->startVideo();
}

auto ASICamera::stopVideo() -> bool {
    return controller_->stopVideo();
}

auto ASICamera::isVideoRunning() const -> bool {
    return controller_->isVideoRunning();
}

auto ASICamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    return controller_->getVideoFrame();
}

auto ASICamera::setVideoFormat(const std::string& format) -> bool {
    return controller_->setVideoFormat(format);
}

auto ASICamera::getVideoFormats() -> std::vector<std::string> {
    return controller_->getVideoFormats();
}

// Advanced video features
auto ASICamera::startVideoRecording(const std::string& filename) -> bool {
    return controller_->startVideoRecording(filename);
}

auto ASICamera::stopVideoRecording() -> bool {
    return controller_->stopVideoRecording();
}

auto ASICamera::isVideoRecording() const -> bool {
    return controller_->isVideoRecording();
}

auto ASICamera::setVideoExposure(double exposure) -> bool {
    return controller_->setVideoExposure(exposure);
}

auto ASICamera::getVideoExposure() const -> double {
    return controller_->getVideoExposure();
}

auto ASICamera::setVideoGain(int gain) -> bool {
    return controller_->setVideoGain(gain);
}

auto ASICamera::getVideoGain() const -> int {
    return controller_->getVideoGain();
}

// Temperature control
auto ASICamera::startCooling(double targetTemp) -> bool {
    return controller_->startCooling(targetTemp);
}

auto ASICamera::stopCooling() -> bool {
    return controller_->stopCooling();
}

auto ASICamera::isCoolerOn() const -> bool {
    return controller_->isCoolerOn();
}

auto ASICamera::getTemperature() const -> std::optional<double> {
    return controller_->getTemperature();
}

auto ASICamera::getTemperatureInfo() const -> TemperatureInfo {
    return controller_->getTemperatureInfo();
}

auto ASICamera::getCoolingPower() const -> std::optional<double> {
    return controller_->getCoolingPower();
}

auto ASICamera::hasCooler() const -> bool {
    return controller_->hasCooler();
}

// Camera properties
auto ASICamera::setGain(int gain) -> bool {
    return controller_->setGain(gain);
}

auto ASICamera::getGain() -> std::optional<int> {
    return controller_->getGain();
}

auto ASICamera::getGainRange() -> std::pair<int, int> {
    return controller_->getGainRange();
}

auto ASICamera::setOffset(int offset) -> bool {
    return controller_->setOffset(offset);
}

auto ASICamera::getOffset() -> std::optional<int> {
    return controller_->getOffset();
}

auto ASICamera::getOffsetRange() -> std::pair<int, int> {
    return controller_->getOffsetRange();
}

auto ASICamera::setISO(int iso) -> bool {
    return controller_->setISO(iso);
}

auto ASICamera::getISO() -> std::optional<int> {
    return controller_->getISO();
}

auto ASICamera::getISOList() -> std::vector<int> {
    return controller_->getISOValues();
}

// Additional methods for ASI-specific functionality
auto ASICamera::getBayerPattern() const -> BayerPattern {
    // Return the bayer pattern from controller
    return BayerPattern::RGGB; // Placeholder
}

auto ASICamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    // Return current resolution
    AtomCameraFrame::Resolution res;
    res.width = controller_->getMaxWidth();
    res.height = controller_->getMaxHeight();
    res.maxWidth = controller_->getMaxWidth();
    res.maxHeight = controller_->getMaxHeight();
    return res;
}

auto ASICamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    res.width = controller_->getMaxWidth();
    res.height = controller_->getMaxHeight();
    res.maxWidth = controller_->getMaxWidth();
    res.maxHeight = controller_->getMaxHeight();
    return res;
}

auto ASICamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    auto binning = controller_->getBinning();
    AtomCameraFrame::Binning bin;
    bin.horizontal = binning.binX;
    bin.vertical = binning.binY;
    return bin;
}

auto ASICamera::setBinning(int horizontal, int vertical) -> bool {
    return controller_->setBinning(horizontal, vertical);
}

// Auto white balance
auto ASICamera::enableAutoWhiteBalance(bool enable) -> bool {
    return controller_->setAutoWhiteBalance(enable);
}

auto ASICamera::isAutoWhiteBalanceEnabled() const -> bool {
    return controller_->isAutoWhiteBalanceEnabled();
}

// ASI EAF (Electronic Auto Focuser) control - Placeholder implementations
auto ASICamera::hasEAFFocuser() -> bool {
    LOG_F(INFO, "EAF focuser check");
    return false; // Placeholder - would check for connected EAF
}

auto ASICamera::connectEAFFocuser() -> bool {
    LOG_F(INFO, "Connecting EAF focuser");
    return false; // Placeholder implementation
}

auto ASICamera::disconnectEAFFocuser() -> bool {
    LOG_F(INFO, "Disconnecting EAF focuser");
    return false; // Placeholder implementation
}

auto ASICamera::isEAFFocuserConnected() -> bool {
    return false; // Placeholder implementation
}

auto ASICamera::setEAFFocuserPosition(int position) -> bool {
    LOG_F(INFO, "Setting EAF focuser position to: {}", position);
    return false; // Placeholder implementation
}

auto ASICamera::getEAFFocuserPosition() -> int {
    return 0; // Placeholder implementation
}

auto ASICamera::getEAFFocuserMaxPosition() -> int {
    return 31000; // Placeholder implementation
}

auto ASICamera::isEAFFocuserMoving() -> bool {
    return false; // Placeholder implementation
}

auto ASICamera::stopEAFFocuser() -> bool {
    LOG_F(INFO, "Stopping EAF focuser");
    return false; // Placeholder implementation
}

auto ASICamera::setEAFFocuserStepSize(int stepSize) -> bool {
    LOG_F(INFO, "Setting EAF focuser step size to: {}", stepSize);
    return false; // Placeholder implementation
}

auto ASICamera::getEAFFocuserStepSize() -> int {
    return 1; // Placeholder implementation
}

auto ASICamera::homeEAFFocuser() -> bool {
    LOG_F(INFO, "Homing EAF focuser");
    return false; // Placeholder implementation
}

auto ASICamera::calibrateEAFFocuser() -> bool {
    LOG_F(INFO, "Calibrating EAF focuser");
    return false; // Placeholder implementation
}

auto ASICamera::getEAFFocuserTemperature() -> double {
    return 25.0; // Placeholder implementation
}

auto ASICamera::enableEAFFocuserBacklashCompensation(bool enable) -> bool {
    LOG_F(INFO, "EAF focuser backlash compensation: {}", enable ? "enabled" : "disabled");
    return false; // Placeholder implementation
}

auto ASICamera::setEAFFocuserBacklashSteps(int steps) -> bool {
    LOG_F(INFO, "Setting EAF focuser backlash steps to: {}", steps);
    return false; // Placeholder implementation
}

// ASI EFW (Electronic Filter Wheel) control - Placeholder implementations
auto ASICamera::hasEFWFilterWheel() -> bool {
    LOG_F(INFO, "EFW filter wheel check");
    return false; // Placeholder implementation
}

auto ASICamera::connectEFWFilterWheel() -> bool {
    LOG_F(INFO, "Connecting EFW filter wheel");
    return false; // Placeholder implementation
}

auto ASICamera::disconnectEFWFilterWheel() -> bool {
    LOG_F(INFO, "Disconnecting EFW filter wheel");
    return false; // Placeholder implementation
}

auto ASICamera::isEFWFilterWheelConnected() -> bool {
    return false; // Placeholder implementation
}

auto ASICamera::setEFWFilterPosition(int position) -> bool {
    LOG_F(INFO, "Setting EFW filter position to: {}", position);
    return false; // Placeholder implementation
}

auto ASICamera::getEFWFilterPosition() -> int {
    return 1; // Placeholder implementation
}

auto ASICamera::getEFWFilterCount() -> int {
    return 8; // Placeholder implementation
}

auto ASICamera::isEFWFilterWheelMoving() -> bool {
    return false; // Placeholder implementation
}

auto ASICamera::homeEFWFilterWheel() -> bool {
    LOG_F(INFO, "Homing EFW filter wheel");
    return false; // Placeholder implementation
}

auto ASICamera::getEFWFilterWheelFirmware() -> std::string {
    return "EFW Simulator v1.0"; // Placeholder implementation
}

auto ASICamera::setEFWFilterNames(const std::vector<std::string>& names) -> bool {
    LOG_F(INFO, "Setting EFW filter names: {} filters", names.size());
    return false; // Placeholder implementation
}

auto ASICamera::getEFWFilterNames() -> std::vector<std::string> {
    return {"Red", "Green", "Blue", "Luminance", "H-Alpha", "OIII", "SII", "Clear"}; // Placeholder
}

auto ASICamera::getEFWUnidirectionalMode() -> bool {
    return false; // Placeholder implementation
}

auto ASICamera::setEFWUnidirectionalMode(bool enable) -> bool {
    LOG_F(INFO, "EFW unidirectional mode: {}", enable ? "enabled" : "disabled");
    return false; // Placeholder implementation
}

auto ASICamera::calibrateEFWFilterWheel() -> bool {
    LOG_F(INFO, "Calibrating EFW filter wheel");
    return false; // Placeholder implementation
}

} // namespace lithium::device::asi::camera
