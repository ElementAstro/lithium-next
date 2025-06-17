/*
 * mock_rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Rotator Implementation for testing

*************************************************/

#pragma once

#include "../rotator.hpp"

#include <random>
#include <thread>

class MockRotator : public AtomRotator {
public:
    explicit MockRotator(const std::string& name = "MockRotator");
    ~MockRotator() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000, int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;

    // State
    bool isMoving() const override;

    // Position control
    auto getPosition() -> std::optional<double> override;
    auto setPosition(double angle) -> bool override;
    auto moveToAngle(double angle) -> bool override;
    auto rotateByAngle(double angle) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(double angle) -> bool override;

    // Direction control
    auto getDirection() -> std::optional<RotatorDirection> override;
    auto setDirection(RotatorDirection direction) -> bool override;
    auto isReversed() -> bool override;
    auto setReversed(bool reversed) -> bool override;

    // Speed control
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Limits
    auto getMinPosition() -> double override;
    auto getMaxPosition() -> double override;
    auto setLimits(double min, double max) -> bool override;

    // Backlash compensation
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Presets
    auto savePreset(int slot, double angle) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getLastMoveAngle() -> double override;
    auto getLastMoveDuration() -> int override;

private:
    // Simulation parameters
    bool is_moving_{false};
    double move_speed_{10.0}; // degrees per second
    std::thread move_thread_;
    mutable std::mutex move_mutex_;
    
    // Random number generation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    mutable std::uniform_real_distribution<> noise_dist_;
    
    // Simulation methods
    void simulateMove(double target_angle);
    void addPositionNoise();
    double generateTemperature() const;
};
