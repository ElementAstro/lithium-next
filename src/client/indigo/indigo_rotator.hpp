/*
 * indigo_rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Rotator Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_ROTATOR_HPP
#define LITHIUM_CLIENT_INDIGO_ROTATOR_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>

namespace lithium::client::indigo {

/**
 * @brief Rotator status
 */
struct RotatorStatus {
    double position{0.0};        // Current angle in degrees
    double targetPosition{0.0};  // Target angle
    bool moving{false};
    bool reversed{false};
    PropertyState state{PropertyState::Idle};
};

/**
 * @brief Movement callback
 */
using RotatorMovementCallback = std::function<void(double currentAngle, double targetAngle)>;

/**
 * @brief INDIGO Rotator Device
 */
class INDIGORotator : public INDIGODeviceBase {
public:
    explicit INDIGORotator(const std::string& deviceName);
    ~INDIGORotator() override;

    // ==================== Position Control ====================

    auto moveToAngle(double angle) -> DeviceResult<bool>;
    auto moveRelative(double degrees) -> DeviceResult<bool>;
    auto abortMove() -> DeviceResult<bool>;
    auto syncAngle(double angle) -> DeviceResult<bool>;

    [[nodiscard]] auto isMoving() const -> bool;
    [[nodiscard]] auto getAngle() const -> double;
    [[nodiscard]] auto getTargetAngle() const -> double;

    // ==================== Direction Control ====================

    auto setReverse(bool reverse) -> DeviceResult<bool>;
    [[nodiscard]] auto isReversed() const -> bool;

    // ==================== Backlash ====================

    auto setBacklash(double degrees) -> DeviceResult<bool>;
    [[nodiscard]] auto getBacklash() const -> double;

    // ==================== Callbacks ====================

    void setMovementCallback(RotatorMovementCallback callback);

    // ==================== Status ====================

    [[nodiscard]] auto getRotatorStatus() const -> RotatorStatus;
    [[nodiscard]] auto getStatus() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> rotatorImpl_;
};

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_ROTATOR_HPP
