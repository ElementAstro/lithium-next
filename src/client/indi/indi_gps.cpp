/*
 * indi_gps.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI GPS Device Implementation

**************************************************/

#include "indi_gps.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIGPS::INDIGPS(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIGPS created: {}", name_);
}

INDIGPS::~INDIGPS() { LOG_DEBUG("INDIGPS destroyed: {}", name_); }

// ==================== Connection ====================

auto INDIGPS::connect(const std::string& deviceName, int timeout,
                      int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("GPS {} connected", deviceName);
    return true;
}

auto INDIGPS::disconnect() -> bool { return INDIDeviceBase::disconnect(); }

// ==================== Position ====================

auto INDIGPS::getPosition() const -> GPSPosition {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return position_;
}

auto INDIGPS::getLatitude() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return position_.latitude;
}

auto INDIGPS::getLongitude() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return position_.longitude;
}

auto INDIGPS::getElevation() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return position_.elevation;
}

// ==================== Time ====================

auto INDIGPS::getTime() const -> GPSTime {
    std::lock_guard<std::mutex> lock(timeMutex_);
    return gpsTime_;
}

auto INDIGPS::syncSystemTime() -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setSwitchProperty("TIME_SYNC", "SYNC", true)) {
        LOG_ERROR("Failed to sync system time");
        return false;
    }

    return true;
}

// ==================== Satellite Info ====================

auto INDIGPS::getSatelliteInfo() const -> GPSSatelliteInfo {
    std::lock_guard<std::mutex> lock(satelliteMutex_);
    return satelliteInfo_;
}

auto INDIGPS::getFixType() const -> GPSFixType { return fixType_.load(); }

auto INDIGPS::hasFix() const -> bool {
    auto fix = fixType_.load();
    return fix == GPSFixType::Fix2D || fix == GPSFixType::Fix3D ||
           fix == GPSFixType::DGPS;
}

// ==================== Refresh ====================

auto INDIGPS::refresh() -> bool {
    if (!isConnected()) {
        return false;
    }

    gpsState_.store(GPSState::Acquiring);

    if (!setSwitchProperty("GPS_REFRESH", "REFRESH", true)) {
        LOG_ERROR("Failed to refresh GPS data");
        gpsState_.store(GPSState::Error);
        return false;
    }

    return true;
}

// ==================== Status ====================

auto INDIGPS::getGPSState() const -> GPSState { return gpsState_.load(); }

auto INDIGPS::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["gpsState"] = static_cast<int>(gpsState_.load());
    status["fixType"] = static_cast<int>(fixType_.load());
    status["hasFix"] = hasFix();
    status["position"] = getPosition().toJson();
    status["time"] = getTime().toJson();
    status["satellite"] = getSatelliteInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDIGPS::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "GEOGRAPHIC_COORD") {
        handlePositionProperty(property);
    } else if (property.name == "TIME_UTC") {
        handleTimeProperty(property);
    } else if (property.name == "GPS_STATUS") {
        handleSatelliteProperty(property);
    }
}

void INDIGPS::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "GEOGRAPHIC_COORD") {
        handlePositionProperty(property);
    } else if (property.name == "TIME_UTC") {
        handleTimeProperty(property);
    } else if (property.name == "GPS_STATUS") {
        handleSatelliteProperty(property);

        if (property.state == PropertyState::Ok) {
            gpsState_.store(GPSState::Locked);
        } else if (property.state == PropertyState::Busy) {
            gpsState_.store(GPSState::Acquiring);
        } else if (property.state == PropertyState::Alert) {
            gpsState_.store(GPSState::Error);
        }
    }
}

// ==================== Internal Methods ====================

void INDIGPS::handlePositionProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "LAT") {
            position_.latitude = elem.value;
        } else if (elem.name == "LONG") {
            position_.longitude = elem.value;
        } else if (elem.name == "ELEV") {
            position_.elevation = elem.value;
        }
    }
}

void INDIGPS::handleTimeProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(timeMutex_);

    for (const auto& elem : property.texts) {
        if (elem.name == "UTC") {
            // Parse UTC time string (format: YYYY-MM-DDTHH:MM:SS)
            // This is a simplified parser
            if (elem.value.length() >= 19) {
                gpsTime_.year = std::stoi(elem.value.substr(0, 4));
                gpsTime_.month = std::stoi(elem.value.substr(5, 2));
                gpsTime_.day = std::stoi(elem.value.substr(8, 2));
                gpsTime_.hour = std::stoi(elem.value.substr(11, 2));
                gpsTime_.minute = std::stoi(elem.value.substr(14, 2));
                gpsTime_.second = std::stod(elem.value.substr(17, 2));
            }
        }
    }

    for (const auto& elem : property.numbers) {
        if (elem.name == "OFFSET") {
            gpsTime_.utcOffset = elem.value;
        }
    }
}

void INDIGPS::handleSatelliteProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(satelliteMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "GPS_FIX") {
            int fix = static_cast<int>(elem.value);
            switch (fix) {
                case 0:
                    fixType_.store(GPSFixType::NoFix);
                    break;
                case 1:
                    fixType_.store(GPSFixType::Fix2D);
                    break;
                case 2:
                    fixType_.store(GPSFixType::Fix3D);
                    break;
                case 3:
                    fixType_.store(GPSFixType::DGPS);
                    break;
                default:
                    fixType_.store(GPSFixType::Unknown);
                    break;
            }
        } else if (elem.name == "GPS_SATELLITES_IN_VIEW") {
            satelliteInfo_.satellitesInView = static_cast<int>(elem.value);
        } else if (elem.name == "GPS_SATELLITES_USED") {
            satelliteInfo_.satellitesUsed = static_cast<int>(elem.value);
        } else if (elem.name == "GPS_HDOP") {
            satelliteInfo_.hdop = elem.value;
        } else if (elem.name == "GPS_VDOP") {
            satelliteInfo_.vdop = elem.value;
        } else if (elem.name == "GPS_PDOP") {
            satelliteInfo_.pdop = elem.value;
        }
    }
}

void INDIGPS::setupPropertyWatchers() {
    // Watch position
    watchProperty("GEOGRAPHIC_COORD", [this](const INDIProperty& prop) {
        handlePositionProperty(prop);
    });

    // Watch time
    watchProperty("TIME_UTC", [this](const INDIProperty& prop) {
        handleTimeProperty(prop);
    });

    // Watch GPS status
    watchProperty("GPS_STATUS", [this](const INDIProperty& prop) {
        handleSatelliteProperty(prop);
    });
}

}  // namespace lithium::client::indi
