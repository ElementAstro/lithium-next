#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace lithium::device::asi::filterwheel {

namespace components {
    class HardwareInterface;
}

/**
 * @brief Records a single operation in the filterwheel history
 */
struct OperationRecord {
    std::chrono::system_clock::time_point timestamp;
    std::string operation_type;
    int from_position;
    int to_position;
    std::chrono::milliseconds duration;
    bool success;
    std::string error_message;
    
    OperationRecord()
        : from_position(-1), to_position(-1), duration(0), success(false) {}
};



/**
 * @brief Health metrics for the filterwheel
 */
struct HealthMetrics {
    bool is_connected;
    bool is_responding;
    bool is_moving;
    int current_position;
    std::chrono::system_clock::time_point last_position_change;
    std::chrono::system_clock::time_point last_health_check;
    double success_rate;        // Percentage of successful operations
    int consecutive_failures;
    std::vector<std::string> recent_errors;
    
    HealthMetrics()
        : is_connected(false), is_responding(false), is_moving(false)
        , current_position(-1), success_rate(0.0), consecutive_failures(0) {}
};

/**
 * @brief Manages monitoring, logging, and health tracking for the filterwheel
 */
class MonitoringSystem {
public:
    explicit MonitoringSystem(std::shared_ptr<components::HardwareInterface> hw);
    ~MonitoringSystem();

    // Operation logging
    void logOperation(const std::string& operation_type, int from_pos, int to_pos, 
                     std::chrono::milliseconds duration, bool success, 
                     const std::string& error_message = "");
    void startOperationTimer(const std::string& operation_type);
    void endOperationTimer(bool success, const std::string& error_message = "");
    
    // History management
    std::vector<OperationRecord> getOperationHistory(int max_records = 100) const;
    std::vector<OperationRecord> getOperationHistoryByType(const std::string& operation_type, int max_records = 50) const;
    std::vector<OperationRecord> getOperationHistoryByTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) const;
    void clearOperationHistory();
    void setMaxHistorySize(int max_size);
    
    // Statistics
    int getOverallStatistics() const;
    int getStatisticsByType(const std::string& operation_type) const;
    int getStatisticsByTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) const;
    
    // Health monitoring
    void startHealthMonitoring(int check_interval_ms = 5000);
    void stopHealthMonitoring();
    bool isHealthMonitoringActive() const;
    HealthMetrics getCurrentHealthMetrics() const;
    std::vector<HealthMetrics> getHealthHistory(int max_records = 100) const;
    
    // Performance monitoring
    double getAverageOperationTime() const;
    double getSuccessRate() const;
    int getConsecutiveFailures() const;
    std::chrono::system_clock::time_point getLastOperationTime() const;
    
    // Alerts and thresholds
    void setFailureThreshold(int max_consecutive_failures);
    void setResponseTimeThreshold(std::chrono::milliseconds max_response_time);
    bool isHealthy() const;
    std::vector<std::string> getHealthWarnings() const;
    
    // Export and reporting
    bool exportOperationHistory(const std::string& filepath) const;
    bool exportHealthReport(const std::string& filepath) const;
    std::string generateHealthSummary() const;
    std::string generatePerformanceReport() const;
    
    // Real-time monitoring callbacks
    using HealthCallback = std::function<void(const HealthMetrics& metrics)>;
    using AlertCallback = std::function<void(const std::string& alert_type, const std::string& message)>;
    
    void setHealthCallback(HealthCallback callback);
    void setAlertCallback(AlertCallback callback);
    void clearCallbacks();

private:
    std::shared_ptr<components::HardwareInterface> hardware_;
    
    // Operation history
    mutable std::mutex history_mutex_;
    std::vector<OperationRecord> operation_history_;
    int max_history_size_;
    
    // Current operation tracking
    std::string current_operation_;
    std::chrono::steady_clock::time_point operation_start_time_;
    int current_from_position_;
    int current_to_position_;
    
    // Health monitoring
    std::atomic<bool> health_monitoring_active_;
    std::thread health_monitoring_thread_;
    int health_check_interval_ms_;
    mutable std::mutex health_mutex_;
    std::vector<HealthMetrics> health_history_;
    int max_health_history_size_;
    
    // Thresholds and alerting
    int failure_threshold_;
    std::chrono::milliseconds response_time_threshold_;
    
    // Callbacks
    HealthCallback health_callback_;
    AlertCallback alert_callback_;
    
    // Helper methods
    void performHealthCheck();
    void healthMonitoringLoop();
    void updateHealthMetrics(HealthMetrics& metrics) const;
    void checkAlertConditions(const HealthMetrics& metrics);
    void triggerAlert(const std::string& alert_type, const std::string& message);
    void pruneHistory();
    void pruneHealthHistory();
    
    // Statistics calculation helpers
    std::vector<OperationRecord> filterRecordsByType(const std::vector<OperationRecord>& records, 
                                                     const std::string& operation_type) const;
    std::vector<OperationRecord> filterRecordsByTimeRange(const std::vector<OperationRecord>& records,
                                                          std::chrono::system_clock::time_point start,
                                                          std::chrono::system_clock::time_point end) const;
};

} // namespace lithium::device::asi::filterwheel
