#include "coordinates.hpp"
#include <cmath>
#include <chrono>

TelescopeCoordinates::TelescopeCoordinates(const std::string& name) : name_(name) {
    spdlog::debug("Creating telescope coordinates component for {}", name_);
    
    // Initialize with default location (Greenwich)
    location_.latitude = 51.4769;
    location_.longitude = -0.0005;
    location_.elevation = 46.0;
    location_.timezone = "UTC";
}

auto TelescopeCoordinates::initialize(INDI::BaseDevice device) -> bool {
    device_ = device;
    spdlog::info("Initializing telescope coordinates component");
    watchCoordinateProperties();
    watchLocationProperties();
    watchTimeProperties();
    return true;
}

auto TelescopeCoordinates::destroy() -> bool {
    spdlog::info("Destroying telescope coordinates component");
    return true;
}

auto TelescopeCoordinates::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_COORD property");
        return std::nullopt;
    }
    
    EquatorialCoordinates coords;
    coords.ra = property[0].getValue();
    coords.dec = property[1].getValue();
    currentRADECJ2000_ = coords;
    return coords;
}

auto TelescopeCoordinates::setRADECJ2000(double raHours, double decDegrees) -> bool {
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::debug("Set RA/DEC J2000: {:.6f}h, {:.6f}°", raHours, decDegrees);
    return true;
}

auto TelescopeCoordinates::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return std::nullopt;
    }
    
    EquatorialCoordinates coords;
    coords.ra = property[0].getValue();
    coords.dec = property[1].getValue();
    currentRADECJNow_ = coords;
    return coords;
}

auto TelescopeCoordinates::setRADECJNow(double raHours, double decDegrees) -> bool {
    INDI::PropertyNumber property = device_.getProperty("EQUATORIAL_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find EQUATORIAL_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::debug("Set RA/DEC JNow: {:.6f}h, {:.6f}°", raHours, decDegrees);
    return true;
}

auto TelescopeCoordinates::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    INDI::PropertyNumber property = device_.getProperty("TARGET_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find TARGET_EOD_COORD property");
        return std::nullopt;
    }
    
    EquatorialCoordinates coords;
    coords.ra = property[0].getValue();
    coords.dec = property[1].getValue();
    targetRADECJNow_ = coords;
    return coords;
}

auto TelescopeCoordinates::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    INDI::PropertyNumber property = device_.getProperty("TARGET_EOD_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find TARGET_EOD_COORD property");
        return false;
    }
    
    property[0].setValue(raHours);
    property[1].setValue(decDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    targetRADECJNow_.ra = raHours;
    targetRADECJNow_.dec = decDegrees;
    
    spdlog::debug("Set target RA/DEC JNow: {:.6f}h, {:.6f}°", raHours, decDegrees);
    return true;
}

auto TelescopeCoordinates::getAZALT() -> std::optional<HorizontalCoordinates> {
    INDI::PropertyNumber property = device_.getProperty("HORIZONTAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find HORIZONTAL_COORD property");
        return std::nullopt;
    }
    
    HorizontalCoordinates coords;
    coords.az = property[0].getValue();
    coords.alt = property[1].getValue();
    currentAZALT_ = coords;
    return coords;
}

auto TelescopeCoordinates::setAZALT(double azDegrees, double altDegrees) -> bool {
    INDI::PropertyNumber property = device_.getProperty("HORIZONTAL_COORD");
    if (!property.isValid()) {
        spdlog::error("Unable to find HORIZONTAL_COORD property");
        return false;
    }
    
    property[0].setValue(azDegrees);
    property[1].setValue(altDegrees);
    device_.getBaseClient()->sendNewProperty(property);
    
    spdlog::debug("Set AZ/ALT: {:.6f}°, {:.6f}°", azDegrees, altDegrees);
    return true;
}

auto TelescopeCoordinates::getLocation() -> std::optional<GeographicLocation> {
    INDI::PropertyNumber property = device_.getProperty("GEOGRAPHIC_COORD");
    if (!property.isValid()) {
        spdlog::debug("GEOGRAPHIC_COORD property not available, using stored location");
        return location_;
    }
    
    if (property.count() >= 3) {
        location_.latitude = property[0].getValue();
        location_.longitude = property[1].getValue();
        location_.elevation = property[2].getValue();
    }
    
    return location_;
}

auto TelescopeCoordinates::setLocation(const GeographicLocation& location) -> bool {
    INDI::PropertyNumber property = device_.getProperty("GEOGRAPHIC_COORD");
    if (!property.isValid()) {
        spdlog::warn("GEOGRAPHIC_COORD property not available, storing locally");
        location_ = location;
        return true;
    }
    
    if (property.count() >= 3) {
        property[0].setValue(location.latitude);
        property[1].setValue(location.longitude);
        property[2].setValue(location.elevation);
        device_.getBaseClient()->sendNewProperty(property);
    }
    
    location_ = location;
    spdlog::info("Location set: lat={:.6f}°, lon={:.6f}°, elev={:.1f}m", 
                location.latitude, location.longitude, location.elevation);
    return true;
}

auto TelescopeCoordinates::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> {
    INDI::PropertyText property = device_.getProperty("TIME_UTC");
    if (!property.isValid()) {
        spdlog::debug("TIME_UTC property not available, using system time");
        return std::chrono::system_clock::now();
    }
    
    // Parse INDI time format (ISO 8601)
    // This is a simplified implementation
    return std::chrono::system_clock::now();
}

auto TelescopeCoordinates::setUTCTime(const std::chrono::system_clock::time_point& time) -> bool {
    INDI::PropertyText property = device_.getProperty("TIME_UTC");
    if (!property.isValid()) {
        spdlog::warn("TIME_UTC property not available");
        utcTime_ = time;
        return true;
    }
    
    // Convert time_point to ISO 8601 string
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto tm = *std::gmtime(&time_t);
    
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
    
    property[0].setText(buffer);
    device_.getBaseClient()->sendNewProperty(property);
    
    utcTime_ = time;
    spdlog::debug("UTC time set: {}", buffer);
    return true;
}

auto TelescopeCoordinates::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> {
    // For simplicity, return UTC time
    // In a full implementation, this would account for timezone
    return getUTCTime();
}

auto TelescopeCoordinates::degreesToHours(double degrees) -> double {
    return degrees / 15.0;
}

auto TelescopeCoordinates::hoursToDegrees(double hours) -> double {
    return hours * 15.0;
}

auto TelescopeCoordinates::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    bool negative = degrees < 0;
    degrees = std::abs(degrees);
    
    int deg = static_cast<int>(degrees);
    double remainder = (degrees - deg) * 60.0;
    int min = static_cast<int>(remainder);
    double sec = (remainder - min) * 60.0;
    
    if (negative) {
        deg = -deg;
    }
    
    return std::make_tuple(deg, min, sec);
}

auto TelescopeCoordinates::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    double hours = degreesToHours(degrees);
    
    int hour = static_cast<int>(hours);
    double remainder = (hours - hour) * 60.0;
    int min = static_cast<int>(remainder);
    double sec = (remainder - min) * 60.0;
    
    return std::make_tuple(hour, min, sec);
}

auto TelescopeCoordinates::j2000ToJNow(const EquatorialCoordinates& j2000) -> EquatorialCoordinates {
    // Simplified precession calculation
    // In a full implementation, this would use proper astronomical algorithms
    // For now, assume minimal difference for short time periods
    
    EquatorialCoordinates jnow = j2000;
    
    // Apply approximate precession (very simplified)
    auto now = std::chrono::system_clock::now();
    auto j2000_epoch = std::chrono::system_clock::from_time_t(946684800); // 2000-01-01 12:00:00 UTC
    auto years = std::chrono::duration<double>(now - j2000_epoch).count() / (365.25 * 24 * 3600);
    
    // Simplified precession in RA (arcsec/year)
    double precession_ra = 50.29 * years / 3600.0; // convert to degrees
    double precession_dec = 0.0; // simplified
    
    jnow.ra += degreesToHours(precession_ra);
    jnow.dec += precession_dec;
    
    return jnow;
}

auto TelescopeCoordinates::jNowToJ2000(const EquatorialCoordinates& jnow) -> EquatorialCoordinates {
    // Simplified inverse precession calculation
    EquatorialCoordinates j2000 = jnow;
    
    auto now = std::chrono::system_clock::now();
    auto j2000_epoch = std::chrono::system_clock::from_time_t(946684800);
    auto years = std::chrono::duration<double>(now - j2000_epoch).count() / (365.25 * 24 * 3600);
    
    double precession_ra = 50.29 * years / 3600.0;
    double precession_dec = 0.0;
    
    j2000.ra -= degreesToHours(precession_ra);
    j2000.dec -= precession_dec;
    
    return j2000;
}

auto TelescopeCoordinates::equatorialToHorizontal(const EquatorialCoordinates& eq, 
                                                 const GeographicLocation& location,
                                                 const std::chrono::system_clock::time_point& time) -> HorizontalCoordinates {
    // Simplified coordinate transformation
    // In a full implementation, this would use proper spherical astronomy
    
    HorizontalCoordinates hz;
    
    // This is a placeholder implementation
    // Proper implementation would calculate:
    // 1. Local Sidereal Time
    // 2. Hour Angle
    // 3. Apply spherical trigonometry formulas
    
    hz.az = 180.0; // placeholder
    hz.alt = 45.0; // placeholder
    
    return hz;
}

auto TelescopeCoordinates::horizontalToEquatorial(const HorizontalCoordinates& hz,
                                                 const GeographicLocation& location,
                                                 const std::chrono::system_clock::time_point& time) -> EquatorialCoordinates {
    // Simplified inverse coordinate transformation
    EquatorialCoordinates eq;
    
    // Placeholder implementation
    eq.ra = 12.0; // placeholder
    eq.dec = 0.0; // placeholder
    
    return eq;
}

auto TelescopeCoordinates::watchCoordinateProperties() -> void {
    spdlog::debug("Setting up coordinate property watchers");
    
    // Watch for coordinate updates
    device_.watchProperty("EQUATORIAL_COORD",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 2) {
                currentRADECJ2000_.ra = property[0].getValue();
                currentRADECJ2000_.dec = property[1].getValue();
                spdlog::trace("RA/DEC J2000 updated: {:.6f}h, {:.6f}°",
                            currentRADECJ2000_.ra, currentRADECJ2000_.dec);
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    device_.watchProperty("EQUATORIAL_EOD_COORD",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 2) {
                currentRADECJNow_.ra = property[0].getValue();
                currentRADECJNow_.dec = property[1].getValue();
                spdlog::trace("RA/DEC JNow updated: {:.6f}h, {:.6f}°",
                            currentRADECJNow_.ra, currentRADECJNow_.dec);
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
    
    device_.watchProperty("HORIZONTAL_COORD",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 2) {
                currentAZALT_.az = property[0].getValue();
                currentAZALT_.alt = property[1].getValue();
                spdlog::trace("AZ/ALT updated: {:.6f}°, {:.6f}°",
                            currentAZALT_.az, currentAZALT_.alt);
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeCoordinates::watchLocationProperties() -> void {
    spdlog::debug("Setting up location property watchers");
    
    device_.watchProperty("GEOGRAPHIC_COORD",
        [this](const INDI::PropertyNumber& property) {
            if (property.isValid() && property.count() >= 3) {
                location_.latitude = property[0].getValue();
                location_.longitude = property[1].getValue();
                location_.elevation = property[2].getValue();
                spdlog::debug("Location updated: lat={:.6f}°, lon={:.6f}°, elev={:.1f}m",
                            location_.latitude, location_.longitude, location_.elevation);
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeCoordinates::watchTimeProperties() -> void {
    spdlog::debug("Setting up time property watchers");
    
    device_.watchProperty("TIME_UTC",
        [this](const INDI::PropertyText& property) {
            if (property.isValid()) {
                // Parse time string and update utcTime_
                spdlog::debug("UTC time updated: {}", property[0].getText());
            }
        }, INDI::BaseDevice::WATCH_UPDATE);
}

auto TelescopeCoordinates::updateCurrentCoordinates() -> void {
    // Update all coordinate systems
    getRADECJ2000();
    getRADECJNow();
    getAZALT();
}
