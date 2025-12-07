/*
 * device_metrics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device performance metrics implementation

**************************************************/

#include "device_metrics.hpp"

#include <algorithm>
#include <numeric>

namespace lithium::device {

// ============================================================================
// DeviceMetrics Implementation
// ============================================================================

auto DeviceMetrics::getAverageResponseTime() const -> std::chrono::milliseconds {
    if (operationCount == 0) {
        return std::chrono::milliseconds{0};
    }
    return std::chrono::milliseconds{
        totalResponseTime.count() / static_cast<long long>(operationCount)};
}

auto DeviceMetrics::getConnectionSuccessRate() const -> float {
    if (connectionAttempts == 0) {
        return 0.0f;
    }
    return static_cast<float>(connectionSuccesses) /
           static_cast<float>(connectionAttempts) * 100.0f;
}

auto DeviceMetrics::getOperationSuccessRate() const -> float {
    if (operationCount == 0) {
        return 0.0f;
    }
    return static_cast<float>(operationSuccesses) /
           static_cast<float>(operationCount) * 100.0f;
}

auto DeviceMetrics::getUptimePercent() const -> float {
    auto now = std::chrono::system_clock::now();

    // Calculate expected uptime since first connection
    if (connectedAt == std::chrono::system_clock::time_point{}) {
        return 0.0f;  // Never connected
    }

    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - connectedAt);

    if (totalTime.count() == 0) {
        return 0.0f;
    }

    auto actualUptime = totalUptime;

    // Add current session if still connected
    if (disconnectedAt < connectedAt) {
        actualUptime += std::chrono::duration_cast<std::chrono::milliseconds>(
            now - connectedAt);
    }

    return static_cast<float>(actualUptime.count()) /
           static_cast<float>(totalTime.count()) * 100.0f;
}

auto DeviceMetrics::toJson() const -> nlohmann::json {
    nlohmann::json j;

    // Connection metrics
    j["connection"]["attempts"] = connectionAttempts;
    j["connection"]["successes"] = connectionSuccesses;
    j["connection"]["failures"] = connectionFailures;
    j["connection"]["disconnections"] = disconnections;
    j["connection"]["successRate"] = getConnectionSuccessRate();

    // Operation metrics
    j["operations"]["count"] = operationCount;
    j["operations"]["successes"] = operationSuccesses;
    j["operations"]["failures"] = operationFailures;
    j["operations"]["successRate"] = getOperationSuccessRate();

    // Timing metrics
    j["timing"]["averageMs"] = getAverageResponseTime().count();
    j["timing"]["minMs"] = minResponseTime == std::chrono::milliseconds::max()
                               ? 0
                               : minResponseTime.count();
    j["timing"]["maxMs"] = maxResponseTime.count();
    j["timing"]["lastMs"] = lastResponseTime.count();

    // Error metrics
    j["errors"]["count"] = errorCount;
    j["errors"]["lastError"] = lastError;
    if (lastErrorTime != std::chrono::system_clock::time_point{}) {
        j["errors"]["lastErrorTime"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                lastErrorTime.time_since_epoch())
                .count();
    }

    // Uptime
    j["uptime"]["totalMs"] = totalUptime.count();
    j["uptime"]["percent"] = getUptimePercent();

    return j;
}

auto DeviceMetrics::fromJson(const nlohmann::json& j) -> DeviceMetrics {
    DeviceMetrics metrics;

    if (j.contains("connection")) {
        auto& conn = j["connection"];
        metrics.connectionAttempts = conn.value("attempts", 0);
        metrics.connectionSuccesses = conn.value("successes", 0);
        metrics.connectionFailures = conn.value("failures", 0);
        metrics.disconnections = conn.value("disconnections", 0);
    }

    if (j.contains("operations")) {
        auto& ops = j["operations"];
        metrics.operationCount = ops.value("count", 0);
        metrics.operationSuccesses = ops.value("successes", 0);
        metrics.operationFailures = ops.value("failures", 0);
    }

    if (j.contains("timing")) {
        auto& timing = j["timing"];
        metrics.minResponseTime =
            std::chrono::milliseconds{timing.value("minMs", 0)};
        metrics.maxResponseTime =
            std::chrono::milliseconds{timing.value("maxMs", 0)};
        metrics.lastResponseTime =
            std::chrono::milliseconds{timing.value("lastMs", 0)};
    }

    if (j.contains("errors")) {
        auto& errors = j["errors"];
        metrics.errorCount = errors.value("count", 0);
        metrics.lastError = errors.value("lastError", "");
    }

    if (j.contains("uptime")) {
        auto& uptime = j["uptime"];
        metrics.totalUptime =
            std::chrono::milliseconds{uptime.value("totalMs", 0)};
    }

    return metrics;
}

void DeviceMetrics::reset() {
    connectionAttempts = 0;
    connectionSuccesses = 0;
    connectionFailures = 0;
    disconnections = 0;

    operationCount = 0;
    operationSuccesses = 0;
    operationFailures = 0;

    totalResponseTime = std::chrono::milliseconds{0};
    minResponseTime = std::chrono::milliseconds::max();
    maxResponseTime = std::chrono::milliseconds{0};
    lastResponseTime = std::chrono::milliseconds{0};

    errorCount = 0;
    lastError.clear();
    lastErrorTime = {};

    connectedAt = {};
    disconnectedAt = {};
    totalUptime = std::chrono::milliseconds{0};
}

// ============================================================================
// AggregatedMetrics Implementation
// ============================================================================

auto AggregatedMetrics::toJson() const -> nlohmann::json {
    nlohmann::json j;

    j["devices"]["total"] = totalDevices;
    j["devices"]["connected"] = connectedDevices;
    j["devices"]["error"] = errorDevices;

    j["connection"]["totalAttempts"] = totalConnectionAttempts;
    j["connection"]["totalSuccesses"] = totalConnectionSuccesses;
    j["connection"]["avgSuccessRate"] = avgConnectionSuccessRate;

    j["operations"]["total"] = totalOperations;
    j["operations"]["successes"] = totalOperationSuccesses;
    j["operations"]["avgSuccessRate"] = avgOperationSuccessRate;

    j["timing"]["avgMs"] = avgResponseTime.count();
    j["timing"]["minMs"] = minResponseTime == std::chrono::milliseconds::max()
                               ? 0
                               : minResponseTime.count();
    j["timing"]["maxMs"] = maxResponseTime.count();

    j["byType"]["devices"] = devicesByType;
    j["byType"]["successRate"] = successRateByType;

    return j;
}

// ============================================================================
// DeviceMetricsCollector Implementation
// ============================================================================

void DeviceMetricsCollector::registerDevice(const std::string& deviceName,
                                             const std::string& deviceType) {
    std::unique_lock lock(mutex_);

    if (!devices_.contains(deviceName)) {
        DeviceEntry entry;
        entry.deviceType = deviceType;
        devices_[deviceName] = std::move(entry);
    }
}

void DeviceMetricsCollector::unregisterDevice(const std::string& deviceName) {
    std::unique_lock lock(mutex_);
    devices_.erase(deviceName);
}

auto DeviceMetricsCollector::isRegistered(const std::string& deviceName) const
    -> bool {
    std::shared_lock lock(mutex_);
    return devices_.contains(deviceName);
}

void DeviceMetricsCollector::recordConnectionAttempt(
    const std::string& deviceName) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        it->second.metrics.connectionAttempts++;
    }
}

void DeviceMetricsCollector::recordConnectionSuccess(
    const std::string& deviceName) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        it->second.metrics.connectionSuccesses++;
        it->second.metrics.connectedAt = std::chrono::system_clock::now();
    }
}

void DeviceMetricsCollector::recordConnectionFailure(
    const std::string& deviceName, const std::string& error) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        it->second.metrics.connectionFailures++;
        if (!error.empty()) {
            it->second.metrics.errorCount++;
            it->second.metrics.lastError = error;
            it->second.metrics.lastErrorTime =
                std::chrono::system_clock::now();
        }
    }
}

void DeviceMetricsCollector::recordDisconnection(
    const std::string& deviceName) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        auto& metrics = it->second.metrics;
        metrics.disconnections++;
        metrics.disconnectedAt = std::chrono::system_clock::now();

        // Update total uptime
        if (metrics.connectedAt != std::chrono::system_clock::time_point{}) {
            metrics.totalUptime +=
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    metrics.disconnectedAt - metrics.connectedAt);
        }
    }
}

auto DeviceMetricsCollector::recordOperationStart(
    const std::string& deviceName) -> uint64_t {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        uint64_t opId = nextOperationId_++;
        it->second.pendingOperations[opId] =
            std::chrono::steady_clock::now();
        return opId;
    }
    return 0;
}

void DeviceMetricsCollector::recordOperationEnd(const std::string& deviceName,
                                                  uint64_t operationId,
                                                  bool success,
                                                  const std::string& error) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it == devices_.end()) {
        return;
    }

    auto& entry = it->second;
    auto opIt = entry.pendingOperations.find(operationId);
    if (opIt == entry.pendingOperations.end()) {
        return;
    }

    auto startTime = opIt->second;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    entry.pendingOperations.erase(opIt);

    // Update metrics
    auto& metrics = entry.metrics;
    metrics.operationCount++;
    metrics.totalResponseTime += duration;
    metrics.lastResponseTime = duration;

    if (duration < metrics.minResponseTime) {
        metrics.minResponseTime = duration;
    }
    if (duration > metrics.maxResponseTime) {
        metrics.maxResponseTime = duration;
    }

    if (success) {
        metrics.operationSuccesses++;
    } else {
        metrics.operationFailures++;
        if (!error.empty()) {
            metrics.errorCount++;
            metrics.lastError = error;
            metrics.lastErrorTime = std::chrono::system_clock::now();
        }
    }
}

void DeviceMetricsCollector::recordOperationTime(
    const std::string& deviceName, std::chrono::milliseconds duration,
    bool success) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it == devices_.end()) {
        return;
    }

    auto& metrics = it->second.metrics;
    metrics.operationCount++;
    metrics.totalResponseTime += duration;
    metrics.lastResponseTime = duration;

    if (duration < metrics.minResponseTime) {
        metrics.minResponseTime = duration;
    }
    if (duration > metrics.maxResponseTime) {
        metrics.maxResponseTime = duration;
    }

    if (success) {
        metrics.operationSuccesses++;
    } else {
        metrics.operationFailures++;
    }
}

void DeviceMetricsCollector::recordError(const std::string& deviceName,
                                          const std::string& error) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        it->second.metrics.errorCount++;
        it->second.metrics.lastError = error;
        it->second.metrics.lastErrorTime = std::chrono::system_clock::now();
    }
}

auto DeviceMetricsCollector::getDeviceMetrics(const std::string& deviceName) const
    -> DeviceMetrics {
    std::shared_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        return it->second.metrics;
    }
    return DeviceMetrics{};
}

auto DeviceMetricsCollector::getAllDeviceMetrics() const
    -> std::unordered_map<std::string, DeviceMetrics> {
    std::shared_lock lock(mutex_);

    std::unordered_map<std::string, DeviceMetrics> result;
    for (const auto& [name, entry] : devices_) {
        result[name] = entry.metrics;
    }
    return result;
}

auto DeviceMetricsCollector::getAggregatedMetrics() const -> AggregatedMetrics {
    std::shared_lock lock(mutex_);

    AggregatedMetrics agg;
    agg.totalDevices = devices_.size();

    float totalConnRate = 0.0f;
    float totalOpRate = 0.0f;
    size_t devicesWithOps = 0;

    for (const auto& [name, entry] : devices_) {
        const auto& m = entry.metrics;

        // Count connected/error devices
        if (m.connectedAt > m.disconnectedAt) {
            agg.connectedDevices++;
        }
        if (m.errorCount > 0) {
            agg.errorDevices++;
        }

        // Aggregate connection stats
        agg.totalConnectionAttempts += m.connectionAttempts;
        agg.totalConnectionSuccesses += m.connectionSuccesses;

        // Aggregate operation stats
        agg.totalOperations += m.operationCount;
        agg.totalOperationSuccesses += m.operationSuccesses;

        // Track rates
        totalConnRate += m.getConnectionSuccessRate();
        if (m.operationCount > 0) {
            totalOpRate += m.getOperationSuccessRate();
            devicesWithOps++;
        }

        // Timing
        if (m.minResponseTime < agg.minResponseTime &&
            m.minResponseTime != std::chrono::milliseconds::max()) {
            agg.minResponseTime = m.minResponseTime;
        }
        if (m.maxResponseTime > agg.maxResponseTime) {
            agg.maxResponseTime = m.maxResponseTime;
        }

        // By type
        agg.devicesByType[entry.deviceType]++;
    }

    // Calculate averages
    if (!devices_.empty()) {
        agg.avgConnectionSuccessRate = totalConnRate / devices_.size();

        if (agg.totalOperations > 0) {
            agg.avgResponseTime = std::chrono::milliseconds{
                std::accumulate(
                    devices_.begin(), devices_.end(), 0LL,
                    [](long long sum, const auto& pair) {
                        return sum +
                               pair.second.metrics.totalResponseTime.count();
                    }) /
                static_cast<long long>(agg.totalOperations)};
        }
    }

    if (devicesWithOps > 0) {
        agg.avgOperationSuccessRate = totalOpRate / devicesWithOps;
    }

    return agg;
}

auto DeviceMetricsCollector::getMetricsByType(const std::string& deviceType) const
    -> std::vector<std::pair<std::string, DeviceMetrics>> {
    std::shared_lock lock(mutex_);

    std::vector<std::pair<std::string, DeviceMetrics>> result;
    for (const auto& [name, entry] : devices_) {
        if (entry.deviceType == deviceType) {
            result.emplace_back(name, entry.metrics);
        }
    }
    return result;
}

void DeviceMetricsCollector::resetDeviceMetrics(const std::string& deviceName) {
    std::unique_lock lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        it->second.metrics.reset();
        it->second.pendingOperations.clear();
    }
}

void DeviceMetricsCollector::resetAllMetrics() {
    std::unique_lock lock(mutex_);

    for (auto& [name, entry] : devices_) {
        entry.metrics.reset();
        entry.pendingOperations.clear();
    }
}

auto DeviceMetricsCollector::getDeviceCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return devices_.size();
}

auto DeviceMetricsCollector::exportToJson() const -> nlohmann::json {
    nlohmann::json j;

    j["aggregated"] = getAggregatedMetrics().toJson();

    nlohmann::json devicesJson;
    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, entry] : devices_) {
            nlohmann::json deviceJson = entry.metrics.toJson();
            deviceJson["type"] = entry.deviceType;
            devicesJson[name] = deviceJson;
        }
    }
    j["devices"] = devicesJson;

    return j;
}

// ============================================================================
// Factory Function
// ============================================================================

auto createDeviceMetricsCollector() -> std::shared_ptr<DeviceMetricsCollector> {
    return std::make_shared<DeviceMetricsCollector>();
}

}  // namespace lithium::device
