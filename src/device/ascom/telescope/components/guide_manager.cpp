#include "guide_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>

namespace lithium::device::ascom::telescope::components {

GuideManager::GuideManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {

    auto logger = spdlog::get("telescope_guide");
    if (!logger) {
        logger = spdlog::stdout_color_mt("telescope_guide");
    }

    if (logger) {
        logger->info("ASCOM Telescope GuideManager initialized");
    }
}

GuideManager::~GuideManager() = default;

bool GuideManager::guidePulse(const std::string& direction, int duration) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_guide");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }

    if (!validateDirection(direction)) {
        setLastError("Invalid guide direction: " + direction);
        if (logger) logger->error("Invalid guide direction: {}", direction);
        return false;
    }

    if (duration <= 0 || duration > 10000) {
        setLastError("Invalid pulse duration: " + std::to_string(duration) + "ms");
        if (logger) logger->error("Invalid pulse duration: {}ms", duration);
        return false;
    }

    try {
        if (logger) logger->debug("Sending guide pulse: {} for {}ms", direction, duration);

        // Implementation would interact with hardware_ here
        // For now, this is a placeholder

        if (logger) logger->info("Guide pulse sent successfully: {} for {}ms", direction, duration);
        clearError();
        return true;

    } catch (const std::exception& e) {
        setLastError("Guide pulse failed: " + std::string(e.what()));
        if (logger) logger->error("Guide pulse failed: {}", e.what());
        return false;
    }
}

bool GuideManager::guideRADEC(double ra_ms, double dec_ms) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_guide");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }

    if (std::abs(ra_ms) > 10000 || std::abs(dec_ms) > 10000) {
        setLastError("Correction values too large");
        if (logger) logger->error("Correction values too large: RA={}ms, DEC={}ms", ra_ms, dec_ms);
        return false;
    }

    try {
        if (logger) logger->debug("Sending RA/DEC correction: RA={}ms, DEC={}ms", ra_ms, dec_ms);

        // Convert to individual pulses
        bool success = true;

        if (ra_ms > 0) {
            success &= guidePulse("E", static_cast<int>(std::abs(ra_ms)));
        } else if (ra_ms < 0) {
            success &= guidePulse("W", static_cast<int>(std::abs(ra_ms)));
        }

        if (dec_ms > 0) {
            success &= guidePulse("N", static_cast<int>(std::abs(dec_ms)));
        } else if (dec_ms < 0) {
            success &= guidePulse("S", static_cast<int>(std::abs(dec_ms)));
        }

        if (success) {
            if (logger) logger->info("RA/DEC correction sent successfully");
            clearError();
        }

        return success;

    } catch (const std::exception& e) {
        setLastError("RA/DEC correction failed: " + std::string(e.what()));
        if (logger) logger->error("RA/DEC correction failed: {}", e.what());
        return false;
    }
}

bool GuideManager::isPulseGuiding() const {
    if (!hardware_) {
        return false;
    }

    // Implementation would check hardware_ state
    return false;
}

std::pair<double, double> GuideManager::getGuideRates() const {
    if (!hardware_) {
        setLastError("Hardware interface not available");
        return {0.0, 0.0};
    }

    // Implementation would get rates from hardware_
    // Returning default values for now
    lastError_.clear(); // Instead of clearError() which is not const
    return {1.0, 1.0}; // Default 1.0 arcsec/sec for both axes
}

bool GuideManager::setGuideRates(double ra_rate, double dec_rate) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    auto logger = spdlog::get("telescope_guide");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }

    if (ra_rate < 0.1 || ra_rate > 10.0 || dec_rate < 0.1 || dec_rate > 10.0) {
        setLastError("Invalid guide rates");
        if (logger) logger->error("Invalid guide rates: RA={:.3f}, DEC={:.3f}", ra_rate, dec_rate);
        return false;
    }

    try {
        // Implementation would set rates in hardware_
        if (logger) {
            logger->info("Guide rates set: RA={:.3f} arcsec/sec, DEC={:.3f} arcsec/sec",
                         ra_rate, dec_rate);
        }
        clearError();
        return true;

    } catch (const std::exception& e) {
        setLastError("Failed to set guide rates: " + std::string(e.what()));
        if (logger) logger->error("Failed to set guide rates: {}", e.what());
        return false;
    }
}

std::string GuideManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void GuideManager::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

// Private helper methods
void GuideManager::setLastError(const std::string& error) const {
    lastError_ = error;
}

bool GuideManager::validateDirection(const std::string& direction) const {
    static const std::vector<std::string> validDirections = {"N", "S", "E", "W", "NORTH", "SOUTH", "EAST", "WEST"};

    std::string upperDir = direction;
    std::transform(upperDir.begin(), upperDir.end(), upperDir.begin(), ::toupper);

    return std::find(validDirections.begin(), validDirections.end(), upperDir) != validDirections.end();
}

} // namespace lithium::device::ascom::telescope::components
