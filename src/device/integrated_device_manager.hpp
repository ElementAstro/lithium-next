/*
 * integrated_device_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Integrated Device Management System - Central hub for all device operations

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>

#include "template/device.hpp"

namespace lithium {

// Forward declarations
class DeviceConnectionPool;
class DevicePerformanceMonitor;
class DeviceResourceManager;
class DeviceTaskScheduler;
template<typename T> class DeviceCacheSystem;

// Retry strategy for device operations
enum class RetryStrategy {
    NONE,
    LINEAR,
    EXPONENTIAL,
    CUSTOM
};

// Device health status
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

// System configuration
struct SystemConfig {
    // Connection pool settings
    size_t max_connections_per_device{5};
    std::chrono::seconds connection_timeout{30};
    bool enable_connection_pooling{true};
    
    // Performance monitoring
    bool enable_performance_monitoring{true};
    std::chrono::seconds health_check_interval{60};
    
    // Resource management
    size_t max_concurrent_operations{10};
    bool enable_resource_limiting{true};
    
    // Task scheduling
    size_t max_queued_tasks{1000};
    size_t worker_thread_count{4};
    
    // Caching
    size_t cache_size_mb{100};
    bool enable_device_caching{true};
    
    // Retry configuration
    RetryStrategy default_retry_strategy{RetryStrategy::EXPONENTIAL};
    size_t max_retry_attempts{3};
    std::chrono::milliseconds retry_delay{1000};
};

// Event callbacks
using DeviceEventCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;
using HealthEventCallback = std::function<void(const std::string&, const DeviceHealth&)>;
using MetricsEventCallback = std::function<void(const std::string&, const DeviceMetrics&)>;

/**
 * @class IntegratedDeviceManager
 * @brief Central hub for all device management operations
 * 
 * This class integrates all device management components into a single, 
 * cohesive system that provides:
 * - Device lifecycle management
 * - Connection pooling
 * - Performance monitoring
 * - Resource management
 * - Task scheduling
 * - Caching
 * - Health monitoring
 */
class IntegratedDeviceManager {
public:
    IntegratedDeviceManager();
    explicit IntegratedDeviceManager(const SystemConfig& config);
    ~IntegratedDeviceManager();
    
    // System lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Configuration management
    void setConfiguration(const SystemConfig& config);
    SystemConfig getConfiguration() const;
    
    // Device management
    void addDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDeviceByName(const std::string& name);
    
    // Device operations with integrated optimization
    bool connectDevice(const std::string& name, std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});
    bool disconnectDevice(const std::string& name);
    bool isDeviceConnected(const std::string& name) const;
    
    // Batch operations
    std::vector<bool> connectDevices(const std::vector<std::string>& names);
    std::vector<bool> disconnectDevices(const std::vector<std::string>& names);
    
    // Device queries
    std::shared_ptr<AtomDriver> getDevice(const std::string& name) const;
    std::vector<std::shared_ptr<AtomDriver>> getDevicesByType(const std::string& type) const;
    std::vector<std::string> getDeviceNames() const;
    std::vector<std::string> getDeviceTypes() const;
    
    // Task execution with scheduling
    std::string executeTask(const std::string& device_name, 
                           std::function<bool(std::shared_ptr<AtomDriver>)> task,
                           int priority = 0);
    
    bool cancelTask(const std::string& task_id);
    
    // Health monitoring
    DeviceHealth getDeviceHealth(const std::string& name) const;
    std::vector<std::string> getUnhealthyDevices() const;
    void setHealthEventCallback(HealthEventCallback callback);
    
    // Performance monitoring
    DeviceMetrics getDeviceMetrics(const std::string& name) const;
    void setMetricsEventCallback(MetricsEventCallback callback);
    
    // Resource management
    bool requestResource(const std::string& device_name, const std::string& resource_type, double amount);
    void releaseResource(const std::string& device_name, const std::string& resource_type);
    
    // Device state caching
    bool cacheDeviceState(const std::string& device_name, const std::string& state_data);
    bool getCachedDeviceState(const std::string& device_name, std::string& state_data) const;
    void clearDeviceCache(const std::string& device_name);
    
    // Retry management
    void setRetryStrategy(const std::string& device_name, RetryStrategy strategy);
    RetryStrategy getRetryStrategy(const std::string& device_name) const;
    
    // Event handling
    void setDeviceEventCallback(DeviceEventCallback callback);
    
    // System statistics
    struct SystemStatistics {
        size_t total_devices{0};
        size_t connected_devices{0};
        size_t healthy_devices{0};
        size_t active_tasks{0};
        size_t queued_tasks{0};
        size_t active_connections{0};
        size_t cache_hit_rate{0};
        double average_response_time{0.0};
        double system_load{0.0};
        std::chrono::system_clock::time_point last_update;
    };
    
    SystemStatistics getSystemStatistics() const;
    
    // Diagnostics
    void runSystemDiagnostics();
    std::string getSystemStatus() const;
    
    // Maintenance
    void runMaintenance();
    void optimizeSystem();
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Internal optimization methods
    void optimizeConnectionPool();
    void optimizeTaskScheduling();
    void optimizeResourceAllocation();
    void optimizeCaching();
    
    // Health monitoring
    void monitorDeviceHealth();
    void updateDeviceMetrics();
    
    // Background tasks
    void backgroundMaintenance();
    void performanceOptimization();
};

} // namespace lithium
