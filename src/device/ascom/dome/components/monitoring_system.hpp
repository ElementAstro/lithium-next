/*
 * monitoring_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Monitoring System Component

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <unordered_map>

namespace lithium::ascom::dome::components {

class HardwareInterface;

/**
 * @brief Monitoring System Component
 *
 * Provides comprehensive monitoring of dome systems including
 * temperature, humidity, power, motion status, and health checks.
 */
class MonitoringSystem {
public:
    struct MonitoringData {
        double temperature{0.0};        // Celsius
        double humidity{0.0};           // Percentage
        double power_voltage{0.0};      // Volts
        double power_current{0.0};      // Amperes
        bool motor_status{false};
        bool shutter_status{false};
        std::chrono::steady_clock::time_point timestamp;
    };

    using MonitoringCallback = std::function<void(const MonitoringData& data)>;
    using AlertCallback = std::function<void(const std::string& alert_type, const std::string& message)>;

    explicit MonitoringSystem(std::shared_ptr<HardwareInterface> hardware);
    ~MonitoringSystem();

    // === Control ===
    auto startMonitoring() -> bool;
    auto stopMonitoring() -> bool;
    auto isMonitoring() -> bool;
    auto setMonitoringInterval(std::chrono::milliseconds interval);

    // === Data Access ===
    auto getLatestData() -> MonitoringData;
    auto getHistoricalData(int count) -> std::vector<MonitoringData>;
    auto getDataSince(std::chrono::steady_clock::time_point since) -> std::vector<MonitoringData>;

    // === Thresholds and Alerts ===
    auto setTemperatureThreshold(double min_temp, double max_temp);
    auto setHumidityThreshold(double min_humidity, double max_humidity);
    auto setPowerThreshold(double min_voltage, double max_voltage);
    auto setCurrentThreshold(double max_current);

    // === Health Checks ===
    auto performHealthCheck() -> bool;
    auto getSystemHealth() -> std::unordered_map<std::string, bool>;
    auto getLastHealthCheck() -> std::chrono::steady_clock::time_point;

    // === Callbacks ===
    void setMonitoringCallback(MonitoringCallback callback);
    void setAlertCallback(AlertCallback callback);

    // === Statistics ===
    auto getAverageTemperature(std::chrono::minutes duration) -> double;
    auto getAverageHumidity(std::chrono::minutes duration) -> double;
    auto getAveragePower(std::chrono::minutes duration) -> double;
    auto getUptime() -> std::chrono::seconds;

private:
    std::shared_ptr<HardwareInterface> hardware_interface_;

    std::atomic<bool> is_monitoring_{false};
    std::chrono::milliseconds monitoring_interval_{std::chrono::milliseconds(1000)};

    MonitoringData latest_data_;
    std::vector<MonitoringData> historical_data_;
    static constexpr size_t MAX_HISTORICAL_DATA = 1000;

    // Thresholds
    double min_temperature_{-20.0};
    double max_temperature_{60.0};
    double min_humidity_{10.0};
    double max_humidity_{90.0};
    double min_voltage_{11.0};
    double max_voltage_{15.0};
    double max_current_{10.0};

    MonitoringCallback monitoring_callback_;
    AlertCallback alert_callback_;

    std::unique_ptr<std::thread> monitoring_thread_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_health_check_;

    mutable std::mutex data_mutex_;

    // === Internal Methods ===
    void monitoringLoop();
    auto collectData() -> MonitoringData;
    void checkThresholds(const MonitoringData& data);
    void addToHistory(const MonitoringData& data);
    void notifyAlert(const std::string& alert_type, const std::string& message);
    auto checkMotorHealth() -> bool;
    auto checkShutterHealth() -> bool;
    auto checkPowerHealth() -> bool;
    auto checkTemperatureHealth() -> bool;
};

} // namespace lithium::ascom::dome::components
