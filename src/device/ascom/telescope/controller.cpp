/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Telescope Controller Implementation

This modular controller orchestrates the telescope components to provide
a clean, maintainable, and testable interface for ASCOM telescope control.

*************************************************/

#include "controller.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::ascom::telescope {

ASCOMTelescopeController::ASCOMTelescopeController(const std::string& name)
    : AtomTelescope(name) {
    spdlog::info("Creating ASCOM Telescope Controller: {}", name);
}

ASCOMTelescopeController::~ASCOMTelescopeController() {
    spdlog::info("Destroying ASCOM Telescope Controller");
    if (telescope_) {
        telescope_->shutdown();
    }
}

// =========================================================================
// AtomTelescope Interface Implementation
// =========================================================================

auto ASCOMTelescopeController::initialize() -> bool {
    try {
        telescope_ = std::make_unique<ASCOMTelescopeMain>();
        bool success = telescope_->initialize();
        
        if (success) {
            spdlog::info("ASCOM Telescope Controller initialized successfully");
        } else {
            logError("initialize", telescope_->getLastError());
        }
        
        return success;
    } catch (const std::exception& e) {
        logError("initialize", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::destroy() -> bool {
    try {
        if (!telescope_) {
            return true;
        }
        
        bool success = telescope_->shutdown();
        telescope_.reset();
        
        if (success) {
            spdlog::info("ASCOM Telescope Controller destroyed successfully");
        } else {
            spdlog::error("Failed to destroy ASCOM Telescope Controller");
        }
        
        return success;
    } catch (const std::exception& e) {
        logError("destroy", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    if (!telescope_) {
        logError("connect", "Telescope not initialized");
        return false;
    }
    
    try {
        return telescope_->connect(deviceName, timeout, maxRetry);
    } catch (const std::exception& e) {
        logError("connect", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::disconnect() -> bool {
    if (!telescope_) {
        return true;
    }
    
    try {
        return telescope_->disconnect();
    } catch (const std::exception& e) {
        logError("disconnect", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::scan() -> std::vector<std::string> {
    if (!telescope_) {
        logError("scan", "Telescope not initialized");
        return {};
    }
    
    try {
        return telescope_->scanDevices();
    } catch (const std::exception& e) {
        logError("scan", e.what());
        return {};
    }
}

auto ASCOMTelescopeController::isConnected() const -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->isConnected();
    } catch (const std::exception& e) {
        return false;
    }
}

// =========================================================================
// Telescope Information
// =========================================================================

auto ASCOMTelescopeController::getTelescopeInfo() -> std::optional<TelescopeParameters> {
    if (!telescope_) {
        return std::nullopt;
    }
    
    try {
        return telescope_->getTelescopeInfo();
    } catch (const std::exception& e) {
        logError("getTelescopeInfo", e.what());
        return std::nullopt;
    }
}

auto ASCOMTelescopeController::setTelescopeInfo(double aperture, double focalLength,
                                              double guiderAperture, double guiderFocalLength) -> bool {
    // This would typically be handled by the hardware interface
    // For now, return true as this is usually read-only for ASCOM telescopes
    return true;
}

// =========================================================================
// Pier Side (Placeholder implementations)
// =========================================================================

auto ASCOMTelescopeController::getPierSide() -> std::optional<PierSide> {
    // TODO: Implement pier side detection
    return PierSide::PIER_EAST; // Default
}

auto ASCOMTelescopeController::setPierSide(PierSide side) -> bool {
    // TODO: Implement pier side setting
    return true;
}

// =========================================================================
// Tracking
// =========================================================================

auto ASCOMTelescopeController::getTrackRate() -> std::optional<TrackMode> {
    if (!telescope_) {
        return std::nullopt;
    }
    
    try {
        return telescope_->getTrackingRate();
    } catch (const std::exception& e) {
        logError("getTrackRate", e.what());
        return std::nullopt;
    }
}

auto ASCOMTelescopeController::setTrackRate(TrackMode rate) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->setTrackingRate(rate);
    } catch (const std::exception& e) {
        logError("setTrackRate", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::isTrackingEnabled() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->isTracking();
    } catch (const std::exception& e) {
        logError("isTrackingEnabled", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::enableTracking(bool enable) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->setTracking(enable);
    } catch (const std::exception& e) {
        logError("enableTracking", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::getTrackRates() -> MotionRates {
    // Return default motion rates
    MotionRates rates;
    rates.ra_rate = 15.041067; // Sidereal rate in arcsec/sec
    rates.dec_rate = 0.0;
    return rates;
}

auto ASCOMTelescopeController::setTrackRates(const MotionRates& rates) -> bool {
    // TODO: Implement track rates setting
    return true;
}

// =========================================================================
// Motion Control
// =========================================================================

auto ASCOMTelescopeController::abortMotion() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->abortSlew();
    } catch (const std::exception& e) {
        logError("abortMotion", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::getStatus() -> std::optional<std::string> {
    if (!telescope_) {
        return "Disconnected";
    }
    
    try {
        switch (telescope_->getState()) {
            case TelescopeState::DISCONNECTED: return "Disconnected";
            case TelescopeState::CONNECTED: return "Connected";
            case TelescopeState::IDLE: return "Idle";
            case TelescopeState::SLEWING: return "Slewing";
            case TelescopeState::TRACKING: return "Tracking";
            case TelescopeState::PARKED: return "Parked";
            case TelescopeState::HOMING: return "Homing";
            case TelescopeState::GUIDING: return "Guiding";
            case TelescopeState::ERROR: return "Error";
            default: return "Unknown";
        }
    } catch (const std::exception& e) {
        logError("getStatus", e.what());
        return "Error";
    }
}

auto ASCOMTelescopeController::emergencyStop() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->emergencyStop();
    } catch (const std::exception& e) {
        logError("emergencyStop", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::isMoving() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->isSlewing();
    } catch (const std::exception& e) {
        logError("isMoving", e.what());
        return false;
    }
}

// =========================================================================
// Parking
// =========================================================================

auto ASCOMTelescopeController::setParkOption(ParkOptions option) -> bool {
    // TODO: Implement park options
    return true;
}

auto ASCOMTelescopeController::getParkPosition() -> std::optional<EquatorialCoordinates> {
    // TODO: Implement park position retrieval
    EquatorialCoordinates coords;
    coords.ra = 0.0;
    coords.dec = 90.0; // Default to celestial pole
    return coords;
}

auto ASCOMTelescopeController::setParkPosition(double ra, double dec) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->setParkPosition(ra, dec);
    } catch (const std::exception& e) {
        logError("setParkPosition", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::isParked() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->isParked();
    } catch (const std::exception& e) {
        logError("isParked", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::park() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->park();
    } catch (const std::exception& e) {
        logError("park", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::unpark() -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->unpark();
    } catch (const std::exception& e) {
        logError("unpark", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::canPark() -> bool {
    // TODO: Check if telescope supports parking
    return true;
}

// =========================================================================
// Home Position
// =========================================================================

auto ASCOMTelescopeController::initializeHome(std::string_view command) -> bool {
    // TODO: Implement home initialization
    return true;
}

auto ASCOMTelescopeController::findHome() -> bool {
    // TODO: Implement home finding
    return true;
}

auto ASCOMTelescopeController::setHome() -> bool {
    // TODO: Implement home setting
    return true;
}

auto ASCOMTelescopeController::gotoHome() -> bool {
    // TODO: Implement goto home
    return true;
}

// =========================================================================
// Slew Rates
// =========================================================================

auto ASCOMTelescopeController::getSlewRate() -> std::optional<double> {
    // TODO: Implement slew rate retrieval
    return 1.0; // Default rate
}

auto ASCOMTelescopeController::setSlewRate(double speed) -> bool {
    // TODO: Implement slew rate setting
    return true;
}

auto ASCOMTelescopeController::getSlewRates() -> std::vector<double> {
    // Return default slew rates
    return {0.1, 0.5, 1.0, 2.0, 5.0};
}

auto ASCOMTelescopeController::setSlewRateIndex(int index) -> bool {
    // TODO: Implement slew rate index setting
    return true;
}

// =========================================================================
// Directional Movement
// =========================================================================

auto ASCOMTelescopeController::getMoveDirectionEW() -> std::optional<MotionEW> {
    return MotionEW::MOTION_EAST; // Default
}

auto ASCOMTelescopeController::setMoveDirectionEW(MotionEW direction) -> bool {
    return true;
}

auto ASCOMTelescopeController::getMoveDirectionNS() -> std::optional<MotionNS> {
    return MotionNS::MOTION_NORTH; // Default
}

auto ASCOMTelescopeController::setMoveDirectionNS(MotionNS direction) -> bool {
    return true;
}

auto ASCOMTelescopeController::startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        // Convert motion directions to strings
        std::string ns_dir = (ns_direction == MotionNS::MOTION_NORTH) ? "N" : "S";
        std::string ew_dir = (ew_direction == MotionEW::MOTION_EAST) ? "E" : "W";
        
        // Start movements with default rate
        bool success = true;
        if (ns_direction != MotionNS::MOTION_STOP) {
            success &= telescope_->startDirectionalMove(ns_dir, 1.0);
        }
        if (ew_direction != MotionEW::MOTION_STOP) {
            success &= telescope_->startDirectionalMove(ew_dir, 1.0);
        }
        
        return success;
    } catch (const std::exception& e) {
        logError("startMotion", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        // Convert motion directions to strings
        std::string ns_dir = (ns_direction == MotionNS::MOTION_NORTH) ? "N" : "S";
        std::string ew_dir = (ew_direction == MotionEW::MOTION_EAST) ? "E" : "W";
        
        // Stop movements
        bool success = true;
        success &= telescope_->stopDirectionalMove(ns_dir);
        success &= telescope_->stopDirectionalMove(ew_dir);
        
        return success;
    } catch (const std::exception& e) {
        logError("stopMotion", e.what());
        return false;
    }
}

// =========================================================================
// Guiding
// =========================================================================

auto ASCOMTelescopeController::guideNS(int direction, int duration) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        std::string dir = (direction > 0) ? "N" : "S";
        return telescope_->guidePulse(dir, duration);
    } catch (const std::exception& e) {
        logError("guideNS", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::guideEW(int direction, int duration) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        std::string dir = (direction > 0) ? "E" : "W";
        return telescope_->guidePulse(dir, duration);
    } catch (const std::exception& e) {
        logError("guideEW", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::guidePulse(double ra_ms, double dec_ms) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->guideRADEC(ra_ms, dec_ms);
    } catch (const std::exception& e) {
        logError("guidePulse", e.what());
        return false;
    }
}

// =========================================================================
// Coordinate Systems
// =========================================================================

auto ASCOMTelescopeController::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    // TODO: Implement J2000 coordinate retrieval
    return getCurrentRADEC();
}

auto ASCOMTelescopeController::setRADECJ2000(double raHours, double decDegrees) -> bool {
    // TODO: Implement J2000 coordinate setting
    return slewToRADECJNow(raHours, decDegrees);
}

auto ASCOMTelescopeController::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    return getCurrentRADEC();
}

auto ASCOMTelescopeController::setRADECJNow(double raHours, double decDegrees) -> bool {
    return slewToRADECJNow(raHours, decDegrees);
}

auto ASCOMTelescopeController::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    // TODO: Implement target coordinate retrieval
    return getCurrentRADEC();
}

auto ASCOMTelescopeController::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    // Setting target is typically done via slewing
    return slewToRADECJNow(raHours, decDegrees);
}

auto ASCOMTelescopeController::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->slewToRADEC(raHours, decDegrees, enableTracking);
    } catch (const std::exception& e) {
        logError("slewToRADECJNow", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->syncToRADEC(raHours, decDegrees);
    } catch (const std::exception& e) {
        logError("syncToRADECJNow", e.what());
        return false;
    }
}

auto ASCOMTelescopeController::getAZALT() -> std::optional<HorizontalCoordinates> {
    if (!telescope_) {
        return std::nullopt;
    }
    
    try {
        return telescope_->getCurrentAZALT();
    } catch (const std::exception& e) {
        logError("getAZALT", e.what());
        return std::nullopt;
    }
}

auto ASCOMTelescopeController::setAZALT(double azDegrees, double altDegrees) -> bool {
    return slewToAZALT(azDegrees, altDegrees);
}

auto ASCOMTelescopeController::slewToAZALT(double azDegrees, double altDegrees) -> bool {
    if (!telescope_) {
        return false;
    }
    
    try {
        return telescope_->slewToAZALT(azDegrees, altDegrees);
    } catch (const std::exception& e) {
        logError("slewToAZALT", e.what());
        return false;
    }
}

// =========================================================================
// Location and Time
// =========================================================================

auto ASCOMTelescopeController::getLocation() -> std::optional<GeographicLocation> {
    // TODO: Implement location retrieval
    GeographicLocation loc;
    loc.latitude = 40.0; // Default to somewhere reasonable
    loc.longitude = -74.0;
    loc.elevation = 100.0;
    return loc;
}

auto ASCOMTelescopeController::setLocation(const GeographicLocation& location) -> bool {
    // TODO: Implement location setting
    return true;
}

auto ASCOMTelescopeController::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> {
    return std::chrono::system_clock::now();
}

auto ASCOMTelescopeController::setUTCTime(const std::chrono::system_clock::time_point& time) -> bool {
    // TODO: Implement time setting
    return true;
}

auto ASCOMTelescopeController::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> {
    return std::chrono::system_clock::now();
}

// =========================================================================
// Alignment
// =========================================================================

auto ASCOMTelescopeController::getAlignmentMode() -> AlignmentMode {
    return AlignmentMode::POLAR; // Default
}

auto ASCOMTelescopeController::setAlignmentMode(AlignmentMode mode) -> bool {
    // TODO: Implement alignment mode setting
    return true;
}

auto ASCOMTelescopeController::addAlignmentPoint(const EquatorialCoordinates& measured,
                                               const EquatorialCoordinates& target) -> bool {
    // TODO: Implement alignment point addition
    return true;
}

auto ASCOMTelescopeController::clearAlignment() -> bool {
    // TODO: Implement alignment clearing
    return true;
}

// =========================================================================
// Utility Methods
// =========================================================================

auto ASCOMTelescopeController::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    int d = static_cast<int>(degrees);
    double remainder = (degrees - d) * 60.0;
    int m = static_cast<int>(remainder);
    double s = (remainder - m) * 60.0;
    return {d, m, s};
}

auto ASCOMTelescopeController::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    double hours = degrees / 15.0; // Convert degrees to hours
    int h = static_cast<int>(hours);
    double remainder = (hours - h) * 60.0;
    int m = static_cast<int>(remainder);
    double s = (remainder - m) * 60.0;
    return {h, m, s};
}

// =========================================================================
// Private Helper Methods
// =========================================================================

std::optional<EquatorialCoordinates> ASCOMTelescopeController::getCurrentRADEC() {
    if (!telescope_) {
        return std::nullopt;
    }
    
    try {
        return telescope_->getCurrentRADEC();
    } catch (const std::exception& e) {
        logError("getCurrentRADEC", e.what());
        return std::nullopt;
    }
}

void ASCOMTelescopeController::logError(const std::string& operation, const std::string& error) const {
    spdlog::error("ASCOM Telescope Controller [{}]: {}", operation, error);
}

bool ASCOMTelescopeController::validateParameters(const std::string& operation, 
                                                std::function<bool()> validator) const {
    try {
        return validator();
    } catch (const std::exception& e) {
        logError(operation, std::string("Parameter validation failed: ") + e.what());
        return false;
    }
}

} // namespace lithium::device::ascom::telescope
