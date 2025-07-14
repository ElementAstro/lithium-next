/*
 * shutter_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Shutter Management Component

*************************************************/

#pragma once

#include <memory>
#include <optional>
#include <atomic>
#include <functional>
#include <string>

namespace lithium::ascom::dome::components {

class HardwareInterface;

/**
 * @brief Shutter state enumeration matching AtomDome::ShutterState
 */
enum class ShutterState {
    OPEN = 0,
    CLOSED = 1,
    OPENING = 2,
    CLOSING = 3,
    ERROR = 4,
    UNKNOWN = 5
};

/**
 * @brief Shutter Management Component for ASCOM Dome
 */
class ShutterManager {
public:
    explicit ShutterManager(std::shared_ptr<HardwareInterface> hardware);
    virtual ~ShutterManager();

    // === Shutter Control ===
    auto openShutter() -> bool;
    auto closeShutter() -> bool;
    auto abortShutter() -> bool;
    auto getShutterState() -> ShutterState;
    auto hasShutter() -> bool;

    // === Shutter Monitoring ===
    auto isShutterMoving() -> bool;
    auto waitForShutterState(ShutterState state, int timeout_ms = 30000) -> bool;
    auto getShutterOperationProgress() -> std::optional<double>;

    // === Statistics ===
    auto getShutterOperations() -> uint64_t;
    auto resetShutterOperations() -> bool;

    // === Callback Support ===
    using ShutterStateCallback = std::function<void(ShutterState)>;
    auto setShutterStateCallback(ShutterStateCallback callback) -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;
    std::atomic<ShutterState> current_state_{ShutterState::UNKNOWN};
    std::atomic<uint64_t> operation_count_{0};
    ShutterStateCallback state_callback_;

    auto updateShutterState() -> bool;
    auto notifyStateChange(ShutterState state) -> void;
};

} // namespace lithium::ascom::dome::components
