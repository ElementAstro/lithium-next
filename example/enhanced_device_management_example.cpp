/*
 * enhanced_device_management_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Example demonstrating enhanced device management with performance optimizations

*************************************************/

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include "../src/device/manager.hpp"
#include "../src/device/enhanced_device_factory.hpp"
#include "../src/device/device_performance_monitor.hpp"
#include "../src/device/device_resource_manager.hpp"
#include "../src/device/device_connection_pool.hpp"
#include "../src/device/device_task_scheduler.hpp"
#include "../src/device/device_cache_system.hpp"

using namespace lithium;

void demonstrateEnhancedDeviceManager() {
    std::cout << "=== Enhanced Device Manager Demo ===\n";

    // Create device manager with enhanced features
    DeviceManager manager;

    // Configure connection pooling
    ConnectionPoolConfig poolConfig;
    poolConfig.max_connections = 20;
    poolConfig.min_connections = 5;
    poolConfig.idle_timeout = std::chrono::seconds{300};
    poolConfig.enable_keepalive = true;

    manager.configureConnectionPool(poolConfig);
    manager.enableConnectionPooling(true);

    // Enable health monitoring
    manager.enableHealthMonitoring(true);
    manager.setHealthCheckInterval(std::chrono::seconds{30});

    // Set health callback
    manager.setHealthCallback([](const std::string& device_name, const DeviceHealth& health) {
        std::cout << "Device " << device_name << " health: " << health.overall_health
                  << " (errors: " << health.errors_count << ")\n";
    });

    // Enable performance monitoring
    manager.enablePerformanceMonitoring(true);
    manager.setMetricsCallback([](const std::string& device_name, const DeviceMetrics& metrics) {
        std::cout << "Device " << device_name << " metrics - "
                  << "Operations: " << metrics.total_operations
                  << ", Success rate: " << (metrics.successful_operations * 100.0 / metrics.total_operations) << "%\n";
    });

    // Create device groups for batch operations
    std::vector<std::string> camera_group = {"Camera1", "Camera2", "GuideCamera"};
    manager.createDeviceGroup("cameras", camera_group);

    std::vector<std::string> mount_group = {"MainTelescope", "GuideTelescope"};
    manager.createDeviceGroup("telescopes", mount_group);

    // Set device priorities
    manager.setDevicePriority("Camera1", 10);    // High priority
    manager.setDevicePriority("Camera2", 5);     // Medium priority
    manager.setDevicePriority("GuideCamera", 3); // Lower priority

    // Configure resource management
    manager.setMaxConcurrentOperations(15);
    manager.setGlobalTimeout(std::chrono::milliseconds{30000});

    // Demonstrate batch operations
    std::cout << "Executing batch operation on camera group...\n";
    manager.executeGroupOperation("cameras", [](std::shared_ptr<AtomDriver> device) -> bool {
        std::cout << "Processing device: " << device->getName() << "\n";
        // Simulate device operation
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        return true;
    });

    // Get system statistics
    auto stats = manager.getSystemStats();
    std::cout << "System Stats - Total devices: " << stats.total_devices
              << ", Connected: " << stats.connected_devices
              << ", Healthy: " << stats.healthy_devices << "\n";

    std::cout << "Enhanced Device Manager demo completed.\n\n";
}

void demonstrateDeviceFactory() {
    std::cout << "=== Enhanced Device Factory Demo ===\n";

    auto& factory = DeviceFactory::getInstance();

    // Enable advanced features
    factory.enableCaching(true);
    factory.setCacheSize(100);
    factory.enablePooling(true);
    factory.setPoolSize(DeviceType::CAMERA, 5);
    factory.enablePerformanceMonitoring(true);

    // Configure factory settings
    factory.setDefaultTimeout(std::chrono::milliseconds{5000});
    factory.setMaxConcurrentCreations(10);

    // Create devices with enhanced configuration
    DeviceCreationConfig cameraConfig;
    cameraConfig.name = "AdvancedCamera";
    cameraConfig.type = DeviceType::CAMERA;
    cameraConfig.backend = DeviceBackend::MOCK;
    cameraConfig.timeout = std::chrono::milliseconds{3000};
    cameraConfig.priority = 5;
    cameraConfig.enable_simulation = true;
    cameraConfig.enable_caching = true;
    cameraConfig.enable_pooling = true;
    cameraConfig.properties["resolution"] = "4096x4096";
    cameraConfig.properties["cooling"] = "true";

    auto camera = factory.createCamera(cameraConfig);
    if (camera) {
        std::cout << "Created advanced camera: " << camera->getName() << "\n";
    }

    // Batch device creation
    std::vector<DeviceCreationConfig> batch_configs;

    for (int i = 0; i < 3; ++i) {
        DeviceCreationConfig config;
        config.name = "BatchCamera" + std::to_string(i);
        config.type = DeviceType::CAMERA;
        config.backend = DeviceBackend::MOCK;
        batch_configs.push_back(config);
    }

    std::cout << "Creating batch of devices...\n";
    auto batch_devices = factory.createDevicesBatch(batch_configs);
    std::cout << "Created " << batch_devices.size() << " devices in batch\n";

    // Get performance profiles
    auto perfProfile = factory.getPerformanceProfile(DeviceType::CAMERA, DeviceBackend::MOCK);
    std::cout << "Camera creation performance - Average time: "
              << perfProfile.avg_creation_time.count() << "ms, Success rate: "
              << perfProfile.success_rate << "%\n";

    // Get resource usage
    auto resourceUsage = factory.getResourceUsage();
    std::cout << "Factory resource usage - Total created: " << resourceUsage.total_devices_created
              << ", Active: " << resourceUsage.active_devices
              << ", Cached: " << resourceUsage.cached_devices << "\n";

    std::cout << "Enhanced Device Factory demo completed.\n\n";
}

void demonstratePerformanceMonitoring() {
    std::cout << "=== Performance Monitoring Demo ===\n";

    DevicePerformanceMonitor monitor;

    // Configure monitoring
    MonitoringConfig config;
    config.monitoring_interval = std::chrono::seconds{5};
    config.enable_predictive_analysis = true;
    config.enable_real_time_alerts = true;
    monitor.setMonitoringConfig(config);

    // Set up performance thresholds
    PerformanceThresholds thresholds;
    thresholds.max_response_time = std::chrono::milliseconds{2000};
    thresholds.max_error_rate = 5.0;
    thresholds.warning_response_time = std::chrono::milliseconds{1000};
    thresholds.critical_error_rate = 10.0;
    monitor.setGlobalThresholds(thresholds);

    // Set up alert callback
    monitor.setAlertCallback([](const PerformanceAlert& alert) {
        std::cout << "ALERT [" << static_cast<int>(alert.level) << "] "
                  << alert.device_name << ": " << alert.message << "\n";
    });

    // Simulate device operations
    std::cout << "Simulating device operations...\n";
    for (int i = 0; i < 10; ++i) {
        bool success = (i % 4 != 0); // 75% success rate
        auto duration = std::chrono::milliseconds{500 + (i * 100)};
        monitor.recordOperation("TestCamera", duration, success);
    }

    // Get performance statistics
    auto stats = monitor.getStatistics("TestCamera");
    std::cout << "Performance stats for TestCamera:\n";
    std::cout << "  Total operations: " << stats.total_operations << "\n";
    std::cout << "  Success rate: " << (stats.successful_operations * 100.0 / stats.total_operations) << "%\n";
    std::cout << "  Average response: " << stats.current.response_time.count() << "ms\n";

    // Get optimization suggestions
    auto suggestions = monitor.getOptimizationSuggestions("TestCamera");
    std::cout << "Optimization suggestions:\n";
    for (const auto& suggestion : suggestions) {
        std::cout << "  " << suggestion.category << ": " << suggestion.suggestion << "\n";
    }

    std::cout << "Performance Monitoring demo completed.\n\n";
}

void demonstrateResourceManagement() {
    std::cout << "=== Resource Management Demo ===\n";

    DeviceResourceManager resourceManager;

    // Create resource pools
    ResourcePoolConfig cpuPool;
    cpuPool.type = ResourceType::CPU;
    cpuPool.name = "CPU_Pool";
    cpuPool.total_capacity = 8.0; // 8 cores
    cpuPool.warning_threshold = 0.8;
    cpuPool.critical_threshold = 0.95;
    resourceManager.createResourcePool(cpuPool);

    ResourcePoolConfig memoryPool;
    memoryPool.type = ResourceType::MEMORY;
    memoryPool.name = "Memory_Pool";
    memoryPool.total_capacity = 16384.0; // 16GB in MB
    memoryPool.warning_threshold = 0.8;
    memoryPool.critical_threshold = 0.9;
    resourceManager.createResourcePool(memoryPool);

    // Configure scheduling
    resourceManager.setSchedulingPolicy(SchedulingPolicy::PRIORITY_BASED);
    resourceManager.enableLoadBalancing(true);

    // Create resource requests
    ResourceRequest request1;
    request1.device_name = "Camera1";
    request1.request_id = "REQ001";
    request1.priority = 10;

    ResourceConstraint cpuConstraint;
    cpuConstraint.type = ResourceType::CPU;
    cpuConstraint.preferred_amount = 2.0;
    cpuConstraint.max_amount = 4.0;
    cpuConstraint.is_critical = true;
    request1.constraints.push_back(cpuConstraint);

    ResourceConstraint memConstraint;
    memConstraint.type = ResourceType::MEMORY;
    memConstraint.preferred_amount = 1024.0; // 1GB
    memConstraint.max_amount = 2048.0; // 2GB
    request1.constraints.push_back(memConstraint);

    // Request resources
    std::string requestId = resourceManager.requestResources(request1);
    std::cout << "Requested resources with ID: " << requestId << "\n";

    if (resourceManager.allocateResources(requestId)) {
        std::cout << "Resources allocated successfully\n";

        // Get resource usage
        auto cpuUsage = resourceManager.getResourceUsage("CPU_Pool");
        auto memUsage = resourceManager.getResourceUsage("Memory_Pool");

        std::cout << "CPU utilization: " << (cpuUsage.utilization_rate * 100) << "%\n";
        std::cout << "Memory utilization: " << (memUsage.utilization_rate * 100) << "%\n";

        // Release resources after some time
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        // resourceManager.releaseResources(lease_id);
    }

    // Get system stats
    auto sysStats = resourceManager.getSystemStats();
    std::cout << "System resource stats - Active leases: " << sysStats.active_leases
              << ", Pending requests: " << sysStats.pending_requests << "\n";

    std::cout << "Resource Management demo completed.\n\n";
}

void demonstrateConnectionPooling() {
    std::cout << "=== Connection Pooling Demo ===\n";

    ConnectionPoolConfig poolConfig;
    poolConfig.initial_size = 3;
    poolConfig.min_size = 2;
    poolConfig.max_size = 10;
    poolConfig.idle_timeout = std::chrono::seconds{60};
    poolConfig.enable_health_monitoring = true;
    poolConfig.enable_load_balancing = true;

    DeviceConnectionPool connectionPool(poolConfig);
    connectionPool.initialize();

    // Set up event callbacks
    connectionPool.setConnectionCreatedCallback([](const std::string& id, const std::string& info) {
        std::cout << "Connection created: " << id << " - " << info << "\n";
    });

    connectionPool.setConnectionErrorCallback([](const std::string& id, const std::string& error) {
        std::cout << "Connection error: " << id << " - " << error << "\n";
    });

    // Acquire connections
    std::cout << "Acquiring connections...\n";
    std::vector<std::shared_ptr<PoolConnection>> connections;

    for (int i = 0; i < 5; ++i) {
        auto conn = connectionPool.acquireConnection("camera", "TestCamera" + std::to_string(i));
        if (conn) {
            connections.push_back(conn);
            std::cout << "Acquired connection: " << conn->connection_id << "\n";
        }
    }

    // Get pool statistics
    auto poolStats = connectionPool.getStatistics();
    std::cout << "Pool stats - Total: " << poolStats.total_connections
              << ", Active: " << poolStats.active_connections
              << ", Idle: " << poolStats.idle_connections
              << ", Hit rate: " << (poolStats.hit_rate * 100) << "%\n";

    // Release connections
    std::cout << "Releasing connections...\n";
    for (auto& conn : connections) {
        connectionPool.releaseConnection(conn);
    }

    std::cout << "Connection Pooling demo completed.\n\n";
}

void demonstrateTaskScheduling() {
    std::cout << "=== Task Scheduling Demo ===\n";

    SchedulerConfig config;
    config.policy = SchedulingPolicy::PRIORITY;
    config.max_concurrent_tasks = 5;
    config.enable_load_balancing = true;
    config.enable_deadline_awareness = true;

    DeviceTaskScheduler scheduler(config);
    scheduler.start();

    // Set up callbacks
    scheduler.setTaskCompletedCallback([](const std::string& taskId, TaskState state, const std::string& msg) {
        std::cout << "Task " << taskId << " completed with state " << static_cast<int>(state) << "\n";
    });

    // Create and submit tasks
    std::vector<std::string> taskIds;

    for (int i = 0; i < 5; ++i) {
        DeviceTask task;
        task.task_name = "ExposureTask" + std::to_string(i);
        task.device_name = "Camera" + std::to_string(i % 2);
        task.priority = static_cast<TaskPriority>(i % 3);
        task.estimated_duration = std::chrono::milliseconds{1000 + (i * 200)};
        task.deadline = std::chrono::system_clock::now() + std::chrono::seconds{30};

        task.task_function = [i](std::shared_ptr<AtomDriver> device) -> bool {
            std::cout << "Executing task " << i << " on device " << device->getName() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds{500 + (i * 100)});
            return true;
        };

        std::string taskId = scheduler.submitTask(task);
        taskIds.push_back(taskId);
        std::cout << "Submitted task: " << taskId << "\n";
    }

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds{3});

    // Get scheduler statistics
    auto schedStats = scheduler.getStatistics();
    std::cout << "Scheduler stats - Total tasks: " << schedStats.total_tasks
              << ", Completed: " << schedStats.completed_tasks
              << ", Running: " << schedStats.running_tasks
              << ", Success rate: " << (schedStats.success_rate) << "%\n";

    scheduler.stop();
    std::cout << "Task Scheduling demo completed.\n\n";
}

void demonstrateCaching() {
    std::cout << "=== Device Caching Demo ===\n";

    CacheConfig cacheConfig;
    cacheConfig.max_memory_size = 50 * 1024 * 1024; // 50MB
    cacheConfig.max_entries = 1000;
    cacheConfig.eviction_policy = EvictionPolicy::LRU;
    cacheConfig.default_ttl = std::chrono::seconds{300};
    cacheConfig.enable_compression = true;
    cacheConfig.enable_persistence = true;

    DeviceCacheSystem<std::string> cache(cacheConfig);
    cache.initialize();

    // Set up cache event callback
    cache.setCacheEventCallback([](const CacheEvent& event) {
        std::cout << "Cache event: " << static_cast<int>(event.type)
                  << " for key " << event.key << "\n";
    });

    // Store device states
    std::cout << "Storing device states in cache...\n";
    cache.putDeviceState("Camera1", "IDLE");
    cache.putDeviceState("Camera2", "EXPOSING");
    cache.putDeviceConfig("Camera1", "{\"binning\": 1, \"gain\": 100}");
    cache.putDeviceCapabilities("Camera1", "{\"cooling\": true, \"guiding\": false}");

    // Store some operation results
    for (int i = 0; i < 10; ++i) {
        std::string key = "operation_result_" + std::to_string(i);
        std::string value = "Result data for operation " + std::to_string(i);
        cache.put(key, value, CacheEntryType::OPERATION_RESULT);
    }

    // Retrieve cached data
    std::string cameraState;
    if (cache.getDeviceState("Camera1", cameraState)) {
        std::cout << "Camera1 state from cache: " << cameraState << "\n";
    }

    std::string cameraConfig;
    if (cache.getDeviceConfig("Camera1", cameraConfig)) {
        std::cout << "Camera1 config from cache: " << cameraConfig << "\n";
    }

    // Get cache statistics
    auto cacheStats = cache.getStatistics();
    std::cout << "Cache stats - Entries: " << cacheStats.current_entries
              << ", Memory usage: " << (cacheStats.current_memory_usage / 1024) << "KB"
              << ", Hit rate: " << (cacheStats.hit_rate * 100) << "%\n";

    // Demonstrate batch operations
    std::vector<std::string> keys = {"operation_result_1", "operation_result_2", "operation_result_3"};
    auto batchResults = cache.getMultiple(keys);
    std::cout << "Retrieved " << batchResults.size() << " entries in batch\n";

    // Clear device-specific cache
    cache.clearDeviceCache("Camera1");
    std::cout << "Cleared cache for Camera1\n";

    std::cout << "Device Caching demo completed.\n\n";
}

int main() {
    std::cout << "=== Lithium Enhanced Device Management Demo ===\n\n";

    try {
        demonstrateEnhancedDeviceManager();
        demonstrateDeviceFactory();
        demonstratePerformanceMonitoring();
        demonstrateResourceManagement();
        demonstrateConnectionPooling();
        demonstrateTaskScheduling();
        demonstrateCaching();

        std::cout << "=== All demonstrations completed successfully ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
