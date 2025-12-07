/*
 * indi_telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Telescope/Mount Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_TELESCOPE_HPP
#define LITHIUM_CLIENT_INDI_TELESCOPE_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief Track mode enumeration
 */
enum class TrackMode : uint8_t { Sidereal, Solar, Lunar, Custom, None };

/**
 * @brief Pier side enumeration
 */
enum class PierSide : uint8_t { East, West, Unknown };

/**
 * @brief Park options enumeration
 */
enum class ParkOption : uint8_t {
    Current,
    Default,
    WriteData,
    PurgeData,
    None
};

/**
 * @brief Slew rate enumeration
 */
enum class SlewRate : uint8_t { Guide, Centering, Find, Max, None };

/**
 * @brief Motion direction (East-West)
 */
enum class MotionEW : uint8_t { West, East, None };

/**
 * @brief Motion direction (North-South)
 */
enum class MotionNS : uint8_t { North, South, None };

/**
 * @brief Dome policy enumeration
 */
enum class DomePolicy : uint8_t { Ignored, Locked, None };

/**
 * @brief Connection mode enumeration
 */
enum class ConnectionMode : uint8_t { Serial, TCP, None };

/**
 * @brief Telescope state enumeration
 */
enum class TelescopeState : uint8_t {
    Idle,
    Slewing,
    Tracking,
    Parking,
    Parked,
    Error,
    Unknown
};

/**
 * @brief Equatorial coordinates
 */
struct EquatorialCoords {
    double ra{0.0};   // Right Ascension in hours
    double dec{0.0};  // Declination in degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"ra", ra}, {"dec", dec}};
    }
};

/**
 * @brief Horizontal coordinates
 */
struct HorizontalCoords {
    double az{0.0};   // Azimuth in degrees
    double alt{0.0};  // Altitude in degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"az", az}, {"alt", alt}};
    }
};

/**
 * @brief Telescope information
 */
struct TelescopeInfo {
    double aperture{0.0};
    double focalLength{0.0};
    double guiderAperture{0.0};
    double guiderFocalLength{0.0};

    [[nodiscard]] auto focalRatio() const -> double {
        return (aperture > 0) ? focalLength / aperture : 0.0;
    }

    [[nodiscard]] auto toJson() const -> json {
        return {{"aperture", aperture},
                {"focalLength", focalLength},
                {"guiderAperture", guiderAperture},
                {"guiderFocalLength", guiderFocalLength},
                {"focalRatio", focalRatio()}};
    }
};

/**
 * @brief Track rate information
 */
struct TrackRateInfo {
    TrackMode mode{TrackMode::Sidereal};
    double raRate{0.0};
    double decRate{0.0};
    bool enabled{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"mode", static_cast<int>(mode)},
                {"raRate", raRate},
                {"decRate", decRate},
                {"enabled", enabled}};
    }
};

/**
 * @brief Park information
 */
struct ParkInfo {
    bool parked{false};
    bool parkEnabled{false};
    double parkRA{0.0};
    double parkDEC{0.0};
    ParkOption option{ParkOption::None};

    [[nodiscard]] auto toJson() const -> json {
        return {{"parked", parked},
                {"parkEnabled", parkEnabled},
                {"parkRA", parkRA},
                {"parkDEC", parkDEC},
                {"option", static_cast<int>(option)}};
    }
};

/**
 * @brief INDI Telescope/Mount Device
 *
 * Provides telescope-specific functionality including:
 * - Coordinate control (RA/DEC, Az/Alt)
 * - Tracking control
 * - Slewing
 * - Parking
 * - Guiding
 */
class INDITelescope : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Telescope
     * @param name Device name
     */
    explicit INDITelescope(std::string name);

    /**
     * @brief Destructor
     */
    ~INDITelescope() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Telescope";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName,
                 int timeout = DEFAULT_TIMEOUT_MS, int maxRetry = 3)
        -> bool override;

    auto disconnect() -> bool override;

    // ==================== Coordinates ====================

    /**
     * @brief Get current RA/DEC (J2000)
     * @return Coordinates, or nullopt if not available
     */
    [[nodiscard]] auto getRADECJ2000() const -> std::optional<EquatorialCoords>;

    /**
     * @brief Set RA/DEC (J2000)
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if set successfully
     */
    auto setRADECJ2000(double ra, double dec) -> bool;

    /**
     * @brief Get current RA/DEC (JNow)
     * @return Coordinates, or nullopt if not available
     */
    [[nodiscard]] auto getRADECJNow() const -> std::optional<EquatorialCoords>;

    /**
     * @brief Set RA/DEC (JNow)
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if set successfully
     */
    auto setRADECJNow(double ra, double dec) -> bool;

    /**
     * @brief Get target RA/DEC
     * @return Target coordinates, or nullopt if not available
     */
    [[nodiscard]] auto getTargetRADEC() const
        -> std::optional<EquatorialCoords>;

    /**
     * @brief Set target RA/DEC
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if set successfully
     */
    auto setTargetRADEC(double ra, double dec) -> bool;

    /**
     * @brief Get current Az/Alt
     * @return Coordinates, or nullopt if not available
     */
    [[nodiscard]] auto getAzAlt() const -> std::optional<HorizontalCoords>;

    /**
     * @brief Set Az/Alt
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if set successfully
     */
    auto setAzAlt(double az, double alt) -> bool;

    // ==================== Slewing ====================

    /**
     * @brief Slew to RA/DEC
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @param enableTracking Enable tracking after slew
     * @return true if slew started
     */
    auto slewToRADEC(double ra, double dec, bool enableTracking = true) -> bool;

    /**
     * @brief Slew to Az/Alt
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if slew started
     */
    auto slewToAzAlt(double az, double alt) -> bool;

    /**
     * @brief Sync to RA/DEC
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if synced
     */
    auto syncToRADEC(double ra, double dec) -> bool;

    /**
     * @brief Abort current motion
     * @return true if aborted
     */
    auto abortMotion() -> bool;

    /**
     * @brief Check if telescope is slewing
     * @return true if slewing
     */
    [[nodiscard]] auto isSlewing() const -> bool;

    /**
     * @brief Wait for slew to complete
     * @param timeout Timeout in milliseconds
     * @return true if slew completed
     */
    auto waitForSlew(
        std::chrono::milliseconds timeout = std::chrono::minutes(5)) -> bool;

    // ==================== Tracking ====================

    /**
     * @brief Enable/disable tracking
     * @param enable true to enable
     * @return true if set successfully
     */
    auto enableTracking(bool enable) -> bool;

    /**
     * @brief Check if tracking is enabled
     * @return true if tracking
     */
    [[nodiscard]] auto isTrackingEnabled() const -> bool;

    /**
     * @brief Set track mode
     * @param mode Track mode
     * @return true if set successfully
     */
    auto setTrackMode(TrackMode mode) -> bool;

    /**
     * @brief Get current track mode
     * @return Track mode
     */
    [[nodiscard]] auto getTrackMode() const -> TrackMode;

    /**
     * @brief Set custom track rate
     * @param raRate RA rate in arcsec/sec
     * @param decRate DEC rate in arcsec/sec
     * @return true if set successfully
     */
    auto setTrackRate(double raRate, double decRate) -> bool;

    /**
     * @brief Get track rate information
     * @return Track rate info
     */
    [[nodiscard]] auto getTrackRateInfo() const -> TrackRateInfo;

    // ==================== Parking ====================

    /**
     * @brief Park the telescope
     * @return true if park started
     */
    auto park() -> bool;

    /**
     * @brief Unpark the telescope
     * @return true if unparked
     */
    auto unpark() -> bool;

    /**
     * @brief Check if telescope is parked
     * @return true if parked
     */
    [[nodiscard]] auto isParked() const -> bool;

    /**
     * @brief Set park position
     * @param ra RA in hours
     * @param dec DEC in degrees
     * @return true if set successfully
     */
    auto setParkPosition(double ra, double dec) -> bool;

    /**
     * @brief Get park position
     * @return Park position, or nullopt if not available
     */
    [[nodiscard]] auto getParkPosition() const
        -> std::optional<EquatorialCoords>;

    /**
     * @brief Set park option
     * @param option Park option
     * @return true if set successfully
     */
    auto setParkOption(ParkOption option) -> bool;

    /**
     * @brief Get park information
     * @return Park info
     */
    [[nodiscard]] auto getParkInfo() const -> ParkInfo;

    // ==================== Motion Control ====================

    /**
     * @brief Set slew rate
     * @param rate Slew rate
     * @return true if set successfully
     */
    auto setSlewRate(SlewRate rate) -> bool;

    /**
     * @brief Get current slew rate
     * @return Slew rate
     */
    [[nodiscard]] auto getSlewRate() const -> SlewRate;

    /**
     * @brief Move in NS direction
     * @param direction Direction
     * @return true if started
     */
    auto moveNS(MotionNS direction) -> bool;

    /**
     * @brief Move in EW direction
     * @param direction Direction
     * @return true if started
     */
    auto moveEW(MotionEW direction) -> bool;

    /**
     * @brief Stop NS motion
     * @return true if stopped
     */
    auto stopNS() -> bool;

    /**
     * @brief Stop EW motion
     * @return true if stopped
     */
    auto stopEW() -> bool;

    // ==================== Guiding ====================

    /**
     * @brief Guide in NS direction
     * @param direction Direction (positive = North, negative = South)
     * @param durationMs Duration in milliseconds
     * @return true if guide pulse sent
     */
    auto guideNS(int direction, int durationMs) -> bool;

    /**
     * @brief Guide in EW direction
     * @param direction Direction (positive = East, negative = West)
     * @param durationMs Duration in milliseconds
     * @return true if guide pulse sent
     */
    auto guideEW(int direction, int durationMs) -> bool;

    // ==================== Telescope Info ====================

    /**
     * @brief Get telescope information
     * @return Telescope info
     */
    [[nodiscard]] auto getTelescopeInfo() const -> TelescopeInfo;

    /**
     * @brief Set telescope information
     * @param info Telescope info
     * @return true if set successfully
     */
    auto setTelescopeInfo(const TelescopeInfo& info) -> bool;

    /**
     * @brief Get pier side
     * @return Pier side
     */
    [[nodiscard]] auto getPierSide() const -> PierSide;

    // ==================== Status ====================

    /**
     * @brief Get telescope state
     * @return Telescope state
     */
    [[nodiscard]] auto getTelescopeState() const -> TelescopeState;

    /**
     * @brief Get telescope status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleCoordinateProperty(const INDIProperty& property);
    void handleTrackProperty(const INDIProperty& property);
    void handleParkProperty(const INDIProperty& property);
    void handleTelescopeInfoProperty(const INDIProperty& property);
    void handlePierSideProperty(const INDIProperty& property);
    void handleMotionProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // Telescope state
    std::atomic<TelescopeState> telescopeState_{TelescopeState::Idle};
    std::atomic<bool> isSlewing_{false};

    // Coordinates
    EquatorialCoords currentRADEC_;
    EquatorialCoords targetRADEC_;
    HorizontalCoords currentAzAlt_;
    mutable std::mutex coordsMutex_;
    std::condition_variable slewCondition_;

    // Tracking
    TrackRateInfo trackInfo_;
    mutable std::mutex trackMutex_;

    // Parking
    ParkInfo parkInfo_;
    mutable std::mutex parkMutex_;

    // Telescope info
    TelescopeInfo telescopeInfo_;
    mutable std::mutex infoMutex_;

    // Motion
    std::atomic<SlewRate> slewRate_{SlewRate::None};
    std::atomic<PierSide> pierSide_{PierSide::Unknown};
    std::atomic<MotionNS> motionNS_{MotionNS::None};
    std::atomic<MotionEW> motionEW_{MotionEW::None};

    // Connection
    std::atomic<ConnectionMode> connectionMode_{ConnectionMode::None};
    std::atomic<DomePolicy> domePolicy_{DomePolicy::None};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_TELESCOPE_HPP
