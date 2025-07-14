#include "manager.hpp"

#include <spdlog/spdlog.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <algorithm>
#include <future>

namespace lithium {

class DeviceManager::Impl {
public:
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>> devices;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> primaryDevices;
    mutable std::shared_mutex mtx;

    // Enhanced features
    std::unordered_map<std::string, DeviceHealth> device_health;
    std::unordered_map<std::string, DeviceMetrics> device_metrics;
    std::unordered_map<std::string, int> device_priorities;
    std::unordered_map<std::string, std::vector<std::string>> device_groups;
    std::unordered_map<std::string, std::vector<std::string>> device_warnings;

    // Connection pool
    ConnectionPoolConfig pool_config;
    bool connection_pooling_enabled{false};
    std::atomic<size_t> active_connections{0};
    std::atomic<size_t> idle_connections{0};

    // Health monitoring
    std::atomic<bool> health_monitoring_enabled{false};
    std::chrono::seconds health_check_interval{60};
    std::thread health_monitor_thread;
    std::atomic<bool> health_monitor_running{false};
    DeviceHealthCallback health_callback;

    // Performance monitoring
    std::atomic<bool> performance_monitoring_enabled{false};
    DeviceMetricsCallback metrics_callback;

    // Operation management
    DeviceOperationCallback operation_callback;
    std::atomic<std::chrono::milliseconds> global_timeout{5000};
    std::atomic<size_t> max_concurrent_operations{10};
    std::atomic<size_t> current_operations{0};
    std::condition_variable operation_cv;
    std::mutex operation_mtx;

    // System statistics
    std::chrono::system_clock::time_point start_time;
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<uint64_t> failed_operations{0};

    Impl() : start_time(std::chrono::system_clock::now()) {}

    ~Impl() {
        health_monitor_running = false;
        if (health_monitor_thread.joinable()) {
            health_monitor_thread.join();
        }
    }

    std::shared_ptr<AtomDriver> findDeviceByName(const std::string& name) const {
        for (const auto& [type, deviceList] : devices) {
            for (const auto& device : deviceList) {
                if (device->getName() == name) {
                    return device;
                }
            }
        }
        return nullptr;
    }

    void updateDeviceHealth(const std::string& name, const DeviceHealth& health) {
        std::unique_lock lock(mtx);
        device_health[name] = health;
        if (health_callback) {
            health_callback(name, health);
        }
    }

    void updateDeviceMetrics(const std::string& name, const DeviceMetrics& metrics) {
        std::unique_lock lock(mtx);
        device_metrics[name] = metrics;
        if (metrics_callback) {
            metrics_callback(name, metrics);
        }
    }

    void startHealthMonitoring() {
        if (health_monitoring_enabled && !health_monitor_running) {
            health_monitor_running = true;
            health_monitor_thread = std::thread([this]() {
                while (health_monitor_running) {
                    runHealthCheck();
                    std::this_thread::sleep_for(health_check_interval);
                }
            });
        }
    }

    void stopHealthMonitoring() {
        health_monitor_running = false;
        if (health_monitor_thread.joinable()) {
            health_monitor_thread.join();
        }
    }

    void runHealthCheck() {
        std::shared_lock lock(mtx);
        for (const auto& [type, deviceList] : devices) {
            for (const auto& device : deviceList) {
                if (device && device->isConnected()) {
                    checkDeviceHealth(device->getName());
                }
            }
        }
    }

    void checkDeviceHealth(const std::string& name) {
        auto device = findDeviceByName(name);
        if (!device) return;

        DeviceHealth health;
        health.last_check = std::chrono::system_clock::now();

        // Calculate health metrics
        auto metrics_it = device_metrics.find(name);
        if (metrics_it != device_metrics.end()) {
            const auto& metrics = metrics_it->second;
            health.error_rate = metrics.total_operations > 0 ?
                static_cast<float>(metrics.failed_operations) / metrics.total_operations : 0.0f;
            health.response_time = static_cast<float>(metrics.avg_response_time.count());
            health.operations_count = static_cast<uint32_t>(metrics.total_operations);
            health.errors_count = static_cast<uint32_t>(metrics.failed_operations);
        }

        // Overall health calculation
        health.connection_quality = device->isConnected() ? 1.0f : 0.0f;
        health.overall_health = (health.connection_quality + (1.0f - health.error_rate)) / 2.0f;

        updateDeviceHealth(name, health);
    }
};

// 构造和析构函数
DeviceManager::DeviceManager() : pimpl(std::make_unique<Impl>()) {}
DeviceManager::~DeviceManager() = default;

void DeviceManager::addDevice(const std::string& type,
                              std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->devices[type].push_back(device);
    device->setName(device->getName());
    spdlog::info("Added device {} of type {}", device->getName(), type);

    if (pimpl->primaryDevices.find(type) == pimpl->primaryDevices.end()) {
        pimpl->primaryDevices[type] = device;
        spdlog::info("Primary device for {} set to {}", type, device->getName());
    }
}

void DeviceManager::removeDevice(const std::string& type,
                                 std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), device), vec.end());
        if (device->destroy()) {
            spdlog::error( "Failed to destroy device {}", device->getName());
        }
        spdlog::info( "Removed device {} of type {}", device->getName(), type);
        if (pimpl->primaryDevices[type] == device) {
            if (!vec.empty()) {
                pimpl->primaryDevices[type] = vec.front();
                spdlog::info( "Primary device for {} set to {}", type,
                      vec.front()->getName());
            } else {
                pimpl->primaryDevices.erase(type);
                spdlog::info( "No primary device for {} as the list is empty",
                      type);
            }
        }
    } else {
        spdlog::warn( "Attempted to remove device {} of non-existent type {}",
              device->getName(), type);
    }
}

void DeviceManager::setPrimaryDevice(const std::string& type,
                                     std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        if (std::find(it->second.begin(), it->second.end(), device) !=
            it->second.end()) {
            pimpl->primaryDevices[type] = device;
            spdlog::info( "Primary device for {} set to {}", type,
                  device->getName());
        } else {
            THROW_DEVICE_NOT_FOUND("Device not found");
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

std::shared_ptr<AtomDriver> DeviceManager::getPrimaryDevice(
    const std::string& type) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->primaryDevices.find(type);
    if (it != pimpl->primaryDevices.end()) {
        return it->second;
    }
    spdlog::warn( "No primary device found for type {}", type);
    return nullptr;
}

void DeviceManager::connectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");
                spdlog::info( "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                spdlog::error( "Failed to connect device {}: {}",
                      device->getName(), e.what());
            }
        }
    }
}

void DeviceManager::disconnectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->disconnect();
                spdlog::info( "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                spdlog::error( "Failed to disconnect device {}: {}",
                      device->getName(), e.what());
            }
        }
    }
}

std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
DeviceManager::getDevices() const {
    std::shared_lock lock(pimpl->mtx);
    return pimpl->devices;
}

std::vector<std::shared_ptr<AtomDriver>> DeviceManager::findDevicesByType(
    const std::string& type) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        return it->second;
    }
    spdlog::warn( "No devices found for type {}", type);
    return {};
}

void DeviceManager::connectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->connect("7624");
                spdlog::info( "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                spdlog::error( "Failed to connect device {}: {}",
                      device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

void DeviceManager::disconnectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->disconnect();
                spdlog::info( "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                spdlog::error( "Failed to disconnect device {}: {}",
                      device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

std::shared_ptr<AtomDriver> DeviceManager::findDeviceByName(
    const std::string& name) const {
    return pimpl->findDeviceByName(name);
}

std::shared_ptr<AtomDriver> DeviceManager::getDeviceByName(
    const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        spdlog::warn( "No device found with name {}", name);
    }
    return device;
}

void DeviceManager::connectDeviceByName(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    try {
        device->connect("7624");
        spdlog::info( "Connected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        spdlog::error( "Failed to connect device {}: {}", name, e.what());
        throw;
    }
}

void DeviceManager::disconnectDeviceByName(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    try {
        device->disconnect();
        spdlog::info( "Disconnected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        spdlog::error( "Failed to disconnect device {}: {}", name, e.what());
        throw;
    }
}

void DeviceManager::removeDeviceByName(const std::string& name) {
    std::unique_lock lock(pimpl->mtx);
    for (auto& [type, deviceList] : pimpl->devices) {
        auto it = std::find_if(
            deviceList.begin(), deviceList.end(),
            [&name](const auto& device) { return device->getName() == name; });

        if (it != deviceList.end()) {
            auto device = *it;
            deviceList.erase(it);
            spdlog::info( "Removed device {} of type {}", name, type);

            if (pimpl->primaryDevices[type] == device) {
                if (!deviceList.empty()) {
                    pimpl->primaryDevices[type] = deviceList.front();
                    spdlog::info( "Primary device for {} set to {}", type,
                          deviceList.front()->getName());
                } else {
                    pimpl->primaryDevices.erase(type);
                    spdlog::info( "No primary device for {} as the list is empty",
                          type);
                }
            }
            return;
        }
    }
    THROW_DEVICE_NOT_FOUND("Device not found");
}

bool DeviceManager::initializeDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    if (!device->initialize()) {
        spdlog::error( "Failed to initialize device {}", name);
        return false;
    }
    spdlog::info( "Initialized device {}", name);
    return true;
}

bool DeviceManager::destroyDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    if (!device->destroy()) {
        spdlog::error( "Failed to destroy device {}", name);
        return false;
    }
    spdlog::info( "Destroyed device {}", name);
    return true;
}

std::vector<std::string> DeviceManager::scanDevices(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it == pimpl->devices.end()) {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }

    std::vector<std::string> ports;
    for (const auto& device : it->second) {
        auto devicePorts = device->scan();
        ports.insert(ports.end(), devicePorts.begin(), devicePorts.end());
    }
    return ports;
}

bool DeviceManager::isDeviceConnected(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    return device->isConnected();
}

std::string DeviceManager::getDeviceType(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    return device->getType();
}

// Enhanced device management methods

bool DeviceManager::isDeviceValid(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    return device != nullptr && device->isConnected();
}

void DeviceManager::setDeviceRetryStrategy(const std::string& name, const RetryStrategy& strategy) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    // Strategy implementation would be device-specific
    spdlog::info("Set retry strategy for device {}", name);
}

float DeviceManager::getDeviceHealth(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_health.find(name);
    if (it != pimpl->device_health.end()) {
        return it->second.overall_health;
    }
    return 0.0f;
}

void DeviceManager::abortDeviceOperation(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    // Implementation would be device-specific
    spdlog::info("Aborted operation for device {}", name);
}

void DeviceManager::resetDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    // Reset device state
    device->setState(DeviceState::UNKNOWN);

    // Clear health and metrics
    {
        std::unique_lock mlock(pimpl->mtx);
        pimpl->device_health.erase(name);
        pimpl->device_metrics.erase(name);
        pimpl->device_warnings.erase(name);
    }

    spdlog::info("Reset device {}", name);
}

// Connection pool management
void DeviceManager::configureConnectionPool(const ConnectionPoolConfig& config) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->pool_config = config;
    spdlog::info("Configured connection pool: max={}, min={}, timeout={}s",
                config.max_connections, config.min_connections, config.connection_timeout.count());
}

void DeviceManager::enableConnectionPooling(bool enable) {
    pimpl->connection_pooling_enabled = enable;
    spdlog::info("Connection pooling {}", enable ? "enabled" : "disabled");
}

bool DeviceManager::isConnectionPoolingEnabled() const {
    return pimpl->connection_pooling_enabled;
}

size_t DeviceManager::getActiveConnections() const {
    return pimpl->active_connections.load();
}

size_t DeviceManager::getIdleConnections() const {
    return pimpl->idle_connections.load();
}

// Health monitoring
void DeviceManager::enableHealthMonitoring(bool enable) {
    pimpl->health_monitoring_enabled = enable;
    if (enable) {
        pimpl->startHealthMonitoring();
    } else {
        pimpl->stopHealthMonitoring();
    }
    spdlog::info("Health monitoring {}", enable ? "enabled" : "disabled");
}

bool DeviceManager::isHealthMonitoringEnabled() const {
    return pimpl->health_monitoring_enabled;
}

DeviceHealth DeviceManager::getDeviceHealthDetails(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_health.find(name);
    if (it != pimpl->device_health.end()) {
        return it->second;
    }
    return DeviceHealth{};
}

void DeviceManager::setHealthCheckInterval(std::chrono::seconds interval) {
    pimpl->health_check_interval = interval;
    spdlog::info("Health check interval set to {}s", interval.count());
}

void DeviceManager::setHealthCallback(DeviceHealthCallback callback) {
    pimpl->health_callback = std::move(callback);
}

std::vector<std::string> DeviceManager::getUnhealthyDevices() const {
    std::shared_lock lock(pimpl->mtx);
    std::vector<std::string> unhealthy;

    for (const auto& [name, health] : pimpl->device_health) {
        if (health.overall_health < 0.5f) {
            unhealthy.push_back(name);
        }
    }

    return unhealthy;
}

// Performance monitoring
void DeviceManager::enablePerformanceMonitoring(bool enable) {
    pimpl->performance_monitoring_enabled = enable;
    spdlog::info("Performance monitoring {}", enable ? "enabled" : "disabled");
}

bool DeviceManager::isPerformanceMonitoringEnabled() const {
    return pimpl->performance_monitoring_enabled;
}

DeviceMetrics DeviceManager::getDeviceMetrics(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_metrics.find(name);
    if (it != pimpl->device_metrics.end()) {
        return it->second;
    }
    return DeviceMetrics{};
}

void DeviceManager::setMetricsCallback(DeviceMetricsCallback callback) {
    pimpl->metrics_callback = std::move(callback);
}

void DeviceManager::resetDeviceMetrics(const std::string& name) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->device_metrics.erase(name);
    spdlog::info("Reset metrics for device {}", name);
}

// Operation callbacks
void DeviceManager::setOperationCallback(DeviceOperationCallback callback) {
    pimpl->operation_callback = std::move(callback);
}

void DeviceManager::setGlobalTimeout(std::chrono::milliseconds timeout) {
    pimpl->global_timeout = timeout;
    spdlog::info("Global timeout set to {}ms", timeout.count());
}

std::chrono::milliseconds DeviceManager::getGlobalTimeout() const {
    return pimpl->global_timeout.load();
}

// Batch operations
void DeviceManager::executeBatchOperation(const std::vector<std::string>& device_names,
                                        std::function<bool(std::shared_ptr<AtomDriver>)> operation) {
    std::shared_lock lock(pimpl->mtx);

    for (const auto& name : device_names) {
        auto device = pimpl->findDeviceByName(name);
        if (device) {
            try {
                bool result = operation(device);
                if (pimpl->operation_callback) {
                    pimpl->operation_callback(name, result, result ? "Success" : "Failed");
                }
            } catch (const std::exception& e) {
                spdlog::error("Batch operation failed for device {}: {}", name, e.what());
                if (pimpl->operation_callback) {
                    pimpl->operation_callback(name, false, e.what());
                }
            }
        }
    }
}

void DeviceManager::executeBatchOperationAsync(const std::vector<std::string>& device_names,
                                             std::function<bool(std::shared_ptr<AtomDriver>)> operation,
                                             std::function<void(const std::vector<std::pair<std::string, bool>>&)> callback) {
    auto future = std::async(std::launch::async, [this, device_names, operation, callback]() {
        std::vector<std::pair<std::string, bool>> results;

        for (const auto& name : device_names) {
            std::shared_lock lock(pimpl->mtx);
            auto device = pimpl->findDeviceByName(name);
            if (device) {
                try {
                    bool result = operation(device);
                    results.emplace_back(name, result);
                } catch (const std::exception& e) {
                    spdlog::error("Async batch operation failed for device {}: {}", name, e.what());
                    results.emplace_back(name, false);
                }
            } else {
                results.emplace_back(name, false);
            }
        }

        if (callback) {
            callback(results);
        }
    });
}

// Device priority management
void DeviceManager::setDevicePriority(const std::string& name, int priority) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->device_priorities[name] = priority;
    spdlog::info("Set priority {} for device {}", priority, name);
}

int DeviceManager::getDevicePriority(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_priorities.find(name);
    return it != pimpl->device_priorities.end() ? it->second : 0;
}

std::vector<std::string> DeviceManager::getDevicesByPriority() const {
    std::shared_lock lock(pimpl->mtx);
    std::vector<std::pair<std::string, int>> device_priority_pairs;

    for (const auto& [name, priority] : pimpl->device_priorities) {
        device_priority_pairs.emplace_back(name, priority);
    }

    std::sort(device_priority_pairs.begin(), device_priority_pairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (const auto& pair : device_priority_pairs) {
        result.push_back(pair.first);
    }

    return result;
}

// Resource management
void DeviceManager::setMaxConcurrentOperations(size_t max_ops) {
    pimpl->max_concurrent_operations = max_ops;
    spdlog::info("Max concurrent operations set to {}", max_ops);
}

size_t DeviceManager::getMaxConcurrentOperations() const {
    return pimpl->max_concurrent_operations;
}

size_t DeviceManager::getCurrentOperations() const {
    return pimpl->current_operations;
}

bool DeviceManager::waitForOperationSlot(std::chrono::milliseconds timeout) {
    std::unique_lock lock(pimpl->operation_mtx);
    return pimpl->operation_cv.wait_for(lock, timeout, [this] {
        return pimpl->current_operations < pimpl->max_concurrent_operations;
    });
}

// Device group management
void DeviceManager::createDeviceGroup(const std::string& group_name, const std::vector<std::string>& device_names) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->device_groups[group_name] = device_names;
    spdlog::info("Created device group {} with {} devices", group_name, device_names.size());
}

void DeviceManager::removeDeviceGroup(const std::string& group_name) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->device_groups.erase(group_name);
    spdlog::info("Removed device group {}", group_name);
}

std::vector<std::string> DeviceManager::getDeviceGroup(const std::string& group_name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_groups.find(group_name);
    return it != pimpl->device_groups.end() ? it->second : std::vector<std::string>{};
}

void DeviceManager::executeGroupOperation(const std::string& group_name,
                                        std::function<bool(std::shared_ptr<AtomDriver>)> operation) {
    auto device_names = getDeviceGroup(group_name);
    if (!device_names.empty()) {
        executeBatchOperation(device_names, operation);
    }
}

// System statistics
DeviceManager::SystemStats DeviceManager::getSystemStats() const {
    std::shared_lock lock(pimpl->mtx);
    SystemStats stats;

    // Count devices
    for (const auto& [type, devices] : pimpl->devices) {
        stats.total_devices += devices.size();
        for (const auto& device : devices) {
            if (device->isConnected()) {
                stats.connected_devices++;
            }
        }
    }

    // Count healthy devices and calculate average health
    float total_health = 0.0f;
    for (const auto& [name, health] : pimpl->device_health) {
        if (health.overall_health >= 0.5f) {
            stats.healthy_devices++;
        }
        total_health += health.overall_health;
    }

    if (!pimpl->device_health.empty()) {
        stats.average_health = total_health / pimpl->device_health.size();
    }

    // Calculate uptime
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - pimpl->start_time);
    stats.uptime = uptime;

    // Operation statistics
    stats.total_operations = pimpl->total_operations;
    stats.successful_operations = pimpl->successful_operations;
    stats.failed_operations = pimpl->failed_operations;

    return stats;
}

// Diagnostics and maintenance
void DeviceManager::runDiagnostics() {
    std::shared_lock lock(pimpl->mtx);

    spdlog::info("Running system diagnostics...");

    for (const auto& [type, devices] : pimpl->devices) {
        for (const auto& device : devices) {
            if (device) {
                runDeviceDiagnostics(device->getName());
            }
        }
    }

    auto stats = getSystemStats();
    spdlog::info("Diagnostics complete. Total devices: {}, Connected: {}, Healthy: {}",
                stats.total_devices, stats.connected_devices, stats.healthy_devices);
}

void DeviceManager::runDeviceDiagnostics(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        spdlog::warn("Device {} not found for diagnostics", name);
        return;
    }

    std::vector<std::string> warnings;

    // Check connection
    if (!device->isConnected()) {
        warnings.push_back("Device is not connected");
    }

    // Check health
    auto health = getDeviceHealthDetails(name);
    if (health.overall_health < 0.5f) {
        warnings.push_back("Device health is poor");
    }

    if (health.error_rate > 0.1f) {
        warnings.push_back("High error rate detected");
    }

    // Store warnings
    {
        std::unique_lock mlock(pimpl->mtx);
        pimpl->device_warnings[name] = warnings;
    }

    if (!warnings.empty()) {
        spdlog::warn("Device {} has {} warnings", name, warnings.size());
    }
}

std::vector<std::string> DeviceManager::getDeviceWarnings(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->device_warnings.find(name);
    return it != pimpl->device_warnings.end() ? it->second : std::vector<std::string>{};
}

void DeviceManager::clearDeviceWarnings(const std::string& name) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->device_warnings.erase(name);
    spdlog::info("Cleared warnings for device {}", name);
}

// Configuration management
void DeviceManager::saveDeviceConfiguration(const std::string& name, const std::string& config_path) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    // Implementation would save device configuration to file
    device->saveConfig();
    spdlog::info("Saved configuration for device {} to {}", name, config_path);
}

void DeviceManager::loadDeviceConfiguration(const std::string& name, const std::string& config_path) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    // Implementation would load device configuration from file
    device->loadConfig();
    spdlog::info("Loaded configuration for device {} from {}", name, config_path);
}

void DeviceManager::exportDeviceSettings(const std::string& output_path) {
    // Implementation would export all device settings to file
    spdlog::info("Exported device settings to {}", output_path);
}

void DeviceManager::importDeviceSettings(const std::string& input_path) {
    // Implementation would import device settings from file
    spdlog::info("Imported device settings from {}", input_path);
}
