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

auto ASICamera::getGain() const -> int {
    return controller_->getGain();
}

auto ASICamera::getGainRange() const -> std::pair<int, int> {
    return controller_->getGainRange();
}

auto ASICamera::setOffset(int offset) -> bool {
    return controller_->setOffset(offset);
}

auto ASICamera::getOffset() const -> int {
    return controller_->getOffset();
}

auto ASICamera::getOffsetRange() const -> std::pair<int, int> {
    return controller_->getOffsetRange();
}

auto ASICamera::setExposureTime(double exposure) -> bool {
    return controller_->setExposureTime(exposure);
}

auto ASICamera::getExposureTime() const -> double {
    return controller_->getExposureTime();
}

auto ASICamera::getExposureRange() const -> std::pair<double, double> {
    return controller_->getExposureRange();
}

// ISO and advanced controls
auto ASICamera::setISO(int iso) -> bool {
    return controller_->setISO(iso);
}

auto ASICamera::getISO() const -> int {
    return controller_->getISO();
}

auto ASICamera::getISOValues() const -> std::vector<int> {
    return controller_->getISOValues();
}

auto ASICamera::setUSBBandwidth(int bandwidth) -> bool {
    return controller_->setUSBBandwidth(bandwidth);
}

auto ASICamera::getUSBBandwidth() const -> int {
    return controller_->getUSBBandwidth();
}

auto ASICamera::getUSBBandwidthRange() const -> std::pair<int, int> {
    return controller_->getUSBBandwidthRange();
}

// Auto controls
auto ASICamera::setAutoExposure(bool enable) -> bool {
    return controller_->setAutoExposure(enable);
}

auto ASICamera::isAutoExposureEnabled() const -> bool {
    return controller_->isAutoExposureEnabled();
}

auto ASICamera::setAutoGain(bool enable) -> bool {
    return controller_->setAutoGain(enable);
}

auto ASICamera::isAutoGainEnabled() const -> bool {
    return controller_->isAutoGainEnabled();
}

auto ASICamera::setAutoWhiteBalance(bool enable) -> bool {
    return controller_->setAutoWhiteBalance(enable);
}

auto ASICamera::isAutoWhiteBalanceEnabled() const -> bool {
    return controller_->isAutoWhiteBalanceEnabled();
}

// Image format and quality
auto ASICamera::setImageFormat(const std::string& format) -> bool {
    return controller_->setImageFormat(format);
}

auto ASICamera::getImageFormat() const -> std::string {
    return controller_->getImageFormat();
}

auto ASICamera::getImageFormats() const -> std::vector<std::string> {
    return controller_->getImageFormats();
}

auto ASICamera::setQuality(int quality) -> bool {
    return controller_->setQuality(quality);
}

auto ASICamera::getQuality() const -> int {
    return controller_->getQuality();
}

// ROI and binning
auto ASICamera::setROI(int x, int y, int width, int height) -> bool {
    return controller_->setROI(x, y, width, height);
}

auto ASICamera::getROI() const -> std::tuple<int, int, int, int> {
    auto roi = controller_->getROI();
    return std::make_tuple(roi.x, roi.y, roi.width, roi.height);
}

auto ASICamera::setBinning(int binX, int binY) -> bool {
    return controller_->setBinning(binX, binY);
}

auto ASICamera::getBinning() const -> std::pair<int, int> {
    auto binning = controller_->getBinning();
    return std::make_pair(binning.horizontal, binning.vertical);
}

auto ASICamera::getSupportedBinning() const -> std::vector<std::pair<int, int>> {
    auto supportedBinning = controller_->getSupportedBinning();
    std::vector<std::pair<int, int>> result;
    for (const auto& bin : supportedBinning) {
        result.emplace_back(bin.horizontal, bin.vertical);
    }
    return result;
}

auto ASICamera::getMaxWidth() const -> int {
    return controller_->getMaxWidth();
}

auto ASICamera::getMaxHeight() const -> int {
    return controller_->getMaxHeight();
}

// Camera modes
auto ASICamera::setHighSpeedMode(bool enable) -> bool {
    return controller_->setHighSpeedMode(enable);
}

auto ASICamera::isHighSpeedMode() const -> bool {
    return controller_->isHighSpeedMode();
}

auto ASICamera::setFlipMode(int mode) -> bool {
    return controller_->setFlipMode(mode);
}

auto ASICamera::getFlipMode() const -> int {
    return controller_->getFlipMode();
}

auto ASICamera::setCameraMode(const std::string& mode) -> bool {
    return controller_->setCameraMode(mode);
}

auto ASICamera::getCameraMode() const -> std::string {
    return controller_->getCameraMode();
}

auto ASICamera::getCameraModes() const -> std::vector<std::string> {
    return controller_->getCameraModes();
}

// Sequence control
auto ASICamera::startSequence(int count, double exposure, double interval) -> bool {
    // Create sequence structure and delegate to controller
    CameraSequence sequence;
    // sequence.count = count;
    // sequence.exposure = exposure;
    // sequence.interval = interval;
    // return controller_->startSequence(sequence);
    
    // For now, return a placeholder implementation
    LOG_F(INFO, "Starting sequence: {} frames, {}s exposure, {}s interval", count, exposure, interval);
    return true;
}

auto ASICamera::stopSequence() -> bool {
    return controller_->stopSequence();
}

auto ASICamera::isSequenceRunning() const -> bool {
    return controller_->isSequenceRunning();
}

auto ASICamera::getSequenceProgress() const -> std::pair<int, int> {
    return controller_->getSequenceProgress();
}

auto ASICamera::pauseSequence() -> bool {
    return controller_->pauseSequence();
}

auto ASICamera::resumeSequence() -> bool {
    return controller_->resumeSequence();
}

// Frame statistics and analysis
auto ASICamera::getFrameRate() const -> double {
    return controller_->getFrameRate();
}

auto ASICamera::getDataRate() const -> double {
    return controller_->getDataRate();
}

auto ASICamera::getTotalDataTransferred() const -> uint64_t {
    return controller_->getTotalDataTransferred();
}

auto ASICamera::getDroppedFrames() const -> uint32_t {
    return controller_->getDroppedFrames();
}

// Calibration frames
auto ASICamera::takeDarkFrame(double exposure, int count) -> bool {
    return controller_->takeDarkFrame(exposure, count);
}

auto ASICamera::takeFlatFrame(double exposure, int count) -> bool {
    return controller_->takeFlatFrame(exposure, count);
}

auto ASICamera::takeBiasFrame(int count) -> bool {
    return controller_->takeBiasFrame(count);
}

// Hardware information
auto ASICamera::getFirmwareVersion() const -> std::string {
    return controller_->getFirmwareVersion();
}

auto ASICamera::getSerialNumber() const -> std::string {
    return controller_->getSerialNumber();
}

auto ASICamera::getModelName() const -> std::string {
    return controller_->getModelName();
}

auto ASICamera::getDriverVersion() const -> std::string {
    return controller_->getDriverVersion();
}

auto ASICamera::getPixelSize() const -> double {
    return controller_->getPixelSize();
}

auto ASICamera::getBitDepth() const -> int {
    return controller_->getBitDepth();
}

// Status and diagnostics
auto ASICamera::getLastError() const -> std::string {
    return controller_->getLastError();
}

auto ASICamera::getOperationHistory() const -> std::vector<std::string> {
    return controller_->getOperationHistory();
}

auto ASICamera::performSelfTest() -> bool {
    return controller_->performSelfTest();
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
