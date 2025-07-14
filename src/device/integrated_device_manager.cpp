/*
 * integrated_device_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "integrated_device_manager.hpp"
#include "device_connection_pool.hpp"
#include "device_performance_monitor.hpp"
#include "template/device.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace lithium {

class IntegratedDeviceManager::Impl {
public:
    // Configuration
    SystemConfig config_;
    std::atomic<bool> initialized_{false};

    // Device storage
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
        devices_;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>>
        primary_devices_;
    mutable std::mutex devices_mutex_;

    // Integrated components
    std::unique_ptr<DeviceConnectionPool> connection_pool_;
    std::unique_ptr<DevicePerformanceMonitor> performance_monitor_;

    // Retry strategies
    std::unordered_map<std::string, RetryStrategy> retry_strategies_;

    // Health and metrics
    std::unordered_map<std::string, DeviceHealth> device_health_;

    // Event callbacks
    DeviceEventCallback device_event_callback_;
    HealthEventCallback health_event_callback_;
    MetricsEventCallback metrics_event_callback_;

    // Background threads
    std::thread maintenance_thread_;
    std::atomic<bool> running_{false};

    // Statistics
    std::atomic<size_t> total_operations_{0};
    std::atomic<size_t> successful_operations_{0};
    std::atomic<size_t> failed_operations_{0};
    std::chrono::system_clock::time_point start_time_;

    Impl() : start_time_(std::chrono::system_clock::now()) {}

    ~Impl() { shutdown(); }

    bool initialize(const SystemConfig& config) {
        if (initialized_.exchange(true)) {
            return true;  // Already initialized
        }

        config_ = config;

        try {
            // Initialize connection pool
            if (config_.enable_connection_pooling) {
                ConnectionPoolConfig pool_config;
                pool_config.max_size = config_.max_connections_per_device;
                pool_config.connection_timeout =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        config_.connection_timeout);
                pool_config.enable_health_monitoring =
                    config_.enable_performance_monitoring;

                connection_pool_ =
                    std::make_unique<DeviceConnectionPool>(pool_config);
                connection_pool_->initialize();

                spdlog::info(
                    "Connection pool initialized with max {} connections per "
                    "device",
                    config_.max_connections_per_device);
            }

            // Initialize performance monitor
            if (config_.enable_performance_monitoring) {
                performance_monitor_ =
                    std::make_unique<DevicePerformanceMonitor>();
                MonitoringConfig monitor_config;
                monitor_config.monitoring_interval = std::chrono::seconds(
                    config_.health_check_interval.count() / 1000);
                monitor_config.enable_real_time_alerts = true;

                performance_monitor_->setMonitoringConfig(monitor_config);
                performance_monitor_->startMonitoring();

                spdlog::info("Performance monitoring initialized");
            }

            // Start background threads
            running_ = true;
            maintenance_thread_ =
                std::thread(&Impl::backgroundMaintenance, this);

            spdlog::info("Integrated device manager initialized successfully");
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize integrated device manager: {}",
                          e.what());
            initialized_ = false;
            return false;
        }
    }

    void shutdown() {
        if (!initialized_.exchange(false)) {
            return;  // Already shutdown
        }

        running_ = false;

        // Wait for background threads
        if (maintenance_thread_.joinable()) {
            maintenance_thread_.join();
        }

        // Shutdown components
        if (connection_pool_) {
            connection_pool_->shutdown();
        }

        if (performance_monitor_) {
            performance_monitor_->stopMonitoring();
        }

        spdlog::info("Integrated device manager shutdown completed");
    }

    void backgroundMaintenance() {
        while (running_) {
            try {
                // Connection pool maintenance
                if (connection_pool_) {
                    connection_pool_->runMaintenance();
                }

                // Health monitoring
                updateDeviceHealth();

                std::this_thread::sleep_for(std::chrono::minutes(1));

            } catch (const std::exception& e) {
                spdlog::error("Error in background maintenance: {}", e.what());
            }
        }
    }

    void updateDeviceHealth() {
        std::lock_guard<std::mutex> lock(devices_mutex_);

        for (const auto& [type, device_list] : devices_) {
            for (const auto& device : device_list) {
                if (!device)
                    continue;

                DeviceHealth health;
                health.last_check = std::chrono::system_clock::now();
                health.connection_quality = device->isConnected() ? 1.0f : 0.0f;

                // Get metrics if available
                if (performance_monitor_) {
                    auto metrics = performance_monitor_->getCurrentMetrics(
                        device->getName());
                    health.response_time =
                        static_cast<float>(metrics.response_time.count());
                    health.error_rate = static_cast<float>(metrics.error_rate);
                    health.operations_count =
                        static_cast<uint32_t>(total_operations_.load());
                }

                // Calculate overall health
                health.overall_health =
                    (health.connection_quality + (1.0f - health.error_rate)) /
                    2.0f;

                device_health_[device->getName()] = health;

                // Trigger callback if health is poor
                if (health.overall_health < 0.5f && health_event_callback_) {
                    health_event_callback_(device->getName(), health);
                }
            }
        }
    }

    std::shared_ptr<AtomDriver> findDeviceByName(
        const std::string& name) const {
        for (const auto& [type, device_list] : devices_) {
            for (const auto& device : device_list) {
                if (device && device->getName() == name) {
                    return device;
                }
            }
        }
        return nullptr;
    }

    bool executeDeviceOperation(
        const std::string& device_name,
        std::function<bool(std::shared_ptr<AtomDriver>)> operation) {
        auto device = findDeviceByName(device_name);
        if (!device) {
            spdlog::error("Device {} not found", device_name);
            return false;
        }

        total_operations_++;
        auto start_time = std::chrono::steady_clock::now();

        try {
            bool result = operation(device);

            auto end_time = std::chrono::steady_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);

            // Update metrics
            if (performance_monitor_) {
                performance_monitor_->recordOperation(device_name, duration,
                                                      result);
            }

            if (result) {
                successful_operations_++;
            } else {
                failed_operations_++;
            }

            // Trigger callback
            if (device_event_callback_) {
                device_event_callback_(device_name, "operation",
                                       result ? "success" : "failure");
            }

            return result;

        } catch (const std::exception& e) {
            failed_operations_++;
            spdlog::error("Device operation failed for {}: {}", device_name,
                          e.what());

            if (device_event_callback_) {
                device_event_callback_(device_name, "operation",
                                       "error: " + std::string(e.what()));
            }

            return false;
        }
    }

    bool connectDeviceWithRetry(const std::string& device_name,
                                std::chrono::milliseconds timeout) {
        auto device = findDeviceByName(device_name);
        if (!device) {
            return false;
        }

        // Get retry strategy
        RetryStrategy strategy = config_.default_retry_strategy;
        auto it = retry_strategies_.find(device_name);
        if (it != retry_strategies_.end()) {
            strategy = it->second;
        }

        size_t attempts = 0;
        std::chrono::milliseconds delay = config_.retry_delay;

        while (attempts < config_.max_retry_attempts) {
            try {
                if (device->connect("", timeout.count(), 1)) {
                    spdlog::info(
                        "Device {} connected successfully on attempt {}",
                        device_name, attempts + 1);
                    return true;
                }
            } catch (const std::exception& e) {
                spdlog::warn("Connection attempt {} failed for device {}: {}",
                             attempts + 1, device_name, e.what());
            }

            attempts++;

            if (attempts < config_.max_retry_attempts) {
                std::this_thread::sleep_for(delay);

                // Adjust delay based on strategy
                switch (strategy) {
                    case RetryStrategy::LINEAR:
                        delay += config_.retry_delay;
                        break;
                    case RetryStrategy::EXPONENTIAL:
                        delay *= 2;
                        break;
                    case RetryStrategy::NONE:
                        break;
                    case RetryStrategy::CUSTOM:
                        // Custom strategy could be implemented here
                        break;
                }
            }
        }

        spdlog::error("Failed to connect device {} after {} attempts",
                      device_name, attempts);
        return false;
    }
};

IntegratedDeviceManager::IntegratedDeviceManager()
    : pimpl_(std::make_unique<Impl>()) {}

IntegratedDeviceManager::IntegratedDeviceManager(const SystemConfig& config)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->initialize(config);
}

IntegratedDeviceManager::~IntegratedDeviceManager() = default;

bool IntegratedDeviceManager::initialize() {
    SystemConfig default_config;
    return pimpl_->initialize(default_config);
}

void IntegratedDeviceManager::shutdown() { pimpl_->shutdown(); }

bool IntegratedDeviceManager::isInitialized() const {
    return pimpl_->initialized_;
}

void IntegratedDeviceManager::setConfiguration(const SystemConfig& config) {
    pimpl_->config_ = config;
}

SystemConfig IntegratedDeviceManager::getConfiguration() const {
    return pimpl_->config_;
}

void IntegratedDeviceManager::addDevice(const std::string& type,
                                        std::shared_ptr<AtomDriver> device) {
    if (!device) {
        spdlog::error("Cannot add null device");
        return;
    }

    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    pimpl_->devices_[type].push_back(device);

    // Set as primary if none exists
    if (pimpl_->primary_devices_.find(type) == pimpl_->primary_devices_.end()) {
        pimpl_->primary_devices_[type] = device;
    }

    // Register with components
    if (pimpl_->performance_monitor_) {
        pimpl_->performance_monitor_->addDevice(device->getName(), device);
    }

    if (pimpl_->connection_pool_) {
        pimpl_->connection_pool_->registerDevice(device->getName(), device);
    }

    spdlog::info("Added device {} of type {}", device->getName(), type);
}

void IntegratedDeviceManager::removeDevice(const std::string& type,
                                           std::shared_ptr<AtomDriver> device) {
    if (!device)
        return;

    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    auto it = pimpl_->devices_.find(type);
    if (it != pimpl_->devices_.end()) {
        auto& device_list = it->second;
        device_list.erase(
            std::remove(device_list.begin(), device_list.end(), device),
            device_list.end());

        // Update primary if necessary
        if (pimpl_->primary_devices_[type] == device) {
            if (!device_list.empty()) {
                pimpl_->primary_devices_[type] = device_list.front();
            } else {
                pimpl_->primary_devices_.erase(type);
            }
        }
    }

    // Unregister from components
    if (pimpl_->performance_monitor_) {
        pimpl_->performance_monitor_->removeDevice(device->getName());
    }

    if (pimpl_->connection_pool_) {
        pimpl_->connection_pool_->unregisterDevice(device->getName());
    }

    spdlog::info("Removed device {} of type {}", device->getName(), type);
}

void IntegratedDeviceManager::removeDeviceByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);

    for (auto& [type, device_list] : pimpl_->devices_) {
        auto it =
            std::find_if(device_list.begin(), device_list.end(),
                         [&name](const std::shared_ptr<AtomDriver>& device) {
                             return device && device->getName() == name;
                         });

        if (it != device_list.end()) {
            auto device = *it;
            device_list.erase(it);

            // Update primary if necessary
            if (pimpl_->primary_devices_[type] == device) {
                if (!device_list.empty()) {
                    pimpl_->primary_devices_[type] = device_list.front();
                } else {
                    pimpl_->primary_devices_.erase(type);
                }
            }

            spdlog::info("Removed device {} of type {}", name, type);
            return;
        }
    }

    spdlog::warn("Device {} not found for removal", name);
}

bool IntegratedDeviceManager::connectDevice(const std::string& name,
                                            std::chrono::milliseconds timeout) {
    return pimpl_->connectDeviceWithRetry(name, timeout);
}

bool IntegratedDeviceManager::disconnectDevice(const std::string& name) {
    return pimpl_->executeDeviceOperation(
        name, [](std::shared_ptr<AtomDriver> device) {
            return device->disconnect();
        });
}

bool IntegratedDeviceManager::isDeviceConnected(const std::string& name) const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    auto device = pimpl_->findDeviceByName(name);
    return device && device->isConnected();
}

std::vector<bool> IntegratedDeviceManager::connectDevices(
    const std::vector<std::string>& names) {
    std::vector<bool> results;
    results.reserve(names.size());

    for (const auto& name : names) {
        results.push_back(connectDevice(name));
    }

    return results;
}

std::vector<bool> IntegratedDeviceManager::disconnectDevices(
    const std::vector<std::string>& names) {
    std::vector<bool> results;
    results.reserve(names.size());

    for (const auto& name : names) {
        results.push_back(disconnectDevice(name));
    }

    return results;
}

std::shared_ptr<AtomDriver> IntegratedDeviceManager::getDevice(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    return pimpl_->findDeviceByName(name);
}

std::vector<std::shared_ptr<AtomDriver>>
IntegratedDeviceManager::getDevicesByType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    auto it = pimpl_->devices_.find(type);
    return it != pimpl_->devices_.end()
               ? it->second
               : std::vector<std::shared_ptr<AtomDriver>>{};
}

std::vector<std::string> IntegratedDeviceManager::getDeviceNames() const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    std::vector<std::string> names;

    for (const auto& [type, device_list] : pimpl_->devices_) {
        for (const auto& device : device_list) {
            if (device) {
                names.push_back(device->getName());
            }
        }
    }

    return names;
}

std::vector<std::string> IntegratedDeviceManager::getDeviceTypes() const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    std::vector<std::string> types;

    for (const auto& [type, device_list] : pimpl_->devices_) {
        if (!device_list.empty()) {
            types.push_back(type);
        }
    }

    return types;
}

std::string IntegratedDeviceManager::executeTask(
    const std::string& device_name,
    std::function<bool(std::shared_ptr<AtomDriver>)> task, int priority) {
    // Execute synchronously since no task scheduler
    bool result = pimpl_->executeDeviceOperation(device_name, task);
    return result ? "sync_success" : "sync_failure";
}

bool IntegratedDeviceManager::cancelTask(const std::string& task_id) {
    // No task scheduler, so no tasks to cancel
    return false;
}

DeviceHealth IntegratedDeviceManager::getDeviceHealth(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    auto it = pimpl_->device_health_.find(name);
    return it != pimpl_->device_health_.end() ? it->second : DeviceHealth{};
}

std::vector<std::string> IntegratedDeviceManager::getUnhealthyDevices() const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    std::vector<std::string> unhealthy;

    for (const auto& [name, health] : pimpl_->device_health_) {
        if (health.overall_health < 0.5f) {
            unhealthy.push_back(name);
        }
    }

    return unhealthy;
}

void IntegratedDeviceManager::setHealthEventCallback(
    HealthEventCallback callback) {
    pimpl_->health_event_callback_ = std::move(callback);
}

DeviceMetrics IntegratedDeviceManager::getDeviceMetrics(
    const std::string& name) const {
    if (!pimpl_->performance_monitor_) {
        return DeviceMetrics{};
    }

    auto perf_metrics = pimpl_->performance_monitor_->getCurrentMetrics(name);

    // Convert PerformanceMetrics to DeviceMetrics
    DeviceMetrics metrics;
    metrics.avg_response_time = perf_metrics.response_time;
    metrics.min_response_time = perf_metrics.response_time;
    metrics.max_response_time = perf_metrics.response_time;
    metrics.last_operation = perf_metrics.timestamp;

    return metrics;
}

void IntegratedDeviceManager::setMetricsEventCallback(
    MetricsEventCallback callback) {
    pimpl_->metrics_event_callback_ = std::move(callback);
}

bool IntegratedDeviceManager::requestResource(const std::string& device_name,
                                              const std::string& resource_type,
                                              double amount) {
    // No resource manager, allow all requests
    return true;
}

void IntegratedDeviceManager::releaseResource(
    const std::string& device_name, const std::string& resource_type) {
    // No resource manager, no-op
}

bool IntegratedDeviceManager::cacheDeviceState(const std::string& device_name,
                                               const std::string& state_data) {
    // No cache system, fail
    return false;
}

bool IntegratedDeviceManager::getCachedDeviceState(
    const std::string& device_name, std::string& state_data) const {
    // No cache system, fail
    return false;
}

void IntegratedDeviceManager::clearDeviceCache(const std::string& device_name) {
    // No cache system, no-op
}

void IntegratedDeviceManager::setRetryStrategy(const std::string& device_name,
                                               RetryStrategy strategy) {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    pimpl_->retry_strategies_[device_name] = strategy;
}

RetryStrategy IntegratedDeviceManager::getRetryStrategy(
    const std::string& device_name) const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);
    auto it = pimpl_->retry_strategies_.find(device_name);
    return it != pimpl_->retry_strategies_.end()
               ? it->second
               : pimpl_->config_.default_retry_strategy;
}

void IntegratedDeviceManager::setDeviceEventCallback(
    DeviceEventCallback callback) {
    pimpl_->device_event_callback_ = std::move(callback);
}

IntegratedDeviceManager::SystemStatistics
IntegratedDeviceManager::getSystemStatistics() const {
    std::lock_guard<std::mutex> lock(pimpl_->devices_mutex_);

    SystemStatistics stats;
    stats.last_update = std::chrono::system_clock::now();

    // Count devices
    for (const auto& [type, device_list] : pimpl_->devices_) {
        stats.total_devices += device_list.size();
        for (const auto& device : device_list) {
            if (device && device->isConnected()) {
                stats.connected_devices++;
            }
        }
    }

    // Count healthy devices
    for (const auto& [name, health] : pimpl_->device_health_) {
        if (health.overall_health >= 0.5f) {
            stats.healthy_devices++;
        }
    }

    // Connection statistics
    if (pimpl_->connection_pool_) {
        stats.active_connections =
            pimpl_->connection_pool_->getStatistics().active_connections;
    }

    return stats;
}

void IntegratedDeviceManager::runSystemDiagnostics() {
    spdlog::info("Running system diagnostics...");

    auto stats = getSystemStatistics();

    spdlog::info("System Statistics:");
    spdlog::info("  Total devices: {}", stats.total_devices);
    spdlog::info("  Connected devices: {}", stats.connected_devices);
    spdlog::info("  Healthy devices: {}", stats.healthy_devices);
    spdlog::info("  Active connections: {}", stats.active_connections);

    // Component-specific diagnostics
    if (pimpl_->connection_pool_) {
        spdlog::info("Connection pool status: {}",
                     pimpl_->connection_pool_->getPoolStatus());
    }

    spdlog::info("System diagnostics completed");
}

std::string IntegratedDeviceManager::getSystemStatus() const {
    auto stats = getSystemStatistics();

    std::string status = "IntegratedDeviceManager Status:\n";
    status +=
        "  Initialized: " + std::string(isInitialized() ? "Yes" : "No") + "\n";
    status += "  Total devices: " + std::to_string(stats.total_devices) + "\n";
    status +=
        "  Connected devices: " + std::to_string(stats.connected_devices) +
        "\n";
    status +=
        "  Healthy devices: " + std::to_string(stats.healthy_devices) + "\n";
    status += "  System load: " + std::to_string(stats.system_load) + "\n";

    return status;
}

void IntegratedDeviceManager::runMaintenance() {
    spdlog::info("Running manual maintenance...");

    // Force maintenance on all components
    if (pimpl_->connection_pool_) {
        pimpl_->connection_pool_->runMaintenance();
    }

    // Update health metrics
    pimpl_->updateDeviceHealth();

    spdlog::info("Manual maintenance completed");
}

void IntegratedDeviceManager::optimizeSystem() {
    spdlog::info("Running system optimization...");

    // Optimize connection pool
    if (pimpl_->connection_pool_) {
        pimpl_->connection_pool_->optimizePool();
    }

    spdlog::info("System optimization completed");
}

}  // namespace lithium
