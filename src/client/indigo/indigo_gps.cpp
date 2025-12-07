/*
 * indigo_gps.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO GPS Device Implementation

**************************************************/

#include "indigo_gps.hpp"

#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// Fix status conversion helpers
// ============================================================================

auto fixStatusFromString(std::string_view str) -> GPSFixStatus {
    if (str == "2D" || str == "Fix2D") return GPSFixStatus::Fix2D;
    if (str == "3D" || str == "Fix3D") return GPSFixStatus::Fix3D;
    return GPSFixStatus::NoFix;
}

// ============================================================================
// INDIGOGPS Implementation
// ============================================================================

class INDIGOGPS::Impl {
public:
    explicit Impl(INDIGOGPS* parent) : parent_(parent) {}

    void onConnected() {
        // Update cached GPS data on connection
        updateCoordinates();
        updateTime();
        updateStatus();

        LOG_F(INFO, "INDIGO GPS[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        LOG_F(INFO, "INDIGO GPS[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "GPS_GEOGRAPHIC_COORDINATES") {
            updateCoordinates();
            handleCoordinateUpdate();
        } else if (name == "GPS_UTC_TIME") {
            updateTime();
            handleTimeUpdate();
        } else if (name == "GPS_STATUS") {
            updateStatus();
            handleStatusUpdate();
        }
    }

    [[nodiscard]] auto getCoordinates() const -> GeographicCoordinates {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return coordinates_;
    }

    [[nodiscard]] auto getLatitude() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return coordinates_.latitude;
    }

    [[nodiscard]] auto getLongitude() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return coordinates_.longitude;
    }

    [[nodiscard]] auto getElevation() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return coordinates_.elevation;
    }

    auto setCoordinates(const GeographicCoordinates& coordinates)
        -> DeviceResult<bool> {
        // Validate coordinates
        if (coordinates.latitude < -90.0 || coordinates.latitude > 90.0) {
            return DeviceError{DeviceErrorCode::InvalidParameter,
                               "Latitude must be between -90 and 90"};
        }
        if (coordinates.longitude < -180.0 || coordinates.longitude > 180.0) {
            return DeviceError{DeviceErrorCode::InvalidParameter,
                               "Longitude must be between -180 and 180"};
        }

        // Set latitude
        auto latResult = parent_->setNumberProperty("GPS_GEOGRAPHIC_COORDINATES",
                                                    "LAT", coordinates.latitude);
        if (!latResult.has_value()) {
            return latResult;
        }

        // Set longitude
        auto lonResult = parent_->setNumberProperty("GPS_GEOGRAPHIC_COORDINATES",
                                                    "LONG", coordinates.longitude);
        if (!lonResult.has_value()) {
            return lonResult;
        }

        // Set elevation
        auto elevResult = parent_->setNumberProperty(
            "GPS_GEOGRAPHIC_COORDINATES", "ELEV", coordinates.elevation);

        if (elevResult.has_value()) {
            std::lock_guard<std::mutex> lock(dataMutex_);
            coordinates_ = coordinates;
            lastUpdate_ = std::chrono::system_clock::now();

            LOG_F(INFO,
                  "INDIGO GPS[{}]: Coordinates set to Lat: {:.6f}, "
                  "Lon: {:.6f}, Elev: {:.1f}m",
                  parent_->getINDIGODeviceName(), coordinates.latitude,
                  coordinates.longitude, coordinates.elevation);
        }

        return elevResult;
    }

    [[nodiscard]] auto getUTCTime() const -> UTCTime {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return utcTime_;
    }

    [[nodiscard]] auto getUTCTimeString() const -> std::string {
        std::lock_guard<std::mutex> lock(dataMutex_);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(4) << utcTime_.year << "-"
            << std::setw(2) << utcTime_.month << "-" << std::setw(2)
            << utcTime_.day << "T" << std::setw(2) << utcTime_.hour << ":"
            << std::setw(2) << utcTime_.minute << ":" << std::setw(2)
            << utcTime_.second;

        if (utcTime_.fractionalSecond > 0.0) {
            oss << "." << std::setprecision(3) << std::fixed
                << utcTime_.fractionalSecond;
        }

        oss << "Z";
        return oss.str();
    }

    static auto parseUTCTime(const std::string& timeStr) -> UTCTime {
        UTCTime time{};

        // Parse ISO 8601 format: "2024-12-07T12:30:45.123Z"
        try {
            size_t pos = 0;

            // Year
            time.year = std::stoi(timeStr.substr(pos, 4));
            pos += 5;  // skip "-"

            // Month
            time.month = std::stoi(timeStr.substr(pos, 2));
            pos += 3;  // skip "-"

            // Day
            time.day = std::stoi(timeStr.substr(pos, 2));
            pos += 3;  // skip "T"

            // Hour
            time.hour = std::stoi(timeStr.substr(pos, 2));
            pos += 3;  // skip ":"

            // Minute
            time.minute = std::stoi(timeStr.substr(pos, 2));
            pos += 3;  // skip ":"

            // Second
            time.second = std::stoi(timeStr.substr(pos, 2));
            pos += 2;

            // Fractional seconds
            if (pos < timeStr.length() && timeStr[pos] == '.') {
                pos++;
                size_t fracEnd = timeStr.find_first_of("Zz", pos);
                if (fracEnd != std::string::npos) {
                    std::string fracStr = timeStr.substr(pos, fracEnd - pos);
                    time.fractionalSecond = std::stod("0." + fracStr);
                }
            }
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Failed to parse UTC time string: {}", timeStr);
        }

        return time;
    }

    [[nodiscard]] auto getFixStatus() const -> GPSFixStatus {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.fixType;
    }

    [[nodiscard]] auto hasFixStatus() const -> bool {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.fixed;
    }

    [[nodiscard]] auto getGPSStatus() const -> GPSStatus {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_;
    }

    [[nodiscard]] auto getSatellitesUsed() const -> int {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.satellitesUsed;
    }

    [[nodiscard]] auto getSatellitesVisible() const -> int {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.satellitesVisible;
    }

    [[nodiscard]] auto getDOP() const -> DilutionOfPrecision {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.dop;
    }

    [[nodiscard]] auto getHDOP() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.dop.hdop;
    }

    [[nodiscard]] auto getVDOP() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.dop.vdop;
    }

    [[nodiscard]] auto getPDOP() const -> double {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return gpsStatus_.dop.pdop;
    }

    auto refresh() -> DeviceResult<bool> {
        // Try to trigger GPS refresh/update via switch property
        auto result = parent_->setSwitchProperty("GPS_REFRESH", "GPS_REFRESH", true);

        if (result.has_value()) {
            LOG_F(INFO, "INDIGO GPS[{}]: Data refresh requested",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    [[nodiscard]] auto getLastUpdateTime() const
        -> std::chrono::system_clock::time_point {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return lastUpdate_;
    }

    void onTimeUpdate(GPSTimeCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        timeCallbacks_.push_back(std::move(callback));
    }

    void onPositionUpdate(GPSPositionCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        positionCallbacks_.push_back(std::move(callback));
    }

    void onStatusUpdate(GPSStatusCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        statusCallbacks_.push_back(std::move(callback));
    }

    [[nodiscard]] auto getStatus() const -> json {
        std::lock_guard<std::mutex> lock(dataMutex_);

        json status;
        status["connected"] = parent_->isConnected();
        status["lastUpdate"] = lastUpdate_.time_since_epoch().count();

        // Coordinates
        json coords;
        coords["latitude"] = coordinates_.latitude;
        coords["longitude"] = coordinates_.longitude;
        coords["elevation"] = coordinates_.elevation;
        status["coordinates"] = coords;

        // UTC Time
        json time;
        time["year"] = utcTime_.year;
        time["month"] = utcTime_.month;
        time["day"] = utcTime_.day;
        time["hour"] = utcTime_.hour;
        time["minute"] = utcTime_.minute;
        time["second"] = utcTime_.second;
        time["fractionalSecond"] = utcTime_.fractionalSecond;
        status["utcTime"] = time;

        // GPS Status
        json gpsStatus;
        gpsStatus["fixed"] = gpsStatus_.fixed;
        gpsStatus["fixType"] =
            std::string(fixStatusToString(gpsStatus_.fixType));
        gpsStatus["satellitesUsed"] = gpsStatus_.satellitesUsed;
        gpsStatus["satellitesVisible"] = gpsStatus_.satellitesVisible;

        json dop;
        dop["hdop"] = gpsStatus_.dop.hdop;
        dop["vdop"] = gpsStatus_.dop.vdop;
        dop["pdop"] = gpsStatus_.dop.pdop;
        gpsStatus["dop"] = dop;

        status["gpsStatus"] = gpsStatus;

        return status;
    }

    [[nodiscard]] auto getCapabilities() const -> json {
        json caps;

        // Check for coordinate property
        auto coordsProp = parent_->getProperty("GPS_GEOGRAPHIC_COORDINATES");
        caps["hasCoordinates"] = coordsProp.has_value();

        // Check for time property
        auto timeProp = parent_->getProperty("GPS_UTC_TIME");
        caps["hasTime"] = timeProp.has_value();

        // Check for status property
        auto statusProp = parent_->getProperty("GPS_STATUS");
        caps["hasStatus"] = statusProp.has_value();

        // Check for refresh property
        auto refreshProp = parent_->getProperty("GPS_REFRESH");
        caps["hasRefresh"] = refreshProp.has_value();

        return caps;
    }

private:
    void updateCoordinates() {
        std::lock_guard<std::mutex> lock(dataMutex_);

        auto latResult = parent_->getNumberValue("GPS_GEOGRAPHIC_COORDINATES",
                                                 "LAT");
        if (latResult.has_value()) {
            coordinates_.latitude = latResult.value();
        }

        auto lonResult = parent_->getNumberValue("GPS_GEOGRAPHIC_COORDINATES",
                                                 "LONG");
        if (lonResult.has_value()) {
            coordinates_.longitude = lonResult.value();
        }

        auto elevResult = parent_->getNumberValue("GPS_GEOGRAPHIC_COORDINATES",
                                                  "ELEV");
        if (elevResult.has_value()) {
            coordinates_.elevation = elevResult.value();
        }

        lastUpdate_ = std::chrono::system_clock::now();
    }

    void updateTime() {
        std::lock_guard<std::mutex> lock(dataMutex_);

        auto timeResult = parent_->getTextValue("GPS_UTC_TIME", "UTC");
        if (timeResult.has_value()) {
            utcTime_ = parseUTCTime(timeResult.value());
        }
    }

    void updateStatus() {
        std::lock_guard<std::mutex> lock(dataMutex_);

        // Get fix status
        auto fixResult = parent_->getSwitchValue("GPS_STATUS", "FIX");
        gpsStatus_.fixed = fixResult.value_or(false);

        // Determine fix type based on available switches
        auto fix2dResult = parent_->getSwitchValue("GPS_STATUS", "FIX_2D");
        auto fix3dResult = parent_->getSwitchValue("GPS_STATUS", "FIX_3D");

        if (fix3dResult.value_or(false)) {
            gpsStatus_.fixType = GPSFixStatus::Fix3D;
        } else if (fix2dResult.value_or(false)) {
            gpsStatus_.fixType = GPSFixStatus::Fix2D;
        } else {
            gpsStatus_.fixType = GPSFixStatus::NoFix;
        }

        // Get satellite counts
        auto satUsedResult =
            parent_->getNumberValue("GPS_STATUS", "SATS_USED");
        if (satUsedResult.has_value()) {
            gpsStatus_.satellitesUsed = static_cast<int>(satUsedResult.value());
        }

        auto satVisibleResult =
            parent_->getNumberValue("GPS_STATUS", "SATS_VISIBLE");
        if (satVisibleResult.has_value()) {
            gpsStatus_.satellitesVisible =
                static_cast<int>(satVisibleResult.value());
        }

        // Get DOP values
        auto hdopResult = parent_->getNumberValue("GPS_STATUS", "HDOP");
        if (hdopResult.has_value()) {
            gpsStatus_.dop.hdop = hdopResult.value();
        }

        auto vdopResult = parent_->getNumberValue("GPS_STATUS", "VDOP");
        if (vdopResult.has_value()) {
            gpsStatus_.dop.vdop = vdopResult.value();
        }

        auto pdopResult = parent_->getNumberValue("GPS_STATUS", "PDOP");
        if (pdopResult.has_value()) {
            gpsStatus_.dop.pdop = pdopResult.value();
        }
    }

    void handleCoordinateUpdate() {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        std::lock_guard<std::mutex> dataLock(dataMutex_);

        for (const auto& cb : positionCallbacks_) {
            cb(coordinates_);
        }
    }

    void handleTimeUpdate() {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        std::lock_guard<std::mutex> dataLock(dataMutex_);

        for (const auto& cb : timeCallbacks_) {
            cb(utcTime_);
        }
    }

    void handleStatusUpdate() {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        std::lock_guard<std::mutex> dataLock(dataMutex_);

        for (const auto& cb : statusCallbacks_) {
            cb(gpsStatus_);
        }
    }

    INDIGOGPS* parent_;

    mutable std::mutex dataMutex_;
    mutable std::mutex callbackMutex_;

    GeographicCoordinates coordinates_;
    UTCTime utcTime_;
    GPSStatus gpsStatus_;
    std::chrono::system_clock::time_point lastUpdate_;

    std::vector<GPSTimeCallback> timeCallbacks_;
    std::vector<GPSPositionCallback> positionCallbacks_;
    std::vector<GPSStatusCallback> statusCallbacks_;
};

// ============================================================================
// INDIGOGPS public interface
// ============================================================================

INDIGOGPS::INDIGOGPS(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "GPS"),
      gpsImpl_(std::make_unique<Impl>(this)) {}

INDIGOGPS::~INDIGOGPS() = default;

auto INDIGOGPS::getCoordinates() const -> GeographicCoordinates {
    return gpsImpl_->getCoordinates();
}

auto INDIGOGPS::getLatitude() const -> double {
    return gpsImpl_->getLatitude();
}

auto INDIGOGPS::getLongitude() const -> double {
    return gpsImpl_->getLongitude();
}

auto INDIGOGPS::getElevation() const -> double {
    return gpsImpl_->getElevation();
}

auto INDIGOGPS::setCoordinates(const GeographicCoordinates& coordinates)
    -> DeviceResult<bool> {
    return gpsImpl_->setCoordinates(coordinates);
}

auto INDIGOGPS::getUTCTime() const -> UTCTime {
    return gpsImpl_->getUTCTime();
}

auto INDIGOGPS::getUTCTimeString() const -> std::string {
    return gpsImpl_->getUTCTimeString();
}

auto INDIGOGPS::parseUTCTime(const std::string& timeStr) -> UTCTime {
    return Impl::parseUTCTime(timeStr);
}

auto INDIGOGPS::getFixStatus() const -> GPSFixStatus {
    return gpsImpl_->getFixStatus();
}

auto INDIGOGPS::hasFixStatus() const -> bool {
    return gpsImpl_->hasFixStatus();
}

auto INDIGOGPS::getGPSStatus() const -> GPSStatus {
    return gpsImpl_->getGPSStatus();
}

auto INDIGOGPS::getSatellitesUsed() const -> int {
    return gpsImpl_->getSatellitesUsed();
}

auto INDIGOGPS::getSatellitesVisible() const -> int {
    return gpsImpl_->getSatellitesVisible();
}

auto INDIGOGPS::getDOP() const -> DilutionOfPrecision {
    return gpsImpl_->getDOP();
}

auto INDIGOGPS::getHDOP() const -> double {
    return gpsImpl_->getHDOP();
}

auto INDIGOGPS::getVDOP() const -> double {
    return gpsImpl_->getVDOP();
}

auto INDIGOGPS::getPDOP() const -> double {
    return gpsImpl_->getPDOP();
}

auto INDIGOGPS::refresh() -> DeviceResult<bool> {
    return gpsImpl_->refresh();
}

auto INDIGOGPS::getLastUpdateTime() const -> std::chrono::system_clock::time_point {
    return gpsImpl_->getLastUpdateTime();
}

void INDIGOGPS::onTimeUpdate(GPSTimeCallback callback) {
    gpsImpl_->onTimeUpdate(std::move(callback));
}

void INDIGOGPS::onPositionUpdate(GPSPositionCallback callback) {
    gpsImpl_->onPositionUpdate(std::move(callback));
}

void INDIGOGPS::onStatusUpdate(GPSStatusCallback callback) {
    gpsImpl_->onStatusUpdate(std::move(callback));
}

auto INDIGOGPS::getStatus() const -> json {
    return gpsImpl_->getStatus();
}

auto INDIGOGPS::getCapabilities() const -> json {
    return gpsImpl_->getCapabilities();
}

auto INDIGOGPS::fixStatusToString(GPSFixStatus status) -> std::string_view {
    return lithium::client::indigo::fixStatusToString(status);
}

auto INDIGOGPS::fixStatusFromString(std::string_view str) -> GPSFixStatus {
    return lithium::client::indigo::fixStatusFromString(str);
}

void INDIGOGPS::onConnected() {
    gpsImpl_->onConnected();
}

void INDIGOGPS::onDisconnected() {
    gpsImpl_->onDisconnected();
}

void INDIGOGPS::onPropertyUpdated(const Property& property) {
    gpsImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
