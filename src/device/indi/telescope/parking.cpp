#include "parking.hpp"

TelescopeParking::TelescopeParking(const std::string& name) : name_(name) {
    spdlog::debug("Creating telescope parking component for {}", name_);
}

auto TelescopeParking::initialize(INDI::BaseDevice device) -> bool {
    device_ = device;
    spdlog::info("Initializing telescope parking component");
    watchParkingProperties();
    watchHomeProperties();
    return true;
}

auto TelescopeParking::destroy() -> bool {
    spdlog::info("Destroying telescope parking component");
    return true;
}

auto TelescopeParking::canPark() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    return property.isValid();
}

auto TelescopeParking::isParked() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    if (!property.isValid()) {
        spdlog::debug("TELESCOPE_PARK property not available");
        return false;
    }
    
    bool parked = property[0].getState() == ISS_ON;
    isParked_.store(parked);
    return parked;
}

auto TelescopeParking::park() -> bool {
    if (!canPark()) {
        spdlog::error("Parking is not supported by this telescope");
        return false;
    }
    
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK property");
        return false;
    }
    
    property[0].setState(ISS_ON);
    property[1].setState(ISS_OFF);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Parking telescope {}", name_);
    return true;
}

auto TelescopeParking::unpark() -> bool {
    if (!canPark()) {
        spdlog::error("Parking is not supported by this telescope");
        return false;
    }
    
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK property");
        return false;
    }
    
    property[0].setState(ISS_OFF);
    property[1].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Unparking telescope {}", name_);
    return true;
}

auto TelescopeParking::setParkOption(ParkOptions option) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK_OPTION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK_OPTION property");
        return false;
    }
    
    // Reset all options
    for (int i = 0; i < property.count(); ++i) {
        property[i].setState(ISS_OFF);
    }
    
    switch (option) {
    case ParkOptions::CURRENT:
        if (property.count() > 0) property[0].setState(ISS_ON);
        break;
    case ParkOptions::DEFAULT:
        if (property.count() > 1) property[1].setState(ISS_ON);
        break;
    case ParkOptions::WRITE_DATA:
        if (property.count() > 2) property[2].setState(ISS_ON);
        break;
    case ParkOptions::PURGE_DATA:
        if (property.count() > 3) property[3].setState(ISS_ON);
        break;
    case ParkOptions::NONE:
        // All remain OFF
        break;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    parkOption_ = option;
    spdlog::info("Park option set to: {}", static_cast<int>(option));
    return true;
}

auto TelescopeParking::getParkPosition() -> std::optional<EquatorialCoordinates> {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_PARK_POSITION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK_POSITION property");
        return std::nullopt;
    }
    
    EquatorialCoordinates coords;
    coords.ra = property[0].getValue();
    coords.dec = property[1].getValue();
    parkPosition_ = coords;
    return coords;
}

auto TelescopeParking::setParkPosition(double parkRA, double parkDEC) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_PARK_POSITION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK_POSITION property");
        return false;
    }
    
    property[0].setValue(parkRA);
    property[1].setValue(parkDEC);
    device_.getBaseClient()->sendNewProperty(property);
    
    parkPosition_.ra = parkRA;
    parkPosition_.dec = parkDEC;
    
    spdlog::info("Park position set to: RA={:.6f}h, DEC={:.6f}°", parkRA, parkDEC);
    return true;
}

auto TelescopeParking::initializeHome(std::string_view command) -> bool {
    INDI::PropertySwitch property = device_.getProperty("HOME_INIT");
    if (!property.isValid()) {
        spdlog::error("Unable to find HOME_INIT property");
        return false;
    }
    
    if (command.empty() || command == "SLEWHOME") {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
        spdlog::info("Initializing home by slewing to home position");
    } else if (command == "SYNCHOME") {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
        spdlog::info("Initializing home by syncing to current position");
    } else {
        spdlog::error("Unknown home initialization command: {}", command);
        return false;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    isHomeInitInProgress_.store(true);
    return true;
}

auto TelescopeParking::findHome() -> bool {
    INDI::PropertySwitch property = device_.getProperty("HOME_FIND");
    if (!property.isValid()) {
        spdlog::warn("HOME_FIND property not available, using HOME_INIT instead");
        return initializeHome("SLEWHOME");
    }
    
    property[0].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Finding home position for telescope {}", name_);
    return true;
}

auto TelescopeParking::setHome() -> bool {
    INDI::PropertySwitch property = device_.getProperty("HOME_SET");
    if (!property.isValid()) {
        spdlog::warn("HOME_SET property not available, using HOME_INIT SYNC instead");
        return initializeHome("SYNCHOME");
    }
    
    property[0].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Setting current position as home for telescope {}", name_);
    return true;
}

auto TelescopeParking::gotoHome() -> bool {
    INDI::PropertySwitch property = device_.getProperty("HOME_GOTO");
    if (!property.isValid()) {
        spdlog::warn("HOME_GOTO property not available, using HOME_INIT SLEW instead");
        return initializeHome("SLEWHOME");
    }
    
    property[0].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Going to home position for telescope {}", name_);
    return true;
}

auto TelescopeParking::isAtHome() -> bool {
    return isHomed_.load();
}

auto TelescopeParking::isHomeSet() -> bool {
    return isHomeSet_.load();
}

auto TelescopeParking::watchParkingProperties() -> void {
    spdlog::debug("Setting up parking property watchers");
    
    device_.watchProperty("TELESCOPE_PARK",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                bool parked = property[0].getState() == ISS_ON;
                isParked_.store(parked);
                spdlog::debug("Parking state changed: {}", parked ? "PARKED" : "UNPARKED");
                updateParkingState();
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    device_.watchProperty("TELESCOPE_PARK_POSITION",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 2) {
                parkPosition_.ra = property[0].getValue();
                parkPosition_.dec = property[1].getValue();
                spdlog::debug("Park position updated: RA={:.6f}h, DEC={:.6f}°",
                            parkPosition_.ra, parkPosition_.dec);
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    device_.watchProperty("TELESCOPE_PARK_OPTION",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                // Update park option based on which switch is ON
                for (int i = 0; i < property.count(); ++i) {
                    if (property[i].getState() == ISS_ON) {
                        parkOption_ = static_cast<ParkOptions>(i);
                        break;
                    }
                }
                spdlog::debug("Park option changed to: {}", static_cast<int>(parkOption_));
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeParking::watchHomeProperties() -> void {
    spdlog::debug("Setting up home property watchers");
    
    device_.watchProperty("HOME_INIT",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                bool inProgress = property[0].getState() == ISS_ON || property[1].getState() == ISS_ON;
                isHomeInitInProgress_.store(inProgress);
                
                if (!inProgress) {
                    // Home initialization completed
                    isHomed_.store(true);
                    isHomeSet_.store(true);
                    spdlog::info("Home initialization completed");
                }
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    // Watch for other home-related properties if available
    device_.watchProperty("HOME_FIND",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                bool finding = property[0].getState() == ISS_ON;
                if (!finding && isHomeInitInProgress_.load()) {
                    isHomed_.store(true);
                    isHomeSet_.store(true);
                    isHomeInitInProgress_.store(false);
                    spdlog::info("Home finding completed");
                }
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeParking::updateParkingState() -> void {
    isParkEnabled_ = canPark();
    
    if (isParked_.load()) {
        spdlog::debug("Telescope {} is parked", name_);
    } else {
        spdlog::debug("Telescope {} is unparked", name_);
    }
}

auto TelescopeParking::updateHomeState() -> void {
    if (isHomed_.load()) {
        spdlog::debug("Telescope {} is at home position", name_);
    }
    
    if (isHomeSet_.load()) {
        spdlog::debug("Telescope {} has home position set", name_);
    }
}
