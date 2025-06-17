#include "tracking.hpp"

TelescopeTracking::TelescopeTracking(const std::string& name) : name_(name) {
    spdlog::debug("Creating telescope tracking component for {}", name_);
    
    // Initialize default sidereal tracking rates
    trackRates_.guideRateNS = 0.5;   // arcsec/sec
    trackRates_.guideRateEW = 0.5;   // arcsec/sec
    trackRates_.slewRateRA = 3.0;    // degrees/sec
    trackRates_.slewRateDEC = 3.0;   // degrees/sec
}

auto TelescopeTracking::initialize(INDI::BaseDevice device) -> bool {
    device_ = device;
    spdlog::info("Initializing telescope tracking component");
    watchTrackingProperties();
    watchPierSideProperties();
    return true;
}

auto TelescopeTracking::destroy() -> bool {
    spdlog::info("Destroying telescope tracking component");
    return true;
}

auto TelescopeTracking::isTrackingEnabled() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_STATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_STATE property");
        return false;
    }
    
    bool enabled = property[0].getState() == ISS_ON;
    isTrackingEnabled_.store(enabled);
    return enabled;
}

auto TelescopeTracking::enableTracking(bool enable) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_STATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_STATE property");
        return false;
    }
    
    property[0].setState(enable ? ISS_ON : ISS_OFF);
    property[1].setState(enable ? ISS_OFF : ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    isTrackingEnabled_.store(enable);
    isTracking_.store(enable);
    spdlog::info("Tracking {}", enable ? "enabled" : "disabled");
    return true;
}

auto TelescopeTracking::getTrackRate() -> std::optional<TrackMode> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_MODE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_MODE property");
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        return TrackMode::SIDEREAL;
    } else if (property[1].getState() == ISS_ON) {
        return TrackMode::SOLAR;
    } else if (property[2].getState() == ISS_ON) {
        return TrackMode::LUNAR;
    } else if (property[3].getState() == ISS_ON) {
        return TrackMode::CUSTOM;
    }
    
    return TrackMode::NONE;
}

auto TelescopeTracking::setTrackRate(TrackMode rate) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_MODE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_MODE property");
        return false;
    }
    
    // Reset all states
    for (int i = 0; i < property.count(); ++i) {
        property[i].setState(ISS_OFF);
    }
    
    switch (rate) {
    case TrackMode::SIDEREAL:
        if (property.count() > 0) property[0].setState(ISS_ON);
        trackRateRA_.store(15.041067); // sidereal rate
        break;
    case TrackMode::SOLAR:
        if (property.count() > 1) property[1].setState(ISS_ON);
        trackRateRA_.store(15.0); // solar rate
        break;
    case TrackMode::LUNAR:
        if (property.count() > 2) property[2].setState(ISS_ON);
        trackRateRA_.store(14.685); // lunar rate
        break;
    case TrackMode::CUSTOM:
        if (property.count() > 3) property[3].setState(ISS_ON);
        // Custom rate will be set separately
        break;
    case TrackMode::NONE:
        // All states remain OFF
        trackRateRA_.store(0.0);
        break;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    trackMode_ = rate;
    spdlog::info("Track mode set to: {}", static_cast<int>(rate));
    return true;
}

auto TelescopeTracking::getTrackRates() -> MotionRates {
    // Update current rates from device if available
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_TRACK_RATE");
    if (property.isValid() && property.count() >= 2) {
        trackRates_.slewRateRA = property[0].getValue();
        trackRates_.slewRateDEC = property[1].getValue();
    }
    
    return trackRates_;
}

auto TelescopeTracking::setTrackRates(const MotionRates& rates) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_TRACK_RATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_RATE property");
        return false;
    }
    
    if (property.count() >= 2) {
        property[0].setValue(rates.slewRateRA);
        property[1].setValue(rates.slewRateDEC);
        device_.getBaseClient()->sendNewProperty(property);
    }
    
    trackRates_ = rates;
    trackRateRA_.store(rates.slewRateRA);
    trackRateDEC_.store(rates.slewRateDEC);
    
    spdlog::info("Custom track rates set: RA={:.6f}, DEC={:.6f}", 
                rates.slewRateRA, rates.slewRateDEC);
    return true;
}

auto TelescopeTracking::getPierSide() -> std::optional<PierSide> {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PIER_SIDE");
    if (!property.isValid()) {
        spdlog::debug("TELESCOPE_PIER_SIDE property not available");
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        pierSide_ = PierSide::EAST;
        return PierSide::EAST;
    } else if (property[1].getState() == ISS_ON) {
        pierSide_ = PierSide::WEST;
        return PierSide::WEST;
    }
    
    pierSide_ = PierSide::UNKNOWN;
    return PierSide::UNKNOWN;
}

auto TelescopeTracking::setPierSide(PierSide side) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PIER_SIDE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PIER_SIDE property");
        return false;
    }
    
    switch (side) {
    case PierSide::EAST:
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
        break;
    case PierSide::WEST:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
        break;
    case PierSide::UNKNOWN:
    case PierSide::NONE:
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
        break;
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    pierSide_ = side;
    spdlog::info("Pier side set to: {}", static_cast<int>(side));
    return true;
}

auto TelescopeTracking::canFlipPierSide() -> bool {
    // Check if pier side property is available and mount supports flipping
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PIER_SIDE");
    return property.isValid();
}

auto TelescopeTracking::flipPierSide() -> bool {
    if (!canFlipPierSide()) {
        spdlog::error("Pier side flipping not supported");
        return false;
    }
    
    auto currentSide = getPierSide();
    if (!currentSide) {
        spdlog::error("Unable to determine current pier side");
        return false;
    }
    
    PierSide newSide = (*currentSide == PierSide::EAST) ? PierSide::WEST : PierSide::EAST;
    
    spdlog::info("Performing meridian flip from {} to {}", 
                static_cast<int>(*currentSide), 
                static_cast<int>(newSide));
    
    return setPierSide(newSide);
}

auto TelescopeTracking::watchTrackingProperties() -> void {
    spdlog::debug("Setting up tracking property watchers");
    
    // Watch for tracking state changes
    device_.watchProperty("TELESCOPE_TRACK_STATE",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                bool tracking = property[0].getState() == ISS_ON;
                isTracking_.store(tracking);
                spdlog::debug("Tracking state changed: {}", tracking ? "ON" : "OFF");
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    // Watch for track mode changes
    device_.watchProperty("TELESCOPE_TRACK_MODE",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                updateTrackingState();
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    // Watch for track rate changes
    device_.watchProperty("TELESCOPE_TRACK_RATE",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 2) {
                trackRateRA_.store(property[0].getValue());
                trackRateDEC_.store(property[1].getValue());
                spdlog::debug("Track rates updated: RA={:.6f}, DEC={:.6f}",
                            property[0].getValue(), property[1].getValue());
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeTracking::watchPierSideProperties() -> void {
    spdlog::debug("Setting up pier side property watchers");
    
    device_.watchProperty("TELESCOPE_PIER_SIDE",
        [this](const INDI::PropertySwitch& property) {
            if (property.isValid()) {
                auto side = getPierSide();
                if (side) {
                    spdlog::debug("Pier side changed to: {}", static_cast<int>(*side));
                }
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeTracking::updateTrackingState() -> void {
    auto mode = getTrackRate();
    if (mode) {
        trackMode_ = *mode;
        spdlog::debug("Track mode updated to: {}", static_cast<int>(*mode));
    }
}
