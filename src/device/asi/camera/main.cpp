/*
 * main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera dedicated module implementation

*************************************************/

#include "main.hpp"
#include "controller.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::device::asi::camera {

ASICamera::ASICamera(const std::string& name)
    : AtomCamera(name), m_device_name(name) {
    LOG_F(INFO, "Creating ASI Camera: %s", name.c_str());
    m_controller = std::make_unique<ASICameraController>();
}

ASICamera::~ASICamera() {
    LOG_F(INFO, "Destroying ASI Camera: %s", m_device_name.c_str());
    if (m_controller) {
        m_controller->shutdown();
    }
}

// =========================================================================
// Basic Device Interface
// =========================================================================

auto ASICamera::initialize() -> bool {
    std::lock_guard<std::mutex> lock(m_state_mutex);

    LOG_F(INFO, "Initializing ASI Camera: %s", m_device_name.c_str());

    if (!m_controller) {
        LOG_F(ERROR, "Controller not available");
        return false;
    }

    if (!m_controller->initialize()) {
        LOG_F(ERROR, "Failed to initialize camera controller");
        return false;
    }

    initializeDefaultSettings();
    setupCallbacks();

    LOG_F(INFO, "ASI Camera initialized successfully: %s", m_device_name.c_str());
    return true;
}

auto ASICamera::destroy() -> bool {
    std::lock_guard<std::mutex> lock(m_state_mutex);

    LOG_F(INFO, "Destroying ASI Camera: %s", m_device_name.c_str());

    if (m_controller) {
        m_controller->shutdown();
    }

    LOG_F(INFO, "ASI Camera destroyed successfully: %s", m_device_name.c_str());
    return true;
}

auto ASICamera::connect(const std::string &port, int timeout, int maxRetry) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Connecting ASI Camera: %s", m_device_name.c_str());

    // For now, try to connect to the first available camera
    // In the future, this could be made configurable
    return connectToCamera(0);
}

auto ASICamera::disconnect() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Disconnecting ASI Camera: %s", m_device_name.c_str());
    return m_controller->disconnectFromCamera();
}

auto ASICamera::isConnected() const -> bool {
    return m_controller && m_controller->isConnected();
}

auto ASICamera::scan() -> std::vector<std::string> {
    return getAvailableCameras();
}

// =========================================================================
// Camera Interface Implementation
// =========================================================================

auto ASICamera::startExposure(double duration) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Starting exposure: %.2f seconds", duration);
    return m_controller->startExposure(duration * 1000.0); // Convert to milliseconds
}

auto ASICamera::abortExposure() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Aborting exposure");
    return m_controller->stopExposure();
}

auto ASICamera::isExposing() const -> bool {
    return m_controller && m_controller->isExposing();
}

auto ASICamera::getExposureProgress() const -> double {
    if (!m_controller) {
        return 0.0;
    }
    return m_controller->getExposureProgress();
}

auto ASICamera::getExposureRemaining() const -> double {
    if (!m_controller) {
        return 0.0;
    }
    return m_controller->getRemainingExposureTime();
}

auto ASICamera::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    if (!validateConnection()) {
        return nullptr;
    }

    if (!m_controller->isImageReady()) {
        LOG_F(WARNING, "No image ready for download");
        return nullptr;
    }

    auto image_data = m_controller->downloadImage();
    if (image_data.empty()) {
        LOG_F(ERROR, "Failed to download image data");
        return nullptr;
    }

    // Create camera frame from image data
    // This would need to be implemented based on AtomCameraFrame interface
    // For now, return nullptr as placeholder
    LOG_F(INFO, "Image downloaded successfully, size: %zu bytes", image_data.size());
    return nullptr; // TODO: Implement AtomCameraFrame creation
}

auto ASICamera::saveImage(const std::string &path) -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Saving image to: %s", path.c_str());
    return m_controller->saveImage(path);
}

// Exposure statistics
auto ASICamera::getLastExposureDuration() const -> double {
    // TODO: Implement exposure duration tracking
    return m_last_exposure_duration;
}

auto ASICamera::getExposureCount() const -> uint32_t {
    // TODO: Implement exposure count tracking
    return m_exposure_count;
}

auto ASICamera::resetExposureCount() -> bool {
    m_exposure_count = 0;
    return true;
}

// =========================================================================
// Temperature Control
// =========================================================================

auto ASICamera::setTemperature(double temp) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting target temperature: %.1f°C", temp);
    return m_controller->setTargetTemperature(temp);
}

auto ASICamera::getTemperature() const -> std::optional<double> {
    if (!m_controller) {
        return std::nullopt;
    }
    return m_controller->getCurrentTemperature();
}

// Remove setCooling method - not in base class

// Remove isCoolingEnabled method - not in base class

// =========================================================================
// Video/Streaming
// =========================================================================

auto ASICamera::startVideo() -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Starting video mode");
    return m_controller->startVideo();
}

auto ASICamera::stopVideo() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Stopping video mode");
    return m_controller->stopVideo();
}

auto ASICamera::isVideoRunning() const -> bool {
    return m_controller && m_controller->isVideoActive();
}

auto ASICamera::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    // TODO: Implement video frame capture
    return nullptr;
}

// =========================================================================
// Image Settings
// =========================================================================

auto ASICamera::setBinning(int binx, int biny) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting binning: %dx%d", binx, biny);
    return m_controller->setProperty("binning", std::to_string(binx) + "x" + std::to_string(biny));
}

auto ASICamera::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!m_controller) {
        return std::nullopt;
    }

    auto binning_str = m_controller->getProperty("binning");
    // Parse binning string like "2x2" - simplified implementation
    AtomCameraFrame::Binning binning{1, 1}; // TODO: Implement proper parsing
    return binning;
}

auto ASICamera::setImageFormat(const std::string& format) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting image format: %s", format.c_str());
    return m_controller->setProperty("format", format);
}

auto ASICamera::getImageFormat() const -> std::string {
    if (!m_controller) {
        return "RAW16";
    }
    return m_controller->getProperty("format");
}

auto ASICamera::setFrameType(FrameType type) -> bool {
    if (!validateConnection()) {
        return false;
    }

    std::string type_str;
    switch (type) {
        case FrameType::FITS: type_str = "FITS"; break;
        case FrameType::NATIVE: type_str = "NATIVE"; break;
        case FrameType::XISF: type_str = "XISF"; break;
        case FrameType::JPG: type_str = "JPG"; break;
        case FrameType::PNG: type_str = "PNG"; break;
        case FrameType::TIFF: type_str = "TIFF"; break;
        default: type_str = "FITS"; break;
    }

    LOG_F(INFO, "Setting frame type: %s", type_str.c_str());
    return m_controller->setProperty("frame_type", type_str);
}

auto ASICamera::getFrameType() -> FrameType {
    if (!m_controller) {
        return FrameType::FITS;
    }

    auto type_str = m_controller->getProperty("frame_type");
    if (type_str == "NATIVE") return FrameType::NATIVE;
    if (type_str == "XISF") return FrameType::XISF;
    if (type_str == "JPG") return FrameType::JPG;
    if (type_str == "PNG") return FrameType::PNG;
    if (type_str == "TIFF") return FrameType::TIFF;
    return FrameType::FITS;
}

// =========================================================================
// Gain and Offset - Remove incorrect methods, rely on base class interface
// =========================================================================

// Remove the duplicate/invalid setGain, getGain, setOffset, getOffset methods
// The correct ones are implemented later in the file

// =========================================================================
// ASI-Specific Features
// =========================================================================

auto ASICamera::getAvailableCameras() -> std::vector<std::string> {
    // TODO: Implement ASI SDK camera enumeration
    return {"ASI Camera (Simulated)"};
}

auto ASICamera::connectToCamera(int camera_id) -> bool {
    if (!m_controller) {
        LOG_F(ERROR, "Controller not available");
        return false;
    }

    LOG_F(INFO, "Connecting to camera ID: %d", camera_id);
    return m_controller->connectToCamera(camera_id);
}

auto ASICamera::getCameraInfo() const -> std::string {
    if (!m_controller) {
        return "Controller not available";
    }
    return m_controller->getCameraInfo();
}

auto ASICamera::setUSBTraffic(int bandwidth) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting USB traffic: %d", bandwidth);
    return m_controller->setProperty("usb_traffic", std::to_string(bandwidth));
}

auto ASICamera::getUSBTraffic() const -> int {
    if (!m_controller) {
        return 40; // Default value
    }
    return std::stoi(m_controller->getProperty("usb_traffic"));
}

auto ASICamera::setHardwareBinning(bool enable) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "%s hardware binning", enable ? "Enabling" : "Disabling");
    return m_controller->setProperty("hardware_binning", enable ? "true" : "false");
}

auto ASICamera::isHardwareBinningEnabled() const -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->getProperty("hardware_binning") == "true";
}

auto ASICamera::setHighSpeedMode(bool enable) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "%s high speed mode", enable ? "Enabling" : "Disabling");
    return m_controller->setProperty("high_speed", enable ? "true" : "false");
}

auto ASICamera::isHighSpeedModeEnabled() const -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->getProperty("high_speed") == "true";
}

auto ASICamera::setFlip(bool horizontal, bool vertical) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting flip: H=%s, V=%s", horizontal ? "true" : "false", vertical ? "true" : "false");

    bool success = true;
    success &= m_controller->setProperty("flip_horizontal", horizontal ? "true" : "false");
    success &= m_controller->setProperty("flip_vertical", vertical ? "true" : "false");

    return success;
}

auto ASICamera::getFlip() const -> std::pair<bool, bool> {
    if (!m_controller) {
        return {false, false};
    }

    bool horizontal = m_controller->getProperty("flip_horizontal") == "true";
    bool vertical = m_controller->getProperty("flip_vertical") == "true";

    return {horizontal, vertical};
}

auto ASICamera::setWhiteBalance(double red_gain, double green_gain, double blue_gain) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting white balance: R=%.2f, G=%.2f, B=%.2f", red_gain, green_gain, blue_gain);

    bool success = true;
    success &= m_controller->setProperty("wb_red", std::to_string(red_gain));
    success &= m_controller->setProperty("wb_green", std::to_string(green_gain));
    success &= m_controller->setProperty("wb_blue", std::to_string(blue_gain));

    return success;
}

auto ASICamera::getWhiteBalance() const -> std::tuple<double, double, double> {
    if (!m_controller) {
        return {1.0, 1.0, 1.0};
    }

    double red = std::stod(m_controller->getProperty("wb_red"));
    double green = std::stod(m_controller->getProperty("wb_green"));
    double blue = std::stod(m_controller->getProperty("wb_blue"));

    return {red, green, blue};
}

auto ASICamera::setAutoWhiteBalance(bool enable) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "%s auto white balance", enable ? "Enabling" : "Disabling");
    return m_controller->setProperty("auto_wb", enable ? "true" : "false");
}

auto ASICamera::isAutoWhiteBalanceEnabled() const -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->getProperty("auto_wb") == "true";
}

// =========================================================================
// Sequence and Automation
// =========================================================================

auto ASICamera::startSequence(const std::string& sequence_config) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Starting imaging sequence");
    return m_controller->startSequence(sequence_config);
}

auto ASICamera::stopSequence() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Stopping imaging sequence");
    return m_controller->stopSequence();
}

auto ASICamera::isSequenceActive() const -> bool {
    return m_controller && m_controller->isSequenceActive();
}

auto ASICamera::getSequenceProgress() const -> std::pair<int, int> {
    if (!m_controller) {
        return {0, 0};
    }
    // Parse progress from controller - simplified implementation
    // TODO: Implement proper parsing of sequence progress
    return {0, 0};
}

auto ASICamera::pauseSequence() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Pausing imaging sequence");
    return m_controller->setProperty("sequence_pause", "true");
}

auto ASICamera::resumeSequence() -> bool {
    if (!m_controller) {
        return false;
    }

    LOG_F(INFO, "Resuming imaging sequence");
    return m_controller->setProperty("sequence_pause", "false");
}

// =========================================================================
// Advanced Image Processing
// =========================================================================

auto ASICamera::setDarkFrameSubtraction(bool enable) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "%s dark frame subtraction", enable ? "Enabling" : "Disabling");
    return m_controller->setProperty("dark_subtract", enable ? "true" : "false");
}

auto ASICamera::isDarkFrameSubtractionEnabled() const -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->getProperty("dark_subtract") == "true";
}

auto ASICamera::setFlatFieldCorrection(const std::string& flat_frame_path) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Setting flat field frame: %s", flat_frame_path.c_str());
    return m_controller->setProperty("flat_frame_path", flat_frame_path);
}

auto ASICamera::setFlatFieldCorrectionEnabled(bool enable) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "%s flat field correction", enable ? "Enabling" : "Disabling");
    return m_controller->setProperty("flat_correct", enable ? "true" : "false");
}

auto ASICamera::isFlatFieldCorrectionEnabled() const -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->getProperty("flat_correct") == "true";
}

// =========================================================================
// Callback Management
// =========================================================================

void ASICamera::setExposureCallback(std::function<void(bool)> callback) {
    if (m_controller) {
        m_controller->setExposureCallback(std::move(callback));
    }
}

void ASICamera::setTemperatureCallback(std::function<void(double)> callback) {
    if (m_controller) {
        m_controller->setTemperatureCallback(std::move(callback));
    }
}

void ASICamera::setImageReadyCallback(std::function<void()> callback) {
    // TODO: Implement image ready callback through controller
}

void ASICamera::setErrorCallback(std::function<void(const std::string&)> callback) {
    if (m_controller) {
        m_controller->setErrorCallback(std::move(callback));
    }
}

// =========================================================================
// Status and Diagnostics
// =========================================================================

auto ASICamera::getDetailedStatus() const -> std::string {
    if (!m_controller) {
        return R"({"status": "controller_not_available"})";
    }

    // TODO: Return detailed JSON status
    return R"({"status": ")" + m_controller->getStatus() + R"("})";
}

auto ASICamera::getCameraStatistics() const -> std::string {
    // TODO: Implement camera statistics
    return "{}";
}

auto ASICamera::performSelfTest() -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Performing camera self-test");

    // TODO: Implement comprehensive self-test
    return true;
}

auto ASICamera::resetToDefaults() -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Resetting camera to default settings");
    return m_controller->setProperty("reset_defaults", "true");
}

auto ASICamera::saveConfiguration(const std::string& config_name) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Saving configuration: %s", config_name.c_str());
    return m_controller->setProperty("save_config", config_name);
}

auto ASICamera::loadConfiguration(const std::string& config_name) -> bool {
    if (!validateConnection()) {
        return false;
    }

    LOG_F(INFO, "Loading configuration: %s", config_name.c_str());
    return m_controller->setProperty("load_config", config_name);
}

// =========================================================================
// Missing Interface Methods Implementation
// =========================================================================

// Color information
auto ASICamera::isColor() const -> bool {
    return m_controller && m_controller->isColorCamera();
}

auto ASICamera::getBayerPattern() const -> BayerPattern {
    if (!m_controller) {
        return BayerPattern::MONO;
    }
    // TODO: Get actual bayer pattern from controller
    return BayerPattern::RGGB; // Default
}

auto ASICamera::setBayerPattern(BayerPattern pattern) -> bool {
    // TODO: Implement bayer pattern setting
    LOG_F(INFO, "Setting bayer pattern");
    return true;
}

// Parameter control with corrected signatures
auto ASICamera::setGain(int gain) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setGain(gain);
}

auto ASICamera::getGain() -> std::optional<int> {
    if (!m_controller) {
        return std::nullopt;
    }
    return m_controller->getGain();
}

auto ASICamera::getGainRange() -> std::pair<int, int> {
    if (!m_controller) {
        return {0, 0};
    }
    return m_controller->getGainRange();
}

auto ASICamera::setOffset(int offset) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setOffset(offset);
}

auto ASICamera::getOffset() -> std::optional<int> {
    if (!m_controller) {
        return std::nullopt;
    }
    return m_controller->getOffset();
}

auto ASICamera::getOffsetRange() -> std::pair<int, int> {
    if (!m_controller) {
        return {0, 0};
    }
    return m_controller->getOffsetRange();
}

auto ASICamera::setISO(int iso) -> bool {
    // TODO: Implement ISO setting
    LOG_F(INFO, "Setting ISO: %d", iso);
    return true;
}

auto ASICamera::getISO() -> std::optional<int> {
    // TODO: Implement ISO getting
    return std::nullopt;
}

auto ASICamera::getISOList() -> std::vector<int> {
    // TODO: Implement ISO list
    return {};
}

// Frame settings with corrected signatures
auto ASICamera::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!m_controller) {
        return std::nullopt;
    }
    // TODO: Get actual resolution from controller
    AtomCameraFrame::Resolution res;
    res.width = 1920;
    res.height = 1080;
    return res;
}

auto ASICamera::setResolution(int x, int y, int width, int height) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setROI(x, y, width, height);
}

auto ASICamera::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    if (m_controller) {
        // TODO: Get max resolution from controller
        res.width = 4096;
        res.height = 4096;
    }
    return res;
}

auto ASICamera::getMaxBinning() -> AtomCameraFrame::Binning {
    AtomCameraFrame::Binning bin;
    bin.horizontal = 4;
    bin.vertical = 4;
    return bin;
}

// Removed duplicate setFrameType and getFrameType - already defined earlier

auto ASICamera::setUploadMode(UploadMode mode) -> bool {
    // TODO: Implement upload mode
    return true;
}

auto ASICamera::getUploadMode() -> UploadMode {
    // TODO: Return actual upload mode
    return static_cast<UploadMode>(0); // Default
}

auto ASICamera::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    // TODO: Return frame info
    return nullptr;
}

// Pixel information
auto ASICamera::getPixelSize() -> double {
    if (!m_controller) {
        return 0.0;
    }
    return m_controller->getPixelSize();
}

auto ASICamera::getPixelSizeX() -> double {
    return getPixelSize();
}

auto ASICamera::getPixelSizeY() -> double {
    return getPixelSize();
}

auto ASICamera::getBitDepth() -> int {
    if (!m_controller) {
        return 16;
    }
    return m_controller->getBitDepth();
}

// Shutter control
auto ASICamera::hasShutter() -> bool {
    return m_controller && m_controller->hasShutter();
}

auto ASICamera::setShutter(bool open) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setProperty("shutter", open ? "open" : "closed");
}

auto ASICamera::getShutterStatus() -> bool {
    if (!m_controller) {
        return false;
    }
    auto status = m_controller->getProperty("shutter");
    return status == "open";
}

// Fan control
auto ASICamera::hasFan() -> bool {
    return false; // ASI cameras typically don't have controllable fans
}

auto ASICamera::setFanSpeed(int speed) -> bool {
    // TODO: Implement fan control if supported
    return false;
}

auto ASICamera::getFanSpeed() -> int {
    return 0;
}

// Advanced video features
auto ASICamera::startVideoRecording(const std::string& filename) -> bool {
    if (!m_controller) {
        return false;
    }
    LOG_F(INFO, "Starting video recording: %s", filename.c_str());
    return m_controller->startVideoRecording(filename);
}

auto ASICamera::stopVideoRecording() -> bool {
    if (!m_controller) {
        return false;
    }
    LOG_F(INFO, "Stopping video recording");
    return m_controller->stopVideoRecording();
}

auto ASICamera::isVideoRecording() const -> bool {
    return m_controller && m_controller->isVideoRecording();
}

auto ASICamera::setVideoExposure(double exposure) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setVideoExposure(exposure);
}

auto ASICamera::getVideoExposure() const -> double {
    if (!m_controller) {
        return 0.0;
    }
    return m_controller->getVideoExposure();
}

auto ASICamera::setVideoGain(int gain) -> bool {
    if (!m_controller) {
        return false;
    }
    return m_controller->setVideoGain(gain);
}

auto ASICamera::getVideoGain() const -> int {
    if (!m_controller) {
        return 0;
    }
    return m_controller->getVideoGain();
}

// Image sequence capabilities
auto ASICamera::startSequence(int count, double exposure, double interval) -> bool {
    if (!m_controller) {
        return false;
    }
    LOG_F(INFO, "Starting sequence: %d frames, %.2fs exposure, %.2fs interval", count, exposure, interval);
    // Convert parameters to JSON string for controller
    std::string config = "{\"count\":" + std::to_string(count) +
                        ",\"exposure\":" + std::to_string(exposure) +
                        ",\"interval\":" + std::to_string(interval) + "}";
    return m_controller->startSequence(config);
}

// Removed duplicate methods - these are already implemented earlier in the file

// Image quality and statistics
auto ASICamera::getFrameStatistics() const -> std::map<std::string, double> {
    std::map<std::string, double> stats;
    stats["mean"] = 0.0;
    stats["std"] = 0.0;
    stats["min"] = 0.0;
    stats["max"] = 0.0;
    return stats;
}

auto ASICamera::getTotalFramesReceived() const -> uint64_t {
    return m_exposure_count;
}

auto ASICamera::getDroppedFrames() const -> uint64_t {
    return 0;
}

auto ASICamera::getAverageFrameRate() const -> double {
    return 0.0;
}

auto ASICamera::getLastImageQuality() const -> std::map<std::string, double> {
    return getFrameStatistics();
}

// Video format methods
auto ASICamera::setVideoFormat(const std::string& format) -> bool {
    if (!m_controller) {
        return false;
    }
    LOG_F(INFO, "Setting video format: %s", format.c_str());
    return m_controller->setVideoFormat(format);
}

auto ASICamera::getVideoFormats() -> std::vector<std::string> {
    if (!m_controller) {
        return {};
    }
    return m_controller->getSupportedVideoFormats();
}

// =========================================================================
// Private Helper Methods
// =========================================================================

void ASICamera::initializeDefaultSettings() {
    // Set up default camera settings
    LOG_F(INFO, "Initializing default camera settings");

    // TODO: Set reasonable defaults for ASI cameras
}

auto ASICamera::validateConnection() const -> bool {
    if (!m_controller) {
        LOG_F(ERROR, "Controller not available");
        return false;
    }

    if (!m_controller->isInitialized()) {
        LOG_F(ERROR, "Controller not initialized");
        return false;
    }

    if (!m_controller->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }

    return true;
}

void ASICamera::setupCallbacks() {
    // Set up internal callbacks for monitoring
    LOG_F(INFO, "Setting up camera callbacks");

    // TODO: Set up internal monitoring callbacks
}

} // namespace lithium::device::asi::camera
