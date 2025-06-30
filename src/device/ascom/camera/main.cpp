/*
 * main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Modular Integration Implementation

This file implements the main integration interface for the modular ASCOM camera
system, providing simplified access to camera functionality.

*************************************************/

#include "main.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::ascom::camera {

// =========================================================================
// ASCOMCameraMain Implementation
// =========================================================================

ASCOMCameraMain::ASCOMCameraMain() 
    : state_(CameraState::DISCONNECTED) {
    LOG_F(INFO, "ASCOMCameraMain created");
}

ASCOMCameraMain::~ASCOMCameraMain() {
    if (isConnected()) {
        disconnect();
    }
    LOG_F(INFO, "ASCOMCameraMain destroyed");
}

bool ASCOMCameraMain::initialize(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    try {
        config_ = config;
        
        // Create the controller
        controller_ = std::make_shared<ASCOMCameraController>("ASCOM Camera");
        if (!controller_) {
            setError("Failed to create ASCOM camera controller");
            return false;
        }
        
        // Initialize controller
        if (!controller_->initialize()) {
            setError("Failed to initialize camera controller");
            return false;
        }
        
        setState(CameraState::DISCONNECTED);
        clearLastError();
        
        LOG_F(INFO, "ASCOM camera initialized with device: {}", config_.deviceName);
        return true;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception during initialization: ") + e.what());
        LOG_F(ERROR, "Exception during ASCOM camera initialization: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::connect() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ == CameraState::CONNECTED) {
        return true; // Already connected
    }
    
    if (!controller_) {
        setError("Camera not initialized");
        return false;
    }
    
    try {
        setState(CameraState::CONNECTING);
        
        // Connect via controller
        if (!controller_->connect(config_.deviceName)) {
            setState(CameraState::ERROR);
            setError("Failed to connect to ASCOM camera");
            return false;
        }
        
        setState(CameraState::CONNECTED);
        clearLastError();
        
        LOG_F(INFO, "Connected to ASCOM camera: {}", config_.deviceName);
        return true;
        
    } catch (const std::exception& e) {
        setState(CameraState::ERROR);
        setError(std::string("Exception during connection: ") + e.what());
        LOG_F(ERROR, "Exception during ASCOM camera connection: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::disconnect() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ == CameraState::DISCONNECTED) {
        return true; // Already disconnected
    }
    
    try {
        if (controller_) {
            controller_->disconnect();
        }
        
        setState(CameraState::DISCONNECTED);
        clearLastError();
        
        LOG_F(INFO, "Disconnected from ASCOM camera");
        return true;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception during disconnection: ") + e.what());
        LOG_F(ERROR, "Exception during ASCOM camera disconnection: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::isConnected() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_ == CameraState::CONNECTED || 
           state_ == CameraState::EXPOSING || 
           state_ == CameraState::READING ||
           state_ == CameraState::IDLE;
}

ASCOMCameraMain::CameraState ASCOMCameraMain::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

std::string ASCOMCameraMain::getStateString() const {
    switch (getState()) {
        case CameraState::DISCONNECTED: return "Disconnected";
        case CameraState::CONNECTING: return "Connecting";
        case CameraState::CONNECTED: return "Connected";
        case CameraState::EXPOSING: return "Exposing";
        case CameraState::READING: return "Reading";
        case CameraState::IDLE: return "Idle";
        case CameraState::ERROR: return "Error";
        default: return "Unknown";
    }
}

// =========================================================================
// Basic Camera Operations
// =========================================================================

bool ASCOMCameraMain::startExposure(double duration, bool isDark) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        setState(CameraState::EXPOSING);
        
        bool result = controller_->startExposure(duration);
        if (!result) {
            setState(CameraState::IDLE);
            setError("Failed to start exposure");
            return false;
        }
        
        clearLastError();
        LOG_F(INFO, "Started exposure: {} seconds, dark={}", duration, isDark);
        return true;
        
    } catch (const std::exception& e) {
        setState(CameraState::ERROR);
        setError(std::string("Exception during exposure start: ") + e.what());
        LOG_F(ERROR, "Exception during exposure start: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::abortExposure() {
    if (!controller_) {
        setError("Camera not initialized");
        return false;
    }
    
    try {
        bool result = controller_->abortExposure();
        if (result) {
            setState(CameraState::IDLE);
            clearLastError();
            LOG_F(INFO, "Exposure aborted");
        } else {
            setError("Failed to abort exposure");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception during exposure abort: ") + e.what());
        LOG_F(ERROR, "Exception during exposure abort: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::isExposing() const {
    if (!controller_) {
        return false;
    }
    
    return controller_->isExposing();
}

std::shared_ptr<AtomCameraFrame> ASCOMCameraMain::getLastImage() {
    if (!controller_) {
        setError("Camera not initialized");
        return nullptr;
    }
    
    try {
        auto frame = controller_->getExposureResult();
        if (frame) {
            setState(CameraState::IDLE);
            clearLastError();
        }
        return frame;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception getting last image: ") + e.what());
        LOG_F(ERROR, "Exception getting last image: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<AtomCameraFrame> ASCOMCameraMain::downloadImage() {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return nullptr;
    }
    
    try {
        setState(CameraState::READING);
        
        auto frame = controller_->getExposureResult();
        if (frame) {
            setState(CameraState::IDLE);
            clearLastError();
            LOG_F(INFO, "Image downloaded successfully");
        } else {
            setState(CameraState::ERROR);
            setError("Failed to download image");
        }
        
        return frame;
        
    } catch (const std::exception& e) {
        setState(CameraState::ERROR);
        setError(std::string("Exception during image download: ") + e.what());
        LOG_F(ERROR, "Exception during image download: {}", e.what());
        return nullptr;
    }
}

// =========================================================================
// Camera Properties
// =========================================================================

std::string ASCOMCameraMain::getCameraName() const {
    if (!controller_) return "";
    return controller_->getName();
}

std::string ASCOMCameraMain::getDescription() const {
    if (!controller_) return "";
    return "ASCOM Camera Modular Driver";
}

std::string ASCOMCameraMain::getDriverInfo() const {
    if (!controller_) return "";
    auto info = controller_->getASCOMDriverInfo();
    return info.value_or("");
}

std::string ASCOMCameraMain::getDriverVersion() const {
    if (!controller_) return "";
    auto version = controller_->getASCOMVersion();
    return version.value_or("");
}

int ASCOMCameraMain::getCameraXSize() const {
    if (!controller_) return 0;
    auto resolution = controller_->getMaxResolution();
    return resolution.width;
}

int ASCOMCameraMain::getCameraYSize() const {
    if (!controller_) return 0;
    auto resolution = controller_->getMaxResolution();
    return resolution.height;
}

double ASCOMCameraMain::getPixelSizeX() const {
    if (!controller_) return 0.0;
    return controller_->getPixelSizeX();
}

double ASCOMCameraMain::getPixelSizeY() const {
    if (!controller_) return 0.0;
    return controller_->getPixelSizeY();
}

// =========================================================================
// Temperature Control
// =========================================================================

bool ASCOMCameraMain::setCCDTemperature(double temperature) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = controller_->setTemperature(temperature);
        if (result) {
            clearLastError();
            LOG_F(INFO, "CCD temperature set to: {} °C", temperature);
        } else {
            setError("Failed to set CCD temperature");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception setting CCD temperature: ") + e.what());
        LOG_F(ERROR, "Exception setting CCD temperature: {}", e.what());
        return false;
    }
}

double ASCOMCameraMain::getCCDTemperature() const {
    if (!controller_) return 0.0;
    auto temp = controller_->getTemperature();
    return temp ? *temp : -999.0;
}

bool ASCOMCameraMain::hasCooling() const {
    if (!controller_) return false;
    return controller_->hasCooler();
}

bool ASCOMCameraMain::isCoolingEnabled() const {
    if (!controller_) return false;
    return controller_->isCoolerOn();
}

bool ASCOMCameraMain::setCoolingEnabled(bool enable) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = enable ? controller_->startCooling(20.0) : controller_->stopCooling();
        if (result) {
            clearLastError();
            LOG_F(INFO, "Cooling {}", enable ? "enabled" : "disabled");
        } else {
            setError("Failed to set cooling state");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception setting cooling state: ") + e.what());
        LOG_F(ERROR, "Exception setting cooling state: {}", e.what());
        return false;
    }
}

// =========================================================================
// Video and Live Mode
// =========================================================================

bool ASCOMCameraMain::startLiveMode() {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = controller_->startVideo();
        if (result) {
            clearLastError();
            LOG_F(INFO, "Live mode started");
        } else {
            setError("Failed to start live mode");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception starting live mode: ") + e.what());
        LOG_F(ERROR, "Exception starting live mode: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::stopLiveMode() {
    if (!controller_) {
        setError("Camera not initialized");
        return false;
    }
    
    try {
        bool result = controller_->stopVideo();
        if (result) {
            clearLastError();
            LOG_F(INFO, "Live mode stopped");
        } else {
            setError("Failed to stop live mode");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception stopping live mode: ") + e.what());
        LOG_F(ERROR, "Exception stopping live mode: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::isLiveModeActive() const {
    if (!controller_) return false;
    return controller_->isVideoRunning();
}

std::shared_ptr<AtomCameraFrame> ASCOMCameraMain::getLiveFrame() {
    if (!controller_) {
        setError("Camera not initialized");
        return nullptr;
    }
    
    try {
        return controller_->getVideoFrame();
    } catch (const std::exception& e) {
        setError(std::string("Exception getting live frame: ") + e.what());
        LOG_F(ERROR, "Exception getting live frame: {}", e.what());
        return nullptr;
    }
}

// =========================================================================
// Advanced Features
// =========================================================================

bool ASCOMCameraMain::setROI(int startX, int startY, int width, int height) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = controller_->setResolution(startX, startY, width, height);
        if (result) {
            clearLastError();
            LOG_F(INFO, "ROI set to: ({}, {}) {}x{}", startX, startY, width, height);
        } else {
            setError("Failed to set ROI");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception setting ROI: ") + e.what());
        LOG_F(ERROR, "Exception setting ROI: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::resetROI() {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        auto maxRes = controller_->getMaxResolution();
        bool result = controller_->setResolution(0, 0, maxRes.width, maxRes.height);
        if (result) {
            clearLastError();
            LOG_F(INFO, "ROI reset to full frame");
        } else {
            setError("Failed to reset ROI");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception resetting ROI: ") + e.what());
        LOG_F(ERROR, "Exception resetting ROI: {}", e.what());
        return false;
    }
}

bool ASCOMCameraMain::setBinning(int binning) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = controller_->setBinning(binning, binning);
        if (result) {
            clearLastError();
            LOG_F(INFO, "Binning set to: {}x{}", binning, binning);
        } else {
            setError("Failed to set binning");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception setting binning: ") + e.what());
        LOG_F(ERROR, "Exception setting binning: {}", e.what());
        return false;
    }
}

int ASCOMCameraMain::getBinning() const {
    if (!controller_) return 1;
    auto binning = controller_->getBinning();
    return binning ? binning->horizontal : 1; // Assume symmetric binning
}

bool ASCOMCameraMain::setGain(int gain) {
    if (!isConnected() || !controller_) {
        setError("Camera not connected");
        return false;
    }
    
    try {
        bool result = controller_->setGain(gain);
        if (result) {
            clearLastError();
            LOG_F(INFO, "Gain set to: {}", gain);
        } else {
            setError("Failed to set gain");
        }
        return result;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception setting gain: ") + e.what());
        LOG_F(ERROR, "Exception setting gain: {}", e.what());
        return false;
    }
}

int ASCOMCameraMain::getGain() const {
    if (!controller_) return 0;
    auto gain = controller_->getGain();
    return gain ? *gain : 0;
}

// =========================================================================
// Statistics and Monitoring
// =========================================================================

std::map<std::string, double> ASCOMCameraMain::getStatistics() const {
    if (!controller_) {
        return {};
    }
    
    try {
        return controller_->getFrameStatistics();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception getting statistics: {}", e.what());
        return {};
    }
}

std::string ASCOMCameraMain::getLastError() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return lastError_;
}

void ASCOMCameraMain::clearLastError() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_.clear();
}

std::shared_ptr<ASCOMCameraController> ASCOMCameraMain::getController() const {
    return controller_;
}

// =========================================================================
// Private Helper Methods
// =========================================================================

void ASCOMCameraMain::setState(CameraState newState) {
    state_ = newState;
}

void ASCOMCameraMain::setError(const std::string& error) {
    lastError_ = error;
    LOG_F(ERROR, "ASCOM Camera Error: {}", error);
}

ASCOMCameraMain::CameraState ASCOMCameraMain::convertControllerState() const {
    if (!controller_) {
        return CameraState::DISCONNECTED;
    }
    
    if (!controller_->isConnected()) {
        return CameraState::DISCONNECTED;
    }
    
    if (controller_->isExposing()) {
        return CameraState::EXPOSING;
    }
    
    return CameraState::IDLE;
}

// =========================================================================
// Factory Functions
// =========================================================================

std::shared_ptr<ASCOMCameraMain> createASCOMCamera(const ASCOMCameraMain::CameraConfig& config) {
    try {
        auto camera = std::make_shared<ASCOMCameraMain>();
        if (camera->initialize(config)) {
            LOG_F(INFO, "Created ASCOM camera with device: {}", config.deviceName);
            return camera;
        } else {
            LOG_F(ERROR, "Failed to initialize ASCOM camera: {}", config.deviceName);
            return nullptr;
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception creating ASCOM camera: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<ASCOMCameraMain> createASCOMCamera(const std::string& deviceName) {
    ASCOMCameraMain::CameraConfig config;
    config.deviceName = deviceName;
    config.progId = deviceName; // Assume deviceName is the ProgID for COM
    
    return createASCOMCamera(config);
}

std::vector<std::string> discoverASCOMCameras() {
    // This would typically enumerate ASCOM cameras via registry or Alpaca discovery
    // For now, return a placeholder list
    LOG_F(INFO, "Discovering ASCOM cameras...");
    
    std::vector<std::string> cameras;
    
    // Add some common ASCOM camera drivers for testing
    cameras.push_back("ASCOM.Simulator.Camera");
    cameras.push_back("ASCOM.ASICamera2.Camera");
    cameras.push_back("ASCOM.QHYCamera.Camera");
    
    LOG_F(INFO, "Found {} ASCOM cameras", cameras.size());
    return cameras;
}

std::optional<CameraCapabilities> 
getASCOMCameraCapabilities(const std::string& deviceName) {
    try {
        // This would typically query the ASCOM driver for capabilities
        // For now, return default capabilities
        LOG_F(INFO, "Getting capabilities for ASCOM camera: {}", deviceName);
        
        CameraCapabilities caps;
        caps.maxWidth = 1920;
        caps.maxHeight = 1080;
        caps.pixelSizeX = 5.86;
        caps.pixelSizeY = 5.86;
        caps.maxBinning = 4;
        caps.hasCooler = true;
        caps.hasShutter = true;
        caps.canAbortExposure = true;
        caps.canStopExposure = true;
        caps.canGetCoolerPower = true;
        caps.canSetCCDTemperature = true;
        caps.hasGainControl = true;
        caps.hasOffsetControl = true;
        caps.minExposure = 0.001;
        caps.maxExposure = 3600.0;
        caps.electronsPerADU = 0.37;
        caps.fullWellCapacity = 25000.0;
        caps.maxADU = 65535;
        
        return caps;
        
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception getting ASCOM camera capabilities: {}", e.what());
        return std::nullopt;
    }
}

} // namespace lithium::device::ascom::camera
