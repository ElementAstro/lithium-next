#pragma once

#include "base.hpp"
#include <chrono>
#include <deque>
#include "device_mock.hpp"
#include "validation.hpp"
using TaskResult = bool; // mock TaskResult 类型，实际项目请替换为真实定义
#include "../focuser/base.hpp"
using ::lithium::task::focuser::BaseFocuserTask;

namespace lithium::task::custom::focuser {

/**
 * @brief Task for temperature-based focus compensation
 * 
 * This task monitors temperature changes and adjusts focus position
 * to compensate for thermal expansion/contraction effects on the
 * optical system.
 */
class TemperatureCompensationTask : public ::lithium::task::focuser::BaseFocuserTask {
public:
    struct Config {
        double temperature_coefficient = 0.0;  // Steps per degree Celsius
        double min_temperature_change = 0.5;   // Minimum change to trigger compensation
        std::chrono::seconds monitoring_interval = std::chrono::seconds(30); // How often to check temperature
        std::chrono::seconds averaging_period = std::chrono::seconds(300);   // Period for temperature averaging
        bool auto_compensation = true;          // Enable automatic compensation
        double max_compensation_per_cycle = 50.0; // Maximum steps per compensation cycle
        bool enable_predictive = false;        // Enable predictive compensation
        double prediction_window_minutes = 10.0; // Prediction window in minutes
    };

    struct TemperatureReading {
        std::chrono::steady_clock::time_point timestamp;
        double temperature;
        int focus_position;
    };

    struct CompensationEvent {
        std::chrono::steady_clock::time_point timestamp;
        double old_temperature;
        double new_temperature;
        int old_position;
        int new_position;
        double compensation_steps;
        std::string reason;
    };

    TemperatureCompensationTask(std::shared_ptr<device::Focuser> focuser,
                              std::shared_ptr<device::TemperatureSensor> sensor,
                              const Config& config = Config{});

    // Task interface
    bool validateParameters() const override;
    void resetTask() override;

protected:
    TaskResult executeImpl() override;
    void updateProgress() override;
    std::string getTaskInfo() const override;

public:
    // Configuration
    void setConfig(const Config& config);
    Config getConfig() const;

    // Temperature monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    // Manual compensation
    TaskResult compensateForTemperature(double target_temperature);
    TaskResult compensateBySteps(int steps, const std::string& reason = "Manual");

    // Calibration
    TaskResult calibrateTemperatureCoefficient();
    TaskResult setTemperatureCoefficient(double coefficient);
    double getTemperatureCoefficient() const;

    // Data access
    std::vector<TemperatureReading> getTemperatureHistory() const;
    std::vector<CompensationEvent> getCompensationHistory() const;
    double getCurrentTemperature() const;
    double getAverageTemperature() const;
    double getTemperatureTrend() const; // Degrees per hour

    // Prediction
    double predictTemperature(std::chrono::seconds ahead) const;
    int predictRequiredCompensation(std::chrono::seconds ahead) const;

    // Statistics
    struct Statistics {
        size_t total_compensations = 0;
        double total_compensation_steps = 0.0;
        double average_compensation = 0.0;
        double max_compensation = 0.0;
        double temperature_range_min = 0.0;
        double temperature_range_max = 0.0;
        std::chrono::seconds monitoring_time{0};
        double compensation_accuracy = 0.0; // RMS error in focus quality
    };
    Statistics getStatistics() const;

private:
    // Core functionality
    TaskResult performTemperatureCheck();
    TaskResult calculateRequiredCompensation(double temperature_change, int& required_steps);
    TaskResult applyCompensation(int steps, const std::string& reason);
    
    // Temperature analysis
    void addTemperatureReading(double temperature, int position);
    double calculateAverageTemperature() const;
    double calculateTemperatureTrend() const;
    bool shouldTriggerCompensation(double current_temp, double& compensation_steps);
    
    // Predictive compensation
    std::vector<double> calculateTemperatureForecast(std::chrono::seconds ahead) const;
    double calculatePredictiveCompensation() const;
    
    // Calibration helpers
    TaskResult performCalibrationSequence();
    double calculateOptimalCoefficient(const std::vector<std::pair<double, int>>& temp_focus_pairs);
    
    // Validation
    bool isTemperatureReadingValid(double temperature) const;
    bool isCompensationReasonable(int steps) const;

    // Data management
    void pruneOldReadings();
    void pruneOldEvents();
    void saveCompensationEvent(const CompensationEvent& event);

private:
    std::shared_ptr<device::TemperatureSensor> temperature_sensor_;
    Config config_;
    
    // Temperature data
    std::deque<TemperatureReading> temperature_history_;
    std::deque<CompensationEvent> compensation_history_;
    double last_compensation_temperature_ = 0.0;
    std::chrono::steady_clock::time_point last_compensation_time_;
    
    // Monitoring state
    bool monitoring_active_ = false;
    std::chrono::steady_clock::time_point monitoring_start_time_;
    
    // Calibration state
    bool calibration_in_progress_ = false;
    std::vector<std::pair<double, int>> calibration_data_;
    
    // Statistics
    mutable Statistics cached_statistics_;
    mutable std::chrono::steady_clock::time_point statistics_cache_time_;
    
    // Thread safety
    mutable std::mutex temperature_mutex_;
    mutable std::mutex compensation_mutex_;
    
    // Constants
    static constexpr double MIN_TEMPERATURE = -50.0; // Celsius
    static constexpr double MAX_TEMPERATURE = 80.0;  // Celsius
    static constexpr double MAX_REASONABLE_COEFFICIENT = 10.0; // Steps per degree
    static constexpr size_t MAX_HISTORY_SIZE = 10000;
    static constexpr size_t MAX_EVENTS_SIZE = 1000;
};

/**
 * @brief Simple temperature monitoring task for logging purposes
 */
class TemperatureMonitorTask : public ::lithium::task::focuser::BaseFocuserTask {
public:
    struct Config {
        std::chrono::seconds interval = std::chrono::seconds(60); // Monitoring interval
        bool log_to_file = true;
        std::string log_file_path = "temperature_log.csv";
        bool alert_on_rapid_change = true;
        double rapid_change_threshold = 2.0; // Degrees per minute
    };

    TemperatureMonitorTask(std::shared_ptr<device::TemperatureSensor> sensor,
                          const Config& config = Config{});

    // Task interface
    bool validateParameters() const override;
    void resetTask() override;

protected:
    TaskResult executeImpl() override;
    void updateProgress() override;
    std::string getTaskInfo() const override;

public:
    void setConfig(const Config& config);
    Config getConfig() const;
    
    double getCurrentTemperature() const;
    std::vector<std::pair<std::chrono::steady_clock::time_point, double>> getTemperatureLog() const;

private:
    std::shared_ptr<device::TemperatureSensor> temperature_sensor_;
    Config config_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, double>> temperature_log_;
    mutable std::mutex log_mutex_;
};

} // namespace lithium::task::custom::focuser
