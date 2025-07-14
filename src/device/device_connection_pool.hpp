/*
 * device_connection_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace lithium {

// Forward declarations
class AtomDriver;

// Connection states
enum class ConnectionState {
    IDLE,
    ACTIVE,
    BUSY,
    ERROR,
    TIMEOUT,
    DISCONNECTED
};

// Connection health status
enum class ConnectionHealth {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

// Connection statistics structure
struct ConnectionStatistics {
    size_t active_connections{0};
    size_t total_connections_created{0};
    size_t failed_connections{0};
    uint64_t uptime_seconds{0};
    double pool_efficiency{1.0};
};

// Pool connection
struct PoolConnection {
    std::string connection_id;
    std::shared_ptr<AtomDriver> device;
    ConnectionState state{ConnectionState::IDLE};
    ConnectionHealth health{ConnectionHealth::UNKNOWN};

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_used;

    size_t usage_count{0};
    size_t error_count{0};
};

// Pool configuration
struct ConnectionPoolConfig {
    size_t max_size{10};
    std::chrono::seconds idle_timeout{300};
    std::chrono::seconds connection_timeout{30};
    bool enable_health_monitoring{true};
    bool enable_load_balancing{true};
};

class DeviceConnectionPool {
public:
    DeviceConnectionPool();
    explicit DeviceConnectionPool(const ConnectionPoolConfig& config);
    ~DeviceConnectionPool();

    // Basic management
    void initialize();
    void shutdown();
    bool isInitialized() const;

    // Device registration
    void registerDevice(const std::string& device_name, std::shared_ptr<AtomDriver> device);
    void unregisterDevice(const std::string& device_name);

    // Connection management
    std::string acquireConnection(const std::string& device_name,
                                std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});
    bool releaseConnection(const std::string& connection_id);
    bool isConnectionActive(const std::string& connection_id) const;
    std::shared_ptr<AtomDriver> getDevice(const std::string& connection_id) const;

    // Statistics and monitoring
    ConnectionStatistics getStatistics() const;
    std::string getPoolStatus() const;

    // Maintenance and optimization
    void runMaintenance();
    bool isPerformanceOptimizationEnabled() const;
    void optimizePool();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace lithium
