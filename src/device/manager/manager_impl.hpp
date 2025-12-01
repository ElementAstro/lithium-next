/*
 * manager_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager Implementation Details

**************************************************/

#pragma once

#include "manager.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

namespace lithium {

/**
 * @brief Event subscription entry
 */
struct EventSubscription {
    EventCallbackId id;
    DeviceEventCallback callback;
    std::unordered_set<int> eventTypes;  // Empty means all events
};

/**
 * @brief Statistics tracking
 */
struct ManagerStatistics {
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> successfulConnections{0};
    std::atomic<uint64_t> failedConnections{0};
    std::atomic<uint64_t> totalOperations{0};
    std::atomic<uint64_t> successfulOperations{0};
    std::atomic<uint64_t> failedOperations{0};
    std::atomic<uint64_t> totalRetries{0};
    std::chrono::system_clock::time_point startTime;

    ManagerStatistics() : startTime(std::chrono::system_clock::now()) {}

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["totalConnections"] = totalConnections.load();
        j["successfulConnections"] = successfulConnections.load();
        j["failedConnections"] = failedConnections.load();
        j["totalOperations"] = totalOperations.load();
        j["successfulOperations"] = successfulOperations.load();
        j["failedOperations"] = failedOperations.load();
        j["totalRetries"] = totalRetries.load();
        auto uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now() - startTime)
                            .count();
        j["uptimeMs"] = uptimeMs;
        return j;
    }

    void reset() {
        totalConnections = 0;
        successfulConnections = 0;
        failedConnections = 0;
        totalOperations = 0;
        successfulOperations = 0;
        failedOperations = 0;
        totalRetries = 0;
        startTime = std::chrono::system_clock::now();
    }
};

/**
 * @brief Device Manager Implementation
 */
class DeviceManagerImpl {
public:
    DeviceManagerImpl();
    ~DeviceManagerImpl();

    // ==================== Device Storage ====================

    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
        devices;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> primaryDevices;
    std::unordered_map<std::string, DeviceMetadata> deviceMetadata;
    std::unordered_map<std::string, DeviceState> deviceStates;
    std::unordered_map<std::string, DeviceRetryConfig> retryConfigs;

    // ==================== Event System ====================

    DeviceEventCallback legacyEventCallback;
    std::vector<EventSubscription> eventSubscriptions;
    std::deque<DeviceEvent> pendingEvents;
    std::atomic<EventCallbackId> nextCallbackId{1};
    static constexpr size_t MAX_PENDING_EVENTS = 1000;

    // ==================== Health Monitoring ====================

    std::atomic<bool> healthMonitorRunning{false};
    std::thread healthMonitorThread;
    std::condition_variable healthMonitorCv;
    std::mutex healthMonitorMutex;
    std::chrono::seconds healthCheckInterval{30};

    // ==================== Statistics ====================

    ManagerStatistics statistics;

    // ==================== Synchronization ====================

    mutable std::shared_mutex mtx;
    mutable std::mutex eventMutex;

    // ==================== Helper Methods ====================

    /**
     * @brief Find device by name
     */
    auto findDeviceByName(const std::string& name) const
        -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Find device type by name
     */
    auto findDeviceType(const std::string& name) const -> std::string;

    /**
     * @brief Emit device event
     */
    void emitEvent(const DeviceEvent& event);

    /**
     * @brief Update device state
     */
    void updateDeviceState(const std::string& name, bool connected);

    /**
     * @brief Get retry config for device
     */
    auto getRetryConfig(const std::string& name) const -> DeviceRetryConfig;

    /**
     * @brief Calculate retry delay
     */
    auto calculateRetryDelay(const DeviceRetryConfig& config, int attempt) const
        -> std::chrono::milliseconds;

    /**
     * @brief Start health monitor internal
     */
    void startHealthMonitorInternal();

    /**
     * @brief Stop health monitor internal
     */
    void stopHealthMonitorInternal();

    /**
     * @brief Health check loop
     */
    void healthCheckLoop();
};

}  // namespace lithium
