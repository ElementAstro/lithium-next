/*
 * device_metrics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device performance metrics collection

**************************************************/

#ifndef LITHIUM_DEVICE_BRIDGE_DEVICE_METRICS_HPP
#define LITHIUM_DEVICE_BRIDGE_DEVICE_METRICS_HPP

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::device {

/**
 * @brief Individual device performance metrics
 */
struct DeviceMetrics {
    // Connection metrics
    size_t connectionAttempts{0};      ///< Total connection attempts
    size_t connectionSuccesses{0};     ///< Successful connections
    size_t connectionFailures{0};      ///< Failed connections
    size_t disconnections{0};          ///< Total disconnections

    // Operation metrics
    size_t operationCount{0};          ///< Total operations performed
    size_t operationSuccesses{0};      ///< Successful operations
    size_t operationFailures{0};       ///< Failed operations

    // Timing metrics
    std::chrono::milliseconds totalResponseTime{0};  ///< Total response time
    std::chrono::milliseconds minResponseTime{std::chrono::milliseconds::max()};
    std::chrono::milliseconds maxResponseTime{0};
    std::chrono::milliseconds lastResponseTime{0};

    // Error metrics
    size_t errorCount{0};              ///< Total errors
    std::string lastError;             ///< Last error message
    std::chrono::system_clock::time_point lastErrorTime;

    // Uptime tracking
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point disconnectedAt;
    std::chrono::milliseconds totalUptime{0};

    // Calculated metrics
    [[nodiscard]] auto getAverageResponseTime() const -> std::chrono::milliseconds;
    [[nodiscard]] auto getConnectionSuccessRate() const -> float;
    [[nodiscard]] auto getOperationSuccessRate() const -> float;
    [[nodiscard]] auto getUptimePercent() const -> float;

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceMetrics;

    void reset();
};

/**
 * @brief Aggregated metrics for all devices
 */
struct AggregatedMetrics {
    size_t totalDevices{0};
    size_t connectedDevices{0};
    size_t errorDevices{0};

    // Aggregated connection metrics
    size_t totalConnectionAttempts{0};
    size_t totalConnectionSuccesses{0};
    float avgConnectionSuccessRate{0.0f};

    // Aggregated operation metrics
    size_t totalOperations{0};
    size_t totalOperationSuccesses{0};
    float avgOperationSuccessRate{0.0f};

    // Aggregated timing
    std::chrono::milliseconds avgResponseTime{0};
    std::chrono::milliseconds minResponseTime{std::chrono::milliseconds::max()};
    std::chrono::milliseconds maxResponseTime{0};

    // Per-type metrics
    std::unordered_map<std::string, size_t> devicesByType;
    std::unordered_map<std::string, float> successRateByType;

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Metrics collector for device performance monitoring
 *
 * Provides:
 * - Per-device metrics collection
 * - Aggregated metrics calculation
 * - Thread-safe operations
 * - Export to JSON for API exposure
 *
 * Usage:
 * @code
 * auto collector = std::make_shared<DeviceMetricsCollector>();
 * collector->recordConnectionAttempt("camera1");
 * collector->recordConnectionSuccess("camera1");
 * collector->recordOperationTime("camera1", std::chrono::milliseconds(50));
 * auto metrics = collector->getDeviceMetrics("camera1");
 * @endcode
 */
class DeviceMetricsCollector {
public:
    DeviceMetricsCollector() = default;
    ~DeviceMetricsCollector() = default;

    // Disable copy
    DeviceMetricsCollector(const DeviceMetricsCollector&) = delete;
    DeviceMetricsCollector& operator=(const DeviceMetricsCollector&) = delete;

    // ==================== Device Registration ====================

    /**
     * @brief Register a device for metrics collection
     * @param deviceName Device name
     * @param deviceType Device type for categorization
     */
    void registerDevice(const std::string& deviceName,
                        const std::string& deviceType);

    /**
     * @brief Unregister a device
     * @param deviceName Device name
     */
    void unregisterDevice(const std::string& deviceName);

    /**
     * @brief Check if device is registered
     */
    [[nodiscard]] auto isRegistered(const std::string& deviceName) const -> bool;

    // ==================== Connection Metrics ====================

    /**
     * @brief Record a connection attempt
     */
    void recordConnectionAttempt(const std::string& deviceName);

    /**
     * @brief Record a successful connection
     */
    void recordConnectionSuccess(const std::string& deviceName);

    /**
     * @brief Record a failed connection
     * @param error Error message
     */
    void recordConnectionFailure(const std::string& deviceName,
                                  const std::string& error = {});

    /**
     * @brief Record a disconnection
     */
    void recordDisconnection(const std::string& deviceName);

    // ==================== Operation Metrics ====================

    /**
     * @brief Record an operation start
     * @return Operation ID for timing
     */
    auto recordOperationStart(const std::string& deviceName) -> uint64_t;

    /**
     * @brief Record an operation completion
     * @param operationId Operation ID from recordOperationStart
     * @param success Whether operation succeeded
     * @param error Error message if failed
     */
    void recordOperationEnd(const std::string& deviceName,
                            uint64_t operationId,
                            bool success,
                            const std::string& error = {});

    /**
     * @brief Record operation with explicit timing
     * @param duration Operation duration
     */
    void recordOperationTime(const std::string& deviceName,
                              std::chrono::milliseconds duration,
                              bool success = true);

    // ==================== Error Recording ====================

    /**
     * @brief Record an error
     */
    void recordError(const std::string& deviceName, const std::string& error);

    // ==================== Metrics Query ====================

    /**
     * @brief Get metrics for a specific device
     */
    [[nodiscard]] auto getDeviceMetrics(const std::string& deviceName) const
        -> DeviceMetrics;

    /**
     * @brief Get metrics for all devices
     */
    [[nodiscard]] auto getAllDeviceMetrics() const
        -> std::unordered_map<std::string, DeviceMetrics>;

    /**
     * @brief Get aggregated metrics
     */
    [[nodiscard]] auto getAggregatedMetrics() const -> AggregatedMetrics;

    /**
     * @brief Get metrics for devices of a specific type
     */
    [[nodiscard]] auto getMetricsByType(const std::string& deviceType) const
        -> std::vector<std::pair<std::string, DeviceMetrics>>;

    // ==================== Management ====================

    /**
     * @brief Reset metrics for a device
     */
    void resetDeviceMetrics(const std::string& deviceName);

    /**
     * @brief Reset all metrics
     */
    void resetAllMetrics();

    /**
     * @brief Get registered device count
     */
    [[nodiscard]] auto getDeviceCount() const -> size_t;

    /**
     * @brief Export all metrics to JSON
     */
    [[nodiscard]] auto exportToJson() const -> nlohmann::json;

private:
    struct DeviceEntry {
        DeviceMetrics metrics;
        std::string deviceType;
        std::unordered_map<uint64_t, std::chrono::steady_clock::time_point>
            pendingOperations;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, DeviceEntry> devices_;
    uint64_t nextOperationId_{1};
};

/**
 * @brief Factory function
 */
[[nodiscard]] auto createDeviceMetricsCollector()
    -> std::shared_ptr<DeviceMetricsCollector>;

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_BRIDGE_DEVICE_METRICS_HPP
