#include "indi.hpp"

TelescopeINDI::TelescopeINDI(const std::string& name) : name_(name) {
    spdlog::debug("Creating telescope INDI component for {}", name_);
}

auto TelescopeINDI::initialize(INDI::BaseDevice device) -> bool {
    device_ = device;
    spdlog::info("Initializing telescope INDI component");
    
    // Set default capabilities
    SetTelescopeCapability(TELESCOPE_CAN_GOTO | 
                          TELESCOPE_CAN_SYNC | 
                          TELESCOPE_CAN_PARK | 
                          TELESCOPE_CAN_ABORT |
                          TELESCOPE_HAS_TRACK_MODE |
                          TELESCOPE_HAS_TRACK_RATE |
                          TELESCOPE_HAS_PIER_SIDE, 4);
    
    indiInitialized_.store(true);
    return true;
}

auto TelescopeINDI::destroy() -> bool {
    spdlog::info("Destroying telescope INDI component");
    indiInitialized_.store(false);
    indiConnected_.store(false);
    return true;
}

auto TelescopeINDI::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand cmd) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_NS");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_NS property");
        return false;
    }
    
    if (cmd == MOTION_START) {
        if (dir == DIRECTION_NORTH) {
            property[0].setState(ISS_ON);
            property[1].setState(ISS_OFF);
        } else {
            property[0].setState(ISS_OFF);
            property[1].setState(ISS_ON);
        }
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::debug("Move NS: dir={}, cmd={}", 
                 dir == DIRECTION_NORTH ? "NORTH" : "SOUTH",
                 cmd == MOTION_START ? "START" : "STOP");
    return true;
}

auto TelescopeINDI::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand cmd) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_MOTION_WE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_MOTION_WE property");
        return false;
    }
    
    if (cmd == MOTION_START) {
        if (dir == DIRECTION_WEST) {
            property[0].setState(ISS_ON);
            property[1].setState(ISS_OFF);
        } else {
            property[0].setState(ISS_OFF);
            property[1].setState(ISS_ON);
        }
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_OFF);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::debug("Move WE: dir={}, cmd={}", 
                 dir == DIRECTION_WEST ? "WEST" : "EAST",
                 cmd == MOTION_START ? "START" : "STOP");
    return true;
}

auto TelescopeINDI::Abort() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_ABORT_MOTION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_ABORT_MOTION property");
        return false;
    }
    
    property[0].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::info("Aborting telescope motion via INDI");
    return true;
}

auto TelescopeINDI::Park() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK property");
        return false;
    }
    
    property[0].setState(ISS_ON);
    property[1].setState(ISS_OFF);
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::info("Parking telescope via INDI");
    return true;
}

auto TelescopeINDI::UnPark() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK property");
        return false;
    }
    
    property[0].setState(ISS_OFF);
    property[1].setState(ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::info("Unparking telescope via INDI");
    return true;
}

auto TelescopeINDI::SetTrackMode(uint8_t mode) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_MODE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_MODE property");
        return false;
    }
    
    for (int i = 0; i < property.count(); ++i) {
        property[i].setState(i == mode ? ISS_ON : ISS_OFF);
    }
    
    device_.getBaseClient()->sendNewProperty(property);
    spdlog::info("Set track mode to: {}", mode);
    return true;
}

auto TelescopeINDI::SetTrackEnabled(bool enabled) -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_TRACK_STATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_STATE property");
        return false;
    }
    
    property[0].setState(enabled ? ISS_ON : ISS_OFF);
    property[1].setState(enabled ? ISS_OFF : ISS_ON);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Tracking {}", enabled ? "enabled" : "disabled");
    return true;
}

auto TelescopeINDI::SetTrackRate(double raRate, double deRate) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_TRACK_RATE");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_TRACK_RATE property");
        return false;
    }
    
    if (property.count() >= 2) {
        property[0].setValue(raRate);
        property[1].setValue(deRate);
        device_.getBaseClient()->sendNewProperty(property);
    }
    
    spdlog::info("Set track rates: RA={:.6f}, DEC={:.6f}", raRate, deRate);
    return true;
}

auto TelescopeINDI::Goto(double ra, double dec) -> bool {
    // Set action to SLEW
    INDI::PropertySwitch actionProperty = device_.getProperty("ON_COORD_SET");
    if (actionProperty.isValid()) {
        actionProperty[0].setState(ISS_OFF); // SLEW
        actionProperty[1].setState(ISS_ON);  // TRACK
        actionProperty[2].setState(ISS_OFF); // SYNC
        device_.getBaseClient()->sendNewProperty(actionProperty);
    }
    
    // Set coordinates
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(ra);
    property[1].setValue(dec);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Goto: RA={:.6f}h, DEC={:.6f}°", ra, dec);
    return true;
}

auto TelescopeINDI::Sync(double ra, double dec) -> bool {
    // Set action to SYNC
    INDI::PropertySwitch actionProperty = device_.getProperty("ON_COORD_SET");
    if (actionProperty.isValid()) {
        actionProperty[0].setState(ISS_OFF); // SLEW
        actionProperty[1].setState(ISS_OFF); // TRACK
        actionProperty[2].setState(ISS_ON);  // SYNC
        device_.getBaseClient()->sendNewProperty(actionProperty);
    }
    
    // Set coordinates
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(ra);
    property[1].setValue(dec);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Sync: RA={:.6f}h, DEC={:.6f}°", ra, dec);
    return true;
}

auto TelescopeINDI::UpdateLocation(double latitude, double longitude, double elevation) -> bool {
    INDI::PropertyNumber property = device_.getProperty("GEOGRAPHIC_COORD");
    if (!property.isValid()) {
        spdlog::warn("GEOGRAPHIC_COORD property not available");
        return false;
    }
    
    if (property.count() >= 3) {
        property[0].setValue(latitude);
        property[1].setValue(longitude);
        property[2].setValue(elevation);
        device_.getBaseClient()->sendNewProperty(property);
    }
    
    spdlog::info("Updated location: lat={:.6f}°, lon={:.6f}°, elev={:.1f}m", 
                latitude, longitude, elevation);
    return true;
}

auto TelescopeINDI::UpdateTime(ln_date* utc, double utc_offset) -> bool {
    INDI::PropertyText timeProperty = device_.getProperty("TIME_UTC");
    if (!timeProperty.isValid()) {
        spdlog::warn("TIME_UTC property not available");
        return false;
    }
    
    // Convert ln_date to ISO 8601 string
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%06.3f",
            utc->years, utc->months, utc->days,
            utc->hours, utc->minutes, utc->seconds);
    
    timeProperty[0].setText(timeStr);
    device_.getBaseClient()->sendNewProperty(timeProperty);
    
    // Set UTC offset if available
    INDI::PropertyNumber offsetProperty = device_.getProperty("TIME_LST");
    if (offsetProperty.isValid()) {
        offsetProperty[0].setValue(utc_offset);
        device_.getBaseClient()->sendNewProperty(offsetProperty);
    }
    
    spdlog::info("Updated time: {} (UTC offset: {:.2f}h)", timeStr, utc_offset);
    return true;
}

auto TelescopeINDI::ReadScopeParameters() -> bool {
    INDI::PropertyNumber property = device_.getProperty("TELESCOPE_INFO");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_INFO property");
        return false;
    }
    
    if (property.count() >= 4) {
        double primaryAperture = property[0].getValue();
        double primaryFocalLength = property[1].getValue();
        double guiderAperture = property[2].getValue();
        double guiderFocalLength = property[3].getValue();
        
        spdlog::info("Telescope parameters - Primary: {:.1f}mm f/{:.1f}, Guider: {:.1f}mm f/{:.1f}",
                    primaryAperture, primaryFocalLength,
                    guiderAperture, guiderFocalLength);
    }
    
    return true;
}

auto TelescopeINDI::SetCurrentPark() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK_OPTION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK_OPTION property");
        return false;
    }
    
    // Set to "CURRENT" option
    property[0].setState(ISS_ON);
    property[1].setState(ISS_OFF);
    property[2].setState(ISS_OFF);
    property[3].setState(ISS_OFF);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Set current position as park position");
    return true;
}

auto TelescopeINDI::SetDefaultPark() -> bool {
    INDI::PropertySwitch property = device_.getProperty("TELESCOPE_PARK_OPTION");
    if (!property.isValid()) {
        spdlog::error("Unable to find TELESCOPE_PARK_OPTION property");
        return false;
    }
    
    // Set to "DEFAULT" option
    property[0].setState(ISS_OFF);
    property[1].setState(ISS_ON);
    property[2].setState(ISS_OFF);
    property[3].setState(ISS_OFF);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::info("Set default park position");
    return true;
}

auto TelescopeINDI::saveConfigItems(FILE *fp) -> bool {
    // Save telescope-specific configuration
    spdlog::debug("Saving telescope configuration");
    return true;
}

auto TelescopeINDI::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) -> bool {
    processCoordinateUpdate();
    return true;
}

auto TelescopeINDI::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) -> bool {
    processTrackingUpdate();
    processParkingUpdate();
    return true;
}

auto TelescopeINDI::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) -> bool {
    return true;
}

auto TelescopeINDI::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) -> bool {
    return true;
}

auto TelescopeINDI::getProperties(const char *dev) -> void {
    spdlog::debug("Getting properties for device: {}", dev ? dev : "all");
}

auto TelescopeINDI::TimerHit() -> void {
    // Update telescope state periodically
    processCoordinateUpdate();
}

auto TelescopeINDI::getDefaultName() -> const char* {
    return name_.c_str();
}

auto TelescopeINDI::initProperties() -> bool {
    spdlog::debug("Initializing INDI properties");
    return true;
}

auto TelescopeINDI::updateProperties() -> bool {
    spdlog::debug("Updating INDI properties");
    return true;
}

auto TelescopeINDI::Connect() -> bool {
    indiConnected_.store(true);
    spdlog::info("INDI telescope connected");
    return true;
}

auto TelescopeINDI::Disconnect() -> bool {
    indiConnected_.store(false);
    spdlog::info("INDI telescope disconnected");
    return true;
}

auto TelescopeINDI::SetTelescopeCapability(uint32_t cap, uint8_t slewRateCount) -> void {
    telescopeCapability_ = cap;
    slewRateCount_ = slewRateCount;
    spdlog::info("Telescope capability set: 0x{:08X}, slew rates: {}", cap, slewRateCount);
}

auto TelescopeINDI::SetParkDataType(TelescopeParkData type) -> void {
    parkDataType_ = type;
    spdlog::info("Park data type set: {}", static_cast<int>(type));
}

auto TelescopeINDI::InitPark() -> bool {
    spdlog::info("Initializing park data");
    return true;
}

auto TelescopeINDI::HasTrackMode() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_TRACK_MODE) != 0;
}

auto TelescopeINDI::HasTrackRate() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_TRACK_RATE) != 0;
}

auto TelescopeINDI::HasLocation() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_LOCATION) != 0;
}

auto TelescopeINDI::HasTime() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_TIME) != 0;
}

auto TelescopeINDI::HasPierSide() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_PIER_SIDE) != 0;
}

auto TelescopeINDI::HasPierSideSimulation() -> bool {
    return (telescopeCapability_ & TELESCOPE_HAS_PIER_SIDE_SIMULATION) != 0;
}

auto TelescopeINDI::processCoordinateUpdate() -> void {
    // Handle coordinate property updates
}

auto TelescopeINDI::processTrackingUpdate() -> void {
    // Handle tracking property updates
}

auto TelescopeINDI::processParkingUpdate() -> void {
    // Handle parking property updates
}

auto TelescopeINDI::handlePropertyUpdate(const char* name) -> void {
    spdlog::trace("Property updated: {}", name);
}
