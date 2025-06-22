/*
 * temperature_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Temperature Controller Component

This component manages camera cooling system including temperature
monitoring, cooler control, and thermal management.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <deque>

namespace lithium::device::asi::camera::components {

class HardwareInterface;

/**
 * @brief Temperature Controller for ASI Camera
 * 
 * Manages cooling operations, temperature monitoring, and thermal
 * protection with PID control and temperature history tracking.
 */
class TemperatureController {
public:
    enum class CoolerState {
        OFF,
        STARTING,
        COOLING,
        STABILIZING,
        STABLE,
        STOPPING,
        ERROR
    };

    struct TemperatureInfo {
        double currentTemperature = 25.0;      // Current sensor temperature (°C)
        double targetTemperature = -10.0;      // Target temperature (°C)
        double coolerPower = 0.0;              // Cooler power percentage (0-100)
        bool coolerEnabled = false;            // Cooler on/off state
        bool hasReachedTarget = false;         // Has reached target temperature
        double ambientTemperature = 25.0;      // Ambient temperature (°C)
        std::chrono::steady_clock::time_point timestamp;
    };

    struct CoolingSettings {
        double targetTemperature = -10.0;      // Target cooling temperature (°C)
        double maxCoolerPower = 100.0;         // Maximum cooler power (%)
        double temperatureTolerance = 0.5;     // Tolerance for "stable" state (°C)
        std::chrono::seconds stabilizationTime{30}; // Time to consider stable
        std::chrono::seconds timeout{600};     // Cooling timeout (10 minutes)
        bool enableWarmupProtection = true;    // Prevent condensation on warmup
        double maxCoolingRate = 1.0;           // Max cooling rate (°C/min)
        double maxWarmupRate = 2.0;            // Max warmup rate (°C/min)
    };

    struct PIDParams {
        double kp = 1.0;                       // Proportional gain
        double ki = 0.1;                       // Integral gain  
        double kd = 0.05;                      // Derivative gain
        double maxOutput = 100.0;              // Maximum output (%)
        double minOutput = 0.0;                // Minimum output (%)
        double integralWindup = 50.0;          // Integral windup limit
    };

    using TemperatureCallback = std::function<void(const TemperatureInfo&)>;
    using StateCallback = std::function<void(CoolerState, const std::string&)>;

public:
    explicit TemperatureController(std::shared_ptr<HardwareInterface> hardware);
    ~TemperatureController();

    // Non-copyable and non-movable
    TemperatureController(const TemperatureController&) = delete;
    TemperatureController& operator=(const TemperatureController&) = delete;
    TemperatureController(TemperatureController&&) = delete;
    TemperatureController& operator=(TemperatureController&&) = delete;

    // Cooler Control
    bool startCooling(double targetTemperature);
    bool startCooling(const CoolingSettings& settings);
    bool stopCooling();
    bool isCoolerOn() const { return coolerEnabled_; }
    
    // State and Status
    CoolerState getState() const { return state_; }
    std::string getStateString() const;
    TemperatureInfo getCurrentTemperatureInfo() const;
    bool hasCooler() const;
    
    // Temperature Access
    double getCurrentTemperature() const;
    double getTargetTemperature() const { return currentSettings_.targetTemperature; }
    double getCoolerPower() const;
    bool hasReachedTarget() const;
    double getTemperatureStability() const; // Standard deviation of recent temps
    
    // Settings Management
    CoolingSettings getCurrentSettings() const { return currentSettings_; }
    bool updateSettings(const CoolingSettings& settings);
    bool updateTargetTemperature(double temperature);
    bool updateMaxCoolerPower(double power);
    
    // PID Control
    PIDParams getPIDParams() const { return pidParams_; }
    void setPIDParams(const PIDParams& params);
    void resetPIDController();
    
    // Temperature History
    std::vector<TemperatureInfo> getTemperatureHistory(std::chrono::seconds duration) const;
    void clearTemperatureHistory();
    size_t getHistorySize() const;
    
    // Callbacks
    void setTemperatureCallback(TemperatureCallback callback);
    void setStateCallback(StateCallback callback);
    
    // Configuration
    void setMonitoringInterval(std::chrono::milliseconds interval) { monitoringInterval_ = interval; }
    void setHistoryDuration(std::chrono::minutes duration) { historyDuration_ = duration; }
    void setTemperatureTolerance(double tolerance) { currentSettings_.temperatureTolerance = tolerance; }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;
    
    // State management
    std::atomic<CoolerState> state_{CoolerState::OFF};
    std::atomic<bool> coolerEnabled_{false};
    CoolingSettings currentSettings_;
    
    // Threading
    std::thread monitoringThread_;
    std::thread controlThread_;
    std::atomic<bool> stopRequested_{false};
    std::mutex stateMutex_;
    std::condition_variable stateCondition_;
    
    // Temperature monitoring
    TemperatureInfo currentInfo_;
    std::deque<TemperatureInfo> temperatureHistory_;
    mutable std::mutex temperatureMutex_;
    std::chrono::milliseconds monitoringInterval_{1000};
    std::chrono::minutes historyDuration_{60}; // Keep 1 hour of history
    
    // PID Control
    PIDParams pidParams_;
    double previousError_ = 0.0;
    double integralSum_ = 0.0;
    std::chrono::steady_clock::time_point lastControlUpdate_;
    std::mutex pidMutex_;
    
    // Timing and state tracking
    std::chrono::steady_clock::time_point coolingStartTime_;
    std::chrono::steady_clock::time_point lastStableTime_;
    bool hasBeenStable_ = false;
    
    // Callbacks
    TemperatureCallback temperatureCallback_;
    StateCallback stateCallback_;
    std::mutex callbackMutex_;
    
    // Worker methods
    void monitoringWorker();
    void controlWorker();
    bool readCurrentTemperature();
    bool applyCoolerPower(double power);
    double calculatePIDOutput(double currentTemp, double targetTemp);
    void updateTemperatureHistory(const TemperatureInfo& info);
    void checkTemperatureStability();
    void checkCoolingTimeout();
    void notifyTemperatureChange(const TemperatureInfo& info);
    void notifyStateChange(CoolerState newState, const std::string& message = "");
    
    // Helper methods
    void updateState(CoolerState newState);
    bool validateCoolingSettings(const CoolingSettings& settings);
    bool validateTargetTemperature(double temperature);
    double clampCoolerPower(double power);
    std::string formatTemperatureError(const std::string& operation, const std::string& error);
    void cleanupResources();
};

} // namespace lithium::device::asi::camera::components
