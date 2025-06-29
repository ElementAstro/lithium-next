/*
 * telescope_modular.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "telescope_modular.hpp"
#include "telescope/controller_factory.hpp"
#include <iostream>

namespace lithium::device::indi {

INDITelescopeModular::INDITelescopeModular(const std::string& name)
    : telescopeName_(name) {

    // Create the modular controller
    controller_ = telescope::ControllerFactory::createModularController(
        telescope::ControllerFactory::getDefaultConfig());
}

bool INDITelescopeModular::initialize() {
    if (!controller_) {
        logError("Controller not created");
        return false;
    }

    if (!controller_->initialize()) {
        logError("Failed to initialize modular controller: " + controller_->getLastError());
        return false;
    }

    logInfo("Modular telescope initialized successfully");
    return true;
}

bool INDITelescopeModular::destroy() {
    if (!controller_) {
        return true;
    }

    bool result = controller_->destroy();
    if (result) {
        logInfo("Modular telescope destroyed successfully");
    } else {
        logError("Failed to destroy modular controller: " + controller_->getLastError());
    }

    return result;
}

bool INDITelescopeModular::connect(const std::string& deviceName, int timeout, int maxRetry) {
    if (!controller_) {
        logError("Controller not available");
        return false;
    }

    bool result = controller_->connect(deviceName, timeout, maxRetry);
    if (result) {
        logInfo("Connected to telescope: " + deviceName);
    } else {
        logError("Failed to connect to telescope: " + controller_->getLastError());
    }

    return result;
}

bool INDITelescopeModular::disconnect() {
    if (!controller_) {
        return true;
    }

    bool result = controller_->disconnect();
    if (result) {
        logInfo("Disconnected from telescope");
    } else {
        logError("Failed to disconnect: " + controller_->getLastError());
    }

    return result;
}

std::vector<std::string> INDITelescopeModular::scan() {
    if (!controller_) {
        logError("Controller not available");
        return {};
    }

    auto devices = controller_->scan();
    logInfo("Found " + std::to_string(devices.size()) + " telescope devices");

    return devices;
}

bool INDITelescopeModular::isConnected() const {
    return controller_ && controller_->isConnected();
}

// Delegate all methods to the modular controller
auto INDITelescopeModular::getTelescopeInfo() -> std::optional<TelescopeParameters> {
    return controller_ ? controller_->getTelescopeInfo() : std::nullopt;
}

auto INDITelescopeModular::setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                                           double guiderAperture, double guiderFocal) -> bool {
    return controller_ ? controller_->setTelescopeInfo(telescopeAperture, telescopeFocal,
                                                      guiderAperture, guiderFocal) : false;
}

auto INDITelescopeModular::getStatus() -> std::optional<std::string> {
    return controller_ ? controller_->getStatus() : std::nullopt;
}

auto INDITelescopeModular::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    return controller_ ? controller_->slewToRADECJNow(raHours, decDegrees, enableTracking) : false;
}

auto INDITelescopeModular::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    return controller_ ? controller_->syncToRADECJNow(raHours, decDegrees) : false;
}

auto INDITelescopeModular::slewToAZALT(double azDegrees, double altDegrees) -> bool {
    return controller_ ? controller_->slewToAZALT(azDegrees, altDegrees) : false;
}

auto INDITelescopeModular::abortMotion() -> bool {
    return controller_ ? controller_->abortMotion() : false;
}

auto INDITelescopeModular::emergencyStop() -> bool {
    return controller_ ? controller_->emergencyStop() : false;
}

auto INDITelescopeModular::isMoving() -> bool {
    return controller_ ? controller_->isMoving() : false;
}

auto INDITelescopeModular::startMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool {
    return controller_ ? controller_->startMotion(nsDirection, ewDirection) : false;
}

auto INDITelescopeModular::stopMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool {
    return controller_ ? controller_->stopMotion(nsDirection, ewDirection) : false;
}

auto INDITelescopeModular::enableTracking(bool enable) -> bool {
    return controller_ ? controller_->enableTracking(enable) : false;
}

auto INDITelescopeModular::isTrackingEnabled() -> bool {
    return controller_ ? controller_->isTrackingEnabled() : false;
}

auto INDITelescopeModular::setTrackRate(TrackMode rate) -> bool {
    return controller_ ? controller_->setTrackRate(rate) : false;
}

auto INDITelescopeModular::getTrackRate() -> std::optional<TrackMode> {
    return controller_ ? controller_->getTrackRate() : std::nullopt;
}

auto INDITelescopeModular::setTrackRates(const MotionRates& rates) -> bool {
    return controller_ ? controller_->setTrackRates(rates) : false;
}

auto INDITelescopeModular::getTrackRates() -> MotionRates {
    return controller_ ? controller_->getTrackRates() : MotionRates{};
}

auto INDITelescopeModular::park() -> bool {
    return controller_ ? controller_->park() : false;
}

auto INDITelescopeModular::unpark() -> bool {
    return controller_ ? controller_->unpark() : false;
}

auto INDITelescopeModular::isParked() -> bool {
    return controller_ ? controller_->isParked() : false;
}

auto INDITelescopeModular::canPark() -> bool {
    return controller_ ? controller_->canPark() : false;
}

auto INDITelescopeModular::setParkPosition(double parkRA, double parkDEC) -> bool {
    return controller_ ? controller_->setParkPosition(parkRA, parkDEC) : false;
}

auto INDITelescopeModular::getParkPosition() -> std::optional<EquatorialCoordinates> {
    return controller_ ? controller_->getParkPosition() : std::nullopt;
}

auto INDITelescopeModular::setParkOption(ParkOptions option) -> bool {
    return controller_ ? controller_->setParkOption(option) : false;
}

auto INDITelescopeModular::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    return controller_ ? controller_->getRADECJ2000() : std::nullopt;
}

auto INDITelescopeModular::setRADECJ2000(double raHours, double decDegrees) -> bool {
    return controller_ ? controller_->setRADECJ2000(raHours, decDegrees) : false;
}

auto INDITelescopeModular::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    return controller_ ? controller_->getRADECJNow() : std::nullopt;
}

auto INDITelescopeModular::setRADECJNow(double raHours, double decDegrees) -> bool {
    return controller_ ? controller_->setRADECJNow(raHours, decDegrees) : false;
}

auto INDITelescopeModular::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    return controller_ ? controller_->getTargetRADECJNow() : std::nullopt;
}

auto INDITelescopeModular::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    return controller_ ? controller_->setTargetRADECJNow(raHours, decDegrees) : false;
}

auto INDITelescopeModular::getAZALT() -> std::optional<HorizontalCoordinates> {
    return controller_ ? controller_->getAZALT() : std::nullopt;
}

auto INDITelescopeModular::setAZALT(double azDegrees, double altDegrees) -> bool {
    return controller_ ? controller_->setAZALT(azDegrees, altDegrees) : false;
}

auto INDITelescopeModular::getLocation() -> std::optional<GeographicLocation> {
    return controller_ ? controller_->getLocation() : std::nullopt;
}

auto INDITelescopeModular::setLocation(const GeographicLocation& location) -> bool {
    return controller_ ? controller_->setLocation(location) : false;
}

auto INDITelescopeModular::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> {
    return controller_ ? controller_->getUTCTime() : std::nullopt;
}

auto INDITelescopeModular::setUTCTime(const std::chrono::system_clock::time_point& time) -> bool {
    return controller_ ? controller_->setUTCTime(time) : false;
}

auto INDITelescopeModular::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> {
    return controller_ ? controller_->getLocalTime() : std::nullopt;
}

auto INDITelescopeModular::guideNS(int direction, int duration) -> bool {
    return controller_ ? controller_->guideNS(direction, duration) : false;
}

auto INDITelescopeModular::guideEW(int direction, int duration) -> bool {
    return controller_ ? controller_->guideEW(direction, duration) : false;
}

auto INDITelescopeModular::guidePulse(double ra_ms, double dec_ms) -> bool {
    return controller_ ? controller_->guidePulse(ra_ms, dec_ms) : false;
}

auto INDITelescopeModular::setSlewRate(double speed) -> bool {
    return controller_ ? controller_->setSlewRate(speed) : false;
}

auto INDITelescopeModular::getSlewRate() -> std::optional<double> {
    return controller_ ? controller_->getSlewRate() : std::nullopt;
}

auto INDITelescopeModular::getSlewRates() -> std::vector<double> {
    return controller_ ? controller_->getSlewRates() : std::vector<double>{};
}

auto INDITelescopeModular::setSlewRateIndex(int index) -> bool {
    return controller_ ? controller_->setSlewRateIndex(index) : false;
}

auto INDITelescopeModular::getPierSide() -> std::optional<PierSide> {
    return controller_ ? controller_->getPierSide() : std::nullopt;
}

auto INDITelescopeModular::setPierSide(PierSide side) -> bool {
    return controller_ ? controller_->setPierSide(side) : false;
}

auto INDITelescopeModular::initializeHome(std::string_view command) -> bool {
    return controller_ ? controller_->initializeHome(command) : false;
}

auto INDITelescopeModular::findHome() -> bool {
    return controller_ ? controller_->findHome() : false;
}

auto INDITelescopeModular::setHome() -> bool {
    return controller_ ? controller_->setHome() : false;
}

auto INDITelescopeModular::gotoHome() -> bool {
    return controller_ ? controller_->gotoHome() : false;
}

auto INDITelescopeModular::getAlignmentMode() -> AlignmentMode {
    return controller_ ? controller_->getAlignmentMode() : AlignmentMode::EQ_NORTH_POLE;
}

auto INDITelescopeModular::setAlignmentMode(AlignmentMode mode) -> bool {
    return controller_ ? controller_->setAlignmentMode(mode) : false;
}

auto INDITelescopeModular::addAlignmentPoint(const EquatorialCoordinates& measured,
                                            const EquatorialCoordinates& target) -> bool {
    return controller_ ? controller_->addAlignmentPoint(measured, target) : false;
}

auto INDITelescopeModular::clearAlignment() -> bool {
    return controller_ ? controller_->clearAlignment() : false;
}

auto INDITelescopeModular::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    return controller_ ? controller_->degreesToDMS(degrees) : std::make_tuple(0, 0, 0.0);
}

auto INDITelescopeModular::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    return controller_ ? controller_->degreesToHMS(degrees) : std::make_tuple(0, 0, 0.0);
}

// Additional modular features
bool INDITelescopeModular::configureController(const telescope::TelescopeControllerConfig& config) {
    if (!controller_) {
        logError("Controller not available");
        return false;
    }

    // For now, this would require recreating the controller with new config
    // In a full implementation, we would add a reconfigure method to the controller
    logWarning("Controller reconfiguration not yet implemented");
    return false;
}

std::string INDITelescopeModular::getLastError() const {
    return controller_ ? controller_->getLastError() : "Controller not available";
}

bool INDITelescopeModular::resetToDefaults() {
    if (!controller_) {
        logError("Controller not available");
        return false;
    }

    // Implementation would reset all components to default settings
    logInfo("Reset to defaults requested");
    return true;
}

void INDITelescopeModular::setDebugMode(bool enable) {
    debugMode_ = enable;
    if (enable) {
        logInfo("Debug mode enabled");
    } else {
        logInfo("Debug mode disabled");
    }
}

// Private helper methods
void INDITelescopeModular::logInfo(const std::string& message) const {
    if (debugMode_) {
        std::cout << "[INFO] " << telescopeName_ << ": " << message << std::endl;
    }
}

void INDITelescopeModular::logWarning(const std::string& message) const {
    std::cout << "[WARNING] " << telescopeName_ << ": " << message << std::endl;
}

void INDITelescopeModular::logError(const std::string& message) const {
    std::cerr << "[ERROR] " << telescopeName_ << ": " << message << std::endl;
}

} // namespace lithium::device::indi
