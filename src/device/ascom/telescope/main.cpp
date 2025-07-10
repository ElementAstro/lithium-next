/*
 * main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Modular Integration Implementation

This file implements the main integration interface for the modular ASCOM telescope
system, providing simplified access to telescope functionality.

*************************************************/

#include "main.hpp"

#include "components/hardware_interface.hpp"
#include "components/motion_controller.hpp"
#include "components/coordinate_manager.hpp"
#include "components/guide_manager.hpp"
#include "components/tracking_manager.hpp"
#include "components/parking_manager.hpp"
#include "components/alignment_manager.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::ascom::telescope {

// =========================================================================
// ASCOMTelescopeMain Implementation
// =========================================================================

ASCOMTelescopeMain::ASCOMTelescopeMain() 
    : state_(TelescopeState::DISCONNECTED) {
    spdlog::info("ASCOMTelescopeMain created");
}

ASCOMTelescopeMain::~ASCOMTelescopeMain() {
    spdlog::info("ASCOMTelescopeMain destructor called");
    shutdown();
}

bool ASCOMTelescopeMain::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    spdlog::info("Initializing ASCOM Telescope Main");
    
    try {
        // Initialize components will be called when needed
        // For now, just mark as ready for connection
        spdlog::info("ASCOM Telescope Main initialized successfully");
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to initialize telescope: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::shutdown() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    spdlog::info("Shutting down ASCOM Telescope Main");
    
    // Disconnect if connected
    if (state_ != TelescopeState::DISCONNECTED) {
        disconnect();
    }
    
    // Shutdown components
    shutdownComponents();
    
    setState(TelescopeState::DISCONNECTED);
    spdlog::info("ASCOM Telescope Main shutdown complete");
    return true;
}

bool ASCOMTelescopeMain::connect(const std::string& deviceName, int timeout, int maxRetry) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != TelescopeState::DISCONNECTED) {
        setLastError("Telescope is already connected");
        return false;
    }
    
    spdlog::info("Connecting to telescope device: {}", deviceName);
    
    try {
        // Initialize components if not already done
        if (!initializeComponents()) {
            setLastError("Failed to initialize telescope components");
            return false;
        }
        
        // Prepare connection settings
        components::HardwareInterface::ConnectionSettings settings;
        settings.deviceName = deviceName;
        
        // Determine connection type based on device name
        if (deviceName.find("://") != std::string::npos) {
            settings.type = components::ConnectionType::ALPACA_REST;
            // Parse URL for Alpaca settings
            size_t start = deviceName.find("://") + 3;
            size_t colon = deviceName.find(":", start);
            if (colon != std::string::npos) {
                settings.host = deviceName.substr(start, colon - start);
                size_t slash = deviceName.find("/", colon);
                if (slash != std::string::npos) {
                    settings.port = std::stoi(deviceName.substr(colon + 1, slash - colon - 1));
                    settings.deviceNumber = std::stoi(deviceName.substr(slash + 1));
                }
            }
        } else {
            // Assume COM driver
            settings.type = components::ConnectionType::COM_DRIVER;
            settings.progId = deviceName;
        }
        
        // Attempt connection with retry logic
        bool connected = false;
        for (int attempt = 0; attempt < maxRetry && !connected; ++attempt) {
            spdlog::info("Connection attempt {} of {}", attempt + 1, maxRetry);
            
            if (hardware_->connect(settings)) {
                connected = true;
                setState(TelescopeState::CONNECTED);
                spdlog::info("Successfully connected to telescope: {}", deviceName);
            } else {
                if (attempt < maxRetry - 1) {
                    spdlog::warn("Connection attempt {} failed, retrying...", attempt + 1);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        }
        
        if (!connected) {
            setLastError("Failed to connect after " + std::to_string(maxRetry) + " attempts");
            return false;
        }
        
        // Transition to idle state
        setState(TelescopeState::IDLE);
        return true;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Connection error: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::disconnect() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ == TelescopeState::DISCONNECTED) {
        return true;
    }
    
    spdlog::info("Disconnecting from telescope");
    
    try {
        // Stop any ongoing operations
        if (motion_ && motion_->isMoving()) {
            motion_->emergencyStop();
        }
        
        // Disconnect hardware
        if (hardware_ && hardware_->isConnected()) {
            hardware_->disconnect();
        }
        
        setState(TelescopeState::DISCONNECTED);
        spdlog::info("Successfully disconnected from telescope");
        return true;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Disconnection error: ") + e.what());
        return false;
    }
}

std::vector<std::string> ASCOMTelescopeMain::scanDevices() {
    spdlog::info("Scanning for telescope devices");
    
    std::vector<std::string> devices;
    
    try {
        // Initialize hardware interface if needed for scanning
        if (!hardware_) {
            // Create a temporary hardware interface for scanning
            boost::asio::io_context temp_io_context;
            auto temp_hardware = std::make_shared<components::HardwareInterface>(temp_io_context);
            if (temp_hardware->initialize()) {
                devices = temp_hardware->discoverDevices();
                temp_hardware->shutdown();
            }
        } else if (hardware_->isInitialized()) {
            devices = hardware_->discoverDevices();
        }
        
        spdlog::info("Found {} telescope devices", devices.size());
        return devices;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Device scan error: ") + e.what());
        return {};
    }
}

bool ASCOMTelescopeMain::isConnected() const {
    return state_ != TelescopeState::DISCONNECTED;
}

TelescopeState ASCOMTelescopeMain::getState() const {
    return state_;
}

// =========================================================================
// Coordinate and Position Management
// =========================================================================

std::optional<EquatorialCoordinates> ASCOMTelescopeMain::getCurrentRADEC() {
    if (!validateConnection()) {
        return std::nullopt;
    }
    
    try {
        return coordinates_->getRADECJNow();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to get current RA/DEC: ") + e.what());
        return std::nullopt;
    }
}

std::optional<HorizontalCoordinates> ASCOMTelescopeMain::getCurrentAZALT() {
    if (!validateConnection()) {
        return std::nullopt;
    }
    
    try {
        return coordinates_->getAZALT();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to get current AZ/ALT: ") + e.what());
        return std::nullopt;
    }
}

bool ASCOMTelescopeMain::slewToRADEC(double ra, double dec, bool enableTracking) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        setState(TelescopeState::SLEWING);
        
        bool success = motion_->slewToRADEC(ra, dec, true); // Always async for main interface
        
        if (success && enableTracking) {
            // Enable tracking after slew starts
            tracking_->setTracking(true);
        }
        
        if (!success) {
            setState(TelescopeState::IDLE);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to slew to RA/DEC: ") + e.what());
        setState(TelescopeState::ERROR);
        return false;
    }
}

bool ASCOMTelescopeMain::slewToAZALT(double az, double alt) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        setState(TelescopeState::SLEWING);
        
        bool success = motion_->slewToAZALT(az, alt, true); // Always async
        
        if (!success) {
            setState(TelescopeState::IDLE);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to slew to AZ/ALT: ") + e.what());
        setState(TelescopeState::ERROR);
        return false;
    }
}

bool ASCOMTelescopeMain::syncToRADEC(double ra, double dec) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        // Use hardware interface directly for sync operations
        return hardware_->syncToCoordinates(ra, dec);
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to sync to RA/DEC: ") + e.what());
        return false;
    }
}

// =========================================================================
// Motion Control
// =========================================================================

bool ASCOMTelescopeMain::isSlewing() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return motion_->isSlewing();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to check slewing status: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::abortSlew() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        bool success = motion_->abortSlew();
        if (success) {
            setState(TelescopeState::IDLE);
        }
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to abort slew: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::emergencyStop() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        bool success = motion_->emergencyStop();
        if (success) {
            setState(TelescopeState::IDLE);
        }
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to perform emergency stop: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::startDirectionalMove(const std::string& direction, double rate) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return motion_->startDirectionalMove(direction, rate);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to start directional move: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::stopDirectionalMove(const std::string& direction) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return motion_->stopDirectionalMove(direction);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to stop directional move: ") + e.what());
        return false;
    }
}

// =========================================================================
// Tracking Control
// =========================================================================

bool ASCOMTelescopeMain::isTracking() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return tracking_->isTracking();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to check tracking status: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::setTracking(bool enable) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        bool success = tracking_->setTracking(enable);
        if (success && enable) {
            setState(TelescopeState::TRACKING);
        } else if (success && !enable) {
            setState(TelescopeState::IDLE);
        }
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to set tracking: ") + e.what());
        return false;
    }
}

std::optional<TrackMode> ASCOMTelescopeMain::getTrackingRate() {
    if (!validateConnection()) {
        return std::nullopt;
    }
    
    try {
        return tracking_->getTrackingRate();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to get tracking rate: ") + e.what());
        return std::nullopt;
    }
}

bool ASCOMTelescopeMain::setTrackingRate(TrackMode rate) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return tracking_->setTrackingRate(rate);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to set tracking rate: ") + e.what());
        return false;
    }
}

// =========================================================================
// Parking Operations
// =========================================================================

bool ASCOMTelescopeMain::isParked() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return parking_->isParked();
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to check park status: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::park() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        setState(TelescopeState::PARKING);
        
        bool success = parking_->park();
        if (success) {
            setState(TelescopeState::PARKED);
        } else {
            setState(TelescopeState::IDLE);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to park telescope: ") + e.what());
        setState(TelescopeState::ERROR);
        return false;
    }
}

bool ASCOMTelescopeMain::unpark() {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        bool success = parking_->unpark();
        if (success) {
            setState(TelescopeState::IDLE);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to unpark telescope: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::setParkPosition(double ra, double dec) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return parking_->setParkPosition(ra, dec);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to set park position: ") + e.what());
        return false;
    }
}

// =========================================================================
// Guiding Operations
// =========================================================================

bool ASCOMTelescopeMain::guidePulse(const std::string& direction, int duration) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return guide_->guidePulse(direction, duration);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to send guide pulse: ") + e.what());
        return false;
    }
}

bool ASCOMTelescopeMain::guideRADEC(double ra_ms, double dec_ms) {
    if (!validateConnection()) {
        return false;
    }
    
    try {
        return guide_->guideRADEC(ra_ms, dec_ms);
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to send RA/DEC guide: ") + e.what());
        return false;
    }
}

// =========================================================================
// Status and Information
// =========================================================================

std::optional<TelescopeParameters> ASCOMTelescopeMain::getTelescopeInfo() {
    if (!validateConnection()) {
        return std::nullopt;
    }
    
    try {
        auto hwInfo = hardware_->getTelescopeInfo();
        if (!hwInfo) {
            return std::nullopt;
        }
        
        TelescopeParameters params;
        params.aperture = hwInfo->aperture;
        params.focal_length = hwInfo->focalLength;
        // Add other parameter mappings as needed
        
        return params;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to get telescope info: ") + e.what());
        return std::nullopt;
    }
}

std::string ASCOMTelescopeMain::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void ASCOMTelescopeMain::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

// =========================================================================
// Private Methods
// =========================================================================

void ASCOMTelescopeMain::setState(TelescopeState newState) {
    state_ = newState;
    spdlog::debug("Telescope state changed to: {}", static_cast<int>(newState));
}

void ASCOMTelescopeMain::setLastError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
    spdlog::error("Telescope error: {}", error);
}

bool ASCOMTelescopeMain::validateConnection() const {
    if (state_ == TelescopeState::DISCONNECTED) {
        setLastError("Telescope is not connected");
        return false;
    }
    
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware interface is not connected");
        return false;
    }
    
    return true;
}

bool ASCOMTelescopeMain::initializeComponents() {
    try {
        // Create io_context for hardware interface
        static boost::asio::io_context io_context;
        
        // Initialize hardware interface
        hardware_ = std::make_shared<components::HardwareInterface>(io_context);
        if (!hardware_->initialize()) {
            return false;
        }
        
        // Initialize other components
        motion_ = std::make_shared<components::MotionController>(hardware_);
        coordinates_ = std::make_shared<components::CoordinateManager>(hardware_);
        guide_ = std::make_shared<components::GuideManager>(hardware_);
        tracking_ = std::make_shared<components::TrackingManager>(hardware_);
        parking_ = std::make_shared<components::ParkingManager>(hardware_);
        alignment_ = std::make_shared<components::AlignmentManager>(hardware_);
        
        // Initialize components that need initialization
        if (!motion_->initialize()) {
            return false;
        }
        
        spdlog::info("All telescope components initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to initialize components: ") + e.what());
        return false;
    }
}

void ASCOMTelescopeMain::shutdownComponents() {
    try {
        if (motion_) {
            motion_->shutdown();
        }
        
        if (hardware_) {
            hardware_->shutdown();
        }
        
        // Reset all component pointers
        alignment_.reset();
        parking_.reset();
        tracking_.reset();
        guide_.reset();
        coordinates_.reset();
        motion_.reset();
        hardware_.reset();
        
        spdlog::info("All telescope components shut down successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("Error during component shutdown: {}", e.what());
    }
}

} // namespace lithium::device::ascom::telescope
