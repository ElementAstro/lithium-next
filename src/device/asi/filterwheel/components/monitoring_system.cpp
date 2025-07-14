#include "monitoring_system.hpp"
#include "hardware_interface.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace lithium::device::asi::filterwheel {

MonitoringSystem::MonitoringSystem(std::shared_ptr<components::HardwareInterface> hw)
    : hardware_(std::move(hw))
    , max_history_size_(1000)
    , current_from_position_(-1)
    , current_to_position_(-1)
    , health_monitoring_active_(false)
    , health_check_interval_ms_(5000)
    , max_health_history_size_(100)
    , failure_threshold_(5)
    , response_time_threshold_(std::chrono::milliseconds(10000)) {

    spdlog::info("MonitoringSystem initialized");
}

MonitoringSystem::~MonitoringSystem() {
    stopHealthMonitoring();
    spdlog::info("MonitoringSystem destroyed");
}

void MonitoringSystem::logOperation(const std::string& operation_type, int from_pos, int to_pos,
                                   std::chrono::milliseconds duration, bool success,
                                   const std::string& error_message) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    OperationRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.operation_type = operation_type;
    record.from_position = from_pos;
    record.to_position = to_pos;
    record.duration = duration;
    record.success = success;
    record.error_message = error_message;

    operation_history_.push_back(record);

    // Prune history if it exceeds maximum size
    if (static_cast<int>(operation_history_.size()) > max_history_size_) {
        operation_history_.erase(operation_history_.begin(),
                               operation_history_.begin() + (operation_history_.size() - max_history_size_));
    }

    spdlog::info("Logged operation: {} ({}->{}) duration={} ms success={}",
          operation_type, from_pos, to_pos, duration.count(), success ? "true" : "false");
}

void MonitoringSystem::startOperationTimer(const std::string& operation_type) {
    current_operation_ = operation_type;
    operation_start_time_ = std::chrono::steady_clock::now();

    // Try to get current position for from_position
    if (hardware_) {
        current_from_position_ = hardware_->getCurrentPosition();
    }

    spdlog::info("Started operation timer for: {}", operation_type);
}

void MonitoringSystem::endOperationTimer(bool success, const std::string& error_message) {
    if (current_operation_.empty()) {
        spdlog::warn("endOperationTimer called without startOperationTimer");
        return;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - operation_start_time_);

    // Try to get current position for to_position
    if (hardware_) {
        current_to_position_ = hardware_->getCurrentPosition();
    }

    logOperation(current_operation_, current_from_position_, current_to_position_,
                duration, success, error_message);

    // Reset operation tracking
    current_operation_.clear();
    current_from_position_ = -1;
    current_to_position_ = -1;
}

std::vector<OperationRecord> MonitoringSystem::getOperationHistory(int max_records) const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    if (max_records <= 0 || max_records >= static_cast<int>(operation_history_.size())) {
        return operation_history_;
    }

    // Return the most recent records
    auto start_it = operation_history_.end() - max_records;
    return std::vector<OperationRecord>(start_it, operation_history_.end());
}

std::vector<OperationRecord> MonitoringSystem::getOperationHistoryByType(const std::string& operation_type, int max_records) const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    std::vector<OperationRecord> filtered;
    for (auto it = operation_history_.rbegin(); it != operation_history_.rend() && static_cast<int>(filtered.size()) < max_records; ++it) {
        if (it->operation_type == operation_type) {
            filtered.push_back(*it);
        }
    }

    // Reverse to maintain chronological order
    std::reverse(filtered.begin(), filtered.end());
    return filtered;
}

std::vector<OperationRecord> MonitoringSystem::getOperationHistoryByTimeRange(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) const {

    std::lock_guard<std::mutex> lock(history_mutex_);

    std::vector<OperationRecord> filtered;
    for (const auto& record : operation_history_) {
        if (record.timestamp >= start && record.timestamp <= end) {
            filtered.push_back(record);
        }
    }

    return filtered;
}

void MonitoringSystem::clearOperationHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    operation_history_.clear();
    spdlog::info("Cleared operation history");
}

void MonitoringSystem::setMaxHistorySize(int max_size) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    max_history_size_ = std::max(10, max_size); // Minimum 10 records

    // Prune if current history exceeds new limit
    if (static_cast<int>(operation_history_.size()) > max_history_size_) {
        operation_history_.erase(operation_history_.begin(),
                               operation_history_.begin() + (operation_history_.size() - max_history_size_));
    }

    spdlog::info("Set max history size to {}", max_history_size_);
}

int MonitoringSystem::getOverallStatistics() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return static_cast<int>(operation_history_.size());
}

int MonitoringSystem::getStatisticsByType(const std::string& operation_type) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::vector<OperationRecord> filtered = filterRecordsByType(operation_history_, operation_type);
    return static_cast<int>(filtered.size());
}

int MonitoringSystem::getStatisticsByTimeRange(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) const {

    std::lock_guard<std::mutex> lock(history_mutex_);
    std::vector<OperationRecord> filtered = filterRecordsByTimeRange(operation_history_, start, end);
    return static_cast<int>(filtered.size());
}

void MonitoringSystem::startHealthMonitoring(int check_interval_ms) {
    if (health_monitoring_active_) {
        spdlog::warn("Health monitoring already active");
        return;
    }

    health_check_interval_ms_ = std::max(1000, check_interval_ms); // Minimum 1 second
    health_monitoring_active_ = true;

    health_monitoring_thread_ = std::thread([this]() {
        healthMonitoringLoop();
    });

    spdlog::info("Started health monitoring (interval: {} ms)", health_check_interval_ms_);
}

void MonitoringSystem::stopHealthMonitoring() {
    if (!health_monitoring_active_) {
        return;
    }

    health_monitoring_active_ = false;

    if (health_monitoring_thread_.joinable()) {
        health_monitoring_thread_.join();
    }

    spdlog::info("Stopped health monitoring");
}

bool MonitoringSystem::isHealthMonitoringActive() const {
    return health_monitoring_active_;
}

HealthMetrics MonitoringSystem::getCurrentHealthMetrics() const {
    HealthMetrics metrics;
    updateHealthMetrics(metrics);
    return metrics;
}

std::vector<HealthMetrics> MonitoringSystem::getHealthHistory(int max_records) const {
    std::lock_guard<std::mutex> lock(health_mutex_);

    if (max_records <= 0 || max_records >= static_cast<int>(health_history_.size())) {
        return health_history_;
    }

    // Return the most recent records
    auto start_it = health_history_.end() - max_records;
    return std::vector<HealthMetrics>(start_it, health_history_.end());
}

double MonitoringSystem::getAverageOperationTime() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (operation_history_.empty()) {
        return 0.0;
    }

    std::chrono::milliseconds total_time(0);
    for (const auto& record : operation_history_) {
        total_time += record.duration;
    }

    return static_cast<double>(total_time.count()) / static_cast<double>(operation_history_.size());
}

double MonitoringSystem::getSuccessRate() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (operation_history_.empty()) {
        return 0.0;
    }

    int successful_operations = 0;
    for (const auto& record : operation_history_) {
        if (record.success) {
            successful_operations++;
        }
    }

    return (static_cast<double>(successful_operations) / static_cast<double>(operation_history_.size())) * 100.0;
}

int MonitoringSystem::getConsecutiveFailures() const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    int consecutive_failures = 0;
    for (auto it = operation_history_.rbegin(); it != operation_history_.rend(); ++it) {
        if (!it->success) {
            consecutive_failures++;
        } else {
            break;
        }
    }

    return consecutive_failures;
}

std::chrono::system_clock::time_point MonitoringSystem::getLastOperationTime() const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    if (operation_history_.empty()) {
        return std::chrono::system_clock::time_point{};
    }

    return operation_history_.back().timestamp;
}

void MonitoringSystem::setFailureThreshold(int max_consecutive_failures) {
    failure_threshold_ = std::max(1, max_consecutive_failures);
    spdlog::info("Set failure threshold to {}", failure_threshold_);
}

void MonitoringSystem::setResponseTimeThreshold(std::chrono::milliseconds max_response_time) {
    response_time_threshold_ = max_response_time;
    spdlog::info("Set response time threshold to {} ms", max_response_time.count());
}

bool MonitoringSystem::isHealthy() const {
    HealthMetrics metrics = getCurrentHealthMetrics();

    // Check basic connectivity
    if (!metrics.is_connected || !metrics.is_responding) {
        return false;
    }

    // Check consecutive failures
    if (metrics.consecutive_failures >= failure_threshold_) {
        return false;
    }

    // Check success rate (require at least 80% success rate)
    if (metrics.success_rate < 80.0) {
        return false;
    }

    return true;
}

std::vector<std::string> MonitoringSystem::getHealthWarnings() const {
    std::vector<std::string> warnings;
    HealthMetrics metrics = getCurrentHealthMetrics();

    if (!metrics.is_connected) {
        warnings.push_back("Device not connected");
    }

    if (!metrics.is_responding) {
        warnings.push_back("Device not responding");
    }

    if (metrics.consecutive_failures >= failure_threshold_) {
        warnings.push_back("Too many consecutive failures (" + std::to_string(metrics.consecutive_failures) + ")");
    }

    if (metrics.success_rate < 80.0) {
        warnings.push_back("Low success rate (" + std::to_string(metrics.success_rate) + "%)");
    }

    return warnings;
}

bool MonitoringSystem::exportOperationHistory(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for export: {}", filepath);
            return false;
        }

        // Write CSV header
        file << "Timestamp,Operation,From Position,To Position,Duration (ms),Success,Error Message\n";

        // Write operation records
        for (const auto& record : operation_history_) {
            auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);

            file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
                 << record.operation_type << ","
                 << record.from_position << ","
                 << record.to_position << ","
                 << record.duration.count() << ","
                 << (record.success ? "true" : "false") << ","
                 << "\"" << record.error_message << "\"\n";
        }

        spdlog::info("Exported operation history to: {}", filepath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to export operation history: {}", e.what());
        return false;
    }
}

bool MonitoringSystem::exportHealthReport(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for health report: {}", filepath);
            return false;
        }

        file << generateHealthSummary() << "\n\n";
        file << generatePerformanceReport() << "\n";

        spdlog::info("Exported health report to: {}", filepath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to export health report: {}", e.what());
        return false;
    }
}

std::string MonitoringSystem::generateHealthSummary() const {
    HealthMetrics metrics = getCurrentHealthMetrics();
    std::stringstream ss;

    ss << "=== Filterwheel Health Summary ===\n";
    ss << "Connection Status: " << (metrics.is_connected ? "Connected" : "Disconnected") << "\n";
    ss << "Response Status: " << (metrics.is_responding ? "Responding" : "Not Responding") << "\n";
    ss << "Movement Status: " << (metrics.is_moving ? "Moving" : "Idle") << "\n";
    ss << "Current Position: " << metrics.current_position << "\n";
    ss << "Success Rate: " << std::fixed << std::setprecision(1) << metrics.success_rate << "%\n";
    ss << "Consecutive Failures: " << metrics.consecutive_failures << "\n";
    ss << "Overall Health: " << (isHealthy() ? "Healthy" : "Unhealthy") << "\n";

    auto warnings = getHealthWarnings();
    if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& warning : warnings) {
            ss << "- " << warning << "\n";
        }
    }

    return ss.str();
}

std::string MonitoringSystem::generatePerformanceReport() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::stringstream ss;

    int total_operations = static_cast<int>(operation_history_.size());
    int successful_operations = 0;
    std::chrono::milliseconds total_time(0);
    std::chrono::milliseconds min_time = std::chrono::milliseconds::max();
    std::chrono::milliseconds max_time(0);

    for (const auto& record : operation_history_) {
        if (record.success) {
            successful_operations++;
        }
        total_time += record.duration;

        if (record.duration < min_time) {
            min_time = record.duration;
        }
        if (record.duration > max_time) {
            max_time = record.duration;
        }
    }

    int failed_operations = total_operations - successful_operations;
    double average_time = total_operations > 0 ?
        static_cast<double>(total_time.count()) / static_cast<double>(total_operations) : 0.0;

    ss << "=== Performance Report ===\n";
    ss << "Total Operations: " << total_operations << "\n";
    ss << "Successful Operations: " << successful_operations << "\n";
    ss << "Failed Operations: " << failed_operations << "\n";
    ss << "Success Rate: " << std::fixed << std::setprecision(1) << getSuccessRate() << "%\n";
    ss << "Average Operation Time: " << std::fixed << std::setprecision(1) << average_time << " ms\n";

    if (total_operations > 0) {
        ss << "Min Operation Time: " << min_time.count() << " ms\n";
        ss << "Max Operation Time: " << max_time.count() << " ms\n";
        ss << "Total Operation Time: " << total_time.count() << " ms\n";
    }

    return ss.str();
}

void MonitoringSystem::setHealthCallback(HealthCallback callback) {
    health_callback_ = std::move(callback);
}

void MonitoringSystem::setAlertCallback(AlertCallback callback) {
    alert_callback_ = std::move(callback);
}

void MonitoringSystem::clearCallbacks() {
    health_callback_ = nullptr;
    alert_callback_ = nullptr;
}

void MonitoringSystem::healthMonitoringLoop() {
    while (health_monitoring_active_) {
        try {
            performHealthCheck();
            std::this_thread::sleep_for(std::chrono::milliseconds(health_check_interval_ms_));
        } catch (const std::exception& e) {
            spdlog::error("Exception in health monitoring loop: {}", e.what());
        }
    }
}

void MonitoringSystem::performHealthCheck() {
    HealthMetrics metrics;
    updateHealthMetrics(metrics);

    // Store in history
    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        health_history_.push_back(metrics);

        // Prune history if needed
        if (static_cast<int>(health_history_.size()) > max_health_history_size_) {
            health_history_.erase(health_history_.begin(),
                                health_history_.begin() + (health_history_.size() - max_health_history_size_));
        }
    }

    // Check for alert conditions
    checkAlertConditions(metrics);

    // Notify callback if set
    if (health_callback_) {
        try {
            health_callback_(metrics);
        } catch (const std::exception& e) {
            spdlog::error("Exception in health callback: {}", e.what());
        }
    }
}

void MonitoringSystem::updateHealthMetrics(HealthMetrics& metrics) const {
    metrics.last_health_check = std::chrono::system_clock::now();

    if (hardware_) {
        metrics.is_connected = hardware_->isConnected();
        metrics.is_responding = true; // Assume responding if we can query
        metrics.is_moving = hardware_->isMoving();
        metrics.current_position = hardware_->getCurrentPosition();
    } else {
        metrics.is_connected = false;
        metrics.is_responding = false;
        metrics.is_moving = false;
        metrics.current_position = -1;
    }

    // Calculate success rate and consecutive failures
    metrics.success_rate = getSuccessRate();
    metrics.consecutive_failures = getConsecutiveFailures();

    // Get recent errors (last 5)
    std::lock_guard<std::mutex> lock(history_mutex_);
    metrics.recent_errors.clear();
    int error_count = 0;
    for (auto it = operation_history_.rbegin(); it != operation_history_.rend() && error_count < 5; ++it) {
        if (!it->success && !it->error_message.empty()) {
            metrics.recent_errors.push_back(it->error_message);
            error_count++;
        }
    }
}

void MonitoringSystem::checkAlertConditions(const HealthMetrics& metrics) {
    // Check for connection issues
    if (!metrics.is_connected) {
        triggerAlert("connection", "Device disconnected");
    }

    // Check for consecutive failures
    if (metrics.consecutive_failures >= failure_threshold_) {
        triggerAlert("failures", "Too many consecutive failures: " + std::to_string(metrics.consecutive_failures));
    }

    // Check success rate
    if (metrics.success_rate < 80.0 && metrics.success_rate > 0.0) {
        triggerAlert("performance", "Low success rate: " + std::to_string(metrics.success_rate) + "%");
    }
}

void MonitoringSystem::triggerAlert(const std::string& alert_type, const std::string& message) {
    spdlog::warn("Health alert [{}]: {}", alert_type, message);

    if (alert_callback_) {
        try {
            alert_callback_(alert_type, message);
        } catch (const std::exception& e) {
            spdlog::error("Exception in alert callback: {}", e.what());
        }
    }
}



std::vector<OperationRecord> MonitoringSystem::filterRecordsByType(const std::vector<OperationRecord>& records,
                                                                   const std::string& operation_type) const {
    std::vector<OperationRecord> filtered;
    std::copy_if(records.begin(), records.end(), std::back_inserter(filtered),
                [&operation_type](const OperationRecord& record) {
                    return record.operation_type == operation_type;
                });
    return filtered;
}

std::vector<OperationRecord> MonitoringSystem::filterRecordsByTimeRange(const std::vector<OperationRecord>& records,
                                                                        std::chrono::system_clock::time_point start,
                                                                        std::chrono::system_clock::time_point end) const {
    std::vector<OperationRecord> filtered;
    std::copy_if(records.begin(), records.end(), std::back_inserter(filtered),
                [start, end](const OperationRecord& record) {
                    return record.timestamp >= start && record.timestamp <= end;
                });
    return filtered;
}

} // namespace lithium::device::asi::filterwheel
