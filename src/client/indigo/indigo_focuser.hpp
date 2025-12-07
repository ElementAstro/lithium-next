/*
 * indigo_focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Focuser Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_FOCUSER_HPP
#define LITHIUM_CLIENT_INDIGO_FOCUSER_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Focuser movement direction
 */
enum class FocuserDirection : uint8_t {
    In,     // Focus in (closer)
    Out,    // Focus out (farther)
    None
};

/**
 * @brief Focuser movement status
 */
enum class FocuserMovementStatus : uint8_t {
    Idle,       // Not moving
    Moving,     // Currently moving
    Stopped,    // Movement stopped
    Error       // Error occurred
};

/**
 * @brief Position information for focuser
 */
struct FocuserPositionInfo {
    int currentPosition{0};       // Current absolute position
    int targetPosition{0};        // Target position
    int minPosition{0};           // Minimum allowed position
    int maxPosition{100000};      // Maximum allowed position
    bool isMoving{false};         // Currently moving

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"currentPosition", currentPosition},
            {"targetPosition", targetPosition},
            {"minPosition", minPosition},
            {"maxPosition", maxPosition},
            {"isMoving", isMoving}
        };
    }
};

/**
 * @brief Speed control information
 */
struct FocuserSpeedInfo {
    double currentSpeed{0.0};     // Current movement speed
    double minSpeed{0.0};         // Minimum speed
    double maxSpeed{100.0};       // Maximum speed

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"currentSpeed", currentSpeed},
            {"minSpeed", minSpeed},
            {"maxSpeed", maxSpeed}
        };
    }
};

/**
 * @brief Temperature compensation information
 */
struct TemperatureCompensationInfo {
    bool enabled{false};
    double coefficient{0.0};      // Compensation coefficient
    double lastTemperature{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"enabled", enabled},
            {"coefficient", coefficient},
            {"lastTemperature", lastTemperature}
        };
    }
};

/**
 * @brief Backlash compensation information
 */
struct BacklashCompensationInfo {
    bool enabled{false};
    int steps{0};                 // Backlash steps

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"enabled", enabled},
            {"steps", steps}
        };
    }
};

/**
 * @brief Movement callback for progress
 */
using FocuserMovementCallback = std::function<void(int currentPos, int targetPos)>;

/**
 * @brief INDIGO Focuser Device
 *
 * Provides focuser control functionality for INDIGO-connected focusers:
 * - Position control (absolute and relative movement)
 * - Speed/rate control
 * - Backlash compensation
 * - Temperature compensation
 * - Movement direction control
 * - Limits (min/max position)
 * - Movement progress tracking
 */
class INDIGOFocuser : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOFocuser(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOFocuser() override;

    // ==================== Position Control ====================

    /**
     * @brief Move to absolute position
     * @param position Target absolute position
     * @return Success or error
     */
    auto moveToPosition(int position) -> DeviceResult<bool>;

    /**
     * @brief Move relative by steps
     * @param steps Number of steps (positive = out, negative = in)
     * @return Success or error
     */
    auto moveRelative(int steps) -> DeviceResult<bool>;

    /**
     * @brief Get current focuser position
     * @return Current position or error
     */
    [[nodiscard]] auto getCurrentPosition() const -> int;

    /**
     * @brief Get target position
     * @return Target position
     */
    [[nodiscard]] auto getTargetPosition() const -> int;

    /**
     * @brief Get position information
     */
    [[nodiscard]] auto getPositionInfo() const -> FocuserPositionInfo;

    /**
     * @brief Sync position to specified value
     * @param position Position to sync to
     * @return Success or error
     */
    auto syncPosition(int position) -> DeviceResult<bool>;

    /**
     * @brief Check if focuser is moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Wait for movement to complete
     * @param timeout Timeout in milliseconds
     * @return Success or error
     */
    auto waitForMovement(std::chrono::milliseconds timeout =
                             std::chrono::seconds(60)) -> DeviceResult<bool>;

    /**
     * @brief Abort current movement
     * @return Success or error
     */
    auto abortMovement() -> DeviceResult<bool>;

    // ==================== Speed Control ====================

    /**
     * @brief Set focuser speed
     * @param speed Movement speed value
     * @return Success or error
     */
    auto setSpeed(double speed) -> DeviceResult<bool>;

    /**
     * @brief Get current speed
     */
    [[nodiscard]] auto getSpeed() const -> double;

    /**
     * @brief Get speed information
     */
    [[nodiscard]] auto getSpeedInfo() const -> FocuserSpeedInfo;

    // ==================== Direction Control ====================

    /**
     * @brief Set movement direction
     * @param direction Direction to set
     * @return Success or error
     */
    auto setDirection(FocuserDirection direction) -> DeviceResult<bool>;

    /**
     * @brief Get current direction
     */
    [[nodiscard]] auto getDirection() const -> FocuserDirection;

    /**
     * @brief Reverse motion direction
     * @param reversed Enable/disable reverse
     * @return Success or error
     */
    auto setReverse(bool reversed) -> DeviceResult<bool>;

    /**
     * @brief Check if motion is reversed
     */
    [[nodiscard]] auto isReversed() const -> bool;

    // ==================== Limits ====================

    /**
     * @brief Set minimum position limit
     * @param minPos Minimum position
     * @return Success or error
     */
    auto setMinLimit(int minPos) -> DeviceResult<bool>;

    /**
     * @brief Set maximum position limit
     * @param maxPos Maximum position
     * @return Success or error
     */
    auto setMaxLimit(int maxPos) -> DeviceResult<bool>;

    /**
     * @brief Get minimum position limit
     */
    [[nodiscard]] auto getMinLimit() const -> int;

    /**
     * @brief Get maximum position limit
     */
    [[nodiscard]] auto getMaxLimit() const -> int;

    // ==================== Temperature Compensation ====================

    /**
     * @brief Enable/disable temperature compensation
     * @param enabled Enable or disable
     * @return Success or error
     */
    auto setTemperatureCompensation(bool enabled) -> DeviceResult<bool>;

    /**
     * @brief Set temperature compensation coefficient
     * @param coefficient Compensation coefficient
     * @return Success or error
     */
    auto setTemperatureCoefficient(double coefficient) -> DeviceResult<bool>;

    /**
     * @brief Check if temperature compensation is enabled
     */
    [[nodiscard]] auto isTemperatureCompensationEnabled() const -> bool;

    /**
     * @brief Get temperature compensation information
     */
    [[nodiscard]] auto getTemperatureCompensationInfo() const
        -> TemperatureCompensationInfo;

    // ==================== Backlash Compensation ====================

    /**
     * @brief Enable/disable backlash compensation
     * @param enabled Enable or disable
     * @return Success or error
     */
    auto setBacklashCompensation(bool enabled) -> DeviceResult<bool>;

    /**
     * @brief Set backlash steps
     * @param steps Number of backlash steps
     * @return Success or error
     */
    auto setBacklashSteps(int steps) -> DeviceResult<bool>;

    /**
     * @brief Get backlash compensation information
     */
    [[nodiscard]] auto getBacklashCompensationInfo() const
        -> BacklashCompensationInfo;

    // ==================== Movement Callbacks ====================

    /**
     * @brief Set movement progress callback
     * @param callback Callback function
     */
    void setMovementCallback(FocuserMovementCallback callback);

    // ==================== Status ====================

    /**
     * @brief Get focuser movement status
     */
    [[nodiscard]] auto getMovementStatus() const -> FocuserMovementStatus;

    /**
     * @brief Get full focuser status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> focuserImpl_;
};

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_FOCUSER_HPP
