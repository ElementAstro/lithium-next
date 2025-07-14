/*
 * device_connection_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "device_connection_pool.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>
#include <shared_mutex>
#include <thread>

namespace lithium {

class DeviceConnectionPool::Impl {
public:
    ConnectionPoolConfig config_;
    std::unordered_map<std::string, std::vector<PoolConnection>> device_pools_;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> device_refs_;

    mutable std::shared_mutex pools_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    // Health monitoring
    std::thread health_monitor_thread_;

    // Statistics
    ConnectionStatistics stats_;
    std::chrono::system_clock::time_point start_time_;

    Impl() : start_time_(std::chrono::system_clock::now()) {}

    ~Impl() { shutdown(); }

    bool initialize() {
        if (initialized_.exchange(true)) {
            return true;
        }

        running_ = true;

        if (config_.enable_health_monitoring) {
            health_monitor_thread_ =
                std::thread(&Impl::healthMonitoringLoop, this);
            spdlog::info("Connection pool health monitoring started");
        }

        spdlog::info(
            "Connection pool initialized with max {} connections per device",
            config_.max_size);
        return true;
    }

    void shutdown() {
        if (!initialized_.exchange(false)) {
            return;
        }

        running_ = false;

        if (health_monitor_thread_.joinable()) {
            health_monitor_thread_.join();
        }

        std::unique_lock lock(pools_mutex_);
        device_pools_.clear();
        device_refs_.clear();

        spdlog::info("Connection pool shutdown completed");
    }

    std::string acquireConnection(const std::string& device_name,
                                  std::chrono::milliseconds timeout) {
        std::unique_lock lock(pools_mutex_);

        auto device_it = device_refs_.find(device_name);
        if (device_it == device_refs_.end()) {
            spdlog::error("Device {} not registered in connection pool",
                          device_name);
            return "";
        }

        auto& pool = device_pools_[device_name];

        // Try to find an available connection
        for (auto& conn : pool) {
            if (conn.state == ConnectionState::IDLE &&
                conn.health == ConnectionHealth::HEALTHY) {
                conn.state = ConnectionState::ACTIVE;
                conn.last_used = std::chrono::system_clock::now();
                conn.usage_count++;
                stats_.active_connections++;

                spdlog::debug("Reused existing connection {} for device {}",
                              conn.connection_id, device_name);
                return conn.connection_id;
            }
        }

        // Create new connection if pool not full
        if (pool.size() < config_.max_size) {
            PoolConnection new_conn;
            new_conn.connection_id = generateConnectionId(device_name);
            new_conn.device = device_it->second;
            new_conn.created_at = std::chrono::system_clock::now();
            new_conn.last_used = new_conn.created_at;
            new_conn.state = ConnectionState::ACTIVE;
            new_conn.health = ConnectionHealth::HEALTHY;
            new_conn.usage_count = 1;
            new_conn.error_count = 0;

            pool.push_back(new_conn);
            stats_.active_connections++;
            stats_.total_connections_created++;

            spdlog::info("Created new connection {} for device {}",
                         new_conn.connection_id, device_name);
            return new_conn.connection_id;
        }

        spdlog::warn("Connection pool full for device {}, max size: {}",
                     device_name, config_.max_size);
        return "";
    }

    bool releaseConnection(const std::string& connection_id) {
        std::unique_lock lock(pools_mutex_);

        for (auto& [device_name, pool] : device_pools_) {
            for (auto& conn : pool) {
                if (conn.connection_id == connection_id &&
                    conn.state == ConnectionState::ACTIVE) {
                    conn.state = ConnectionState::IDLE;
                    conn.last_used = std::chrono::system_clock::now();
                    stats_.active_connections--;

                    spdlog::debug("Released connection {} for device {}",
                                  connection_id, device_name);
                    return true;
                }
            }
        }

        spdlog::warn("Connection {} not found or not active", connection_id);
        return false;
    }

    void healthMonitoringLoop() {
        while (running_) {
            try {
                std::this_thread::sleep_for(std::chrono::seconds(30));

            } catch (const std::exception& e) {
                spdlog::error("Error in connection pool health monitoring: {}",
                              e.what());
            }
        }
    }

    void runMaintenance() {
        std::unique_lock lock(pools_mutex_);

        spdlog::info("Running connection pool maintenance");

        for (auto& [device_name, pool] : device_pools_) {
            // Remove unhealthy inactive connections
            auto old_size = pool.size();
            pool.erase(std::remove_if(
                           pool.begin(), pool.end(),
                           [](const PoolConnection& conn) {
                               return conn.state != ConnectionState::ACTIVE &&
                                      conn.health ==
                                          ConnectionHealth::UNHEALTHY;
                           }),
                       pool.end());

            if (pool.size() != old_size) {
                spdlog::info("Removed {} unhealthy connections for device {}",
                             old_size - pool.size(), device_name);
            }
        }

        updateStatistics();
        spdlog::info("Connection pool maintenance completed");
    }

    void updateStatistics() {
        auto now = std::chrono::system_clock::now();
        auto uptime =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        stats_.uptime_seconds = uptime.count();

        // Count current active connections
        size_t active_count = 0;
        for (const auto& [device_name, pool] : device_pools_) {
            active_count += std::count_if(
                pool.begin(), pool.end(), [](const PoolConnection& conn) {
                    return conn.state == ConnectionState::ACTIVE;
                });
        }
        stats_.active_connections = active_count;

        if (stats_.total_connections_created > 0) {
            stats_.pool_efficiency =
                static_cast<double>(stats_.total_connections_created -
                                    stats_.failed_connections) /
                stats_.total_connections_created;
        }
    }

    std::string generateConnectionId(const std::string& device_name) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);

        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        return device_name + "_conn_" + std::to_string(timestamp) + "_" +
               std::to_string(dis(gen));
    }

    void optimizePool() {
        std::unique_lock lock(pools_mutex_);

        spdlog::info("Running connection pool optimization");

        for (auto& [device_name, pool] : device_pools_) {
            // Calculate optimal pool size based on usage patterns
            size_t active_count = std::count_if(
                pool.begin(), pool.end(), [](const PoolConnection& conn) {
                    return conn.state == ConnectionState::ACTIVE;
                });

            size_t optimal_size = std::min(active_count + 2, config_.max_size);

            // Remove excess idle connections
            if (pool.size() > optimal_size) {
                auto remove_it = std::remove_if(
                    pool.begin(), pool.end(), [](const PoolConnection& conn) {
                        return conn.state == ConnectionState::IDLE;
                    });

                size_t remove_count = std::min(
                    pool.size() - optimal_size,
                    static_cast<size_t>(std::distance(remove_it, pool.end())));

                if (remove_count > 0) {
                    pool.erase(pool.end() - remove_count, pool.end());
                    spdlog::debug(
                        "Optimized pool for device {}: removed {} idle "
                        "connections",
                        device_name, remove_count);
                }
            }
        }

        spdlog::info("Connection pool optimization completed");
    }
};

DeviceConnectionPool::DeviceConnectionPool()
    : pimpl_(std::make_unique<Impl>()) {}

DeviceConnectionPool::DeviceConnectionPool(const ConnectionPoolConfig& config)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->config_ = config;
}

DeviceConnectionPool::~DeviceConnectionPool() = default;

void DeviceConnectionPool::initialize() { pimpl_->initialize(); }

void DeviceConnectionPool::shutdown() { pimpl_->shutdown(); }

bool DeviceConnectionPool::isInitialized() const {
    return pimpl_->initialized_;
}

void DeviceConnectionPool::registerDevice(const std::string& device_name,
                                          std::shared_ptr<AtomDriver> device) {
    if (!device) {
        spdlog::error("Cannot register null device {}", device_name);
        return;
    }

    std::unique_lock lock(pimpl_->pools_mutex_);
    pimpl_->device_refs_[device_name] = device;

    // Initialize empty pool for device
    if (pimpl_->device_pools_.find(device_name) ==
        pimpl_->device_pools_.end()) {
        pimpl_->device_pools_[device_name] = std::vector<PoolConnection>();
    }

    spdlog::info("Registered device {} in connection pool", device_name);
}

void DeviceConnectionPool::unregisterDevice(const std::string& device_name) {
    std::unique_lock lock(pimpl_->pools_mutex_);

    // Remove device pool
    auto pool_it = pimpl_->device_pools_.find(device_name);
    if (pool_it != pimpl_->device_pools_.end()) {
        for (auto& conn : pool_it->second) {
            conn.state = ConnectionState::DISCONNECTED;
        }
        pimpl_->device_pools_.erase(pool_it);
    }

    pimpl_->device_refs_.erase(device_name);

    spdlog::info("Unregistered device {} from connection pool", device_name);
}

std::string DeviceConnectionPool::acquireConnection(
    const std::string& device_name, std::chrono::milliseconds timeout) {
    return pimpl_->acquireConnection(device_name, timeout);
}

bool DeviceConnectionPool::releaseConnection(const std::string& connection_id) {
    return pimpl_->releaseConnection(connection_id);
}

bool DeviceConnectionPool::isConnectionActive(
    const std::string& connection_id) const {
    std::shared_lock lock(pimpl_->pools_mutex_);

    for (const auto& [device_name, pool] : pimpl_->device_pools_) {
        for (const auto& conn : pool) {
            if (conn.connection_id == connection_id) {
                return conn.state == ConnectionState::ACTIVE;
            }
        }
    }

    return false;
}

std::shared_ptr<AtomDriver> DeviceConnectionPool::getDevice(
    const std::string& connection_id) const {
    std::shared_lock lock(pimpl_->pools_mutex_);

    for (const auto& [device_name, pool] : pimpl_->device_pools_) {
        for (const auto& conn : pool) {
            if (conn.connection_id == connection_id) {
                return conn.device;
            }
        }
    }

    return nullptr;
}

ConnectionStatistics DeviceConnectionPool::getStatistics() const {
    std::shared_lock lock(pimpl_->pools_mutex_);
    pimpl_->updateStatistics();
    return pimpl_->stats_;
}

void DeviceConnectionPool::runMaintenance() { pimpl_->runMaintenance(); }

std::string DeviceConnectionPool::getPoolStatus() const {
    std::shared_lock lock(pimpl_->pools_mutex_);

    std::string status = "Connection Pool Status:\n";

    for (const auto& [device_name, pool] : pimpl_->device_pools_) {
        size_t active_count = std::count_if(
            pool.begin(), pool.end(), [](const PoolConnection& conn) {
                return conn.state == ConnectionState::ACTIVE;
            });
        size_t healthy_count = std::count_if(
            pool.begin(), pool.end(), [](const PoolConnection& conn) {
                return conn.health == ConnectionHealth::HEALTHY;
            });

        status += "  " + device_name + ": " + std::to_string(pool.size()) +
                  " total, " + std::to_string(active_count) + " active, " +
                  std::to_string(healthy_count) + " healthy\n";
    }

    auto stats = pimpl_->stats_;
    status += "  Total connections created: " +
              std::to_string(stats.total_connections_created) + "\n";
    status +=
        "  Active connections: " + std::to_string(stats.active_connections) +
        "\n";
    status +=
        "  Failed connections: " + std::to_string(stats.failed_connections) +
        "\n";
    status +=
        "  Pool efficiency: " + std::to_string(stats.pool_efficiency * 100) +
        "%\n";

    return status;
}

bool DeviceConnectionPool::isPerformanceOptimizationEnabled() const {
    return pimpl_->config_.enable_load_balancing;
}

void DeviceConnectionPool::optimizePool() { pimpl_->optimizePool(); }

}  // namespace lithium
