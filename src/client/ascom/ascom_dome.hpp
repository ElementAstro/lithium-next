/*
 * ascom_dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Dome Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_DOME_HPP
#define LITHIUM_CLIENT_ASCOM_DOME_HPP

#include "ascom_device_base.hpp"

namespace lithium::client::ascom {

/**
 * @brief Dome state enumeration
 */
enum class DomeState : uint8_t { Idle, Moving, Parking, Parked, Error };

/**
 * @brief Shutter state enumeration
 */
enum class ShutterState : uint8_t { Open, Closed, Opening, Closing, Error };

/**
 * @brief Dome capabilities
 */
struct DomeCapabilities {
    bool canFindHome{false};
    bool canPark{false};
    bool canSetAltitude{false};
    bool canSetAzimuth{false};
    bool canSetPark{false};
    bool canSetShutter{false};
    bool canSlave{false};
    bool canSyncAzimuth{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"canFindHome", canFindHome},
                {"canPark", canPark},
                {"canSetAltitude", canSetAltitude},
                {"canSetAzimuth", canSetAzimuth},
                {"canSetPark", canSetPark},
                {"canSetShutter", canSetShutter},
                {"canSlave", canSlave},
                {"canSyncAzimuth", canSyncAzimuth}};
    }
};

/**
 * @brief ASCOM Dome Device
 */
class ASCOMDome : public ASCOMDeviceBase {
public:
    explicit ASCOMDome(std::string name, int deviceNumber = 0);
    ~ASCOMDome() override;

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Dome";
    }

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    [[nodiscard]] auto getCapabilities() -> DomeCapabilities;

    // ==================== Azimuth Control ====================

    auto slewToAzimuth(double azimuth) -> bool;
    auto syncToAzimuth(double azimuth) -> bool;
    [[nodiscard]] auto getAzimuth() -> double;
    [[nodiscard]] auto isSlewing() -> bool;
    auto abortSlew() -> bool;

    auto waitForSlew(std::chrono::milliseconds timeout = std::chrono::minutes(2))
        -> bool;

    // ==================== Altitude Control ====================

    auto slewToAltitude(double altitude) -> bool;
    [[nodiscard]] auto getAltitude() -> double;

    // ==================== Shutter Control ====================

    auto openShutter() -> bool;
    auto closeShutter() -> bool;
    [[nodiscard]] auto getShutterStatus() -> ShutterState;

    // ==================== Parking ====================

    auto park() -> bool;
    auto findHome() -> bool;
    [[nodiscard]] auto isParked() -> bool;
    [[nodiscard]] auto isAtHome() -> bool;
    auto setParkedPosition() -> bool;

    // ==================== Slaving ====================

    auto setSlaved(bool slaved) -> bool;
    [[nodiscard]] auto isSlaved() -> bool;

    // ==================== Status ====================

    [[nodiscard]] auto getDomeState() const -> DomeState {
        return domeState_.load();
    }

    [[nodiscard]] auto getStatus() const -> json override;

private:
    void refreshCapabilities();

    std::atomic<DomeState> domeState_{DomeState::Idle};
    DomeCapabilities capabilities_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_DOME_HPP
