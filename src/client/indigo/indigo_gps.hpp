/*
 * indigo_gps.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO GPS Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_GPS_HPP
#define LITHIUM_CLIENT_INDIGO_GPS_HPP

#include "indigo_device_base.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace lithium::client::indigo {

/**
 * @brief GPS fix status
 */
enum class GPSFixStatus : uint8_t {
    NoFix,          // No fix
    Fix2D,          // 2D fix
    Fix3D           // 3D fix
};

/**
 * @brief Geographic coordinates
 */
struct GeographicCoordinates {
    double latitude{0.0};       // Latitude in degrees (-90 to 90)
    double longitude{0.0};      // Longitude in degrees (-180 to 180)
    double elevation{0.0};      // Elevation in meters
};

/**
 * @brief Horizontal dilution of precision
 */
struct DilutionOfPrecision {
    double hdop{0.0};           // Horizontal DOP
    double vdop{0.0};           // Vertical DOP
    double pdop{0.0};           // Position DOP
};

/**
 * @brief GPS status information
 */
struct GPSStatus {
    bool fixed{false};                  // Whether GPS has fix
    GPSFixStatus fixType{GPSFixStatus::NoFix};
    int satellitesUsed{0};              // Number of satellites used
    int satellitesVisible{0};           // Number of satellites visible
    DilutionOfPrecision dop;            // Dilution of precision values
};

/**
 * @brief UTC time information
 */
struct UTCTime {
    int year{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    double fractionalSecond{0.0};
};

/**
 * @brief GPS UTC time callback
 */
using GPSTimeCallback = std::function<void(const UTCTime&)>;

/**
 * @brief GPS position callback
 */
using GPSPositionCallback = std::function<void(const GeographicCoordinates&)>;

/**
 * @brief GPS status callback
 */
using GPSStatusCallback = std::function<void(const GPSStatus&)>;

/**
 * @brief INDIGO GPS Device
 *
 * Provides GPS positioning and time synchronization functionality for
 * INDIGO-connected GPS devices:
 * - Geographic coordinates (latitude, longitude, elevation)
 * - UTC time information
 * - Satellite tracking and fix status
 * - Horizontal/vertical dilution of precision
 * - Time synchronization
 */
class INDIGOGPS : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOGPS(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOGPS() override;

    // ==================== Geographic Coordinates ====================

    /**
     * @brief Get geographic coordinates
     */
    [[nodiscard]] auto getCoordinates() const -> GeographicCoordinates;

    /**
     * @brief Get latitude in degrees
     */
    [[nodiscard]] auto getLatitude() const -> double;

    /**
     * @brief Get longitude in degrees
     */
    [[nodiscard]] auto getLongitude() const -> double;

    /**
     * @brief Get elevation in meters
     */
    [[nodiscard]] auto getElevation() const -> double;

    /**
     * @brief Set geographic coordinates
     * @param coordinates New coordinates to set
     * @return Success or error
     */
    auto setCoordinates(const GeographicCoordinates& coordinates)
        -> DeviceResult<bool>;

    // ==================== UTC Time ====================

    /**
     * @brief Get UTC time from GPS
     */
    [[nodiscard]] auto getUTCTime() const -> UTCTime;

    /**
     * @brief Get UTC time as ISO 8601 string
     */
    [[nodiscard]] auto getUTCTimeString() const -> std::string;

    /**
     * @brief Parse UTC time string from INDIGO format
     * @param timeStr Time string (e.g., "2024-12-07T12:30:45")
     * @return Parsed UTC time
     */
    static auto parseUTCTime(const std::string& timeStr) -> UTCTime;

    // ==================== GPS Status ====================

    /**
     * @brief Get GPS fix status
     */
    [[nodiscard]] auto getFixStatus() const -> GPSFixStatus;

    /**
     * @brief Check if GPS has a valid fix
     */
    [[nodiscard]] auto hasFixStatus() const -> bool;

    /**
     * @brief Get full GPS status information
     */
    [[nodiscard]] auto getGPSStatus() const -> GPSStatus;

    /**
     * @brief Get number of satellites used for fix
     */
    [[nodiscard]] auto getSatellitesUsed() const -> int;

    /**
     * @brief Get number of satellites visible
     */
    [[nodiscard]] auto getSatellitesVisible() const -> int;

    /**
     * @brief Get dilution of precision values
     */
    [[nodiscard]] auto getDOP() const -> DilutionOfPrecision;

    /**
     * @brief Get horizontal dilution of precision
     */
    [[nodiscard]] auto getHDOP() const -> double;

    /**
     * @brief Get vertical dilution of precision
     */
    [[nodiscard]] auto getVDOP() const -> double;

    /**
     * @brief Get position dilution of precision
     */
    [[nodiscard]] auto getPDOP() const -> double;

    // ==================== Refresh/Update ====================

    /**
     * @brief Request GPS data refresh
     * @return Success or error
     */
    auto refresh() -> DeviceResult<bool>;

    /**
     * @brief Get last update time
     */
    [[nodiscard]] auto getLastUpdateTime() const -> std::chrono::system_clock::time_point;

    // ==================== Callbacks ====================

    /**
     * @brief Register callback for time updates
     */
    void onTimeUpdate(GPSTimeCallback callback);

    /**
     * @brief Register callback for position updates
     */
    void onPositionUpdate(GPSPositionCallback callback);

    /**
     * @brief Register callback for status updates
     */
    void onStatusUpdate(GPSStatusCallback callback);

    // ==================== Utility ====================

    /**
     * @brief Get GPS status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

    /**
     * @brief Get GPS capabilities as JSON
     */
    [[nodiscard]] auto getCapabilities() const -> json;

    /**
     * @brief Convert fix status to string
     */
    static auto fixStatusToString(GPSFixStatus status) -> std::string_view;

    /**
     * @brief Convert string to fix status
     */
    static auto fixStatusFromString(std::string_view str) -> GPSFixStatus;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> gpsImpl_;
};

// ============================================================================
// Fix status conversion helpers
// ============================================================================

[[nodiscard]] constexpr auto fixStatusToString(GPSFixStatus status) noexcept
    -> std::string_view {
    switch (status) {
        case GPSFixStatus::NoFix: return "NoFix";
        case GPSFixStatus::Fix2D: return "2D";
        case GPSFixStatus::Fix3D: return "3D";
        default: return "NoFix";
    }
}

[[nodiscard]] auto fixStatusFromString(std::string_view str) -> GPSFixStatus;

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_GPS_HPP
