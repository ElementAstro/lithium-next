/*
 * shutter_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "shutter_controller.hpp"
#include "core/indi_dome_core.hpp"
#include "property_manager.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi {

ShutterController::ShutterController(std::shared_ptr<INDIDomeCore> core)
    : DomeComponentBase(std::move(core), "ShutterController") {
    last_activity_time_ = std::chrono::steady_clock::now();
}

auto ShutterController::initialize() -> bool {
    if (isInitialized()) {
        logWarning("Already initialized");
        return true;
    }

    auto core = getCore();
    if (!core) {
        logError("Core is null, cannot initialize");
        return false;
    }

    try {
        shutter_state_.store(static_cast<int>(ShutterState::UNKNOWN));
        is_moving_.store(false);
        emergency_close_active_.store(false);
        shutter_operations_.store(0);

        logInfo("Shutter controller initialized");
        setInitialized(true);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize: " + std::string(ex.what()));
        return false;
    }
}

auto ShutterController::cleanup() -> bool {
    if (!isInitialized()) {
        return true;
    }

    try {
        setInitialized(false);
        logInfo("Shutter controller cleaned up");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to cleanup: " + std::string(ex.what()));
        return false;
    }
}

void ShutterController::handlePropertyUpdate(const INDI::Property& property) {
    if (!isOurProperty(property)) {
        return;
    }

    const std::string prop_name = property.getName();
    if (prop_name == "DOME_SHUTTER") {
        handleShutterPropertyUpdate(property);
    }
}

auto ShutterController::openShutter() -> bool {
    std::lock_guard<std::recursive_mutex> lock(shutter_mutex_);

    if (!canOpenShutter()) {
        return false;
    }

    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }

    startOperationTimer();
    bool success = prop_mgr->openShutter();

    if (success) {
        updateMovingState(true);
        shutter_operations_.fetch_add(1);
        logInfo("Opening shutter");
    } else {
        logError("Failed to open shutter");
        stopOperationTimer();
    }

    return success;
}

auto ShutterController::closeShutter() -> bool {
    std::lock_guard<std::recursive_mutex> lock(shutter_mutex_);

    if (!canCloseShutter()) {
        return false;
    }

    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }

    startOperationTimer();
    bool success = prop_mgr->closeShutter();

    if (success) {
        updateMovingState(true);
        shutter_operations_.fetch_add(1);
        logInfo("Closing shutter");
    } else {
        logError("Failed to close shutter");
        stopOperationTimer();
    }

    return success;
}

auto ShutterController::getShutterState() const -> ShutterState {
    return static_cast<ShutterState>(shutter_state_.load());
}

auto ShutterController::isShutterMoving() const -> bool {
    ShutterState state = getShutterState();
    return state == ShutterState::OPENING || state == ShutterState::CLOSING;
}

void ShutterController::updateShutterState(ShutterState state) {
    ShutterState old_state = static_cast<ShutterState>(shutter_state_.exchange(static_cast<int>(state)));

    if (old_state != state) {
        updateMovingState(state == ShutterState::OPENING || state == ShutterState::CLOSING);
        notifyStateChange(state);

        // Update open time tracking
        if (state == ShutterState::OPEN && old_state != ShutterState::OPEN) {
            open_time_start_ = std::chrono::steady_clock::now();
        } else if (state != ShutterState::OPEN && old_state == ShutterState::OPEN) {
            updateOpenTime();
        }

        // Check for operation completion
        if ((old_state == ShutterState::OPENING && state == ShutterState::OPEN) ||
            (old_state == ShutterState::CLOSING && state == ShutterState::CLOSED)) {
            auto duration = getOperationDuration();
            recordOperation(duration);
            stopOperationTimer();
            notifyOperationComplete(true, "Shutter operation completed");
        }
    }
}

void ShutterController::notifyStateChange(ShutterState state) {
    if (shutter_state_callback_) {
        shutter_state_callback_(state);
    }

    auto core = getCore();
    if (core) {
        core->notifyShutterChange(state);
    }
}

void ShutterController::handleShutterPropertyUpdate(const INDI::Property& property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }

    auto switch_prop = property.getSwitch();
    if (!switch_prop) {
        return;
    }

    auto open_widget = switch_prop->findWidgetByName("SHUTTER_OPEN");
    auto close_widget = switch_prop->findWidgetByName("SHUTTER_CLOSE");

    if (open_widget && open_widget->getState() == ISS_ON) {
        if (property.getState() == IPS_BUSY) {
            updateShutterState(ShutterState::OPENING);
        } else if (property.getState() == IPS_OK) {
            updateShutterState(ShutterState::OPEN);
        }
    } else if (close_widget && close_widget->getState() == ISS_ON) {
        if (property.getState() == IPS_BUSY) {
            updateShutterState(ShutterState::CLOSING);
        } else if (property.getState() == IPS_OK) {
            updateShutterState(ShutterState::CLOSED);
        }
    }
}

// Simplified implementations for other methods
auto ShutterController::abortShutter() -> bool {
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        return false;
    }
    return prop_mgr->abortShutter();
}

auto ShutterController::canOpenShutter() const -> bool {
    return performSafetyChecks() && !emergency_close_active_.load();
}

auto ShutterController::canCloseShutter() const -> bool {
    return canPerformOperation();
}

auto ShutterController::canPerformOperation() const -> bool {
    auto core = getCore();
    return core && core->isConnected() && !is_moving_.load();
}

auto ShutterController::performSafetyChecks() const -> bool {
    if (!safety_interlock_enabled_.load()) {
        return true;
    }

    if (safety_callback_ && !safety_callback_()) {
        return false;
    }

    if (weather_response_enabled_.load() && weather_callback_ && !weather_callback_()) {
        return false;
    }

    return true;
}

void ShutterController::updateMovingState(bool moving) {
    is_moving_.store(moving);
}

void ShutterController::notifyOperationComplete(bool success, const std::string& message) {
    if (shutter_complete_callback_) {
        shutter_complete_callback_(success, message);
    }
}

void ShutterController::startOperationTimer() {
    operation_start_time_ = std::chrono::steady_clock::now();
}

void ShutterController::stopOperationTimer() {
    auto duration = getOperationDuration();
    last_operation_duration_ms_.store(duration.count());
}

auto ShutterController::getOperationDuration() const -> std::chrono::milliseconds {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - operation_start_time_);
}

void ShutterController::recordOperation(std::chrono::milliseconds duration) {
    total_operation_time_ms_.fetch_add(duration.count());
}

void ShutterController::updateOpenTime() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - open_time_start_);
    total_open_time_ms_.fetch_add(duration.count());
}

auto ShutterController::resetShutterOperations() -> bool {
    shutter_operations_.store(0);
    return true;
}

} // namespace lithium::device::indi
