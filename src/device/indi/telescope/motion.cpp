#include "motion.hpp"

TelescopeMotion::TelescopeMotion(const std::string& name) : name_(name) {
    spdlog::debug("Creating telescope motion component for {}", name_);
}

auto TelescopeMotion::initialize(INDI::BaseDevice device) -> bool {
    device_ = device;
    spdlog::info("Initializing telescope motion component");
    watchMotionProperties();
    watchSlewRateProperties();
    watchGuideProperties();
    return true;
}

auto TelescopeMotion::destroy() -> bool {
    spdlog::info("Destroying telescope motion component");
    return true;
}

auto TelescopeMotion::abortMotion() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_ABORT_MOTION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_ABORT_MOTION property");
        return false;
    }
    
    property[0].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::info("Telescope motion aborted");
    return true;
}

auto TelescopeMotion::emergencyStop() -> bool {
    spdlog::warn("EMERGENCY STOP activated for telescope {}", name_);
    return abortMotion();
}

auto TelescopeMotion::isMoving() -> bool {
    return isMoving_.load();
}

auto TelescopeMotion::getStatus() -> std::optional<std::string> {
    INDI::PropertyText property = device_.getProperty("TELESCOPE_STATUS");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_STATUS property");
        return std::nullopt;
    }
    return std::string(property[0].getText());
}

auto TelescopeMotion::getMoveDirectionEW() -> std::optional<MotionEW> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_WE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_WE property");
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        return MotionEW::EAST;
    } else if (property[1].getState() == ISS_ON) {
        return MotionEW::WEST;
    }
    return MotionEW::NONE;
}

auto TelescopeMotion::setMoveDirectionEW(MotionEW direction) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_WE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_WE property");
        return false;
    }
    
    switch (direction) {
    case MotionEW::EAST:
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
        break;
    case MotionEW::WEST:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
        break;
    case MotionEW::NONE:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
        break;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    motionEW_ = direction;
    return true;
}

auto TelescopeMotion::getMoveDirectionNS() -> std::optional<MotionNS> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_NS");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_NS property");
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        return MotionNS::NORTH;
    } else if (property[1].getState() == ISS_ON) {
        return MotionNS::SOUTH;
    }
    return MotionNS::NONE;
}

auto TelescopeMotion::setMoveDirectionNS(MotionNS direction) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_NS");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_NS property");
        return false;
    }
    
    switch (direction) {
    case MotionNS::NORTH:
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
        break;
    case MotionNS::SOUTH:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
        break;
    case MotionNS::NONE:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
        break;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    motionNS_ = direction;
    return true;
}

auto TelescopeMotion::startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    bool success = true;
    
    if (ns_direction != MotionNS::NONE) {
        success &= setMoveDirectionNS(ns_direction);
    }
    
    if (ew_direction != MotionEW::NONE) {
        success &= setMoveDirectionEW(ew_direction);
    }
    
    if (success) {
        isMoving_.store(true);
        spdlog::info("Started telescope motion: NS={}, EW={}", 
                    static_cast<int>(ns_direction), 
                    static_cast<int>(ew_direction));
    }
    
    return success;
}

auto TelescopeMotion::stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    bool success = true;
    
    if (ns_direction != MotionNS::NONE) {
        success &= setMoveDirectionNS(MotionNS::NONE);
    }
    
    if (ew_direction != MotionEW::NONE) {
        success &= setMoveDirectionEW(MotionEW::NONE);
    }
    
    if (success) {
        isMoving_.store(false);
        spdlog::info("Stopped telescope motion");
    }
    
    return success;
}

auto TelescopeMotion::getSlewRate() -> std::optional<double> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_SLEW_RATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_SLEW_RATE property");
        return std::nullopt;
    }
    
    for (int i = 0; i < property.count(); ++i) {
        if (property[i].getState() == ISS_ON) {
            return static_cast<double>(i);
        }
    }
    return std::nullopt;
}

auto TelescopeMotion::setSlewRate(double speed) -> bool {
    return setSlewRateIndex(static_cast<int>(speed));
}

auto TelescopeMotion::getSlewRates() -> std::vector<double> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_SLEW_RATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_SLEW_RATE property");
        return {};
    }
    
    std::vector<double> rates;
    for (int i = 0; i < property.count(); ++i) {
        rates.push_back(static_cast<double>(i));
    }
    return rates;
}

auto TelescopeMotion::setSlewRateIndex(int index) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_SLEW_RATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_SLEW_RATE property");
        return false;
    }
    
    if (index < 0 || index >= property.count()) {
        spdlog::error("Invalid slew rate index: {}", index);
        return false;
    }
    
    for (int i = 0; i < property.count(); ++i) {
        property[i].setState(i == index ? ISS_ON : ISS_OFF);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    currentSlewRateIndex_ = index;
    spdlog::info("Slew rate set to index: {}", index);
    return true;
}

auto TelescopeMotion::guideNS(int direction, int duration) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_TIMED_GUIDE_NS");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TIMED_GUIDE_NS property");
        return false;
    }
    
    if (direction > 0) {
        // North
        property[0].setValue(duration);
        property[1].setValue(0);
    } else {
        // South
        property[0].setValue(0);
        property[1].setValue(duration);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::debug("Guiding NS: direction={}, duration={}ms", direction, duration);
    return true;
}

auto TelescopeMotion::guideEW(int direction, int duration) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_TIMED_GUIDE_WE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TIMED_GUIDE_WE property");
        return false;
    }
    
    if (direction > 0) {
        // East
        property[0].setValue(duration);
        property[1].setValue(0);
    } else {
        // West
        property[0].setValue(0);
        property[1].setValue(duration);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::debug("Guiding EW: direction={}, duration={}ms", direction, duration);
    return true;
}

auto TelescopeMotion::guidePulse(double ra_ms, double dec_ms) -> bool {
    bool success = true;
    
    if (ra_ms != 0) {
        success &= guideEW(ra_ms > 0 ? 1 : -1, static_cast<int>(std::abs(ra_ms)));
    }
    
    if (dec_ms != 0) {
        success &= guideNS(dec_ms > 0 ? 1 : -1, static_cast<int>(std::abs(dec_ms)));
    }
    
    return success;
}

auto TelescopeMotion::slewToRADECJ2000(double raHours, double decDegrees, bool enableTracking) -> bool {
    setActionAfterPositionSet(enableTracking ? "TRACK" : "STOP");
    
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Slewing to RA/DEC J2000: {:.4f}h, {:.4f}°", raHours, decDegrees);
    return true;
}

auto TelescopeMotion::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    setActionAfterPositionSet(enableTracking ? "TRACK" : "STOP");
    
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Slewing to RA/DEC JNow: {:.4f}h, {:.4f}°", raHours, decDegrees);
    return true;
}

auto TelescopeMotion::slewToAZALT(double azDegrees, double altDegrees) -> bool {
    INDI::PropertyNumber property = device_.getProperty("HORIZONTAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find HORIZONTAL_COORD property");
        return false;
    }
    
    property[0].setValue(azDegrees);
    property[1].setValue(altDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Slewing to AZ/ALT: {:.4f}°, {:.4f}°", azDegrees, altDegrees);
    return true;
}

auto TelescopeMotion::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    setActionAfterPositionSet("SYNC");
    
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Syncing to RA/DEC JNow: {:.4f}h, {:.4f}°", raHours, decDegrees);
    return true;
}

auto TelescopeMotion::setActionAfterPositionSet(std::string_view action) -> bool {
    INDI::PropertySwitch property = device_.getProperty("ON_COORD_SET");
    if (!property.isValid()) {
        spdlog::error("Unable to find ON_COORD_SET property");
        return false;
    }
    
    if (action == "STOP") {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
        property[2].setState(ISS_OFF);
    } else if (action == "TRACK") {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
        property[2].setState(ISS_OFF);
    } else if (action == "SYNC") {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
        property[2].setState(ISS_ON);
    } else {
        spdlog::error("Unknown action: {}", action);
        return false;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::debug("Action after position set: {}", action);
    return true;
}

auto TelescopeMotion::watchMotionProperties() -> void {
    // Implementation for watching motion-related INDI properties
    spdlog::debug("Setting up motion property watchers");
}

auto TelescopeMotion::watchSlewRateProperties() -> void {
    // Implementation for watching slew rate properties
    spdlog::debug("Setting up slew rate property watchers");
}

auto TelescopeMotion::watchGuideProperties() -> void {
    // Implementation for watching guiding properties
    spdlog::debug("Setting up guide property watchers");
}
