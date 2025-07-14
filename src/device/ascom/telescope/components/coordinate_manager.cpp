#include "coordinate_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <chrono>

namespace lithium::device::ascom::telescope::components {

CoordinateManager::CoordinateManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {

    auto logger = spdlog::get("telescope_coords");

    if (logger) {
        logger->info("ASCOM Telescope CoordinateManager initialized");
    }
}

CoordinateManager::~CoordinateManager() = default;

// =========================================================================
// Coordinate Retrieval
// =========================================================================

std::optional<EquatorialCoordinates> CoordinateManager::getRADECJ2000() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get coordinates from hardware_
        // For now, return dummy coordinates
        if (logger) logger->debug("Getting J2000 RA/DEC coordinates");

        EquatorialCoordinates coords;
        coords.ra = 0.0;   // Hours
        coords.dec = 0.0;  // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to get J2000 coordinates: " + std::string(e.what()));
        if (logger) logger->error("Failed to get J2000 coordinates: {}", e.what());
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::getRADECJNow() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get current epoch coordinates from hardware_
        if (logger) logger->debug("Getting JNow RA/DEC coordinates");

        EquatorialCoordinates coords;
        coords.ra = 0.0;   // Hours
        coords.dec = 0.0;  // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to get JNow coordinates: " + std::string(e.what()));
        if (logger) logger->error("Failed to get JNow coordinates: {}", e.what());
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::getTargetRADEC() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get target coordinates from hardware_
        if (logger) logger->debug("Getting target RA/DEC coordinates");

        EquatorialCoordinates coords;
        coords.ra = 0.0;   // Hours
        coords.dec = 0.0;  // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to get target coordinates: " + std::string(e.what()));
        if (logger) logger->error("Failed to get target coordinates: {}", e.what());
        return std::nullopt;
    }
}

std::optional<HorizontalCoordinates> CoordinateManager::getAZALT() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get horizontal coordinates from hardware_
        if (logger) logger->debug("Getting AZ/ALT coordinates");

        HorizontalCoordinates coords;
        coords.az = 0.0;   // Degrees
        coords.alt = 0.0;  // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to get AZ/ALT coordinates: " + std::string(e.what()));
        if (logger) logger->error("Failed to get AZ/ALT coordinates: {}", e.what());
        return std::nullopt;
    }
}

// =========================================================================
// Location and Time Management
// =========================================================================

std::optional<GeographicLocation> CoordinateManager::getLocation() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get location from hardware_
        if (logger) logger->debug("Getting observer location");

        GeographicLocation location;
        location.latitude = 0.0;   // Degrees
        location.longitude = 0.0;  // Degrees
        location.elevation = 0.0;  // Meters

        clearError();
        return location;

    } catch (const std::exception& e) {
        setLastError("Failed to get location: " + std::string(e.what()));
        if (logger) logger->error("Failed to get location: {}", e.what());
        return std::nullopt;
    }
}

bool CoordinateManager::setLocation(const GeographicLocation& location) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }

    if (location.latitude < -90.0 || location.latitude > 90.0) {
        setLastError("Invalid latitude");
        if (logger) logger->error("Invalid latitude: {:.6f}", location.latitude);
        return false;
    }

    if (location.longitude < -180.0 || location.longitude > 180.0) {
        setLastError("Invalid longitude");
        if (logger) logger->error("Invalid longitude: {:.6f}", location.longitude);
        return false;
    }

    try {
        // Implementation would set location in hardware_
        if (logger) {
            logger->info("Setting observer location: Lat={:.6f}°, Lon={:.6f}°, Elev={:.1f}m",
                         location.latitude, location.longitude, location.elevation);
        }

        clearError();
        return true;

    } catch (const std::exception& e) {
        setLastError("Failed to set location: " + std::string(e.what()));
        if (logger) logger->error("Failed to set location: {}", e.what());
        return false;
    }
}

std::optional<std::chrono::system_clock::time_point> CoordinateManager::getUTCTime() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get UTC time from hardware_
        // For now, return current system time
        auto now = std::chrono::system_clock::now();

        if (logger) logger->debug("Getting UTC time");

        clearError();
        return now;

    } catch (const std::exception& e) {
        setLastError("Failed to get UTC time: " + std::string(e.what()));
        if (logger) logger->error("Failed to get UTC time: {}", e.what());
        return std::nullopt;
    }
}

bool CoordinateManager::setUTCTime(const std::chrono::system_clock::time_point& time) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }

    try {
        // Implementation would set UTC time in hardware_
        if (logger) logger->info("Setting UTC time");

        clearError();
        return true;

    } catch (const std::exception& e) {
        setLastError("Failed to set UTC time: " + std::string(e.what()));
        if (logger) logger->error("Failed to set UTC time: {}", e.what());
        return false;
    }
}

std::optional<std::chrono::system_clock::time_point> CoordinateManager::getLocalTime() {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would get local time from hardware_
        // For now, return current system time
        auto now = std::chrono::system_clock::now();

        if (logger) logger->debug("Getting local time");

        clearError();
        return now;

    } catch (const std::exception& e) {
        setLastError("Failed to get local time: " + std::string(e.what()));
        if (logger) logger->error("Failed to get local time: {}", e.what());
        return std::nullopt;
    }
}

// =========================================================================
// Coordinate Transformations
// =========================================================================

std::optional<HorizontalCoordinates> CoordinateManager::convertRADECToAZALT(double ra, double dec) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would perform coordinate transformation
        if (logger) logger->debug("Converting RA/DEC to AZ/ALT: RA={:.6f}h, DEC={:.6f}°", ra, dec);

        // Placeholder transformation
        HorizontalCoordinates coords;
        coords.az = 180.0;   // Degrees
        coords.alt = 45.0;   // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to convert RA/DEC to AZ/ALT: " + std::string(e.what()));
        if (logger) logger->error("Failed to convert RA/DEC to AZ/ALT: {}", e.what());
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::convertAZALTToRADEC(double az, double alt) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return std::nullopt;
    }

    try {
        // Implementation would perform coordinate transformation
        if (logger) logger->debug("Converting AZ/ALT to RA/DEC: AZ={:.6f}°, ALT={:.6f}°", az, alt);

        // Placeholder transformation
        EquatorialCoordinates coords;
        coords.ra = 12.0;   // Hours
        coords.dec = 45.0;  // Degrees

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to convert AZ/ALT to RA/DEC: " + std::string(e.what()));
        if (logger) logger->error("Failed to convert AZ/ALT to RA/DEC: {}", e.what());
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::convertJ2000ToJNow(double ra_j2000, double dec_j2000) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    try {
        // Implementation would perform precession calculation
        if (logger) logger->debug("Converting J2000 to JNow: RA={:.6f}h, DEC={:.6f}°", ra_j2000, dec_j2000);

        // Simplified precession - in reality this would use proper IAU algorithms
        EquatorialCoordinates coords;
        coords.ra = ra_j2000;    // Hours (simplified, no precession applied)
        coords.dec = dec_j2000;  // Degrees (simplified, no precession applied)

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to convert J2000 to JNow: " + std::string(e.what()));
        if (logger) logger->error("Failed to convert J2000 to JNow: {}", e.what());
        return std::nullopt;
    }
}

std::optional<EquatorialCoordinates> CoordinateManager::convertJNowToJ2000(double ra_jnow, double dec_jnow) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_coords");

    try {
        // Implementation would perform inverse precession calculation
        if (logger) logger->debug("Converting JNow to J2000: RA={:.6f}h, DEC={:.6f}°", ra_jnow, dec_jnow);

        // Simplified precession - in reality this would use proper IAU algorithms
        EquatorialCoordinates coords;
        coords.ra = ra_jnow;    // Hours (simplified, no precession applied)
        coords.dec = dec_jnow;  // Degrees (simplified, no precession applied)

        clearError();
        return coords;

    } catch (const std::exception& e) {
        setLastError("Failed to convert JNow to J2000: " + std::string(e.what()));
        if (logger) logger->error("Failed to convert JNow to J2000: {}", e.what());
        return std::nullopt;
    }
}

// =========================================================================
// Utility Methods
// =========================================================================

std::tuple<int, int, double> CoordinateManager::degreesToDMS(double degrees) {
    bool negative = degrees < 0.0;
    degrees = std::abs(degrees);

    int deg = static_cast<int>(degrees);
    double remaining = (degrees - deg) * 60.0;
    int min = static_cast<int>(remaining);
    double sec = (remaining - min) * 60.0;

    if (negative) {
        deg = -deg;
    }

    return std::make_tuple(deg, min, sec);
}

std::tuple<int, int, double> CoordinateManager::degreesToHMS(double degrees) {
    // Convert degrees to hours first
    double hours = degrees / 15.0;

    int hr = static_cast<int>(hours);
    double remaining = (hours - hr) * 60.0;
    int min = static_cast<int>(remaining);
    double sec = (remaining - min) * 60.0;

    return std::make_tuple(hr, min, sec);
}

double CoordinateManager::calculateAngularSeparation(double ra1, double dec1, double ra2, double dec2) {
    // Convert to radians
    const double deg_to_rad = M_PI / 180.0;
    const double hour_to_rad = M_PI / 12.0;

    double ra1_rad = ra1 * hour_to_rad;
    double dec1_rad = dec1 * deg_to_rad;
    double ra2_rad = ra2 * hour_to_rad;
    double dec2_rad = dec2 * deg_to_rad;

    // Use spherical law of cosines
    double cos_sep = std::sin(dec1_rad) * std::sin(dec2_rad) +
                     std::cos(dec1_rad) * std::cos(dec2_rad) * std::cos(ra1_rad - ra2_rad);

    // Clamp to valid range to avoid numerical errors
    cos_sep = std::max(-1.0, std::min(1.0, cos_sep));

    double separation_rad = std::acos(cos_sep);
    return separation_rad * 180.0 / M_PI; // Return in degrees
}

std::string CoordinateManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void CoordinateManager::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

// Private helper methods
void CoordinateManager::setLastError(const std::string& error) const {
    lastError_ = error;
}

} // namespace lithium::device::ascom::telescope::components
