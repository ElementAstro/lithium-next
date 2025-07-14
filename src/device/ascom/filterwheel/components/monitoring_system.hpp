/*
 * monitoring_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Monitoring System Component

This component provides continuous monitoring, health checks, and
diagnostic capabilities for the ASCOM filterwheel.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lithium::device::ascom::filterwheel::components {

// Forward declarations
class HardwareInterface;
class PositionManager;

// Health status enumeration
enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    UNKNOWN
};

// Monitoring metrics
struct MonitoringMetrics {
    // Performance metrics
    double movement_success_rate{100.0};
    std::chrono::milliseconds average_move_time{0};
    std::chrono::milliseconds max_move_time{0};
    std::chrono::milliseconds min_move_time{0};

    // Connection metrics
    std::chrono::steady_clock::time_point last_communication;
    int communication_errors{0};
    int total_commands{0};

    // Temperature metrics (if available)
    std::optional<double> current_temperature;
    std::optional<double> min_temperature;
    std::optional<double> max_temperature;

    // Usage statistics
    uint64_t total_movements{0};
    std::map<int, uint64_t> position_usage;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds uptime{0};
};

// Health check result
struct HealthCheck {
    HealthStatus status;
    std::string description;
    std::vector<std::string> issues;
    std::vector<std::string> recommendations;
    std::chrono::system_clock::time_point timestamp;
};

// Alert level enumeration
enum class AlertLevel {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// Alert structure
struct Alert {
    AlertLevel level;
    std::string message;
    std::string component;
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged{false};
};

/**
 * @brief Monitoring System for ASCOM Filter Wheels
 *
 * This component provides continuous monitoring, health checks, and
 * diagnostic capabilities for filterwheel operations.
 */
class MonitoringSystem {
public:
    using AlertCallback = std::function<void(const Alert& alert)>;
    using HealthCallback = std::function<void(HealthStatus status, const std::string& message)>;
    using MetricsCallback = std::function<void(const MonitoringMetrics& metrics)>;

    MonitoringSystem(std::shared_ptr<HardwareInterface> hardware,
                    std::shared_ptr<PositionManager> position_manager);
    ~MonitoringSystem();

    // Initialization
    auto initialize() -> bool;
    auto shutdown() -> void;
    auto startMonitoring() -> bool;
    auto stopMonitoring() -> void;
    auto isMonitoring() -> bool;

    // Health monitoring
    auto performHealthCheck() -> HealthCheck;
    auto getHealthStatus() -> HealthStatus;
    auto getLastHealthCheck() -> std::optional<HealthCheck>;
    auto setHealthCheckInterval(std::chrono::milliseconds interval) -> void;
    auto getHealthCheckInterval() -> std::chrono::milliseconds;

    // Metrics collection
    auto getMetrics() -> MonitoringMetrics;
    auto resetMetrics() -> void;
    auto recordMovement(int from_position, int to_position, bool success, std::chrono::milliseconds duration) -> void;
    auto recordCommunication(bool success) -> void;
    auto recordTemperature(double temperature) -> void;

    // Alert management
    auto getAlerts(AlertLevel min_level = AlertLevel::INFO) -> std::vector<Alert>;
    auto getUnacknowledgedAlerts() -> std::vector<Alert>;
    auto acknowledgeAlert(size_t alert_index) -> bool;
    auto clearAlerts() -> void;
    auto addAlert(AlertLevel level, const std::string& message, const std::string& component = "") -> void;

    // Diagnostic capabilities
    auto performDiagnostics() -> std::map<std::string, std::string>;
    auto testCommunication() -> bool;
    auto testMovement() -> bool;
    auto getSystemInfo() -> std::map<std::string, std::string>;

    // Performance analysis
    auto getPerformanceReport() -> std::string;
    auto analyzeTrends() -> std::map<std::string, double>;
    auto predictMaintenanceNeeds() -> std::vector<std::string>;

    // Configuration
    auto setMonitoringInterval(std::chrono::milliseconds interval) -> void;
    auto getMonitoringInterval() -> std::chrono::milliseconds;
    auto enableTemperatureMonitoring(bool enable) -> void;
    auto isTemperatureMonitoringEnabled() -> bool;

    // Callbacks
    auto setAlertCallback(AlertCallback callback) -> void;
    auto setHealthCallback(HealthCallback callback) -> void;
    auto setMetricsCallback(MetricsCallback callback) -> void;

    // Data export
    auto exportMetrics(const std::string& file_path) -> bool;
    auto exportAlerts(const std::string& file_path) -> bool;
    auto generateReport(const std::string& file_path) -> bool;

    // Error handling
    auto getLastError() -> std::string;
    auto clearError() -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<PositionManager> position_manager_;

    // Monitoring state
    std::atomic<bool> is_monitoring_{false};
    std::atomic<HealthStatus> current_health_{HealthStatus::UNKNOWN};

    // Configuration
    std::chrono::milliseconds monitoring_interval_{1000};
    std::chrono::milliseconds health_check_interval_{30000};
    bool temperature_monitoring_enabled_{true};

    // Data storage
    MonitoringMetrics metrics_;
    std::vector<Alert> alerts_;
    std::optional<HealthCheck> last_health_check_;

    // Threading
    std::unique_ptr<std::thread> monitoring_thread_;
    std::unique_ptr<std::thread> health_check_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Synchronization
    mutable std::mutex metrics_mutex_;
    mutable std::mutex alerts_mutex_;
    mutable std::mutex health_mutex_;

    // Callbacks
    AlertCallback alert_callback_;
    HealthCallback health_callback_;
    MetricsCallback metrics_callback_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Internal monitoring methods
    auto monitoringLoop() -> void;
    auto healthCheckLoop() -> void;
    auto updateMetrics() -> void;
    auto checkCommunication() -> void;
    auto checkTemperature() -> void;
    auto checkPerformance() -> void;

    // Health assessment
    auto assessOverallHealth() -> HealthStatus;
    auto checkHardwareHealth() -> std::pair<HealthStatus, std::string>;
    auto checkPositionHealth() -> std::pair<HealthStatus, std::string>;
    auto checkTemperatureHealth() -> std::pair<HealthStatus, std::string>;
    auto checkPerformanceHealth() -> std::pair<HealthStatus, std::string>;

    // Alert generation
    auto generateAlert(AlertLevel level, const std::string& message, const std::string& component) -> void;
    auto notifyAlert(const Alert& alert) -> void;
    auto notifyHealthChange(HealthStatus status, const std::string& message) -> void;
    auto notifyMetricsUpdate(const MonitoringMetrics& metrics) -> void;

    // Data analysis
    auto calculateSuccessRate() -> double;
    auto calculateAverageTime() -> std::chrono::milliseconds;
    auto detectAnomalies() -> std::vector<std::string>;
    auto analyzeUsagePatterns() -> std::map<std::string, double>;

    // Utility methods
    auto setError(const std::string& error) -> void;
    auto formatDuration(std::chrono::milliseconds duration) -> std::string;
    auto formatTimestamp(std::chrono::system_clock::time_point timestamp) -> std::string;
    auto trimAlerts(size_t max_alerts = 1000) -> void;
};

} // namespace lithium::device::ascom::filterwheel::components
