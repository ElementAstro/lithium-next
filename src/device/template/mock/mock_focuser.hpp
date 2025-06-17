/*
 * mock_focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Focuser Implementation for testing

*************************************************/

#pragma once

#include "../template/focuser.hpp"

#include <random>

class MockFocuser : public AtomFocuser {
public:
    explicit MockFocuser(const std::string& name = "MockFocuser");
    ~MockFocuser() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000, int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;
    bool isMoving() const override;

    // Speed control
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;

    // Direction control
    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;

    // Limits
    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;

    // Reverse control
    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;

    // Movement control
    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;

    // Relative movement
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;

    // Backlash compensation
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature sensing
    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Temperature compensation
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp) -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;

    // Auto focus
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;

    // Presets
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

private:
    // Mock configuration
    static constexpr int MOCK_MAX_POSITION = 65535;
    static constexpr int MOCK_MIN_POSITION = 0;
    static constexpr double MOCK_MAX_SPEED = 100.0;
    static constexpr double MOCK_MIN_SPEED = 1.0;
    static constexpr int MOCK_STEPS_PER_REV = 200;
    
    // State variables
    bool is_moving_{false};
    bool is_auto_focusing_{false};
    double auto_focus_progress_{0.0};
    
    // Position tracking
    int target_position_{30000}; // Middle position
    
    // Temperature simulation
    double external_temperature_{20.0};
    double chip_temperature_{25.0};
    
    // Settings
    int max_limit_{MOCK_MAX_POSITION};
    int min_limit_{MOCK_MIN_POSITION};
    FocusDirection current_direction_{FocusDirection::OUT};
    
    // Backlash compensation
    bool backlash_enabled_{false};
    FocusDirection last_direction_{FocusDirection::NONE};
    
    // Auto focus state
    int af_start_position_{0};
    int af_end_position_{0};
    int af_current_step_{0};
    int af_total_steps_{0};
    
    // Random number generation for simulation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    
    // Helper methods
    void simulateMovement(int steps);
    void simulateTemperatureCompensation();
    void simulateAutoFocus();
    bool validatePosition(int position);
    int applyBacklashCompensation(int steps);
};
