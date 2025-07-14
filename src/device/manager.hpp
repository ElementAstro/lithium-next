#ifndef LITHIUM_DEVICE_MANAGER_HPP
#define LITHIUM_DEVICE_MANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

#include "template/device.hpp"
#include "atom/error/exception.hpp"

class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...)                               \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...)                              \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

namespace lithium {

// Retry strategy for device operations
enum class RetryStrategy {
    NONE,
    LINEAR,
    EXPONENTIAL,
    CUSTOM
};

// Enhanced device health monitoring
struct DeviceHealth {
    float overall_health{1.0f};
    float connection_quality{1.0f};
    float response_time{0.0f};
    float error_rate{0.0f};
    uint32_t operations_count{0};
    uint32_t errors_count{0};
    std::chrono::system_clock::time_point last_check;
    std::vector<std::string> recent_errors;
};

// Device performance metrics
struct DeviceMetrics {
    std::chrono::milliseconds avg_response_time{0};
    std::chrono::milliseconds min_response_time{0};
    std::chrono::milliseconds max_response_time{0};
    uint64_t total_operations{0};
    uint64_t successful_operations{0};
    uint64_t failed_operations{0};
    double uptime_percentage{100.0};
    std::chrono::system_clock::time_point last_operation;
};

// Connection pool configuration
struct ConnectionPoolConfig {
    size_t max_connections{10};
    size_t min_connections{2};
    std::chrono::seconds idle_timeout{300};
    std::chrono::seconds connection_timeout{30};
    size_t max_retry_attempts{3};
    bool enable_keepalive{true};
};

// Device operation callback types
using DeviceOperationCallback = std::function<void(const std::string&, bool, const std::string&)>;
using DeviceHealthCallback = std::function<void(const std::string&, const DeviceHealth&)>;
using DeviceMetricsCallback = std::function<void(const std::string&, const DeviceMetrics&)>;

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // 禁用拷贝
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    // 设备管理接口
    void addDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDeviceByName(const std::string& name);
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>> getDevices() const;

    // 主设备管理
    void setPrimaryDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    std::shared_ptr<AtomDriver> getPrimaryDevice(const std::string& type) const;

    // 设备操作接口
    void connectAllDevices();
    void disconnectAllDevices();
    void connectDevicesByType(const std::string& type);
    void disconnectDevicesByType(const std::string& type);
    void connectDeviceByName(const std::string& name);
    void disconnectDeviceByName(const std::string& name);

    // 查询接口
    std::shared_ptr<AtomDriver> getDeviceByName(const std::string& name) const;
    std::shared_ptr<AtomDriver> findDeviceByName(const std::string& name) const;
    std::vector<std::shared_ptr<AtomDriver>> findDevicesByType(const std::string& type) const;
    bool isDeviceConnected(const std::string& name) const;
    std::string getDeviceType(const std::string& name) const;

    // 设备生命周期管理
    bool initializeDevice(const std::string& name);
    bool destroyDevice(const std::string& name);
    std::vector<std::string> scanDevices(const std::string& type);

    // 原有方法
    bool isDeviceValid(const std::string& name) const;
    void setDeviceRetryStrategy(const std::string& name, const RetryStrategy& strategy);
    float getDeviceHealth(const std::string& name) const;
    void abortDeviceOperation(const std::string& name);
    void resetDevice(const std::string& name);

    // 新增优化功能
    // 连接池管理
    void configureConnectionPool(const ConnectionPoolConfig& config);
    void enableConnectionPooling(bool enable);
    bool isConnectionPoolingEnabled() const;
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;

    // 设备健康监控
    void enableHealthMonitoring(bool enable);
    bool isHealthMonitoringEnabled() const;
    DeviceHealth getDeviceHealthDetails(const std::string& name) const;
    void setHealthCheckInterval(std::chrono::seconds interval);
    void setHealthCallback(DeviceHealthCallback callback);
    std::vector<std::string> getUnhealthyDevices() const;

    // 性能监控
    void enablePerformanceMonitoring(bool enable);
    bool isPerformanceMonitoringEnabled() const;
    DeviceMetrics getDeviceMetrics(const std::string& name) const;
    void setMetricsCallback(DeviceMetricsCallback callback);
    void resetDeviceMetrics(const std::string& name);

    // 操作回调
    void setOperationCallback(DeviceOperationCallback callback);
    void setGlobalTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds getGlobalTimeout() const;

    // 批量操作
    void executeBatchOperation(const std::vector<std::string>& device_names,
                              std::function<bool(std::shared_ptr<AtomDriver>)> operation);
    void executeBatchOperationAsync(const std::vector<std::string>& device_names,
                                   std::function<bool(std::shared_ptr<AtomDriver>)> operation,
                                   std::function<void(const std::vector<std::pair<std::string, bool>>&)> callback);

    // 设备优先级管理
    void setDevicePriority(const std::string& name, int priority);
    int getDevicePriority(const std::string& name) const;
    std::vector<std::string> getDevicesByPriority() const;

    // 资源管理
    void setMaxConcurrentOperations(size_t max_ops);
    size_t getMaxConcurrentOperations() const;
    size_t getCurrentOperations() const;
    bool waitForOperationSlot(std::chrono::milliseconds timeout);

    // 设备组管理
    void createDeviceGroup(const std::string& group_name, const std::vector<std::string>& device_names);
    void removeDeviceGroup(const std::string& group_name);
    std::vector<std::string> getDeviceGroup(const std::string& group_name) const;
    void executeGroupOperation(const std::string& group_name,
                              std::function<bool(std::shared_ptr<AtomDriver>)> operation);

    // 诊断和维护
    void runDiagnostics();
    void runDeviceDiagnostics(const std::string& name);
    std::vector<std::string> getDeviceWarnings(const std::string& name) const;
    void clearDeviceWarnings(const std::string& name);

    // 配置管理
    void saveDeviceConfiguration(const std::string& name, const std::string& config_path);
    void loadDeviceConfiguration(const std::string& name, const std::string& config_path);
    void exportDeviceSettings(const std::string& output_path);
    void importDeviceSettings(const std::string& input_path);

    // 统计信息
    struct SystemStats {
        size_t total_devices{0};
        size_t connected_devices{0};
        size_t healthy_devices{0};
        double average_health{0.0};
        std::chrono::seconds uptime{0};
        uint64_t total_operations{0};
        uint64_t successful_operations{0};
        uint64_t failed_operations{0};
    };
    SystemStats getSystemStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}  // namespace lithium

#endif  // LITHIUM_DEVICE_MANAGER_HPP
