/*
 * telescope_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "telescope_controller.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi::telescope {

INDITelescopeController::INDITelescopeController() 
    : INDITelescopeController("INDITelescope") {
}

INDITelescopeController::INDITelescopeController(const std::string& name)
    : AtomTelescope(name), telescopeName_(name) {
}

INDITelescopeController::~INDITelescopeController() {
    destroy();
}

bool INDITelescopeController::initialize() {
    if (initialized_.load()) {
        logWarning("Controller already initialized");
        return true;
    }
    
    try {
        logInfo("Initializing INDI telescope controller: " + telescopeName_);
        
        // Initialize components in proper order
        if (!initializeComponents()) {
            logError("Failed to initialize components");
            return false;
        }
        
        // Setup component callbacks
        setupComponentCallbacks();
        
        // Validate component dependencies
        validateComponentDependencies();
        
        initialized_.store(true);
        logInfo("INDI telescope controller initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::destroy() {
    if (!initialized_.load()) {
        return true;
    }
    
    try {
        logInfo("Shutting down INDI telescope controller");
        
        // Disconnect if connected
        if (connected_.load()) {
            disconnect();
        }
        
        // Shutdown components
        if (!shutdownComponents()) {
            logWarning("Some components failed to shutdown cleanly");
        }
        
        initialized_.store(false);
        logInfo("INDI telescope controller shutdown completed");
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Shutdown failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::connect(const std::string& deviceName, int timeout, int maxRetry) {
    if (!initialized_.load()) {
        setLastError("Controller not initialized");
        return false;
    }
    
    if (connected_.load()) {
        if (hardware_->getCurrentDeviceName() == deviceName) {
            logInfo("Already connected to device: " + deviceName);
            return true;
        } else {
            // Disconnect from current device first
            disconnect();
        }
    }
    
    try {
        logInfo("Connecting to telescope device: " + deviceName);
        
        // Try to connect with retries
        int attempts = 0;
        bool success = false;
        
        while (attempts < maxRetry && !success) {
            if (hardware_->connectToDevice(deviceName, timeout)) {
                success = true;
                break;
            }
            
            attempts++;
            if (attempts < maxRetry) {
                logWarning("Connection attempt " + std::to_string(attempts) + 
                          " failed, retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        
        if (!success) {
            setLastError("Failed to connect after " + std::to_string(maxRetry) + " attempts");
            return false;
        }
        
        // Initialize component states with hardware
        coordinateComponentStates();
        
        connected_.store(true);
        clearLastError();
        
        logInfo("Successfully connected to: " + deviceName);
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Connection failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    try {
        logInfo("Disconnecting from telescope device");
        
        // Stop all operations before disconnecting
        if (motionController_ && motionController_->isMoving()) {
            motionController_->abortSlew();
        }
        
        if (trackingManager_ && trackingManager_->isTrackingEnabled()) {
            trackingManager_->enableTracking(false);
        }
        
        // Disconnect hardware
        if (hardware_->isConnected()) {
            if (!hardware_->disconnectFromDevice()) {
                logWarning("Hardware disconnect returned false");
            }
        }
        
        connected_.store(false);
        clearLastError();
        
        logInfo("Disconnected from telescope device");
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Disconnect failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> INDITelescopeController::scan() {
    if (!initialized_.load()) {
        setLastError("Controller not initialized");
        return {};
    }
    
    try {
        return hardware_->scanDevices();
    } catch (const std::exception& e) {
        setLastError("Scan failed: " + std::string(e.what()));
        return {};
    }
}

bool INDITelescopeController::isConnected() const {
    return connected_.load() && hardware_ && hardware_->isConnected();
}

std::optional<TelescopeParameters> INDITelescopeController::getTelescopeInfo() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    try {
        // This would typically come from INDI properties
        // For now, return default values
        TelescopeParameters params;
        params.aperture = 200.0;      // mm
        params.focalLength = 1000.0;  // mm
        params.guiderAperture = 50.0; // mm
        params.guiderFocalLength = 200.0; // mm
        
        return params;
        
    } catch (const std::exception& e) {
        setLastError("Failed to get telescope info: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool INDITelescopeController::setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                                              double guiderAperture, double guiderFocal) {
    if (!validateController()) {
        return false;
    }
    
    try {
        // Set telescope parameters via INDI properties
        bool success = true;
        
        success &= hardware_->setNumberProperty("TELESCOPE_INFO", "TELESCOPE_APERTURE", telescopeAperture);
        success &= hardware_->setNumberProperty("TELESCOPE_INFO", "TELESCOPE_FOCAL_LENGTH", telescopeFocal);
        success &= hardware_->setNumberProperty("TELESCOPE_INFO", "GUIDER_APERTURE", guiderAperture);
        success &= hardware_->setNumberProperty("TELESCOPE_INFO", "GUIDER_FOCAL_LENGTH", guiderFocal);
        
        if (success) {
            clearLastError();
        } else {
            setLastError("Failed to set some telescope parameters");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError("Failed to set telescope info: " + std::string(e.what()));
        return false;
    }
}

std::optional<std::string> INDITelescopeController::getStatus() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    try {
        std::string status = "IDLE";
        
        if (motionController_->isMoving()) {
            status = "SLEWING";
        } else if (trackingManager_->isTrackingEnabled()) {
            status = "TRACKING";
        } else if (parkingManager_->isParked()) {
            status = "PARKED";
        }
        
        return status;
        
    } catch (const std::exception& e) {
        setLastError("Failed to get status: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool INDITelescopeController::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) {
    if (!validateController()) {
        return false;
    }
    
    try {
        // Set coordinates first
        if (!coordinateManager_->setTargetRADEC(raHours, decDegrees)) {
            setLastError("Failed to set target coordinates");
            return false;
        }
        
        // Start slewing
        if (!motionController_->slewToCoordinates(raHours, decDegrees, enableTracking)) {
            setLastError("Failed to start slew");
            return false;
        }
        
        clearLastError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Slew failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::syncToRADECJNow(double raHours, double decDegrees) {
    if (!validateController()) {
        return false;
    }
    
    try {
        // Set coordinates first
        if (!coordinateManager_->setTargetRADEC(raHours, decDegrees)) {
            setLastError("Failed to set sync coordinates");
            return false;
        }
        
        // Perform sync
        if (!motionController_->syncToCoordinates(raHours, decDegrees)) {
            setLastError("Failed to sync");
            return false;
        }
        
        clearLastError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Sync failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::abortMotion() {
    if (!validateController()) {
        return false;
    }
    
    try {
        bool success = motionController_->abortSlew();
        
        if (success) {
            clearLastError();
        } else {
            setLastError("Failed to abort motion");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError("Abort failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::emergencyStop() {
    if (!validateController()) {
        return false;
    }
    
    try {
        bool success = motionController_->emergencyStop();
        
        if (success) {
            clearLastError();
        } else {
            setLastError("Emergency stop failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError("Emergency stop failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::isMoving() {
    if (!validateController()) {
        return false;
    }
    
    return motionController_->isMoving();
}

bool INDITelescopeController::enableTracking(bool enable) {
    if (!validateController()) {
        return false;
    }
    
    try {
        bool success = trackingManager_->enableTracking(enable);
        
        if (success) {
            clearLastError();
        } else {
            setLastError("Failed to " + std::string(enable ? "enable" : "disable") + " tracking");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError("Tracking control failed: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::isTrackingEnabled() {
    if (!validateController()) {
        return false;
    }
    
    return trackingManager_->isTrackingEnabled();
}

std::optional<TrackMode> INDITelescopeController::getTrackRate() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return static_cast<TrackMode>(trackingManager_->getTrackingMode());
}

bool INDITelescopeController::setTrackRate(TrackMode rate) {
    if (!validateController()) {
        return false;
    }
    
    return trackingManager_->setTrackingMode(rate);
}

MotionRates INDITelescopeController::getTrackRates() {
    if (!validateController()) {
        return MotionRates{};
    }
    
    auto rates = trackingManager_->getTrackRates();
    return rates ? *rates : MotionRates{};
}

bool INDITelescopeController::setTrackRates(const MotionRates& rates) {
    if (!validateController()) {
        return false;
    }
    
    return trackingManager_->setTrackRates(rates);
}

bool INDITelescopeController::park() {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->park();
}

bool INDITelescopeController::unpark() {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->unpark();
}

bool INDITelescopeController::isParked() {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->isParked();
}

bool INDITelescopeController::canPark() {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->canPark();
}

bool INDITelescopeController::setParkPosition(double parkRA, double parkDEC) {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->setParkPosition(parkRA, parkDEC);
}

std::optional<EquatorialCoordinates> INDITelescopeController::getParkPosition() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    auto parkPos = parkingManager_->getCurrentParkPosition();
    if (parkPos) {
        return EquatorialCoordinates{parkPos->ra, parkPos->dec};
    }
    return std::nullopt;
}

bool INDITelescopeController::setParkOption(ParkOptions option) {
    if (!validateController()) {
        return false;
    }
    
    return parkingManager_->setParkOption(option);
}

std::optional<EquatorialCoordinates> INDITelescopeController::getRADECJ2000() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    auto current = coordinateManager_->getCurrentRADEC();
    if (current) {
        // Convert JNow to J2000
        auto j2000 = coordinateManager_->jNowToJ2000(*current);
        return j2000;
    }
    return std::nullopt;
}

bool INDITelescopeController::setRADECJ2000(double raHours, double decDegrees) {
    if (!validateController()) {
        return false;
    }
    
    // Convert J2000 to JNow and set
    EquatorialCoordinates j2000{raHours, decDegrees};
    auto jnow = coordinateManager_->j2000ToJNow(j2000);
    if (jnow) {
        return coordinateManager_->setTargetRADEC(*jnow);
    }
    return false;
}

std::optional<EquatorialCoordinates> INDITelescopeController::getRADECJNow() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getCurrentRADEC();
}

bool INDITelescopeController::setRADECJNow(double raHours, double decDegrees) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setTargetRADEC(raHours, decDegrees);
}

std::optional<EquatorialCoordinates> INDITelescopeController::getTargetRADECJNow() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getTargetRADEC();
}

bool INDITelescopeController::setTargetRADECJNow(double raHours, double decDegrees) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setTargetRADEC(raHours, decDegrees);
}

std::optional<HorizontalCoordinates> INDITelescopeController::getAZALT() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getCurrentAltAz();
}

bool INDITelescopeController::setAZALT(double azDegrees, double altDegrees) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setTargetAltAz(azDegrees, altDegrees);
}

bool INDITelescopeController::slewToAZALT(double azDegrees, double altDegrees) {
    if (!validateController()) {
        return false;
    }
    
    return motionController_->slewToAltAz(azDegrees, altDegrees);
}

std::optional<GeographicLocation> INDITelescopeController::getLocation() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getLocation();
}

bool INDITelescopeController::setLocation(const GeographicLocation& location) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setLocation(location);
}

std::optional<std::chrono::system_clock::time_point> INDITelescopeController::getUTCTime() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getTime();
}

bool INDITelescopeController::setUTCTime(const std::chrono::system_clock::time_point& time) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setTime(time);
}

std::optional<std::chrono::system_clock::time_point> INDITelescopeController::getLocalTime() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return coordinateManager_->getLocalTime();
}

bool INDITelescopeController::guideNS(int direction, int duration) {
    if (!validateController()) {
        return false;
    }
    
    components::GuideManager::GuideDirection guideDir = 
        (direction > 0) ? components::GuideManager::GuideDirection::NORTH : 
                         components::GuideManager::GuideDirection::SOUTH;
    
    return guideManager_->guidePulse(guideDir, std::chrono::milliseconds(duration));
}

bool INDITelescopeController::guideEW(int direction, int duration) {
    if (!validateController()) {
        return false;
    }
    
    components::GuideManager::GuideDirection guideDir = 
        (direction > 0) ? components::GuideManager::GuideDirection::EAST : 
                         components::GuideManager::GuideDirection::WEST;
    
    return guideManager_->guidePulse(guideDir, std::chrono::milliseconds(duration));
}

bool INDITelescopeController::guidePulse(double ra_ms, double dec_ms) {
    if (!validateController()) {
        return false;
    }
    
    return guideManager_->guidePulse(ra_ms, dec_ms);
}

bool INDITelescopeController::startMotion(MotionNS nsDirection, MotionEW ewDirection) {
    if (!validateController()) {
        return false;
    }
    
    return motionController_->startDirectionalMove(nsDirection, ewDirection);
}

bool INDITelescopeController::stopMotion(MotionNS nsDirection, MotionEW ewDirection) {
    if (!validateController()) {
        return false;
    }
    
    return motionController_->stopDirectionalMove(nsDirection, ewDirection);
}

bool INDITelescopeController::setSlewRate(double speed) {
    if (!validateController()) {
        return false;
    }
    
    return motionController_->setSlewRate(speed);
}

std::optional<double> INDITelescopeController::getSlewRate() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    return motionController_->getCurrentSlewSpeed();
}

std::vector<double> INDITelescopeController::getSlewRates() {
    if (!validateController()) {
        return {};
    }
    
    return motionController_->getAvailableSlewRates();
}

bool INDITelescopeController::setSlewRateIndex(int index) {
    if (!validateController()) {
        return false;
    }
    
    auto rates = motionController_->getAvailableSlewRates();
    if (index >= 0 && index < static_cast<int>(rates.size())) {
        return motionController_->setSlewRate(rates[index]);
    }
    return false;
}

std::optional<PierSide> INDITelescopeController::getPierSide() {
    if (!validateController()) {
        return std::nullopt;
    }
    
    // This would typically come from INDI properties
    return PierSide::UNKNOWN;
}

bool INDITelescopeController::setPierSide(PierSide side) {
    if (!validateController()) {
        return false;
    }
    
    // This would typically set INDI properties
    return true;
}

bool INDITelescopeController::initializeHome(std::string_view command) {
    if (!validateController()) {
        return false;
    }
    
    // This would typically send initialization command via INDI
    return true;
}

bool INDITelescopeController::findHome() {
    if (!validateController()) {
        return false;
    }
    
    // This would typically start home finding procedure
    return true;
}

bool INDITelescopeController::setHome() {
    if (!validateController()) {
        return false;
    }
    
    // This would typically set current position as home
    return true;
}

bool INDITelescopeController::gotoHome() {
    if (!validateController()) {
        return false;
    }
    
    // This would typically slew to home position
    return true;
}

AlignmentMode INDITelescopeController::getAlignmentMode() {
    if (!validateController()) {
        return AlignmentMode::EQ_NORTH_POLE;
    }
    
    return coordinateManager_->getAlignmentMode();
}

bool INDITelescopeController::setAlignmentMode(AlignmentMode mode) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->setAlignmentMode(mode);
}

bool INDITelescopeController::addAlignmentPoint(const EquatorialCoordinates& measured,
                                              const EquatorialCoordinates& target) {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->addAlignmentPoint(measured, target);
}

bool INDITelescopeController::clearAlignment() {
    if (!validateController()) {
        return false;
    }
    
    return coordinateManager_->clearAlignment();
}

std::tuple<int, int, double> INDITelescopeController::degreesToDMS(double degrees) {
    return coordinateManager_->degreesToDMS(degrees);
}

std::tuple<int, int, double> INDITelescopeController::degreesToHMS(double degrees) {
    return coordinateManager_->degreesToHMS(degrees);
}

std::string INDITelescopeController::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

// Private methods
bool INDITelescopeController::initializeComponents() {
    try {
        // Create components
        hardware_ = std::make_shared<components::HardwareInterface>();
        motionController_ = std::make_shared<components::MotionController>(hardware_);
        trackingManager_ = std::make_shared<components::TrackingManager>(hardware_);
        parkingManager_ = std::make_shared<components::ParkingManager>(hardware_);
        coordinateManager_ = std::make_shared<components::CoordinateManager>(hardware_);
        guideManager_ = std::make_shared<components::GuideManager>(hardware_);
        
        // Initialize each component
        if (!hardware_->initialize()) {
            logError("Failed to initialize hardware interface");
            return false;
        }
        
        if (!motionController_->initialize()) {
            logError("Failed to initialize motion controller");
            return false;
        }
        
        if (!trackingManager_->initialize()) {
            logError("Failed to initialize tracking manager");
            return false;
        }
        
        if (!parkingManager_->initialize()) {
            logError("Failed to initialize parking manager");
            return false;
        }
        
        if (!coordinateManager_->initialize()) {
            logError("Failed to initialize coordinate manager");
            return false;
        }
        
        if (!guideManager_->initialize()) {
            logError("Failed to initialize guide manager");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception initializing components: " + std::string(e.what()));
        return false;
    }
}

bool INDITelescopeController::shutdownComponents() {
    bool allSuccess = true;
    
    if (guideManager_) {
        if (!guideManager_->shutdown()) {
            logWarning("Guide manager shutdown failed");
            allSuccess = false;
        }
        guideManager_.reset();
    }
    
    if (coordinateManager_) {
        if (!coordinateManager_->shutdown()) {
            logWarning("Coordinate manager shutdown failed");
            allSuccess = false;
        }
        coordinateManager_.reset();
    }
    
    if (parkingManager_) {
        if (!parkingManager_->shutdown()) {
            logWarning("Parking manager shutdown failed");
            allSuccess = false;
        }
        parkingManager_.reset();
    }
    
    if (trackingManager_) {
        if (!trackingManager_->shutdown()) {
            logWarning("Tracking manager shutdown failed");
            allSuccess = false;
        }
        trackingManager_.reset();
    }
    
    if (motionController_) {
        if (!motionController_->shutdown()) {
            logWarning("Motion controller shutdown failed");
            allSuccess = false;
        }
        motionController_.reset();
    }
    
    if (hardware_) {
        if (!hardware_->shutdown()) {
            logWarning("Hardware interface shutdown failed");
            allSuccess = false;
        }
        hardware_.reset();
    }
    
    return allSuccess;
}

void INDITelescopeController::setupComponentCallbacks() {
    if (hardware_) {
        hardware_->setConnectionCallback([this](bool connected) {
            if (!connected) {
                connected_.store(false);
            }
        });
        
        hardware_->setMessageCallback([this](const std::string& message, int messageID) {
            logInfo("Hardware message: " + message);
        });
    }
    
    if (motionController_) {
        motionController_->setMotionCompleteCallback([this](bool success, const std::string& message) {
            if (!success) {
                setLastError("Motion failed: " + message);
            }
        });
    }
}

void INDITelescopeController::coordinateComponentStates() {
    // Synchronize component states after connection
    if (!connected_.load()) {
        return;
    }
    
    try {
        // Update coordinate manager with current position
        coordinateManager_->updateCoordinateStatus();
        
        // Update tracking state
        trackingManager_->updateTrackingStatus();
        
        // Update parking state
        parkingManager_->updateParkingStatus();
        
        // Update motion state
        motionController_->updateMotionStatus();
        
    } catch (const std::exception& e) {
        logWarning("Failed to coordinate component states: " + std::string(e.what()));
    }
}

void INDITelescopeController::validateComponentDependencies() {
    if (!hardware_) {
        throw std::runtime_error("Hardware interface is required");
    }
    
    if (!motionController_) {
        throw std::runtime_error("Motion controller is required");
    }
    
    if (!trackingManager_) {
        throw std::runtime_error("Tracking manager is required");
    }
    
    if (!coordinateManager_) {
        throw std::runtime_error("Coordinate manager is required");
    }
}

bool INDITelescopeController::validateController() const {
    if (!initialized_.load()) {
        setLastError("Controller not initialized");
        return false;
    }
    
    if (!connected_.load()) {
        setLastError("Controller not connected");
        return false;
    }
    
    if (!hardware_ || !motionController_ || !trackingManager_ || 
        !parkingManager_ || !coordinateManager_ || !guideManager_) {
        setLastError("Required components not available");
        return false;
    }
    
    return true;
}

void INDITelescopeController::setLastError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
    logError(error);
}

void INDITelescopeController::clearLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

void INDITelescopeController::logInfo(const std::string& message) {
    spdlog::info("[INDITelescopeController] {}", message);
}

void INDITelescopeController::logWarning(const std::string& message) {
    spdlog::warn("[INDITelescopeController] {}", message);
}

void INDITelescopeController::logError(const std::string& message) {
    spdlog::error("[INDITelescopeController] {}", message);
}

} // namespace lithium::device::indi::telescope
