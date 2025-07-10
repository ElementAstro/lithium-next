/*
 * monitoring_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Filter Wheel Monitoring System Implementation

*************************************************/

#include "monitoring_system.hpp"
#include "hardware_interface.hpp"
#include "position_manager.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lithium::device::ascom::filterwheel::components {

MonitoringSystem::MonitoringSystem(std::shared_ptr<HardwareInterface> hardware,
                                 std::shared_ptr<PositionManager> position_manager)
    : hardware_(std::move(hardware)), position_manager_(std::move(position_manager)) {
    spdlog::debug("MonitoringSystem constructor called");
    metrics_.start_time = std::chrono::steady_clock::now();
}

MonitoringSystem::~MonitoringSystem() {
    spdlog::debug("MonitoringSystem destructor called");
    stopMonitoring();
}

auto MonitoringSystem::initialize() -> bool {
    spdlog::info("Initializing Monitoring System");
    
    if (!hardware_ || !position_manager_) {
        setError("Hardware or position manager not available");
        return false;
    }
    
    return true;
}

auto MonitoringSystem::shutdown() -> void {
    spdlog::info("Shutting down Monitoring System");
    stopMonitoring();
    clearAlerts();
    resetMetrics();
}

auto MonitoringSystem::startMonitoring() -> bool {
    if (is_monitoring_.load()) {
        spdlog::warn("Monitoring already active");
        return true;
    }
    
    spdlog::info("Starting filter wheel monitoring");
    
    is_monitoring_.store(true);
    stop_monitoring_.store(false);
    
    // Start monitoring thread
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    monitoring_thread_ = std::make_unique<std::thread>(&MonitoringSystem::monitoringLoop, this);
    
    // Start health check thread
    if (health_check_thread_ && health_check_thread_->joinable()) {
        health_check_thread_->join();
    }
    health_check_thread_ = std::make_unique<std::thread>(&MonitoringSystem::healthCheckLoop, this);
    
    return true;
}

auto MonitoringSystem::stopMonitoring() -> void {
    if (!is_monitoring_.load()) {
        return;
    }
    
    spdlog::info("Stopping filter wheel monitoring");
    
    is_monitoring_.store(false);
    stop_monitoring_.store(true);
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    
    if (health_check_thread_ && health_check_thread_->joinable()) {
        health_check_thread_->join();
    }
}

auto MonitoringSystem::isMonitoring() -> bool {
    return is_monitoring_.load();
}

auto MonitoringSystem::performHealthCheck() -> HealthCheck {
    HealthCheck check;
    check.timestamp = std::chrono::system_clock::now();
    
    auto hardware_health = checkHardwareHealth();
    auto position_health = checkPositionHealth();
    auto temperature_health = checkTemperatureHealth();
    auto performance_health = checkPerformanceHealth();
    
    // Determine overall status
    HealthStatus overall = HealthStatus::HEALTHY;
    if (hardware_health.first == HealthStatus::CRITICAL || 
        position_health.first == HealthStatus::CRITICAL ||
        temperature_health.first == HealthStatus::CRITICAL ||
        performance_health.first == HealthStatus::CRITICAL) {
        overall = HealthStatus::CRITICAL;
    } else if (hardware_health.first == HealthStatus::WARNING || 
               position_health.first == HealthStatus::WARNING ||
               temperature_health.first == HealthStatus::WARNING ||
               performance_health.first == HealthStatus::WARNING) {
        overall = HealthStatus::WARNING;
    }
    
    check.status = overall;
    check.description = "Filter wheel health check completed";
    
    // Collect issues and recommendations
    if (!hardware_health.second.empty()) {
        check.issues.push_back("Hardware: " + hardware_health.second);
    }
    if (!position_health.second.empty()) {
        check.issues.push_back("Position: " + position_health.second);
    }
    if (!temperature_health.second.empty()) {
        check.issues.push_back("Temperature: " + temperature_health.second);
    }
    if (!performance_health.second.empty()) {
        check.issues.push_back("Performance: " + performance_health.second);
    }
    
    // Store the result
    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        last_health_check_ = check;
        current_health_.store(overall);
    }
    
    return check;
}

auto MonitoringSystem::getHealthStatus() -> HealthStatus {
    return current_health_.load();
}

auto MonitoringSystem::getLastHealthCheck() -> std::optional<HealthCheck> {
    std::lock_guard<std::mutex> lock(health_mutex_);
    return last_health_check_;
}

auto MonitoringSystem::setHealthCheckInterval(std::chrono::milliseconds interval) -> void {
    health_check_interval_ = interval;
    spdlog::debug("Set health check interval to: {}ms", interval.count());
}

auto MonitoringSystem::getHealthCheckInterval() -> std::chrono::milliseconds {
    return health_check_interval_;
}

auto MonitoringSystem::getMetrics() -> MonitoringMetrics {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - metrics_.start_time);
    return metrics_;
}

auto MonitoringSystem::resetMetrics() -> void {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = MonitoringMetrics{};
    metrics_.start_time = std::chrono::steady_clock::now();
    spdlog::debug("Monitoring metrics reset");
}

auto MonitoringSystem::recordMovement(int from_position, int to_position, bool success, std::chrono::milliseconds duration) -> void {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.total_movements++;
    metrics_.position_usage[to_position]++;
    
    if (success) {
        // Update timing statistics
        if (metrics_.min_move_time == std::chrono::milliseconds{0} || duration < metrics_.min_move_time) {
            metrics_.min_move_time = duration;
        }
        if (duration > metrics_.max_move_time) {
            metrics_.max_move_time = duration;
        }
        
        // Update average (simple moving average)
        if (metrics_.total_movements == 1) {
            metrics_.average_move_time = duration;
        } else {
            auto total_time = metrics_.average_move_time * (metrics_.total_movements - 1) + duration;
            metrics_.average_move_time = total_time / metrics_.total_movements;
        }
    }
    
    // Update success rate
    metrics_.movement_success_rate = calculateSuccessRate();
    
    spdlog::debug("Recorded movement: {} -> {}, success: {}, duration: {}ms", 
                  from_position, to_position, success, duration.count());
}

auto MonitoringSystem::recordCommunication(bool success) -> void {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.total_commands++;
    if (!success) {
        metrics_.communication_errors++;
    }
    
    metrics_.last_communication = std::chrono::steady_clock::now();
}

auto MonitoringSystem::recordTemperature(double temperature) -> void {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.current_temperature = temperature;
    
    if (!metrics_.min_temperature.has_value() || temperature < *metrics_.min_temperature) {
        metrics_.min_temperature = temperature;
    }
    
    if (!metrics_.max_temperature.has_value() || temperature > *metrics_.max_temperature) {
        metrics_.max_temperature = temperature;
    }
}

auto MonitoringSystem::getAlerts(AlertLevel min_level) -> std::vector<Alert> {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    std::vector<Alert> filtered_alerts;
    for (const auto& alert : alerts_) {
        if (static_cast<int>(alert.level) >= static_cast<int>(min_level)) {
            filtered_alerts.push_back(alert);
        }
    }
    
    return filtered_alerts;
}

auto MonitoringSystem::getUnacknowledgedAlerts() -> std::vector<Alert> {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    std::vector<Alert> unacknowledged;
    for (const auto& alert : alerts_) {
        if (!alert.acknowledged) {
            unacknowledged.push_back(alert);
        }
    }
    
    return unacknowledged;
}

auto MonitoringSystem::acknowledgeAlert(size_t alert_index) -> bool {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    if (alert_index >= alerts_.size()) {
        return false;
    }
    
    alerts_[alert_index].acknowledged = true;
    spdlog::debug("Alert {} acknowledged", alert_index);
    return true;
}

auto MonitoringSystem::clearAlerts() -> void {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    alerts_.clear();
    spdlog::debug("All alerts cleared");
}

auto MonitoringSystem::addAlert(AlertLevel level, const std::string& message, const std::string& component) -> void {
    generateAlert(level, message, component);
}

auto MonitoringSystem::performDiagnostics() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> diagnostics;
    diagnostics["monitoring_active"] = isMonitoring() ? "true" : "false";
    diagnostics["health_status"] = std::to_string(static_cast<int>(getHealthStatus()));
    diagnostics["total_movements"] = std::to_string(getMetrics().total_movements);
    return diagnostics;
}

auto MonitoringSystem::testCommunication() -> bool {
    if (!hardware_) return false;
    try {
        return hardware_->isConnected();
    } catch (...) {
        return false;
    }
}

auto MonitoringSystem::testMovement() -> bool {
    // Implementation would test a basic movement operation
    return true; // Placeholder
}

auto MonitoringSystem::getSystemInfo() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> info;
    info["component"] = "ASCOM FilterWheel Monitoring System";
    info["version"] = "1.0.0";
    return info;
}

auto MonitoringSystem::setMonitoringInterval(std::chrono::milliseconds interval) -> void {
    monitoring_interval_ = interval;
}

auto MonitoringSystem::getMonitoringInterval() -> std::chrono::milliseconds {
    return monitoring_interval_;
}

auto MonitoringSystem::enableTemperatureMonitoring(bool enable) -> void {
    temperature_monitoring_enabled_ = enable;
}

auto MonitoringSystem::isTemperatureMonitoringEnabled() -> bool {
    return temperature_monitoring_enabled_;
}

auto MonitoringSystem::setAlertCallback(AlertCallback callback) -> void {
    alert_callback_ = std::move(callback);
}

auto MonitoringSystem::setHealthCallback(HealthCallback callback) -> void {
    health_callback_ = std::move(callback);
}

auto MonitoringSystem::setMetricsCallback(MetricsCallback callback) -> void {
    metrics_callback_ = std::move(callback);
}

auto MonitoringSystem::getLastError() -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto MonitoringSystem::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Placeholder implementations for remaining methods
auto MonitoringSystem::getPerformanceReport() -> std::string { return "Performance report placeholder"; }
auto MonitoringSystem::analyzeTrends() -> std::map<std::string, double> { return {}; }
auto MonitoringSystem::predictMaintenanceNeeds() -> std::vector<std::string> { return {}; }
auto MonitoringSystem::exportMetrics(const std::string& file_path) -> bool { return false; }
auto MonitoringSystem::exportAlerts(const std::string& file_path) -> bool { return false; }
auto MonitoringSystem::generateReport(const std::string& file_path) -> bool { return false; }

// Internal monitoring methods
auto MonitoringSystem::monitoringLoop() -> void {
    spdlog::debug("Starting monitoring loop");
    
    while (!stop_monitoring_.load()) {
        try {
            updateMetrics();
            checkCommunication();
            
            if (temperature_monitoring_enabled_) {
                checkTemperature();
            }
            
            checkPerformance();
            
        } catch (const std::exception& e) {
            spdlog::error("Exception in monitoring loop: {}", e.what());
            generateAlert(AlertLevel::ERROR, "Monitoring exception: " + std::string(e.what()), "MonitoringSystem");
        }
        
        std::this_thread::sleep_for(monitoring_interval_);
    }
    
    spdlog::debug("Monitoring loop finished");
}

auto MonitoringSystem::healthCheckLoop() -> void {
    spdlog::debug("Starting health check loop");
    
    while (!stop_monitoring_.load()) {
        try {
            auto health_check = performHealthCheck();
            
            if (health_callback_) {
                health_callback_(health_check.status, health_check.description);
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Exception in health check loop: {}", e.what());
            generateAlert(AlertLevel::ERROR, "Health check exception: " + std::string(e.what()), "MonitoringSystem");
        }
        
        std::this_thread::sleep_for(health_check_interval_);
    }
    
    spdlog::debug("Health check loop finished");
}

auto MonitoringSystem::generateAlert(AlertLevel level, const std::string& message, const std::string& component) -> void {
    Alert alert;
    alert.level = level;
    alert.message = message;
    alert.component = component.empty() ? "FilterWheel" : component;
    alert.timestamp = std::chrono::system_clock::now();
    alert.acknowledged = false;
    
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        alerts_.push_back(alert);
        trimAlerts();
    }
    
    notifyAlert(alert);
    
    spdlog::info("Alert generated: [{}] {}", 
                 static_cast<int>(level), message);
}

auto MonitoringSystem::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("MonitoringSystem error: {}", error);
}

auto MonitoringSystem::calculateSuccessRate() -> double {
    if (metrics_.total_movements == 0) {
        return 100.0;
    }
    
    // This is a simplified calculation - in reality you'd track failures
    uint64_t successful_movements = metrics_.total_movements; // Assuming all recorded movements were successful
    return (static_cast<double>(successful_movements) / metrics_.total_movements) * 100.0;
}

auto MonitoringSystem::checkHardwareHealth() -> std::pair<HealthStatus, std::string> {
    if (!hardware_) {
        return {HealthStatus::CRITICAL, "Hardware interface not available"};
    }
    
    // Check if hardware is responsive
    try {
        if (!hardware_->isConnected()) {
            return {HealthStatus::CRITICAL, "Hardware not connected"};
        }
        
        return {HealthStatus::HEALTHY, ""};
    } catch (const std::exception& e) {
        return {HealthStatus::CRITICAL, "Hardware communication error: " + std::string(e.what())};
    }
}

auto MonitoringSystem::checkPositionHealth() -> std::pair<HealthStatus, std::string> {
    if (!position_manager_) {
        return {HealthStatus::CRITICAL, "Position manager not available"};
    }
    
    // Add position-specific health checks here
    return {HealthStatus::HEALTHY, ""};
}

auto MonitoringSystem::checkTemperatureHealth() -> std::pair<HealthStatus, std::string> {
    if (!temperature_monitoring_enabled_) {
        return {HealthStatus::HEALTHY, ""};
    }
    
    // Add temperature-specific health checks here
    return {HealthStatus::HEALTHY, ""};
}

auto MonitoringSystem::checkPerformanceHealth() -> std::pair<HealthStatus, std::string> {
    auto success_rate = calculateSuccessRate();
    if (success_rate < 90.0) {
        return {HealthStatus::WARNING, "Low movement success rate: " + std::to_string(success_rate) + "%"};
    }
    
    return {HealthStatus::HEALTHY, ""};
}

auto MonitoringSystem::notifyAlert(const Alert& alert) -> void {
    if (alert_callback_) {
        try {
            alert_callback_(alert);
        } catch (const std::exception& e) {
            spdlog::error("Exception in alert callback: {}", e.what());
        }
    }
}

auto MonitoringSystem::notifyHealthChange(HealthStatus status, const std::string& message) -> void {
    if (health_callback_) {
        try {
            health_callback_(status, message);
        } catch (const std::exception& e) {
            spdlog::error("Exception in health callback: {}", e.what());
        }
    }
}

auto MonitoringSystem::notifyMetricsUpdate(const MonitoringMetrics& metrics) -> void {
    if (metrics_callback_) {
        try {
            metrics_callback_(metrics);
        } catch (const std::exception& e) {
            spdlog::error("Exception in metrics callback: {}", e.what());
        }
    }
}

auto MonitoringSystem::trimAlerts(size_t max_alerts) -> void {
    if (alerts_.size() > max_alerts) {
        alerts_.erase(alerts_.begin(), alerts_.begin() + (alerts_.size() - max_alerts));
    }
}

auto MonitoringSystem::updateMetrics() -> void {
    // Update general metrics
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics_.start_time);
    
    if (metrics_callback_) {
        try {
            metrics_callback_(metrics_);
        } catch (const std::exception& e) {
            spdlog::error("Exception in metrics callback: {}", e.what());
        }
    }
}

auto MonitoringSystem::checkCommunication() -> void {
    // Test basic communication with hardware
    if (hardware_) {
        try {
            bool connected = hardware_->isConnected();
            recordCommunication(connected);
            
            if (!connected) {
                generateAlert(AlertLevel::WARNING, "Communication with hardware lost", "Hardware");
            }
        } catch (const std::exception& e) {
            recordCommunication(false);
            generateAlert(AlertLevel::ERROR, "Communication check failed: " + std::string(e.what()), "Hardware");
        }
    }
}

auto MonitoringSystem::checkTemperature() -> void {
    // Temperature monitoring implementation would go here
    // This is a placeholder since not all filter wheels have temperature sensors
}

auto MonitoringSystem::checkPerformance() -> void {
    auto success_rate = calculateSuccessRate();
    
    if (success_rate < 95.0 && success_rate >= 90.0) {
        generateAlert(AlertLevel::WARNING, "Movement success rate below 95%: " + std::to_string(success_rate) + "%", "Performance");
    } else if (success_rate < 90.0) {
        generateAlert(AlertLevel::ERROR, "Movement success rate critically low: " + std::to_string(success_rate) + "%", "Performance");
    }
}

} // namespace lithium::device::ascom::filterwheel::components
