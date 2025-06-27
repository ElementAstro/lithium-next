/*
 * temperature_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Temperature Controller Component

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

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Temperature Controller for ASCOM Camera
 * 
 * Manages cooling operations, temperature monitoring, and thermal
 * protection with temperature history tracking.
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

    struct TemperatureHistory {
        struct DataPoint {
            std::chrono::steady_clock::time_point timestamp;
            double temperature;
            double coolerPower;
            bool coolerEnabled;
        };
        
        std::deque<DataPoint> data;
        size_t maxSize = 1000;  // Maximum history points to keep
        
        void addPoint(double temp, double power, bool enabled);
        std::vector<DataPoint> getLastPoints(size_t count) const;
        std::vector<DataPoint> getPointsSince(std::chrono::steady_clock::time_point since) const;
        double getAverageTemperature(std::chrono::seconds duration) const;
        double getTemperatureStability(std::chrono::seconds duration) const;
        void clear();
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

    // =========================================================================
    // Cooler Control
    // =========================================================================

    /**
     * @brief Start cooling to target temperature
     * @param targetTemperature Target temperature in Celsius
     * @return true if cooling started successfully
     */
    bool startCooling(double targetTemperature);

    /**
     * @brief Start cooling with custom settings
     * @param settings Cooling configuration
     * @return true if cooling started successfully
     */
    bool startCooling(const CoolingSettings& settings);

    /**
     * @brief Stop cooling and turn off cooler
     * @return true if cooling stopped successfully
     */
    bool stopCooling();

    /**
     * @brief Enable/disable cooler
     * @param enable True to enable cooler
     * @return true if operation successful
     */
    bool setCoolerEnabled(bool enable);

    /**
     * @brief Check if cooler is enabled
     * @return true if cooler is on
     */
    bool isCoolerOn() const { return coolerEnabled_.load(); }

    /**
     * @brief Check if camera has a cooler
     * @return true if cooler available
     */
    bool hasCooler() const;
    
    // =========================================================================
    // Temperature Control
    // =========================================================================

    /**
     * @brief Set target temperature
     * @param temperature Target temperature in Celsius
     * @return true if set successfully
     */
    bool setTargetTemperature(double temperature);

    /**
     * @brief Get current temperature
     * @return Current temperature in Celsius
     */
    double getCurrentTemperature() const;

    /**
     * @brief Get target temperature
     * @return Target temperature in Celsius
     */
    double getTargetTemperature() const { return targetTemperature_.load(); }

    /**
     * @brief Get cooler power
     * @return Cooler power percentage (0-100)
     */
    double getCoolerPower() const;

    /**
     * @brief Get complete temperature information
     * @return Temperature info structure
     */
    TemperatureInfo getTemperatureInfo() const;

    // =========================================================================
    // State Management
    // =========================================================================

    /**
     * @brief Get current cooler state
     * @return Current state
     */
    CoolerState getState() const { return state_.load(); }

    /**
     * @brief Get state as string
     * @return State description
     */
    std::string getStateString() const;

    /**
     * @brief Check if temperature has reached target
     * @return true if at target temperature
     */
    bool hasReachedTarget() const;

    /**
     * @brief Check if temperature is stable
     * @return true if temperature is stable
     */
    bool isTemperatureStable() const;

    /**
     * @brief Get time since cooler started
     * @return Duration since cooling started
     */
    std::chrono::duration<double> getTimeSinceCoolingStarted() const;

    // =========================================================================
    // Temperature History
    // =========================================================================

    /**
     * @brief Get temperature history
     * @return Reference to temperature history
     */
    const TemperatureHistory& getTemperatureHistory() const { return temperatureHistory_; }

    /**
     * @brief Get average temperature over time period
     * @param duration Time period to average over
     * @return Average temperature
     */
    double getAverageTemperature(std::chrono::seconds duration) const;

    /**
     * @brief Get temperature stability measure
     * @param duration Time period to analyze
     * @return Stability measure (lower is more stable)
     */
    double getTemperatureStability(std::chrono::seconds duration) const;

    /**
     * @brief Clear temperature history
     */
    void clearHistory();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set temperature update callback
     * @param callback Callback function
     */
    void setTemperatureCallback(TemperatureCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        temperatureCallback_ = std::move(callback);
    }

    /**
     * @brief Set state change callback
     * @param callback Callback function
     */
    void setStateCallback(StateCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        stateCallback_ = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set monitoring interval
     * @param intervalMs Interval in milliseconds
     */
    void setMonitoringInterval(int intervalMs) {
        monitoringInterval_ = std::chrono::milliseconds(intervalMs);
    }

    /**
     * @brief Set temperature tolerance for stability
     * @param toleranceDegC Tolerance in degrees Celsius
     */
    void setTemperatureTolerance(double toleranceDegC) {
        temperatureTolerance_ = toleranceDegC;
    }

    /**
     * @brief Set stabilization time requirement
     * @param duration Time to be stable before considering reached
     */
    void setStabilizationTime(std::chrono::seconds duration) {
        stabilizationTime_ = duration;
    }

    /**
     * @brief Get current cooling settings
     * @return Current settings
     */
    CoolingSettings getCurrentSettings() const { return currentSettings_; }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<CoolerState> state_{CoolerState::OFF};
    std::atomic<bool> coolerEnabled_{false};
    std::atomic<double> targetTemperature_{-10.0};
    mutable std::mutex stateMutex_;

    // Current settings
    CoolingSettings currentSettings_;

    // Temperature monitoring
    mutable std::mutex temperatureMutex_;
    TemperatureHistory temperatureHistory_;
    std::chrono::steady_clock::time_point lastTemperatureUpdate_;
    std::chrono::steady_clock::time_point coolingStartTime_;
    std::chrono::steady_clock::time_point stableStartTime_;

    // Monitoring thread
    std::unique_ptr<std::thread> monitorThread_;
    std::atomic<bool> monitorRunning_{false};
    std::condition_variable monitorCondition_;

    // Callbacks
    mutable std::mutex callbackMutex_;
    TemperatureCallback temperatureCallback_;
    StateCallback stateCallback_;

    // Configuration
    std::chrono::milliseconds monitoringInterval_{1000}; // 1 second
    double temperatureTolerance_ = 0.5; // ±0.5°C
    std::chrono::seconds stabilizationTime_{30}; // 30 seconds stable

    // Helper methods
    void setState(CoolerState newState);
    void monitorTemperature();
    void updateTemperature();
    void checkStability();
    void handleCoolingTimeout();
    void invokeTemperatureCallback(const TemperatureInfo& info);
    void invokeStateCallback(CoolerState state, const std::string& message);
    bool validateTargetTemperature(double temperature) const;
    std::string formatTemperature(double temp) const;
};

} // namespace lithium::device::ascom::camera::components
