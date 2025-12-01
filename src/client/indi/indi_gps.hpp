/*
 * indi_gps.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI GPS Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_GPS_HPP
#define LITHIUM_CLIENT_INDI_GPS_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief GPS state enumeration
 */
enum class GPSState : uint8_t { Idle, Acquiring, Locked, Error, Unknown };

/**
 * @brief GPS fix type
 */
enum class GPSFixType : uint8_t { NoFix, Fix2D, Fix3D, DGPS, Unknown };

/**
 * @brief GPS position information
 */
struct GPSPosition {
    double latitude{0.0};   // Latitude in degrees
    double longitude{0.0};  // Longitude in degrees
    double elevation{0.0};  // Elevation in meters
    double accuracy{0.0};   // Position accuracy in meters

    [[nodiscard]] auto toJson() const -> json {
        return {{"latitude", latitude},
                {"longitude", longitude},
                {"elevation", elevation},
                {"accuracy", accuracy}};
    }
};

/**
 * @brief GPS time information
 */
struct GPSTime {
    int year{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    double second{0.0};
    double utcOffset{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"year", year},     {"month", month},   {"day", day},
                {"hour", hour},     {"minute", minute}, {"second", second},
                {"utcOffset", utcOffset}};
    }
};

/**
 * @brief GPS satellite information
 */
struct GPSSatelliteInfo {
    int satellitesInView{0};
    int satellitesUsed{0};
    double hdop{0.0};  // Horizontal dilution of precision
    double vdop{0.0};  // Vertical dilution of precision
    double pdop{0.0};  // Position dilution of precision

    [[nodiscard]] auto toJson() const -> json {
        return {{"satellitesInView", satellitesInView},
                {"satellitesUsed", satellitesUsed},
                {"hdop", hdop},
                {"vdop", vdop},
                {"pdop", pdop}};
    }
};

/**
 * @brief INDI GPS Device
 *
 * Provides GPS functionality including:
 * - Position (lat/lon/elevation)
 * - Time synchronization
 * - Satellite information
 */
class INDIGPS : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI GPS
     * @param name Device name
     */
    explicit INDIGPS(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIGPS() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "GPS";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Position ====================

    /**
     * @brief Get GPS position
     * @return Position info
     */
    [[nodiscard]] auto getPosition() const -> GPSPosition;

    /**
     * @brief Get latitude
     * @return Latitude in degrees
     */
    [[nodiscard]] auto getLatitude() const -> std::optional<double>;

    /**
     * @brief Get longitude
     * @return Longitude in degrees
     */
    [[nodiscard]] auto getLongitude() const -> std::optional<double>;

    /**
     * @brief Get elevation
     * @return Elevation in meters
     */
    [[nodiscard]] auto getElevation() const -> std::optional<double>;

    // ==================== Time ====================

    /**
     * @brief Get GPS time
     * @return Time info
     */
    [[nodiscard]] auto getTime() const -> GPSTime;

    /**
     * @brief Sync system time to GPS
     * @return true if synced
     */
    auto syncSystemTime() -> bool;

    // ==================== Satellite Info ====================

    /**
     * @brief Get satellite information
     * @return Satellite info
     */
    [[nodiscard]] auto getSatelliteInfo() const -> GPSSatelliteInfo;

    /**
     * @brief Get fix type
     * @return Fix type
     */
    [[nodiscard]] auto getFixType() const -> GPSFixType;

    /**
     * @brief Check if GPS has fix
     * @return true if has fix
     */
    [[nodiscard]] auto hasFix() const -> bool;

    // ==================== Refresh ====================

    /**
     * @brief Refresh GPS data
     * @return true if refresh started
     */
    auto refresh() -> bool;

    // ==================== Status ====================

    /**
     * @brief Get GPS state
     * @return GPS state
     */
    [[nodiscard]] auto getGPSState() const -> GPSState;

    /**
     * @brief Get GPS status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handlePositionProperty(const INDIProperty& property);
    void handleTimeProperty(const INDIProperty& property);
    void handleSatelliteProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // GPS state
    std::atomic<GPSState> gpsState_{GPSState::Idle};
    std::atomic<GPSFixType> fixType_{GPSFixType::NoFix};

    // Position
    GPSPosition position_;
    mutable std::mutex positionMutex_;

    // Time
    GPSTime gpsTime_;
    mutable std::mutex timeMutex_;

    // Satellite info
    GPSSatelliteInfo satelliteInfo_;
    mutable std::mutex satelliteMutex_;
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_GPS_HPP
