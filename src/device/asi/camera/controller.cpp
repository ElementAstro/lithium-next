/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASI Camera Controller V2 Implementation

*************************************************/

#include "controller.hpp"
#include <spdlog/spdlog.h>

namespace lithium::device::asi::camera {

// Helper functions for property name conversion
namespace {
    ASI_CONTROL_TYPE stringToControlType(const std::string& propertyName) {
        if (propertyName == "gain" || propertyName == "Gain") return ASI_GAIN;
        if (propertyName == "exposure" || propertyName == "Exposure") return ASI_EXPOSURE;
        if (propertyName == "gamma" || propertyName == "Gamma") return ASI_GAMMA;
        if (propertyName == "offset" || propertyName == "Offset") return ASI_OFFSET;
        if (propertyName == "wb_r" || propertyName == "WhiteBalanceR") return ASI_WB_R;
        if (propertyName == "wb_b" || propertyName == "WhiteBalanceB") return ASI_WB_B;
        if (propertyName == "bandwidth" || propertyName == "Bandwidth") return ASI_BANDWIDTHOVERLOAD;
        if (propertyName == "temperature" || propertyName == "Temperature") return ASI_TEMPERATURE;
        if (propertyName == "flip" || propertyName == "Flip") return ASI_FLIP;
        if (propertyName == "auto_max_gain" || propertyName == "AutoMaxGain") return ASI_AUTO_MAX_GAIN;
        if (propertyName == "auto_max_exp" || propertyName == "AutoMaxExp") return ASI_AUTO_MAX_EXP;
        if (propertyName == "auto_target_brightness" || propertyName == "AutoTargetBrightness") return ASI_AUTO_TARGET_BRIGHTNESS;
        if (propertyName == "hardware_bin" || propertyName == "HardwareBin") return ASI_HARDWARE_BIN;
        if (propertyName == "high_speed_mode" || propertyName == "HighSpeedMode") return ASI_HIGH_SPEED_MODE;
        if (propertyName == "cooler_on" || propertyName == "CoolerOn") return ASI_COOLER_ON;
        if (propertyName == "mono_bin" || propertyName == "MonoBin") return ASI_MONO_BIN;
        if (propertyName == "fan_on" || propertyName == "FanOn") return ASI_FAN_ON;
        if (propertyName == "pattern_adjust" || propertyName == "PatternAdjust") return ASI_PATTERN_ADJUST;
        if (propertyName == "anti_dew_heater" || propertyName == "AntiDewHeater") return ASI_ANTI_DEW_HEATER;
        
        // Return a default value for unknown properties
        return ASI_GAIN; // or could return an invalid enum value
    }
    
    std::string controlTypeToString(ASI_CONTROL_TYPE controlType) {
        switch (controlType) {
            case ASI_GAIN: return "gain";
            case ASI_EXPOSURE: return "exposure";
            case ASI_GAMMA: return "gamma";
            case ASI_OFFSET: return "offset";
            case ASI_WB_R: return "wb_r";
            case ASI_WB_B: return "wb_b";
            case ASI_BANDWIDTHOVERLOAD: return "bandwidth";
            case ASI_TEMPERATURE: return "temperature";
            case ASI_FLIP: return "flip";
            case ASI_AUTO_MAX_GAIN: return "auto_max_gain";
            case ASI_AUTO_MAX_EXP: return "auto_max_exp";
            case ASI_AUTO_TARGET_BRIGHTNESS: return "auto_target_brightness";
            case ASI_HARDWARE_BIN: return "hardware_bin";
            case ASI_HIGH_SPEED_MODE: return "high_speed_mode";
            case ASI_COOLER_ON: return "cooler_on";
            case ASI_MONO_BIN: return "mono_bin";
            case ASI_FAN_ON: return "fan_on";
            case ASI_PATTERN_ADJUST: return "pattern_adjust";
            case ASI_ANTI_DEW_HEATER: return "anti_dew_heater";
            default: return "unknown";
        }
    }
} // anonymous namespace

ASICameraController::ASICameraController() = default;

ASICameraController::~ASICameraController() {
    shutdown();
}

// =========================================================================
// Initialization and Device Management
// =========================================================================

auto ASICameraController::initialize() -> bool {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    if (m_initialized) {
        LOG_F(WARNING, "Camera controller already initialized");
        return true;
    }

    LOG_F(INFO, "Initializing ASI Camera Controller V2");

    try {
        if (!initializeComponents()) {
            setLastError("Failed to initialize components");
            return false;
        }

        m_initialized = true;
        LOG_F(INFO, "ASI Camera Controller V2 initialized successfully");
        return true;
    } catch (const std::exception& e) {
        const std::string error = "Exception during initialization: " + std::string(e.what());
        setLastError(error);
        LOG_F(ERROR, "%s", error.c_str());
        return false;
    }
}

auto ASICameraController::shutdown() -> bool {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    if (!m_initialized) {
        return true;
    }

    LOG_F(INFO, "Shutting down ASI Camera Controller V2");

    try {
        // Stop any active operations
        if (m_connected) {
            disconnectFromCamera();
        }

        shutdownComponents();
        m_initialized = false;
        
        LOG_F(INFO, "ASI Camera Controller V2 shut down successfully");
        return true;
    } catch (const std::exception& e) {
        const std::string error = "Exception during shutdown: " + std::string(e.what());
        setLastError(error);
        LOG_F(ERROR, "%s", error.c_str());
        return false;
    }
}

auto ASICameraController::isInitialized() const -> bool {
    return m_initialized;
}

auto ASICameraController::connectToCamera(int camera_id) -> bool {
    if (!m_initialized) {
        setLastError("Controller not initialized");
        return false;
    }

    if (!m_hardware) {
        setLastError("Hardware interface not available");
        return false;
    }

    LOG_F(INFO, "Connecting to camera ID: %d", camera_id);

    if (m_hardware->openCamera(camera_id)) {
        m_connected = true;
        LOG_F(INFO, "Successfully connected to camera ID: %d", camera_id);
        return true;
    } else {
        setLastError("Failed to connect to camera");
        return false;
    }
}

auto ASICameraController::disconnectFromCamera() -> bool {
    if (!m_connected) {
        return true;
    }

    LOG_F(INFO, "Disconnecting from camera");

    // Stop any active operations first
    if (isExposing()) {
        stopExposure();
    }
    if (isVideoActive()) {
        stopVideo();
    }
    if (isSequenceActive()) {
        stopSequence();
    }

    if (m_hardware && m_hardware->closeCamera()) {
        m_connected = false;
        LOG_F(INFO, "Successfully disconnected from camera");
        return true;
    } else {
        setLastError("Failed to disconnect from camera");
        return false;
    }
}

auto ASICameraController::isConnected() const -> bool {
    return m_connected;
}

// =========================================================================
// Camera Information and Status
// =========================================================================

auto ASICameraController::getCameraInfo() const -> std::string {
    if (!m_hardware) {
        return "Hardware interface not available";
    }
    auto info = m_hardware->getCameraInfo();
    if (info.has_value()) {
        return "Camera: " + info->name + " (ID: " + std::to_string(info->cameraId) + ")";
    }
    return "No camera information available";
}

auto ASICameraController::getStatus() const -> std::string {
    if (!m_initialized) {
        return "Not initialized";
    }
    if (!m_connected) {
        return "Not connected";
    }
    if (isExposing()) {
        return "Exposing";
    }
    if (isVideoActive()) {
        return "Video mode";
    }
    if (isSequenceActive()) {
        return "Sequence running";
    }
    return "Ready";
}

auto ASICameraController::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    return m_last_error;
}

// =========================================================================
// Exposure Control
// =========================================================================

auto ASICameraController::startExposure(double duration_ms, bool is_dark) -> bool {
    if (!m_exposure) {
        setLastError("Exposure manager not available");
        return false;
    }
    
    components::ExposureManager::ExposureSettings settings;
    settings.duration = duration_ms / 1000.0; // Convert ms to seconds
    settings.isDark = is_dark;
    settings.width = 0; // Full frame
    settings.height = 0; // Full frame
    settings.binning = 1;
    settings.format = "RAW16";
    
    return m_exposure->startExposure(settings);
}

auto ASICameraController::stopExposure() -> bool {
    if (!m_exposure) {
        return false;
    }
    return m_exposure->abortExposure();
}

auto ASICameraController::isExposing() const -> bool {
    if (!m_exposure) {
        return false;
    }
    return m_exposure->isExposing();
}

auto ASICameraController::getExposureProgress() const -> double {
    if (!m_exposure) {
        return 0.0;
    }
    return m_exposure->getProgress();
}

auto ASICameraController::getRemainingExposureTime() const -> double {
    if (!m_exposure) {
        return 0.0;
    }
    return m_exposure->getRemainingTime();
}

// =========================================================================
// Image Management
// =========================================================================

auto ASICameraController::isImageReady() const -> bool {
    if (!m_image_processor) {
        return false;
    }
    
    // For this simplified controller, assume that if the last exposure was successful,
    // an image is ready for processing. In a real implementation, this would check
    // the exposure manager's state and results.
    return m_exposure && m_exposure->hasResult();
}

auto ASICameraController::downloadImage() -> std::vector<uint8_t> {
    if (!m_exposure) {
        return {};
    }
    
    // Get the last exposure result and extract the frame data
    auto result = m_exposure->getLastResult();
    if (!result.success || !result.frame) {
        return {};
    }
    
    // Convert the frame data to a vector of bytes
    auto frame = result.frame;
    if (!frame->data || frame->size == 0) {
        return {};
    }
    
    const uint8_t* data = reinterpret_cast<const uint8_t*>(frame->data);
    return std::vector<uint8_t>(data, data + frame->size);
}

auto ASICameraController::saveImage(const std::string& filename, const std::string& format) -> bool {
    if (!m_image_processor || !m_exposure) {
        setLastError("Image processor or exposure manager not available");
        return false;
    }
    
    // Get the last exposure result
    auto result = m_exposure->getLastResult();
    if (!result.success || !result.frame) {
        setLastError("No image data available");
        return false;
    }
    
    // Use the image processor to save the frame in the desired format
    if (format == "FITS") {
        return m_image_processor->convertToFITS(result.frame, filename);
    } else if (format == "TIFF") {
        return m_image_processor->convertToTIFF(result.frame, filename);
    } else if (format == "JPEG") {
        return m_image_processor->convertToJPEG(result.frame, filename);
    } else if (format == "PNG") {
        return m_image_processor->convertToPNG(result.frame, filename);
    }
    
    setLastError("Unsupported image format: " + format);
    return false;
}

// =========================================================================
// Temperature Control
// =========================================================================

auto ASICameraController::setTargetTemperature(double target_temp) -> bool {
    if (!m_temperature) {
        setLastError("Temperature controller not available");
        return false;
    }
    return m_temperature->updateTargetTemperature(target_temp);
}

auto ASICameraController::getCurrentTemperature() const -> double {
    if (!m_temperature) {
        return 0.0;
    }
    return m_temperature->getCurrentTemperature();
}

auto ASICameraController::setCoolingEnabled(bool enable) -> bool {
    if (!m_temperature) {
        setLastError("Temperature controller not available");
        return false;
    }
    if (enable) {
        return m_temperature->startCooling(m_temperature->getTargetTemperature());
    } else {
        return m_temperature->stopCooling();
    }
}

auto ASICameraController::isCoolingEnabled() const -> bool {
    if (!m_temperature) {
        return false;
    }
    return m_temperature->isCoolerOn();
}

// =========================================================================
// Video/Live View
// =========================================================================

auto ASICameraController::startVideo() -> bool {
    if (!m_video) {
        setLastError("Video manager not available");
        return false;
    }
    
    // Create default video settings
    components::VideoManager::VideoSettings settings;
    settings.width = 0;  // Use full frame
    settings.height = 0; // Use full frame
    settings.fps = 30.0;
    settings.format = "RAW16";
    settings.exposure = 33000; // 33ms
    settings.gain = 0;
    
    return m_video->startVideo(settings);
}

auto ASICameraController::stopVideo() -> bool {
    if (!m_video) {
        return false;
    }
    return m_video->stopVideo();
}

auto ASICameraController::isVideoActive() const -> bool {
    if (!m_video) {
        return false;
    }
    return m_video->isStreaming();
}

// =========================================================================
// Sequence Management
// =========================================================================

auto ASICameraController::startSequence(const std::string& sequence_config) -> bool {
    if (!m_sequence) {
        setLastError("Sequence manager not available");
        return false;
    }
    
    // For simplicity, create a basic sequence from the config string
    // In a real implementation, this would parse the JSON config
    components::SequenceManager::SequenceSettings settings;
    settings.name = "SimpleSequence";
    settings.type = components::SequenceManager::SequenceType::SIMPLE;
    settings.outputDirectory = "/tmp/images";
    settings.saveImages = true;
    
    // Add a single exposure step (1 second, gain 0)
    components::SequenceManager::ExposureStep step;
    step.duration = 1.0;
    step.gain = 0;
    step.filename = "image_{counter}.fits";
    settings.steps.push_back(step);
    
    return m_sequence->startSequence(settings);
}

auto ASICameraController::stopSequence() -> bool {
    if (!m_sequence) {
        return false;
    }
    return m_sequence->stopSequence();
}

auto ASICameraController::isSequenceActive() const -> bool {
    if (!m_sequence) {
        return false;
    }
    return m_sequence->isRunning();
}

auto ASICameraController::getSequenceProgress() const -> std::string {
    if (!m_sequence) {
        return "Sequence manager not available";
    }
    
    auto progress = m_sequence->getProgress();
    return "Progress: " + std::to_string(progress.progress) + "% (" + 
           std::to_string(progress.completedExposures) + "/" + 
           std::to_string(progress.totalExposures) + " exposures)";
}

// =========================================================================
// Properties and Configuration
// =========================================================================

auto ASICameraController::setProperty(const std::string& property, const std::string& value) -> bool {
    if (!m_properties) {
        setLastError("Property manager not available");
        return false;
    }
    
    // Convert string property name to ASI_CONTROL_TYPE
    ASI_CONTROL_TYPE controlType = stringToControlType(property);
    
    // Convert string value to long
    try {
        long longValue = std::stol(value);
        return m_properties->setProperty(controlType, longValue);
    } catch (const std::exception&) {
        setLastError("Invalid property value: " + value);
        return false;
    }
}

auto ASICameraController::getProperty(const std::string& property) const -> std::string {
    if (!m_properties) {
        return "";
    }
    
    // Convert string property name to ASI_CONTROL_TYPE
    ASI_CONTROL_TYPE controlType = stringToControlType(property);
    
    long value;
    bool isAuto;
    if (m_properties->getProperty(controlType, value, isAuto)) {
        return std::to_string(value) + (isAuto ? " (auto)" : "");
    }
    
    return "";
}

auto ASICameraController::getAvailableProperties() const -> std::vector<std::string> {
    if (!m_properties) {
        return {};
    }
    
    // Get available control types and convert to strings
    auto controlTypes = m_properties->getAvailableProperties();
    std::vector<std::string> propertyNames;
    
    for (auto controlType : controlTypes) {
        propertyNames.push_back(controlTypeToString(controlType));
    }
    
    return propertyNames;
}

// =========================================================================
// Callback Management
// =========================================================================

void ASICameraController::setExposureCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_exposure_callback = std::move(callback);
    if (m_exposure) {
        // Create a wrapper callback that adapts ExposureResult to bool
        auto wrapper = [this](const components::ExposureManager::ExposureResult& result) {
            if (m_exposure_callback) {
                m_exposure_callback(result.success);
            }
        };
        m_exposure->setExposureCallback(wrapper);
    }
}

void ASICameraController::setTemperatureCallback(std::function<void(double)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_temperature_callback = std::move(callback);
    if (m_temperature) {
        // Create a wrapper callback that adapts TemperatureInfo to double
        auto wrapper = [this](const components::TemperatureController::TemperatureInfo& info) {
            if (m_temperature_callback) {
                m_temperature_callback(info.currentTemperature);
            }
        };
        m_temperature->setTemperatureCallback(wrapper);
    }
}

void ASICameraController::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_error_callback = std::move(callback);
}

// =========================================================================
// Private Helper Methods
// =========================================================================

void ASICameraController::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_last_error = error;
    LOG_F(ERROR, "ASI Camera Controller Error: %s", error.c_str());
}

void ASICameraController::notifyError(const std::string& error) {
    setLastError(error);
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    if (m_error_callback) {
        m_error_callback(error);
    }
}

auto ASICameraController::initializeComponents() -> bool {
    try {
        // Initialize hardware interface first
        m_hardware = std::make_unique<components::HardwareInterface>();
        if (!m_hardware->initializeSDK()) {
            setLastError("Failed to initialize hardware interface");
            return false;
        }

        // Create shared pointer for component dependencies
        auto hardware_shared = std::shared_ptr<components::HardwareInterface>(m_hardware.get(), [](components::HardwareInterface*){});

        m_exposure = std::make_unique<components::ExposureManager>(hardware_shared);
        
        m_temperature = std::make_unique<components::TemperatureController>(hardware_shared);
        
        m_properties = std::make_unique<components::PropertyManager>(hardware_shared);

        // SequenceManager needs ExposureManager and PropertyManager
        auto exposure_shared = std::shared_ptr<components::ExposureManager>(m_exposure.get(), [](components::ExposureManager*){});
        auto properties_shared = std::shared_ptr<components::PropertyManager>(m_properties.get(), [](components::PropertyManager*){});
        m_sequence = std::make_unique<components::SequenceManager>(exposure_shared, properties_shared);

        m_video = std::make_unique<components::VideoManager>(hardware_shared);

        m_image_processor = std::make_unique<components::ImageProcessor>();

        // Set up callbacks using correct method names and wrapper functions
        if (m_exposure_callback && m_exposure) {
            auto exposure_wrapper = [this](const components::ExposureManager::ExposureResult& result) {
                if (m_exposure_callback) {
                    m_exposure_callback(result.success);
                }
            };
            m_exposure->setExposureCallback(exposure_wrapper);
        }
        if (m_temperature_callback && m_temperature) {
            auto temperature_wrapper = [this](const components::TemperatureController::TemperatureInfo& info) {
                if (m_temperature_callback) {
                    m_temperature_callback(info.currentTemperature);
                }
            };
            m_temperature->setTemperatureCallback(temperature_wrapper);
        }

        LOG_F(INFO, "All camera components initialized successfully");
        return true;
    } catch (const std::exception& e) {
        setLastError("Exception during component initialization: " + std::string(e.what()));
        return false;
    }
}

void ASICameraController::shutdownComponents() {
    // Reset components in reverse order - destructors will handle cleanup
    if (m_image_processor) {
        LOG_F(INFO, "Shutting down image processor");
        m_image_processor.reset();
    }
    if (m_video) {
        LOG_F(INFO, "Shutting down video manager");
        // Stop video if it's running
        if (m_video->isStreaming()) {
            m_video->stopVideo();
        }
        m_video.reset();
    }
    if (m_sequence) {
        LOG_F(INFO, "Shutting down sequence manager");
        // Stop sequence if it's running
        if (m_sequence->isRunning()) {
            m_sequence->stopSequence();
        }
        m_sequence.reset();
    }
    if (m_temperature) {
        LOG_F(INFO, "Shutting down temperature controller");
        // Stop cooling if it's running
        if (m_temperature->isCoolerOn()) {
            m_temperature->stopCooling();
        }
        m_temperature.reset();
    }
    if (m_exposure) {
        LOG_F(INFO, "Shutting down exposure manager");
        // Abort exposure if it's running
        if (m_exposure->isExposing()) {
            m_exposure->abortExposure();
        }
        m_exposure.reset();
    }
    if (m_properties) {
        LOG_F(INFO, "Shutting down property manager");
        m_properties.reset();
    }
    if (m_hardware) {
        LOG_F(INFO, "Shutting down hardware interface");
        m_hardware.reset();
    }

    LOG_F(INFO, "All camera components shut down");
}

} // namespace lithium::device::asi::camera
