/*
 * device_performance_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "device_performance_monitor.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>
#include <numeric>
#include <shared_mutex>

namespace lithium {

// Internal snapshot structure for history tracking
struct PerformanceSnapshot {
    std::chrono::system_clock::time_point timestamp;
    PerformanceMetrics metrics;
};

class DevicePerformanceMonitor::Impl {
public:
    MonitoringConfig config_;
    PerformanceThresholds global_thresholds_;

    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> devices_;
    std::unordered_map<std::string, PerformanceMetrics> current_metrics_;
    std::unordered_map<std::string, PerformanceStatistics> statistics_;
    std::unordered_map<std::string, PerformanceThresholds> device_thresholds_;
    std::unordered_map<std::string, std::vector<PerformanceSnapshot>> history_;
    std::unordered_map<std::string, bool> device_monitoring_enabled_;

    mutable std::shared_mutex monitor_mutex_;
    std::atomic<bool> monitoring_{false};
    std::thread monitoring_thread_;

    // Alert management
    std::vector<PerformanceAlert> active_alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_alert_times_;

    // Callbacks
    PerformanceAlertCallback alert_callback_;
    PerformanceUpdateCallback update_callback_;

    // Statistics
    std::chrono::system_clock::time_point start_time_;

    Impl() : start_time_(std::chrono::system_clock::now()) {}

    ~Impl() {
        stopMonitoring();
    }

    void startMonitoring() {
        if (monitoring_.exchange(true)) {
            return; // Already monitoring
        }

        monitoring_thread_ = std::thread(&Impl::monitoringLoop, this);
        spdlog::info("Device performance monitoring started");
    }

    void stopMonitoring() {
        if (!monitoring_.exchange(false)) {
            return; // Already stopped
        }

        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }

        spdlog::info("Device performance monitoring stopped");
    }

    void monitoringLoop() {
        while (monitoring_) {
            try {
                std::shared_lock<std::shared_mutex> lock(monitor_mutex_);

                auto now = std::chrono::system_clock::now();

                for (const auto& [device_name, device] : devices_) {
                    if (!device || !isDeviceMonitoringEnabled(device_name)) {
                        continue;
                    }

                    // Update device metrics
                    updateDeviceMetrics(device_name, device, now);

                    // Check for alerts
                    checkAlerts(device_name, now);

                    // Store snapshot
                    storeSnapshot(device_name, now);

                    // Trigger update callback
                    if (update_callback_) {
                        update_callback_(device_name, current_metrics_[device_name]);
                    }
                }

                lock.unlock();

                std::this_thread::sleep_for(config_.monitoring_interval);

            } catch (const std::exception& e) {
                spdlog::error("Error in performance monitoring loop: {}", e.what());
            }
        }
    }

    void updateDeviceMetrics(const std::string& device_name,
                           std::shared_ptr<AtomDriver> device,
                           std::chrono::system_clock::time_point now) {
        auto& metrics = current_metrics_[device_name];
        auto& stats = statistics_[device_name];

        // Update timestamp
        metrics.timestamp = now;

        // Update basic connection-based metrics
        bool is_connected = device->isConnected();

        // For demonstration, set some sample metrics
        // In a real implementation, these would come from actual device monitoring
        if (is_connected) {
            // Simulate healthy device metrics
            metrics.response_time = std::chrono::milliseconds(50 + (rand() % 100));
            metrics.operation_time = std::chrono::milliseconds(100 + (rand() % 200));
            metrics.throughput = 10.0 + (rand() % 50) / 10.0;
            metrics.error_rate = (rand() % 100) / 1000.0; // 0-10%
            metrics.cpu_usage = 20.0 + (rand() % 300) / 10.0; // 20-50%
            metrics.memory_usage = 100.0 + (rand() % 500); // 100-600 MB
            metrics.queue_depth = rand() % 20;
            metrics.concurrent_operations = rand() % 5;
        } else {
            // Device disconnected
            metrics.response_time = std::chrono::milliseconds(0);
            metrics.operation_time = std::chrono::milliseconds(0);
            metrics.throughput = 0.0;
            metrics.error_rate = 1.0; // 100% error rate when disconnected
            metrics.cpu_usage = 0.0;
            metrics.memory_usage = 0.0;
            metrics.queue_depth = 0;
            metrics.concurrent_operations = 0;
        }

        // Update statistics
        updateStatistics(device_name, metrics);
    }

    void updateStatistics(const std::string& device_name, const PerformanceMetrics& metrics) {
        auto& stats = statistics_[device_name];

        // Update current metrics
        stats.current = metrics;
        stats.last_update = metrics.timestamp;

        // Initialize start time if needed
        if (stats.start_time == std::chrono::system_clock::time_point{}) {
            stats.start_time = metrics.timestamp;
        }

        // Update operation counts (these would be updated elsewhere in real implementation)
        stats.total_operations++;
        if (metrics.error_rate < 0.1) { // Less than 10% error rate
            stats.successful_operations++;
        } else {
            stats.failed_operations++;
        }

        // Update min/max/average (simple moving average for demonstration)
        if (stats.total_operations == 1) {
            stats.minimum = metrics;
            stats.maximum = metrics;
            stats.average = metrics;
        } else {
            // Update minimums
            if (metrics.response_time < stats.minimum.response_time) {
                stats.minimum.response_time = metrics.response_time;
            }
            if (metrics.error_rate < stats.minimum.error_rate) {
                stats.minimum.error_rate = metrics.error_rate;
            }

            // Update maximums
            if (metrics.response_time > stats.maximum.response_time) {
                stats.maximum.response_time = metrics.response_time;
            }
            if (metrics.error_rate > stats.maximum.error_rate) {
                stats.maximum.error_rate = metrics.error_rate;
            }

            // Update averages (exponential moving average)
            double alpha = 0.1;
            stats.average.response_time = std::chrono::milliseconds(
                static_cast<long>(alpha * metrics.response_time.count() +
                                (1.0 - alpha) * stats.average.response_time.count()));
            stats.average.error_rate = alpha * metrics.error_rate + (1.0 - alpha) * stats.average.error_rate;
            stats.average.throughput = alpha * metrics.throughput + (1.0 - alpha) * stats.average.throughput;
        }
    }

    void checkAlerts(const std::string& device_name, std::chrono::system_clock::time_point now) {
        if (!config_.enable_real_time_alerts) {
            return;
        }

        const auto& metrics = current_metrics_[device_name];
        const auto& thresholds = getDeviceThresholds(device_name);

        // Check for alert cooldown
        auto last_alert_it = last_alert_times_.find(device_name);
        if (last_alert_it != last_alert_times_.end()) {
            auto time_since_last = now - last_alert_it->second;
            if (time_since_last < config_.alert_cooldown) {
                return; // Still in cooldown period
            }
        }

        std::vector<PerformanceAlert> new_alerts;

        // Check response time alerts
        if (metrics.response_time >= thresholds.critical_response_time) {
            PerformanceAlert alert;
            alert.device_name = device_name;
            alert.level = AlertLevel::CRITICAL;
            alert.message = "Critical response time exceeded";
            alert.metric_name = "response_time";
            alert.threshold_value = static_cast<double>(thresholds.critical_response_time.count());
            alert.current_value = static_cast<double>(metrics.response_time.count());
            alert.timestamp = now;
            new_alerts.push_back(alert);
        } else if (metrics.response_time >= thresholds.warning_response_time) {
            PerformanceAlert alert;
            alert.device_name = device_name;
            alert.level = AlertLevel::WARNING;
            alert.message = "High response time detected";
            alert.metric_name = "response_time";
            alert.threshold_value = static_cast<double>(thresholds.warning_response_time.count());
            alert.current_value = static_cast<double>(metrics.response_time.count());
            alert.timestamp = now;
            new_alerts.push_back(alert);
        }

        // Check error rate alerts
        if (metrics.error_rate >= thresholds.critical_error_rate / 100.0) {
            PerformanceAlert alert;
            alert.device_name = device_name;
            alert.level = AlertLevel::CRITICAL;
            alert.message = "Critical error rate exceeded";
            alert.metric_name = "error_rate";
            alert.threshold_value = thresholds.critical_error_rate;
            alert.current_value = metrics.error_rate * 100.0;
            alert.timestamp = now;
            new_alerts.push_back(alert);
        } else if (metrics.error_rate >= thresholds.warning_error_rate / 100.0) {
            PerformanceAlert alert;
            alert.device_name = device_name;
            alert.level = AlertLevel::WARNING;
            alert.message = "High error rate detected";
            alert.metric_name = "error_rate";
            alert.threshold_value = thresholds.warning_error_rate;
            alert.current_value = metrics.error_rate * 100.0;
            alert.timestamp = now;
            new_alerts.push_back(alert);
        }

        // Process new alerts
        for (const auto& alert : new_alerts) {
            active_alerts_.push_back(alert);

            // Trigger callback
            if (alert_callback_) {
                alert_callback_(alert);
            }

            // Update last alert time
            last_alert_times_[device_name] = now;

            // Add to device statistics
            auto& stats = statistics_[device_name];
            stats.recent_alerts.push_back(alert);

            // Keep only recent alerts
            if (stats.recent_alerts.size() > config_.max_alerts_stored) {
                stats.recent_alerts.erase(stats.recent_alerts.begin());
            }
        }

        // Keep only recent global alerts
        if (active_alerts_.size() > config_.max_alerts_stored) {
            active_alerts_.erase(active_alerts_.begin(),
                               active_alerts_.begin() + (active_alerts_.size() - config_.max_alerts_stored));
        }
    }

    void storeSnapshot(const std::string& device_name, std::chrono::system_clock::time_point now) {
        auto& hist = history_[device_name];

        PerformanceSnapshot snapshot;
        snapshot.timestamp = now;
        snapshot.metrics = current_metrics_[device_name];

        hist.push_back(snapshot);

        // Keep only recent history
        if (hist.size() > config_.max_metrics_history) {
            hist.erase(hist.begin(), hist.begin() + (hist.size() - config_.max_metrics_history));
        }
    }

    const PerformanceThresholds& getDeviceThresholds(const std::string& device_name) const {
        auto it = device_thresholds_.find(device_name);
        return it != device_thresholds_.end() ? it->second : global_thresholds_;
    }

    bool isDeviceMonitoringEnabled(const std::string& device_name) const {
        auto it = device_monitoring_enabled_.find(device_name);
        return it != device_monitoring_enabled_.end() ? it->second : true; // Default enabled
    }

    void recordOperation(const std::string& device_name,
                        std::chrono::milliseconds duration,
                        bool success) {
        std::unique_lock<std::shared_mutex> lock(monitor_mutex_);

        auto& metrics = current_metrics_[device_name];
        auto& stats = statistics_[device_name];

        // Update response time with exponential moving average
        if (metrics.response_time.count() == 0) {
            metrics.response_time = duration;
        } else {
            auto alpha = 0.1; // Smoothing factor
            auto new_avg = static_cast<long>(
                alpha * duration.count() + (1.0 - alpha) * metrics.response_time.count());
            metrics.response_time = std::chrono::milliseconds(new_avg);
        }

        // Update operation counts
        stats.total_operations++;
        if (success) {
            stats.successful_operations++;
        } else {
            stats.failed_operations++;
        }

        // Update error rate
        metrics.error_rate = static_cast<double>(stats.failed_operations) / stats.total_operations;

        // Update timestamp
        metrics.timestamp = std::chrono::system_clock::now();
    }
};

DevicePerformanceMonitor::DevicePerformanceMonitor() : pimpl_(std::make_unique<Impl>()) {}

DevicePerformanceMonitor::~DevicePerformanceMonitor() = default;

void DevicePerformanceMonitor::setMonitoringConfig(const MonitoringConfig& config) {
    pimpl_->config_ = config;
}

MonitoringConfig DevicePerformanceMonitor::getMonitoringConfig() const {
    return pimpl_->config_;
}

void DevicePerformanceMonitor::addDevice(const std::string& name, std::shared_ptr<AtomDriver> device) {
    if (!device) {
        spdlog::error("Cannot add null device {} to performance monitor", name);
        return;
    }

    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    pimpl_->devices_[name] = device;
    pimpl_->device_monitoring_enabled_[name] = true;

    // Initialize metrics and statistics
    PerformanceMetrics& metrics = pimpl_->current_metrics_[name];
    metrics.timestamp = std::chrono::system_clock::now();

    PerformanceStatistics& stats = pimpl_->statistics_[name];
    stats.start_time = metrics.timestamp;
    stats.last_update = metrics.timestamp;

    spdlog::info("Added device {} to performance monitoring", name);
}

void DevicePerformanceMonitor::removeDevice(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);

    pimpl_->devices_.erase(name);
    pimpl_->current_metrics_.erase(name);
    pimpl_->statistics_.erase(name);
    pimpl_->device_thresholds_.erase(name);
    pimpl_->history_.erase(name);
    pimpl_->device_monitoring_enabled_.erase(name);

    spdlog::info("Removed device {} from performance monitoring", name);
}

bool DevicePerformanceMonitor::isDeviceMonitored(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    return pimpl_->devices_.find(name) != pimpl_->devices_.end();
}

void DevicePerformanceMonitor::setThresholds(const std::string& device_name, const PerformanceThresholds& thresholds) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    pimpl_->device_thresholds_[device_name] = thresholds;
}

PerformanceThresholds DevicePerformanceMonitor::getThresholds(const std::string& device_name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    return pimpl_->getDeviceThresholds(device_name);
}

void DevicePerformanceMonitor::setGlobalThresholds(const PerformanceThresholds& thresholds) {
    pimpl_->global_thresholds_ = thresholds;
}

PerformanceThresholds DevicePerformanceMonitor::getGlobalThresholds() const {
    return pimpl_->global_thresholds_;
}

void DevicePerformanceMonitor::startMonitoring() {
    pimpl_->startMonitoring();
}

void DevicePerformanceMonitor::stopMonitoring() {
    pimpl_->stopMonitoring();
}

bool DevicePerformanceMonitor::isMonitoring() const {
    return pimpl_->monitoring_;
}

void DevicePerformanceMonitor::startDeviceMonitoring(const std::string& device_name) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    pimpl_->device_monitoring_enabled_[device_name] = true;
}

void DevicePerformanceMonitor::stopDeviceMonitoring(const std::string& device_name) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    pimpl_->device_monitoring_enabled_[device_name] = false;
}

bool DevicePerformanceMonitor::isDeviceMonitoring(const std::string& device_name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    return pimpl_->isDeviceMonitoringEnabled(device_name);
}

void DevicePerformanceMonitor::recordOperation(const std::string& device_name,
                                             std::chrono::milliseconds duration,
                                             bool success) {
    pimpl_->recordOperation(device_name, duration, success);
}

void DevicePerformanceMonitor::recordMetrics(const std::string& device_name, const PerformanceMetrics& metrics) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    pimpl_->current_metrics_[device_name] = metrics;
    pimpl_->updateStatistics(device_name, metrics);
}

PerformanceMetrics DevicePerformanceMonitor::getCurrentMetrics(const std::string& device_name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    auto it = pimpl_->current_metrics_.find(device_name);
    return it != pimpl_->current_metrics_.end() ? it->second : PerformanceMetrics{};
}

PerformanceStatistics DevicePerformanceMonitor::getStatistics(const std::string& device_name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    auto it = pimpl_->statistics_.find(device_name);
    return it != pimpl_->statistics_.end() ? it->second : PerformanceStatistics{};
}

std::vector<PerformanceMetrics> DevicePerformanceMonitor::getMetricsHistory(const std::string& device_name, size_t count) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);

    auto it = pimpl_->history_.find(device_name);
    if (it == pimpl_->history_.end()) {
        return {};
    }

    std::vector<PerformanceMetrics> history;
    const auto& snapshots = it->second;

    size_t start_idx = snapshots.size() > count ? snapshots.size() - count : 0;
    for (size_t i = start_idx; i < snapshots.size(); ++i) {
        history.push_back(snapshots[i].metrics);
    }

    return history;
}

void DevicePerformanceMonitor::setAlertCallback(PerformanceAlertCallback callback) {
    pimpl_->alert_callback_ = std::move(callback);
}

void DevicePerformanceMonitor::setUpdateCallback(PerformanceUpdateCallback callback) {
    pimpl_->update_callback_ = std::move(callback);
}

std::vector<PerformanceAlert> DevicePerformanceMonitor::getActiveAlerts() const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    return pimpl_->active_alerts_;
}

std::vector<PerformanceAlert> DevicePerformanceMonitor::getDeviceAlerts(const std::string& device_name) const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);
    auto it = pimpl_->statistics_.find(device_name);
    return it != pimpl_->statistics_.end() ? it->second.recent_alerts : std::vector<PerformanceAlert>{};
}

void DevicePerformanceMonitor::clearAlerts(const std::string& device_name) {
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);

    if (device_name.empty()) {
        pimpl_->active_alerts_.clear();
        for (auto& [name, stats] : pimpl_->statistics_) {
            stats.recent_alerts.clear();
        }
    } else {
        auto it = pimpl_->statistics_.find(device_name);
        if (it != pimpl_->statistics_.end()) {
            it->second.recent_alerts.clear();
        }

        // Remove from global alerts
        pimpl_->active_alerts_.erase(
            std::remove_if(pimpl_->active_alerts_.begin(), pimpl_->active_alerts_.end(),
                          [&device_name](const PerformanceAlert& alert) {
                              return alert.device_name == device_name;
                          }),
            pimpl_->active_alerts_.end());
    }
}

void DevicePerformanceMonitor::acknowledgeAlert(const PerformanceAlert& alert) {
    // For now, just remove the alert
    std::unique_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);

    auto& alerts = pimpl_->active_alerts_;
    alerts.erase(std::remove_if(alerts.begin(), alerts.end(),
                               [&alert](const PerformanceAlert& a) {
                                   return a.device_name == alert.device_name &&
                                          a.metric_name == alert.metric_name &&
                                          a.timestamp == alert.timestamp;
                               }),
                alerts.end());
}

DevicePerformanceMonitor::SystemPerformance DevicePerformanceMonitor::getSystemPerformance() const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->monitor_mutex_);

    SystemPerformance sys_perf;

    sys_perf.total_devices = pimpl_->devices_.size();

    double total_response_time = 0.0;
    double total_error_rate = 0.0;
    size_t connected_count = 0;
    size_t healthy_count = 0;

    for (const auto& [device_name, device] : pimpl_->devices_) {
        if (device && device->isConnected()) {
            connected_count++;

            auto metrics_it = pimpl_->current_metrics_.find(device_name);
            if (metrics_it != pimpl_->current_metrics_.end()) {
                const auto& metrics = metrics_it->second;

                total_response_time += metrics.response_time.count();
                total_error_rate += metrics.error_rate;

                // Consider device healthy if error rate is low
                if (metrics.error_rate < 0.05) { // Less than 5%
                    healthy_count++;
                }
            }
        }

        auto stats_it = pimpl_->statistics_.find(device_name);
        if (stats_it != pimpl_->statistics_.end()) {
            sys_perf.total_operations += stats_it->second.total_operations;
        }
    }

    sys_perf.active_devices = connected_count;
    sys_perf.healthy_devices = healthy_count;
    sys_perf.total_alerts = pimpl_->active_alerts_.size();

    if (connected_count > 0) {
        sys_perf.average_response_time = total_response_time / connected_count;
        sys_perf.average_error_rate = total_error_rate / connected_count;
    }

    // Calculate system load (simplified)
    if (sys_perf.total_devices > 0) {
        sys_perf.system_load = static_cast<double>(connected_count) / sys_perf.total_devices;
    }

    return sys_perf;
}

} // namespace lithium
