/*
 * mock_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mock_focuser.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

MockFocuser::MockFocuser(const std::string& name)
    : AtomFocuser(name), gen_(rd_()) {

    // Set up mock capabilities
    FocuserCapabilities caps;
    caps.canAbsoluteMove = true;
    caps.canRelativeMove = true;
    caps.canAbort = true;
    caps.canReverse = true;
    caps.canSync = true;
    caps.hasTemperature = true;
    caps.hasBacklash = true;
    caps.hasSpeedControl = true;
    caps.maxPosition = MOCK_MAX_POSITION;
    caps.minPosition = MOCK_MIN_POSITION;
    setFocuserCapabilities(caps);

    // Set device info
    DeviceInfo info;
    info.driverName = "Mock Focuser Driver";
    info.driverVersion = "1.0.0";
    info.manufacturer = "Lithium Astronomy";
    info.model = "MockFocus-1000";
    info.serialNumber = "FOCUS123456";
    setDeviceInfo(info);
}

bool MockFocuser::initialize() {
    setState(DeviceState::IDLE);
    return true;
}

bool MockFocuser::destroy() {
    if (is_moving_) {
        abortMove();
    }
    setState(DeviceState::UNKNOWN);
    return true;
}

bool MockFocuser::connect(const std::string& port, int timeout, int maxRetry) {
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    connected_ = true;
    setState(DeviceState::IDLE);
    updateTimestamp();
    return true;
}

bool MockFocuser::disconnect() {
    if (is_moving_) {
        abortMove();
    }

    connected_ = false;
    setState(DeviceState::UNKNOWN);
    return true;
}

std::vector<std::string> MockFocuser::scan() {
    return {"MockFocuser:USB", "MockFocuser:Serial"};
}

bool MockFocuser::isMoving() const {
    return is_moving_;
}

auto MockFocuser::getSpeed() -> std::optional<double> {
    return current_speed_;
}

auto MockFocuser::setSpeed(double speed) -> bool {
    current_speed_ = std::clamp(speed, MOCK_MIN_SPEED, MOCK_MAX_SPEED);
    return true;
}

auto MockFocuser::getMaxSpeed() -> int {
    return static_cast<int>(MOCK_MAX_SPEED);
}

auto MockFocuser::getSpeedRange() -> std::pair<int, int> {
    return {static_cast<int>(MOCK_MIN_SPEED), static_cast<int>(MOCK_MAX_SPEED)};
}

auto MockFocuser::getDirection() -> std::optional<FocusDirection> {
    return current_direction_;
}

auto MockFocuser::setDirection(FocusDirection direction) -> bool {
    current_direction_ = direction;
    return true;
}

auto MockFocuser::getMaxLimit() -> std::optional<int> {
    return max_limit_;
}

auto MockFocuser::setMaxLimit(int maxLimit) -> bool {
    if (maxLimit > min_limit_ && maxLimit <= MOCK_MAX_POSITION) {
        max_limit_ = maxLimit;
        return true;
    }
    return false;
}

auto MockFocuser::getMinLimit() -> std::optional<int> {
    return min_limit_;
}

auto MockFocuser::setMinLimit(int minLimit) -> bool {
    if (minLimit >= MOCK_MIN_POSITION && minLimit < max_limit_) {
        min_limit_ = minLimit;
        return true;
    }
    return false;
}

auto MockFocuser::isReversed() -> std::optional<bool> {
    return is_reversed_;
}

auto MockFocuser::setReversed(bool reversed) -> bool {
    is_reversed_ = reversed;
    return true;
}

auto MockFocuser::moveSteps(int steps) -> bool {
    if (is_moving_ || !isConnected()) {
        return false;
    }

    int direction_multiplier = is_reversed_ ? -1 : 1;
    int actual_steps = steps * direction_multiplier;

    // Apply backlash compensation if needed
    if (backlash_enabled_) {
        actual_steps = applyBacklashCompensation(actual_steps);
    }

    int new_position = current_position_ + actual_steps;

    if (!validatePosition(new_position)) {
        return false;
    }

    target_position_ = new_position;
    last_move_steps_ = steps;

    // Start movement simulation
    std::thread([this, actual_steps]() { simulateMovement(actual_steps); }).detach();

    return true;
}

auto MockFocuser::moveToPosition(int position) -> bool {
    if (is_moving_ || !isConnected()) {
        return false;
    }

    if (!validatePosition(position)) {
        return false;
    }

    int steps = position - current_position_;
    target_position_ = position;
    last_move_steps_ = std::abs(steps);

    // Apply backlash compensation if needed
    if (backlash_enabled_) {
        steps = applyBacklashCompensation(steps);
    }

    // Start movement simulation
    std::thread([this, steps]() { simulateMovement(steps); }).detach();

    return true;
}

auto MockFocuser::getPosition() -> std::optional<int> {
    return current_position_;
}

auto MockFocuser::moveForDuration(int durationMs) -> bool {
    if (is_moving_ || !isConnected()) {
        return false;
    }

    // Calculate steps based on duration and speed
    double steps_per_ms = current_speed_ / 1000.0;
    int steps = static_cast<int>(durationMs * steps_per_ms);

    if (current_direction_ == FocusDirection::IN) {
        steps = -steps;
    }

    return moveSteps(steps);
}

auto MockFocuser::abortMove() -> bool {
    if (!is_moving_) {
        return false;
    }

    is_moving_ = false;
    updateFocuserState(FocuserState::IDLE);
    notifyMoveComplete(false, "Movement aborted by user");

    return true;
}

auto MockFocuser::syncPosition(int position) -> bool {
    if (is_moving_) {
        return false;
    }

    current_position_ = position;
    notifyPositionChange(position);
    return true;
}

auto MockFocuser::moveInward(int steps) -> bool {
    setDirection(FocusDirection::IN);
    return moveSteps(steps);
}

auto MockFocuser::moveOutward(int steps) -> bool {
    setDirection(FocusDirection::OUT);
    return moveSteps(steps);
}

auto MockFocuser::getBacklash() -> int {
    return backlash_steps_;
}

auto MockFocuser::setBacklash(int backlash) -> bool {
    backlash_steps_ = std::abs(backlash);
    return true;
}

auto MockFocuser::enableBacklashCompensation(bool enable) -> bool {
    backlash_enabled_ = enable;
    return true;
}

auto MockFocuser::isBacklashCompensationEnabled() -> bool {
    return backlash_enabled_;
}

auto MockFocuser::getExternalTemperature() -> std::optional<double> {
    // Simulate temperature with some random variation
    std::uniform_real_distribution<double> temp_dist(-0.5, 0.5);
    external_temperature_ += temp_dist(gen_);
    external_temperature_ = std::clamp(external_temperature_, -20.0, 40.0);

    return external_temperature_;
}

auto MockFocuser::getChipTemperature() -> std::optional<double> {
    // Chip temperature is usually higher than external
    chip_temperature_ = external_temperature_ + 5.0;
    return chip_temperature_;
}

auto MockFocuser::hasTemperatureSensor() -> bool {
    return focuser_capabilities_.hasTemperature;
}

auto MockFocuser::getTemperatureCompensation() -> TemperatureCompensation {
    return temperature_compensation_;
}

auto MockFocuser::setTemperatureCompensation(const TemperatureCompensation& comp) -> bool {
    temperature_compensation_ = comp;

    if (comp.enabled) {
        // Start temperature compensation simulation
        std::thread([this]() { simulateTemperatureCompensation(); }).detach();
    }

    return true;
}

auto MockFocuser::enableTemperatureCompensation(bool enable) -> bool {
    temperature_compensation_.enabled = enable;

    if (enable) {
        std::thread([this]() { simulateTemperatureCompensation(); }).detach();
    }

    return true;
}

auto MockFocuser::startAutoFocus() -> bool {
    if (is_moving_ || is_auto_focusing_) {
        return false;
    }

    is_auto_focusing_ = true;
    auto_focus_progress_ = 0.0;

    // Set up auto focus parameters
    af_start_position_ = current_position_ - 1000;
    af_end_position_ = current_position_ + 1000;
    af_current_step_ = 0;
    af_total_steps_ = 20;

    // Start auto focus simulation
    std::thread([this]() { simulateAutoFocus(); }).detach();

    return true;
}

auto MockFocuser::stopAutoFocus() -> bool {
    is_auto_focusing_ = false;
    auto_focus_progress_ = 0.0;
    return true;
}

auto MockFocuser::isAutoFocusing() -> bool {
    return is_auto_focusing_;
}

auto MockFocuser::getAutoFocusProgress() -> double {
    return auto_focus_progress_;
}

auto MockFocuser::savePreset(int slot, int position) -> bool {
    if (slot >= 0 && slot < static_cast<int>(presets_.size())) {
        presets_[slot] = position;
        return true;
    }
    return false;
}

auto MockFocuser::loadPreset(int slot) -> bool {
    if (slot >= 0 && slot < static_cast<int>(presets_.size()) && presets_[slot].has_value()) {
        return moveToPosition(presets_[slot].value());
    }
    return false;
}

auto MockFocuser::getPreset(int slot) -> std::optional<int> {
    if (slot >= 0 && slot < static_cast<int>(presets_.size())) {
        return presets_[slot];
    }
    return std::nullopt;
}

auto MockFocuser::deletePreset(int slot) -> bool {
    if (slot >= 0 && slot < static_cast<int>(presets_.size())) {
        presets_[slot] = std::nullopt;
        return true;
    }
    return false;
}

auto MockFocuser::getTotalSteps() -> uint64_t {
    return total_steps_;
}

auto MockFocuser::resetTotalSteps() -> bool {
    total_steps_ = 0;
    return true;
}

auto MockFocuser::getLastMoveSteps() -> int {
    return last_move_steps_;
}

auto MockFocuser::getLastMoveDuration() -> int {
    return last_move_duration_;
}

void MockFocuser::simulateMovement(int steps) {
    is_moving_ = true;
    updateFocuserState(FocuserState::MOVING);

    auto start_time = std::chrono::steady_clock::now();

    // Calculate movement duration based on speed and steps
    double movement_time = std::abs(steps) / current_speed_; // seconds
    auto movement_duration = std::chrono::duration<double>(movement_time);

    // Simulate gradual movement
    int total_steps = std::abs(steps);
    int step_direction = (steps > 0) ? 1 : -1;

    for (int i = 0; i < total_steps && is_moving_; ++i) {
        std::this_thread::sleep_for(movement_duration / total_steps);
        current_position_ += step_direction;

        // Update direction tracking for backlash
        last_direction_ = (step_direction > 0) ? FocusDirection::OUT : FocusDirection::IN;

        // Notify position change periodically
        if (i % 10 == 0) {
            notifyPositionChange(current_position_);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    last_move_duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (is_moving_) {
        total_steps_ += std::abs(steps);
        is_moving_ = false;
        updateFocuserState(FocuserState::IDLE);
        notifyPositionChange(current_position_);
        notifyMoveComplete(true, "Movement completed successfully");
    }
}

void MockFocuser::simulateTemperatureCompensation() {
    double last_temp = external_temperature_;

    while (temperature_compensation_.enabled && isConnected()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        double current_temp = getExternalTemperature().value_or(20.0);
        double temp_change = current_temp - last_temp;

        if (std::abs(temp_change) > 0.1) {
            int compensation_steps = static_cast<int>(temp_change * temperature_compensation_.coefficient);

            if (std::abs(compensation_steps) > 0 && !is_moving_) {
                moveSteps(compensation_steps);
                temperature_compensation_.compensationOffset += compensation_steps;
            }

            last_temp = current_temp;
        }
    }
}

void MockFocuser::simulateAutoFocus() {
    // Simulate auto focus process
    int step_size = (af_end_position_ - af_start_position_) / af_total_steps_;

    for (af_current_step_ = 0; af_current_step_ < af_total_steps_ && is_auto_focusing_; ++af_current_step_) {
        int target_pos = af_start_position_ + (af_current_step_ * step_size);

        if (moveToPosition(target_pos)) {
            // Wait for movement to complete
            while (is_moving_ && is_auto_focusing_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Simulate image capture and analysis delay
            std::this_thread::sleep_for(std::chrono::seconds(2));

            auto_focus_progress_ = static_cast<double>(af_current_step_ + 1) / af_total_steps_;
        }
    }

    if (is_auto_focusing_) {
        // Move to best focus position (simulate finding it in the middle)
        int best_position = (af_start_position_ + af_end_position_) / 2;
        moveToPosition(best_position);

        is_auto_focusing_ = false;
        auto_focus_progress_ = 1.0;
    }
}

bool MockFocuser::validatePosition(int position) {
    return position >= min_limit_ && position <= max_limit_;
}

int MockFocuser::applyBacklashCompensation(int steps) {
    if (!backlash_enabled_ || backlash_steps_ == 0) {
        return steps;
    }

    FocusDirection new_direction = (steps > 0) ? FocusDirection::OUT : FocusDirection::IN;

    // If changing direction, add backlash compensation
    if (last_direction_ != FocusDirection::NONE && last_direction_ != new_direction) {
        int backlash_compensation = (new_direction == FocusDirection::OUT) ? backlash_steps_ : -backlash_steps_;
        return steps + backlash_compensation;
    }

    return steps;
}
