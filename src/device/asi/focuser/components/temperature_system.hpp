/*
 * temperature_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Temperature System Component
Handles temperature monitoring and compensation

*************************************************/

#pragma once

#include <functional>
#include <mutex>
#include <optional>

namespace lithium::device::asi::focuser::components {

// Forward declarations
class HardwareInterface;
class PositionManager;

/**
 * @brief Temperature monitoring and compensation system
 *
 * This component handles temperature sensor monitoring and
 * automatic focus compensation based on temperature changes.
 */
class TemperatureSystem {
public:
    TemperatureSystem(HardwareInterface* hardware,
                      PositionManager* positionManager);
    ~TemperatureSystem();

    // Non-copyable and non-movable
    TemperatureSystem(const TemperatureSystem&) = delete;
    TemperatureSystem& operator=(const TemperatureSystem&) = delete;
    TemperatureSystem(TemperatureSystem&&) = delete;
    TemperatureSystem& operator=(TemperatureSystem&&) = delete;

    // Temperature monitoring
    std::optional<double> getCurrentTemperature() const;
    bool hasTemperatureSensor() const;
    double getLastTemperature() const { return lastTemperature_; }

    // Temperature compensation
    bool setTemperatureCoefficient(double coefficient);
    double getTemperatureCoefficient() const { return temperatureCoefficient_; }
    bool enableTemperatureCompensation(bool enable);
    bool isTemperatureCompensationEnabled() const {
        return compensationEnabled_;
    }

    // Compensation settings
    bool setCompensationThreshold(double threshold);
    double getCompensationThreshold() const { return compensationThreshold_; }

    // Manual compensation
    bool applyTemperatureCompensation();
    int calculateCompensationSteps(double temperatureDelta) const;

    // Callbacks
    void setTemperatureCallback(std::function<void(double)> callback) {
        temperatureCallback_ = callback;
    }
    void setCompensationCallback(std::function<void(int, double)> callback) {
        compensationCallback_ = callback;
    }

    // Status
    bool isCompensationActive() const { return compensationActive_; }
    int getLastCompensationSteps() const { return lastCompensationSteps_; }
    double getLastTemperatureDelta() const { return lastTemperatureDelta_; }

private:
    // Dependencies
    HardwareInterface* hardware_;
    PositionManager* positionManager_;

    // Temperature state
    double currentTemperature_ = 20.0;
    double lastTemperature_ = 20.0;
    double referenceTemperature_ = 20.0;

    // Compensation settings
    double temperatureCoefficient_ = 0.0;  // steps per degree C
    bool compensationEnabled_ = false;
    double compensationThreshold_ =
        0.5;  // minimum temp change to trigger compensation

    // Compensation state
    bool compensationActive_ = false;
    int lastCompensationSteps_ = 0;
    double lastTemperatureDelta_ = 0.0;

    // Callbacks
    std::function<void(double)> temperatureCallback_;
    std::function<void(int, double)>
        compensationCallback_;  // steps, temp delta

    // Thread safety
    mutable std::mutex temperatureMutex_;

    // Helper methods
    bool updateTemperature();
    void notifyTemperatureChange(double temperature);
    void notifyCompensationApplied(int steps, double delta);
};

}  // namespace lithium::device::asi::focuser::components
