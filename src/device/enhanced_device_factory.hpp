/*
 * device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced Device Factory with performance optimizations and scalability improvements

*************************************************/

#pragma once

#include "template/device.hpp"
#include "template/camera.hpp"
#include "template/telescope.hpp"
#include "template/focuser.hpp"
#include "template/filterwheel.hpp"
#include "template/rotator.hpp"
#include "template/dome.hpp"
#include "template/guider.hpp"
#include "template/weather.hpp"
#include "template/safety_monitor.hpp"
#include "template/adaptive_optics.hpp"

// Mock implementations
#include "template/mock/mock_camera.hpp"
#include "template/mock/mock_telescope.hpp"
#include "template/mock/mock_focuser.hpp"
#include "template/mock/mock_filterwheel.hpp"
#include "template/mock/mock_rotator.hpp"
#include "template/mock/mock_dome.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

enum class DeviceType {
    CAMERA,
    TELESCOPE,
    FOCUSER,
    FILTERWHEEL,
    ROTATOR,
    DOME,
    GUIDER,
    WEATHER_STATION,
    SAFETY_MONITOR,
    ADAPTIVE_OPTICS,
    UNKNOWN
};

enum class DeviceBackend {
    MOCK,
    INDI,
    ASCOM,
    NATIVE
};

// Device creation configuration
struct DeviceCreationConfig {
    std::string name;
    DeviceType type;
    DeviceBackend backend;
    std::unordered_map<std::string, std::string> properties;
    std::chrono::milliseconds timeout{5000};
    int priority{0};
    bool enable_simulation{false};
    bool enable_caching{true};
    bool enable_pooling{false};
};

// Device performance profile
struct DevicePerformanceProfile {
    std::chrono::milliseconds avg_creation_time{0};
    std::chrono::milliseconds avg_initialization_time{0};
    size_t creation_count{0};
    size_t success_count{0};
    size_t failure_count{0};
    double success_rate{100.0};
};

// Device cache entry
struct DeviceCacheEntry {
    std::weak_ptr<AtomDriver> device;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    size_t access_count{0};
    bool is_pooled{false};
};

class DeviceFactory {
public:
    static DeviceFactory& getInstance() {
        static DeviceFactory instance;
        return instance;
    }

    // Enhanced factory methods with configuration
    std::unique_ptr<AtomCamera> createCamera(const DeviceCreationConfig& config);
    std::unique_ptr<AtomTelescope> createTelescope(const DeviceCreationConfig& config);
    std::unique_ptr<AtomFocuser> createFocuser(const DeviceCreationConfig& config);
    std::unique_ptr<AtomFilterWheel> createFilterWheel(const DeviceCreationConfig& config);
    std::unique_ptr<AtomRotator> createRotator(const DeviceCreationConfig& config);
    std::unique_ptr<AtomDome> createDome(const DeviceCreationConfig& config);
    
    // Legacy factory methods for backwards compatibility
    std::unique_ptr<AtomCamera> createCamera(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomTelescope> createTelescope(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomFocuser> createFocuser(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomFilterWheel> createFilterWheel(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomRotator> createRotator(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomDome> createDome(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    
    // Generic device creation
    std::unique_ptr<AtomDriver> createDevice(const DeviceCreationConfig& config);
    std::unique_ptr<AtomDriver> createDevice(DeviceType type, const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    
    // Device type utilities
    static DeviceType stringToDeviceType(const std::string& typeStr);
    static std::string deviceTypeToString(DeviceType type);
    static DeviceBackend stringToBackend(const std::string& backendStr);
    static std::string backendToString(DeviceBackend backend);
    
    // Available device backends
    std::vector<DeviceBackend> getAvailableBackends(DeviceType type) const;
    bool isBackendAvailable(DeviceType type, DeviceBackend backend) const;
    
    // Enhanced device discovery
    struct DeviceInfo {
        std::string name;
        DeviceType type;
        DeviceBackend backend;
        std::string description;
        std::string version;
        std::unordered_map<std::string, std::string> capabilities;
        bool is_available{true};
        std::chrono::milliseconds response_time{0};
    };
    
    std::vector<DeviceInfo> discoverDevices(DeviceType type = DeviceType::UNKNOWN, DeviceBackend backend = DeviceBackend::MOCK) const;
    
    // Async device discovery
    using DeviceDiscoveryCallback = std::function<void(const std::vector<DeviceInfo>&)>;
    void discoverDevicesAsync(DeviceDiscoveryCallback callback, DeviceType type = DeviceType::UNKNOWN, DeviceBackend backend = DeviceBackend::MOCK);
    
    // Device caching
    void enableCaching(bool enable);
    bool isCachingEnabled() const;
    void setCacheSize(size_t max_size);
    size_t getCacheSize() const;
    void clearCache();
    void clearCacheForType(DeviceType type);
    
    // Device pooling
    void enablePooling(bool enable);
    bool isPoolingEnabled() const;
    void setPoolSize(DeviceType type, size_t size);
    size_t getPoolSize(DeviceType type) const;
    void preloadPool(DeviceType type, size_t count);
    void clearPool(DeviceType type);
    
    // Performance monitoring
    void enablePerformanceMonitoring(bool enable);
    bool isPerformanceMonitoringEnabled() const;
    DevicePerformanceProfile getPerformanceProfile(DeviceType type, DeviceBackend backend) const;
    void resetPerformanceProfile(DeviceType type, DeviceBackend backend);
    
    // Registry for custom device creators
    using DeviceCreator = std::function<std::unique_ptr<AtomDriver>(const DeviceCreationConfig&)>;
    void registerDeviceCreator(DeviceType type, DeviceBackend backend, DeviceCreator creator);
    void unregisterDeviceCreator(DeviceType type, DeviceBackend backend);
    
    // Advanced configuration
    void setDefaultTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds getDefaultTimeout() const;
    void setMaxConcurrentCreations(size_t max_concurrent);
    size_t getMaxConcurrentCreations() const;
    
    // Batch operations
    std::vector<std::unique_ptr<AtomDriver>> createDevicesBatch(const std::vector<DeviceCreationConfig>& configs);
    using BatchCreationCallback = std::function<void(const std::vector<std::pair<DeviceCreationConfig, std::unique_ptr<AtomDriver>>>&)>;
    void createDevicesBatchAsync(const std::vector<DeviceCreationConfig>& configs, BatchCreationCallback callback);
    
    // Device validation
    bool validateDeviceConfig(const DeviceCreationConfig& config) const;
    std::vector<std::string> getConfigErrors(const DeviceCreationConfig& config) const;
    
    // Resource management
    struct ResourceUsage {
        size_t total_devices_created{0};
        size_t active_devices{0};
        size_t cached_devices{0};
        size_t pooled_devices{0};
        size_t memory_usage_bytes{0};
        size_t concurrent_creations{0};
    };
    ResourceUsage getResourceUsage() const;
    
    // Configuration presets
    void savePreset(const std::string& name, const DeviceCreationConfig& config);
    DeviceCreationConfig loadPreset(const std::string& name);
    std::vector<std::string> getPresetNames() const;
    void deletePreset(const std::string& name);
    
    // Factory statistics
    struct FactoryStatistics {
        size_t total_creations{0};
        size_t successful_creations{0};
        size_t failed_creations{0};
        double success_rate{100.0};
        std::chrono::milliseconds avg_creation_time{0};
        std::chrono::system_clock::time_point start_time;
        std::unordered_map<DeviceType, size_t> creation_count_by_type;
        std::unordered_map<DeviceBackend, size_t> creation_count_by_backend;
    };
    FactoryStatistics getStatistics() const;
    void resetStatistics();
    
    // Event callbacks
    using DeviceCreatedCallback = std::function<void(const std::string&, DeviceType, DeviceBackend, bool)>;
    void setDeviceCreatedCallback(DeviceCreatedCallback callback);
    
    // Cleanup and maintenance
    void runMaintenance();
    void cleanup();
    
private:
    DeviceFactory();
    ~DeviceFactory();
    
    // Disable copy and assignment
    DeviceFactory(const DeviceFactory&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = delete;
    
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Helper methods
    std::string makeRegistryKey(DeviceType type, DeviceBackend backend) const;
    std::unique_ptr<AtomDriver> createDeviceInternal(const DeviceCreationConfig& config);
    void updatePerformanceProfile(DeviceType type, DeviceBackend backend, std::chrono::milliseconds creation_time, bool success);
    
    // Backend availability checking
    bool isINDIAvailable() const;
    bool isASCOMAvailable() const;
};