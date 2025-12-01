/*
 * ascom_rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Rotator Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_ROTATOR_HPP
#define LITHIUM_CLIENT_ASCOM_ROTATOR_HPP

#include "ascom_device_base.hpp"

namespace lithium::client::ascom {

/**
 * @brief Rotator state enumeration
 */
enum class RotatorState : uint8_t { Idle, Moving, Error };

/**
 * @brief ASCOM Rotator Device
 */
class ASCOMRotator : public ASCOMDeviceBase {
public:
    explicit ASCOMRotator(std::string name, int deviceNumber = 0);
    ~ASCOMRotator() override;

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Rotator";
    }

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    // ==================== Position Control ====================

    auto moveTo(double position) -> bool;
    auto moveRelative(double offset) -> bool;
    auto halt() -> bool;

    [[nodiscard]] auto getPosition() -> double;
    [[nodiscard]] auto getMechanicalPosition() -> double;
    [[nodiscard]] auto getTargetPosition() -> double;
    [[nodiscard]] auto isMoving() -> bool;

    auto waitForMove(std::chrono::milliseconds timeout = std::chrono::seconds(60))
        -> bool;

    // ==================== Sync ====================

    auto sync(double position) -> bool;

    // ==================== Reverse ====================

    auto setReverse(bool reverse) -> bool;
    [[nodiscard]] auto isReversed() -> bool;

    // ==================== Info ====================

    [[nodiscard]] auto getStepSize() -> double;
    [[nodiscard]] auto canReverse() -> bool;

    // ==================== Status ====================

    [[nodiscard]] auto getRotatorState() const -> RotatorState {
        return rotatorState_.load();
    }

    [[nodiscard]] auto getStatus() const -> json override;

private:
    std::atomic<RotatorState> rotatorState_{RotatorState::Idle};
    bool canReverse_{false};
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_ROTATOR_HPP
