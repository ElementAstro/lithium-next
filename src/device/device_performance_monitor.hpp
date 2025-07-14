/*
 * device_performance_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Device Performance Monitoring System

*************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

#include "template/device.hpp"

namespace lithium {

// Performance metrics
struct PerformanceMetrics {
    std::chrono::milliseconds response_time{0};
    std::chrono::milliseconds operation_time{0};
    double throughput{0.0};  // operations per second
    double error_rate{0.0};  // percentage
    double cpu_usage{0.0};   // percentage
    double memory_usage{0.0}; // MB
    size_t queue_depth{0};
    size_t concurrent_operations{0};

    std::chrono::system_clock::time_point timestamp;
};

// Performance alert levels
enum class AlertLevel {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// Performance alert
struct PerformanceAlert {
    std::string device_name;
    AlertLevel level;
    std::string message;
    std::string metric_name;
    double threshold_value;
    double current_value;
    std::chrono::system_clock::time_point timestamp;
};

// Performance threshold configuration
struct PerformanceThresholds {
    std::chrono::milliseconds max_response_time{5000};
    std::chrono::milliseconds max_operation_time{30000};
    double max_error_rate{5.0};          // percentage
    double max_cpu_usage{80.0};          // percentage
    double max_memory_usage{1024.0};     // MB
    size_t max_queue_depth{100};
    size_t max_concurrent_operations{10};

    // Alert thresholds
    std::chrono::milliseconds warning_response_time{2000};
    std::chrono::milliseconds critical_response_time{10000};
    double warning_error_rate{2.0};
    double critical_error_rate{10.0};
};

// Performance statistics
struct PerformanceStatistics {
    PerformanceMetrics current;
    PerformanceMetrics average;
    PerformanceMetrics minimum;
    PerformanceMetrics maximum;

    size_t total_operations{0};
    size_t successful_operations{0};
    size_t failed_operations{0};

    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;

    std::vector<PerformanceAlert> recent_alerts;
};

// Performance monitoring configuration
struct MonitoringConfig {
    std::chrono::seconds monitoring_interval{10};
    std::chrono::seconds alert_cooldown{60};
    size_t max_alerts_stored{100};
    size_t max_metrics_history{1000};
    bool enable_predictive_analysis{true};
    bool enable_auto_tuning{false};
    bool enable_real_time_alerts{true};
};

// Callbacks
using PerformanceAlertCallback = std::function<void(const PerformanceAlert&)>;
using PerformanceUpdateCallback = std::function<void(const std::string&, const PerformanceMetrics&)>;

class DevicePerformanceMonitor {
public:
    DevicePerformanceMonitor();
    ~DevicePerformanceMonitor();

    // Configuration
    void setMonitoringConfig(const MonitoringConfig& config);
    MonitoringConfig getMonitoringConfig() const;

    // Device management
    void addDevice(const std::string& name, std::shared_ptr<AtomDriver> device);
    void removeDevice(const std::string& name);
    bool isDeviceMonitored(const std::string& name) const;

    // Threshold management
    void setThresholds(const std::string& device_name, const PerformanceThresholds& thresholds);
    PerformanceThresholds getThresholds(const std::string& device_name) const;
    void setGlobalThresholds(const PerformanceThresholds& thresholds);
    PerformanceThresholds getGlobalThresholds() const;

    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    void startDeviceMonitoring(const std::string& device_name);
    void stopDeviceMonitoring(const std::string& device_name);
    bool isDeviceMonitoring(const std::string& device_name) const;

    // Metrics collection
    void recordOperation(const std::string& device_name,
                        std::chrono::milliseconds duration,
                        bool success);
    void recordMetrics(const std::string& device_name, const PerformanceMetrics& metrics);

    // Performance query
    PerformanceMetrics getCurrentMetrics(const std::string& device_name) const;
    PerformanceStatistics getStatistics(const std::string& device_name) const;
    std::vector<PerformanceMetrics> getMetricsHistory(const std::string& device_name,
                                                     size_t count = 100) const;

    // Alert management
    void setAlertCallback(PerformanceAlertCallback callback);
    void setUpdateCallback(PerformanceUpdateCallback callback);

    std::vector<PerformanceAlert> getActiveAlerts() const;
    std::vector<PerformanceAlert> getDeviceAlerts(const std::string& device_name) const;
    void clearAlerts(const std::string& device_name = "");
    void acknowledgeAlert(const PerformanceAlert& alert);

    // Analysis and prediction
    struct PredictionResult {
        std::string device_name;
        std::string metric_name;
        double predicted_value;
        double confidence;
        std::chrono::system_clock::time_point prediction_time;
        std::chrono::seconds time_horizon;
    };

    std::vector<PredictionResult> predictPerformance(const std::string& device_name,
                                                   std::chrono::seconds horizon) const;

    // Performance optimization suggestions
    struct OptimizationSuggestion {
        std::string device_name;
        std::string category;
        std::string suggestion;
        std::string rationale;
        double expected_improvement;
        int priority;
    };

    std::vector<OptimizationSuggestion> getOptimizationSuggestions(const std::string& device_name) const;

    // System-wide monitoring
    struct SystemPerformance {
        size_t total_devices{0};
        size_t active_devices{0};
        size_t healthy_devices{0};
        double average_response_time{0.0};
        double average_error_rate{0.0};
        double system_load{0.0};
        size_t total_operations{0};
        size_t total_alerts{0};
    };

    SystemPerformance getSystemPerformance() const;

    // Reporting
    std::string generateReport(const std::string& device_name,
                              std::chrono::system_clock::time_point start_time,
                              std::chrono::system_clock::time_point end_time) const;

    void exportMetrics(const std::string& device_name,
                      const std::string& output_path,
                      const std::string& format = "csv") const;

    // Maintenance
    void cleanup();
    void resetStatistics(const std::string& device_name = "");

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Internal methods
    void monitoringLoop();
    void updateDeviceMetrics(const std::string& device_name);
    void checkThresholds(const std::string& device_name, const PerformanceMetrics& metrics);
    void triggerAlert(const PerformanceAlert& alert);

    // Analysis methods
    double calculateTrend(const std::vector<double>& values) const;
    double calculateMovingAverage(const std::vector<double>& values, size_t window_size) const;
    bool detectAnomalies(const std::vector<double>& values) const;
};

// Utility functions
namespace performance_utils {
    double calculatePercentile(const std::vector<double>& values, double percentile);
    double calculateStandardDeviation(const std::vector<double>& values);
    std::vector<double> smoothData(const std::vector<double>& values, size_t window_size);

    // Resource monitoring
    double getCurrentCpuUsage();
    double getCurrentMemoryUsage();
    double getProcessMemoryUsage();

    // Time utilities
    std::chrono::milliseconds getCurrentTime();
    std::string formatDuration(std::chrono::milliseconds duration);
    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp);
}

} // namespace lithium
