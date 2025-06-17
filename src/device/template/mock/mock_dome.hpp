/*
 * mock_dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Dome Implementation for testing

*************************************************/

#pragma once

#include "../dome.hpp"

#include <random>
#include <thread>

class MockDome : public AtomDome {
public:
    explicit MockDome(const std::string& name = "MockDome");
    ~MockDome() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000, int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;

    // State
    bool isMoving() const override;
    bool isParked() const override;

    // Azimuth control
    auto getAzimuth() -> std::optional<double> override;
    auto setAzimuth(double azimuth) -> bool override;
    auto moveToAzimuth(double azimuth) -> bool override;
    auto rotateClockwise() -> bool override;
    auto rotateCounterClockwise() -> bool override;
    auto stopRotation() -> bool override;
    auto abortMotion() -> bool override;
    auto syncAzimuth(double azimuth) -> bool override;

    // Parking
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto getParkPosition() -> std::optional<double> override;
    auto setParkPosition(double azimuth) -> bool override;
    auto canPark() -> bool override;

    // Shutter control
    auto openShutter() -> bool override;
    auto closeShutter() -> bool override;
    auto abortShutter() -> bool override;
    auto getShutterState() -> ShutterState override;
    auto hasShutter() -> bool override;

    // Speed control
    auto getRotationSpeed() -> std::optional<double> override;
    auto setRotationSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Telescope coordination
    auto followTelescope(bool enable) -> bool override;
    auto isFollowingTelescope() -> bool override;
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double override;
    auto setTelescopePosition(double az, double alt) -> bool override;

    // Home position
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;
    auto getHomePosition() -> std::optional<double> override;

    // Backlash compensation
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Weather monitoring
    auto canOpenShutter() -> bool override;
    auto isSafeToOperate() -> bool override;
    auto getWeatherStatus() -> std::string override;

    // Statistics
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getShutterOperations() -> uint64_t override;
    auto resetShutterOperations() -> bool override;

    // Presets
    auto savePreset(int slot, double azimuth) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

private:
    // Simulation parameters
    bool is_dome_moving_{false};
    bool is_shutter_moving_{false};
    double rotation_speed_{5.0}; // degrees per second
    double backlash_amount_{1.0}; // degrees
    bool backlash_enabled_{false};
    
    std::thread dome_move_thread_;
    std::thread shutter_thread_;
    mutable std::mutex move_mutex_;
    mutable std::mutex shutter_mutex_;
    
    // Weather simulation
    bool weather_safe_{true};
    
    // Random number generation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    mutable std::uniform_real_distribution<> noise_dist_;
    
    // Simulation methods
    void simulateDomeMove(double target_azimuth);
    void simulateShutterOperation(ShutterState target_state);
    void addPositionNoise();
    bool checkWeatherSafety() const;
};
