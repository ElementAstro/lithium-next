#include "manager.hpp"

INDITelescopeManager::INDITelescopeManager(std::string name) 
    : AtomTelescope(std::move(name)), name_(getName()) {
    spdlog::info("Creating INDI telescope manager: {}", name_);
    
    // Create component instances
    connection_ = std::make_shared<TelescopeConnection>(name_);
    motion_ = std::make_shared<TelescopeMotion>(name_);
    tracking_ = std::make_shared<TelescopeTracking>(name_);
    coordinates_ = std::make_shared<TelescopeCoordinates>(name_);
    parking_ = std::make_shared<TelescopeParking>(name_);
    
    spdlog::debug("All telescope components created for {}", name_);
}

auto INDITelescopeManager::initialize() -> bool {
    if (initialized_.load()) {
        spdlog::warn("Telescope manager {} already initialized", name_);
        return true;
    }
    
    spdlog::info("Initializing telescope manager: {}", name_);
    
    if (!initializeComponents()) {
        spdlog::error("Failed to initialize telescope components");
        return false;
    }
    
    initialized_.store(true);
    updateTelescopeState(TelescopeState::IDLE);
    
    spdlog::info("Telescope manager {} initialized successfully", name_);
    return true;
}

auto INDITelescopeManager::destroy() -> bool {
    spdlog::info("Destroying telescope manager: {}", name_);
    
    if (isConnected()) {
        disconnect();
    }
    
    destroyComponents();
    initialized_.store(false);
    
    spdlog::info("Telescope manager {} destroyed", name_);
    return true;
}

auto INDITelescopeManager::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    if (!initialized_.load()) {
        spdlog::error("Telescope manager not initialized");
        return false;
    }
    
    spdlog::info("Connecting telescope manager {} to device: {}", name_, deviceName);
    
    // Connect using the connection component
    if (!connection_->connect(deviceName, timeout, maxRetry)) {
        spdlog::error("Failed to connect to telescope device: {}", deviceName);
        return false;
    }
    
    // Get the INDI device and initialize other components
    auto device = connection_->getDevice();
    if (!device.isValid()) {
        spdlog::error("Invalid device after connection");
        return false;
    }
    
    // Initialize components with the device
    motion_->initialize(device);
    tracking_->initialize(device);
    coordinates_->initialize(device);
    parking_->initialize(device);
    
    updateTelescopeState(TelescopeState::IDLE);
    spdlog::info("Telescope {} connected and components initialized", name_);
    return true;
}

auto INDITelescopeManager::disconnect() -> bool {
    spdlog::info("Disconnecting telescope manager: {}", name_);
    
    if (!connection_->disconnect()) {
        spdlog::error("Failed to disconnect telescope");
        return false;
    }
    
    updateTelescopeState(TelescopeState::IDLE);
    spdlog::info("Telescope {} disconnected", name_);
    return true;
}

auto INDITelescopeManager::scan() -> std::vector<std::string> {
    return connection_->scan();
}

auto INDITelescopeManager::isConnected() const -> bool {
    return connection_->isConnected();
}

auto INDITelescopeManager::getTelescopeInfo() -> std::optional<TelescopeParameters> {
    if (!ensureConnected()) return std::nullopt;
    
    // Get telescope info from device or return stored parameters
    return telescopeParams_;
}

auto INDITelescopeManager::setTelescopeInfo(double aperture, double focalLength,
                                           double guiderAperture, double guiderFocalLength) -> bool {
    if (!ensureConnected()) return false;
    
    telescopeParams_.aperture = aperture;
    telescopeParams_.focalLength = focalLength;
    telescopeParams_.guiderAperture = guiderAperture;
    telescopeParams_.guiderFocalLength = guiderFocalLength;
    
    spdlog::info("Telescope info set: aperture={:.1f}mm, focal={:.1f}mm, guide_aperture={:.1f}mm, guide_focal={:.1f}mm",
                aperture, focalLength, guiderAperture, guiderFocalLength);
    return true;
}

auto INDITelescopeManager::getPierSide() -> std::optional<PierSide> {
    if (!ensureConnected()) return std::nullopt;
    return tracking_->getPierSide();
}

auto INDITelescopeManager::setPierSide(PierSide side) -> bool {
    if (!ensureConnected()) return false;
    return tracking_->setPierSide(side);
}

auto INDITelescopeManager::getTrackRate() -> std::optional<TrackMode> {
    if (!ensureConnected()) return std::nullopt;
    return tracking_->getTrackRate();
}

auto INDITelescopeManager::setTrackRate(TrackMode rate) -> bool {
    if (!ensureConnected()) return false;
    return tracking_->setTrackRate(rate);
}

auto INDITelescopeManager::isTrackingEnabled() -> bool {
    if (!ensureConnected()) return false;
    return tracking_->isTrackingEnabled();
}

auto INDITelescopeManager::enableTracking(bool enable) -> bool {
    if (!ensureConnected()) return false;
    return tracking_->enableTracking(enable);
}

auto INDITelescopeManager::getTrackRates() -> MotionRates {
    if (!ensureConnected()) return {};
    return tracking_->getTrackRates();
}

auto INDITelescopeManager::setTrackRates(const MotionRates& rates) -> bool {
    if (!ensureConnected()) return false;
    return tracking_->setTrackRates(rates);
}

auto INDITelescopeManager::abortMotion() -> bool {
    if (!ensureConnected()) return false;
    return motion_->abortMotion();
}

auto INDITelescopeManager::getStatus() -> std::optional<std::string> {
    if (!ensureConnected()) return std::nullopt;
    return motion_->getStatus();
}

auto INDITelescopeManager::emergencyStop() -> bool {
    if (!ensureConnected()) return false;
    return motion_->emergencyStop();
}

auto INDITelescopeManager::isMoving() -> bool {
    if (!ensureConnected()) return false;
    return motion_->isMoving();
}

auto INDITelescopeManager::setParkOption(ParkOptions option) -> bool {
    if (!ensureConnected()) return false;
    return parking_->setParkOption(option);
}

auto INDITelescopeManager::getParkPosition() -> std::optional<EquatorialCoordinates> {
    if (!ensureConnected()) return std::nullopt;
    return parking_->getParkPosition();
}

auto INDITelescopeManager::setParkPosition(double ra, double dec) -> bool {
    if (!ensureConnected()) return false;
    return parking_->setParkPosition(ra, dec);
}

auto INDITelescopeManager::isParked() -> bool {
    if (!ensureConnected()) return false;
    return parking_->isParked();
}

auto INDITelescopeManager::park() -> bool {
    if (!ensureConnected()) return false;
    updateTelescopeState(TelescopeState::PARKING);
    bool result = parking_->park();
    if (result) {
        updateTelescopeState(TelescopeState::PARKED);
    } else {
        updateTelescopeState(TelescopeState::ERROR);
    }
    return result;
}

auto INDITelescopeManager::unpark() -> bool {
    if (!ensureConnected()) return false;
    bool result = parking_->unpark();
    if (result) {
        updateTelescopeState(TelescopeState::IDLE);
    }
    return result;
}

auto INDITelescopeManager::canPark() -> bool {
    if (!ensureConnected()) return false;
    return parking_->canPark();
}

auto INDITelescopeManager::initializeHome(std::string_view command) -> bool {
    if (!ensureConnected()) return false;
    return parking_->initializeHome(command);
}

auto INDITelescopeManager::findHome() -> bool {
    if (!ensureConnected()) return false;
    return parking_->findHome();
}

auto INDITelescopeManager::setHome() -> bool {
    if (!ensureConnected()) return false;
    return parking_->setHome();
}

auto INDITelescopeManager::gotoHome() -> bool {
    if (!ensureConnected()) return false;
    return parking_->gotoHome();
}

auto INDITelescopeManager::getSlewRate() -> std::optional<double> {
    if (!ensureConnected()) return std::nullopt;
    return motion_->getSlewRate();
}

auto INDITelescopeManager::setSlewRate(double speed) -> bool {
    if (!ensureConnected()) return false;
    return motion_->setSlewRate(speed);
}

auto INDITelescopeManager::getSlewRates() -> std::vector<double> {
    if (!ensureConnected()) return {};
    return motion_->getSlewRates();
}

auto INDITelescopeManager::setSlewRateIndex(int index) -> bool {
    if (!ensureConnected()) return false;
    return motion_->setSlewRateIndex(index);
}

auto INDITelescopeManager::getMoveDirectionEW() -> std::optional<MotionEW> {
    if (!ensureConnected()) return std::nullopt;
    return motion_->getMoveDirectionEW();
}

auto INDITelescopeManager::setMoveDirectionEW(MotionEW direction) -> bool {
    if (!ensureConnected()) return false;
    return motion_->setMoveDirectionEW(direction);
}

auto INDITelescopeManager::getMoveDirectionNS() -> std::optional<MotionNS> {
    if (!ensureConnected()) return std::nullopt;
    return motion_->getMoveDirectionNS();
}

auto INDITelescopeManager::setMoveDirectionNS(MotionNS direction) -> bool {
    if (!ensureConnected()) return false;
    return motion_->setMoveDirectionNS(direction);
}

auto INDITelescopeManager::startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    if (!ensureConnected()) return false;
    updateTelescopeState(TelescopeState::SLEWING);
    return motion_->startMotion(ns_direction, ew_direction);
}

auto INDITelescopeManager::stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    if (!ensureConnected()) return false;
    bool result = motion_->stopMotion(ns_direction, ew_direction);
    if (result && !motion_->isMoving()) {
        updateTelescopeState(isTrackingEnabled() ? TelescopeState::TRACKING : TelescopeState::IDLE);
    }
    return result;
}

auto INDITelescopeManager::guideNS(int direction, int duration) -> bool {
    if (!ensureConnected()) return false;
    return motion_->guideNS(direction, duration);
}

auto INDITelescopeManager::guideEW(int direction, int duration) -> bool {
    if (!ensureConnected()) return false;
    return motion_->guideEW(direction, duration);
}

auto INDITelescopeManager::guidePulse(double ra_ms, double dec_ms) -> bool {
    if (!ensureConnected()) return false;
    return motion_->guidePulse(ra_ms, dec_ms);
}

auto INDITelescopeManager::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getRADECJ2000();
}

auto INDITelescopeManager::setRADECJ2000(double raHours, double decDegrees) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setRADECJ2000(raHours, decDegrees);
}

auto INDITelescopeManager::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getRADECJNow();
}

auto INDITelescopeManager::setRADECJNow(double raHours, double decDegrees) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setRADECJNow(raHours, decDegrees);
}

auto INDITelescopeManager::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getTargetRADECJNow();
}

auto INDITelescopeManager::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setTargetRADECJNow(raHours, decDegrees);
}

auto INDITelescopeManager::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    if (!ensureConnected()) return false;
    updateTelescopeState(TelescopeState::SLEWING);
    bool result = motion_->slewToRADECJNow(raHours, decDegrees, enableTracking);
    if (result) {
        updateTelescopeState(enableTracking ? TelescopeState::TRACKING : TelescopeState::IDLE);
    } else {
        updateTelescopeState(TelescopeState::ERROR);
    }
    return result;
}

auto INDITelescopeManager::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    if (!ensureConnected()) return false;
    return motion_->syncToRADECJNow(raHours, decDegrees);
}

auto INDITelescopeManager::getAZALT() -> std::optional<HorizontalCoordinates> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getAZALT();
}

auto INDITelescopeManager::setAZALT(double azDegrees, double altDegrees) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setAZALT(azDegrees, altDegrees);
}

auto INDITelescopeManager::slewToAZALT(double azDegrees, double altDegrees) -> bool {
    if (!ensureConnected()) return false;
    updateTelescopeState(TelescopeState::SLEWING);
    bool result = motion_->slewToAZALT(azDegrees, altDegrees);
    if (result) {
        updateTelescopeState(TelescopeState::IDLE);
    } else {
        updateTelescopeState(TelescopeState::ERROR);
    }
    return result;
}

auto INDITelescopeManager::getLocation() -> std::optional<GeographicLocation> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getLocation();
}

auto INDITelescopeManager::setLocation(const GeographicLocation& location) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setLocation(location);
}

auto INDITelescopeManager::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getUTCTime();
}

auto INDITelescopeManager::setUTCTime(const std::chrono::system_clock::time_point& time) -> bool {
    if (!ensureConnected()) return false;
    return coordinates_->setUTCTime(time);
}

auto INDITelescopeManager::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> {
    if (!ensureConnected()) return std::nullopt;
    return coordinates_->getLocalTime();
}

auto INDITelescopeManager::getAlignmentMode() -> AlignmentMode {
    return alignmentMode_;
}

auto INDITelescopeManager::setAlignmentMode(AlignmentMode mode) -> bool {
    alignmentMode_ = mode;
    spdlog::info("Alignment mode set to: {}", static_cast<int>(mode));
    return true;
}

auto INDITelescopeManager::addAlignmentPoint(const EquatorialCoordinates& measured, 
                                            const EquatorialCoordinates& target) -> bool {
    if (!ensureConnected()) return false;
    
    spdlog::info("Adding alignment point: measured(RA={:.6f}h, DEC={:.6f}°) -> target(RA={:.6f}h, DEC={:.6f}°)",
                measured.ra, measured.dec, target.ra, target.dec);
    
    // In a full implementation, this would store alignment points
    // and apply pointing model corrections
    return true;
}

auto INDITelescopeManager::clearAlignment() -> bool {
    spdlog::info("Clearing telescope alignment");
    return true;
}

auto INDITelescopeManager::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    return coordinates_->degreesToDMS(degrees);
}

auto INDITelescopeManager::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    return coordinates_->degreesToHMS(degrees);
}

void INDITelescopeManager::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    // Handle INDI messages
    spdlog::debug("INDI message received from {}: ID={}", baseDevice.getDeviceName(), messageID);
}

auto INDITelescopeManager::initializeComponents() -> bool {
    spdlog::debug("Initializing telescope components");
    
    if (!connection_->initialize()) {
        spdlog::error("Failed to initialize connection component");
        return false;
    }
    
    spdlog::debug("All telescope components initialized successfully");
    return true;
}

auto INDITelescopeManager::destroyComponents() -> bool {
    spdlog::debug("Destroying telescope components");
    
    if (parking_) parking_->destroy();
    if (coordinates_) coordinates_->destroy();
    if (tracking_) tracking_->destroy();
    if (motion_) motion_->destroy();
    if (connection_) connection_->destroy();
    
    spdlog::debug("All telescope components destroyed");
    return true;
}

auto INDITelescopeManager::ensureConnected() -> bool {
    if (!isConnected()) {
        spdlog::error("Telescope not connected");
        return false;
    }
    return true;
}

auto INDITelescopeManager::updateTelescopeState() -> void {
    // Update internal state based on current conditions
    if (isParked()) {
        updateTelescopeState(TelescopeState::PARKED);
    } else if (isMoving()) {
        updateTelescopeState(TelescopeState::SLEWING);
    } else if (isTrackingEnabled()) {
        updateTelescopeState(TelescopeState::TRACKING);
    } else {
        updateTelescopeState(TelescopeState::IDLE);
    }
}
