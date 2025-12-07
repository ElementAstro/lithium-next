/*
 * ascom_telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Telescope Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_TELESCOPE_HPP
#define LITHIUM_CLIENT_ASCOM_TELESCOPE_HPP

#include "ascom_device_base.hpp"

namespace lithium::client::ascom {

/**
 * @brief Telescope state enumeration
 */
enum class TelescopeState : uint8_t { Idle, Slewing, Tracking, Parked, Error };

/**
 * @brief Alignment mode
 */
enum class AlignmentMode : uint8_t { AltAz, Polar, GermanPolar };

/**
 * @brief Pier side
 */
enum class PierSide : int8_t { Unknown = -1, East = 0, West = 1 };

/**
 * @brief Drive rate
 */
enum class DriveRate : uint8_t { Sidereal, Lunar, Solar, King };

/**
 * @brief Guide direction
 */
enum class GuideDirection : uint8_t { North, South, East, West };

/**
 * @brief Telescope capabilities
 */
struct TelescopeCapabilities {
    bool canFindHome{false};
    bool canMoveAxis{false};
    bool canPark{false};
    bool canPulseGuide{false};
    bool canSetDeclinationRate{false};
    bool canSetGuideRates{false};
    bool canSetPark{false};
    bool canSetPierSide{false};
    bool canSetRightAscensionRate{false};
    bool canSetTracking{false};
    bool canSlew{false};
    bool canSlewAltAz{false};
    bool canSlewAltAzAsync{false};
    bool canSlewAsync{false};
    bool canSync{false};
    bool canSyncAltAz{false};
    bool canUnpark{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"canFindHome", canFindHome},
                {"canMoveAxis", canMoveAxis},
                {"canPark", canPark},
                {"canPulseGuide", canPulseGuide},
                {"canSetDeclinationRate", canSetDeclinationRate},
                {"canSetGuideRates", canSetGuideRates},
                {"canSetPark", canSetPark},
                {"canSetPierSide", canSetPierSide},
                {"canSetRightAscensionRate", canSetRightAscensionRate},
                {"canSetTracking", canSetTracking},
                {"canSlew", canSlew},
                {"canSlewAltAz", canSlewAltAz},
                {"canSlewAltAzAsync", canSlewAltAzAsync},
                {"canSlewAsync", canSlewAsync},
                {"canSync", canSync},
                {"canSyncAltAz", canSyncAltAz},
                {"canUnpark", canUnpark}};
    }
};

/**
 * @brief Equatorial coordinates
 */
struct EquatorialCoords {
    double rightAscension{0.0};  // Hours
    double declination{0.0};     // Degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"rightAscension", rightAscension},
                {"declination", declination}};
    }
};

/**
 * @brief Horizontal coordinates
 */
struct HorizontalCoords {
    double altitude{0.0};  // Degrees
    double azimuth{0.0};   // Degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"altitude", altitude}, {"azimuth", azimuth}};
    }
};

/**
 * @brief ASCOM Telescope Device
 *
 * Provides telescope/mount functionality including:
 * - Slewing (equatorial and alt-az)
 * - Tracking control
 * - Parking
 * - Pulse guiding
 * - Sync
 */
class ASCOMTelescope : public ASCOMDeviceBase {
public:
    explicit ASCOMTelescope(std::string name, int deviceNumber = 0);
    ~ASCOMTelescope() override;

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Telescope";
    }

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    // ==================== Capabilities ====================

    [[nodiscard]] auto getCapabilities() -> TelescopeCapabilities;

    // ==================== Coordinates ====================

    [[nodiscard]] auto getRightAscension() -> double;
    [[nodiscard]] auto getDeclination() -> double;
    [[nodiscard]] auto getEquatorialCoords() -> EquatorialCoords;

    [[nodiscard]] auto getAltitude() -> double;
    [[nodiscard]] auto getAzimuth() -> double;
    [[nodiscard]] auto getHorizontalCoords() -> HorizontalCoords;

    [[nodiscard]] auto getSiderealTime() -> double;

    // ==================== Slewing ====================

    auto slewToCoordinates(double ra, double dec) -> bool;
    auto slewToCoordinatesAsync(double ra, double dec) -> bool;
    auto slewToAltAz(double alt, double az) -> bool;
    auto slewToAltAzAsync(double alt, double az) -> bool;
    auto slewToTarget() -> bool;
    auto slewToTargetAsync() -> bool;

    auto abortSlew() -> bool;
    [[nodiscard]] auto isSlewing() -> bool;

    auto waitForSlew(
        std::chrono::milliseconds timeout = std::chrono::minutes(5)) -> bool;

    // ==================== Sync ====================

    auto syncToCoordinates(double ra, double dec) -> bool;
    auto syncToAltAz(double alt, double az) -> bool;
    auto syncToTarget() -> bool;

    // ==================== Target ====================

    auto setTargetRightAscension(double ra) -> bool;
    auto setTargetDeclination(double dec) -> bool;
    [[nodiscard]] auto getTargetRightAscension() -> double;
    [[nodiscard]] auto getTargetDeclination() -> double;

    // ==================== Tracking ====================

    auto setTracking(bool enable) -> bool;
    [[nodiscard]] auto isTracking() -> bool;

    auto setTrackingRate(DriveRate rate) -> bool;
    [[nodiscard]] auto getTrackingRate() -> DriveRate;

    auto setRightAscensionRate(double rate) -> bool;
    auto setDeclinationRate(double rate) -> bool;
    [[nodiscard]] auto getRightAscensionRate() -> double;
    [[nodiscard]] auto getDeclinationRate() -> double;

    // ==================== Parking ====================

    auto park() -> bool;
    auto unpark() -> bool;
    [[nodiscard]] auto isParked() -> bool;
    auto findHome() -> bool;
    [[nodiscard]] auto isAtHome() -> bool;
    auto setParkedPosition() -> bool;

    // ==================== Guiding ====================

    auto pulseGuide(GuideDirection direction, int duration) -> bool;
    [[nodiscard]] auto isPulseGuiding() -> bool;

    auto setGuideRateRightAscension(double rate) -> bool;
    auto setGuideRateDeclination(double rate) -> bool;
    [[nodiscard]] auto getGuideRateRightAscension() -> double;
    [[nodiscard]] auto getGuideRateDeclination() -> double;

    // ==================== Motion ====================

    auto moveAxis(int axis, double rate) -> bool;

    // ==================== Info ====================

    [[nodiscard]] auto getAlignmentMode() -> AlignmentMode;
    [[nodiscard]] auto getPierSide() -> PierSide;
    [[nodiscard]] auto getApertureArea() -> double;
    [[nodiscard]] auto getApertureDiameter() -> double;
    [[nodiscard]] auto getFocalLength() -> double;

    // ==================== Status ====================

    [[nodiscard]] auto getTelescopeState() const -> TelescopeState {
        return telescopeState_.load();
    }

    [[nodiscard]] auto getStatus() const -> json override;

private:
    void refreshCapabilities();

    std::atomic<TelescopeState> telescopeState_{TelescopeState::Idle};
    TelescopeCapabilities capabilities_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_TELESCOPE_HPP
