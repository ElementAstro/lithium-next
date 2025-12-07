/*
 * indigo_dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Dome Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_DOME_HPP
#define LITHIUM_CLIENT_INDIGO_DOME_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Dome movement direction
 */
enum class DomeDirection : uint8_t {
    Clockwise,
    CounterClockwise
};

/**
 * @brief Dome shutter state
 */
enum class ShutterState : uint8_t {
    Open,
    Closed,
    Opening,
    Closing,
    Unknown
};

/**
 * @brief Dome park state
 */
enum class ParkState : uint8_t {
    Parked,
    Unparked,
    Parking,
    Unknown
};

/**
 * @brief Horizontal coordinates (azimuth)
 */
struct HorizontalCoordinates {
    double azimuth{0.0};      // Azimuth angle in degrees
};

/**
 * @brief Equatorial coordinates for sync
 */
struct EquatorialCoordinates {
    double rightAscension{0.0};    // RA in hours
    double declination{0.0};        // Dec in degrees
};

/**
 * @brief Dome movement status
 */
struct DomeMovementStatus {
    bool moving{false};
    DomeDirection direction{DomeDirection::Clockwise};
    double currentAzimuth{0.0};
    double targetAzimuth{0.0};
    double speed{0.0};                  // Speed in degrees/second
    PropertyState state{PropertyState::Idle};
};

/**
 * @brief Dome status snapshot
 */
struct DomeStatus {
    bool connected{false};
    ShutterState shutterState{ShutterState::Unknown};
    ParkState parkState{ParkState::Unknown};
    bool slavingEnabled{false};
    double currentAzimuth{0.0};
    DomeMovementStatus movementStatus;
    std::string lastError;
};

/**
 * @brief Movement callback
 */
using MovementCallback = std::function<void(const DomeMovementStatus&)>;

/**
 * @brief Shutter callback
 */
using ShutterCallback = std::function<void(ShutterState)>;

/**
 * @brief Park callback
 */
using ParkCallback = std::function<void(ParkState)>;

/**
 * @brief INDIGO Dome Device
 *
 * Provides dome control functionality for INDIGO-connected domes:
 * - Shutter control (open/close/abort)
 * - Azimuth positioning and movement
 * - Park/unpark operations
 * - Mount synchronization (slaving)
 * - Movement monitoring
 * - Speed and direction control
 */
class INDIGODome : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGODome(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGODome() override;

    // ==================== Shutter Control ====================

    /**
     * @brief Open dome shutter
     * @return Success or error
     */
    auto openShutter() -> DeviceResult<bool>;

    /**
     * @brief Close dome shutter
     * @return Success or error
     */
    auto closeShutter() -> DeviceResult<bool>;

    /**
     * @brief Get current shutter state
     */
    [[nodiscard]] auto getShutterState() const -> ShutterState;

    /**
     * @brief Check if shutter is open
     */
    [[nodiscard]] auto isShutterOpen() const -> bool;

    /**
     * @brief Set shutter callback
     */
    void setShutterCallback(ShutterCallback callback);

    // ==================== Azimuth Control ====================

    /**
     * @brief Move dome to specific azimuth
     * @param azimuth Target azimuth in degrees (0-360)
     * @return Success or error
     */
    auto moveToAzimuth(double azimuth) -> DeviceResult<bool>;

    /**
     * @brief Move dome by relative angle
     * @param offset Relative offset in degrees
     * @return Success or error
     */
    auto moveRelative(double offset) -> DeviceResult<bool>;

    /**
     * @brief Get current azimuth position
     */
    [[nodiscard]] auto getCurrentAzimuth() const -> double;

    /**
     * @brief Get target azimuth position
     */
    [[nodiscard]] auto getTargetAzimuth() const -> double;

    /**
     * @brief Abort current movement
     * @return Success or error
     */
    auto abortMotion() -> DeviceResult<bool>;

    /**
     * @brief Check if dome is moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Get movement status
     */
    [[nodiscard]] auto getMovementStatus() const -> DomeMovementStatus;

    /**
     * @brief Set movement callback for position updates
     */
    void setMovementCallback(MovementCallback callback);

    // ==================== Speed Control ====================

    /**
     * @brief Set dome movement speed
     * @param speed Speed in degrees per second
     * @return Success or error
     */
    auto setSpeed(double speed) -> DeviceResult<bool>;

    /**
     * @brief Get current movement speed
     */
    [[nodiscard]] auto getSpeed() const -> double;

    /**
     * @brief Get speed range
     * @return pair of (min, max) in degrees/second
     */
    [[nodiscard]] auto getSpeedRange() const -> std::pair<double, double>;

    // ==================== Direction Control ====================

    /**
     * @brief Set movement direction
     * @param direction Clockwise or counter-clockwise
     * @return Success or error
     */
    auto setDirection(DomeDirection direction) -> DeviceResult<bool>;

    /**
     * @brief Get current movement direction
     */
    [[nodiscard]] auto getDirection() const -> DomeDirection;

    // ==================== Park/Unpark ====================

    /**
     * @brief Park the dome
     * @return Success or error
     */
    auto park() -> DeviceResult<bool>;

    /**
     * @brief Unpark the dome
     * @return Success or error
     */
    auto unpark() -> DeviceResult<bool>;

    /**
     * @brief Get park state
     */
    [[nodiscard]] auto getParkState() const -> ParkState;

    /**
     * @brief Check if dome is parked
     */
    [[nodiscard]] auto isParked() const -> bool;

    /**
     * @brief Set park callback for status updates
     */
    void setParkCallback(ParkCallback callback);

    // ==================== Synchronization ====================

    /**
     * @brief Enable/disable mount synchronization (slaving)
     * @param enabled Enable or disable slaving
     * @return Success or error
     */
    auto setSlavingEnabled(bool enabled) -> DeviceResult<bool>;

    /**
     * @brief Check if slaving is enabled
     */
    [[nodiscard]] auto isSlavingEnabled() const -> bool;

    /**
     * @brief Sync dome with mount at current mount position
     * @param coords Mount coordinates to sync with
     * @return Success or error
     */
    auto syncWithMount(const EquatorialCoordinates& coords) -> DeviceResult<bool>;

    // ==================== Utility ====================

    /**
     * @brief Get dome capabilities as JSON
     */
    [[nodiscard]] auto getCapabilities() const -> json;

    /**
     * @brief Get current dome status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

    /**
     * @brief Get dome information
     */
    [[nodiscard]] auto getDomeInfo() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> domeImpl_;
};

// ============================================================================
// Helper functions
// ============================================================================

[[nodiscard]] constexpr auto shutterStateToString(ShutterState state) noexcept
    -> std::string_view {
    switch (state) {
        case ShutterState::Open: return "Open";
        case ShutterState::Closed: return "Closed";
        case ShutterState::Opening: return "Opening";
        case ShutterState::Closing: return "Closing";
        case ShutterState::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

[[nodiscard]] auto shutterStateFromString(std::string_view str) -> ShutterState;

[[nodiscard]] constexpr auto parkStateToString(ParkState state) noexcept
    -> std::string_view {
    switch (state) {
        case ParkState::Parked: return "Parked";
        case ParkState::Unparked: return "Unparked";
        case ParkState::Parking: return "Parking";
        case ParkState::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

[[nodiscard]] auto parkStateFromString(std::string_view str) -> ParkState;

[[nodiscard]] constexpr auto directionToString(DomeDirection dir) noexcept
    -> std::string_view {
    switch (dir) {
        case DomeDirection::Clockwise: return "CW";
        case DomeDirection::CounterClockwise: return "CCW";
        default: return "CW";
    }
}

[[nodiscard]] auto directionFromString(std::string_view str) -> DomeDirection;

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_DOME_HPP
