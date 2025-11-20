#include "monitor.hpp"

#include "../factory.hpp"

#include <fstream>
#include <thread>
#include "spdlog/spdlog.h"

namespace lithium::task::script {

ScriptMonitorTask::ScriptMonitorTask(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }) {
    setupMonitoringDefaults();
}

ScriptMonitorTask::~ScriptMonitorTask() {
    shouldStop_ = true;
    cleanupMonitoring();
}

void ScriptMonitorTask::setupMonitoringDefaults() {
    addParamDefinition("scriptId", "string", true, nullptr,
                       "ID of script to monitor");
    addParamDefinition("maxExecutionTime", "number", false, 300,
                       "Maximum execution time in seconds");
    addParamDefinition("maxMemoryUsage", "number", false, 1073741824,
                       "Maximum memory usage in bytes");
    addParamDefinition("maxCpuUsage", "number", false, 80.0,
                       "Maximum CPU usage percentage");
    addParamDefinition("monitorInterval", "number", false, 1,
                       "Monitoring interval in seconds");
    addParamDefinition("alertThresholds", "object", false, json::object(),
                       "Custom alert thresholds");

    setTimeout(std::chrono::seconds(3600));
    setPriority(3);  // Lower priority for monitoring
    setTaskType("script_monitor");

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Monitor task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Monitor exception: " + std::string(e.what()));
    });
}

void ScriptMonitorTask::execute(const json& params) {
    addHistoryEntry("Starting script monitoring");

    try {
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Monitor parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            throw std::invalid_argument(errorMsg);
        }

        std::string scriptId = params["scriptId"].get<std::string>();

        ResourceLimits limits;
        limits.maxExecutionTime =
            std::chrono::seconds(params.value("maxExecutionTime", 300));
        limits.maxMemoryUsage = params.value("maxMemoryUsage", 1073741824UL);
        limits.maxCpuUsage = params.value("maxCpuUsage", 80.0);

        startMonitoring(scriptId, limits);

        // Wait for monitoring to complete or be stopped
        while (!shouldStop_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Check if script is still running
            std::shared_lock lock(metricsMutex_);
            auto it = metrics_.find(scriptId);
            if (it == metrics_.end()) {
                break;  // Script completed or stopped
            }
        }

        addHistoryEntry("Script monitoring completed");

    } catch (const std::exception& e) {
        spdlog::error("Script monitoring failed: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        throw;
    }
}

void ScriptMonitorTask::startMonitoring(const std::string& scriptId,
                                        const ResourceLimits& limits) {
    std::unique_lock lock(metricsMutex_);

    // Initialize metrics
    metrics_[scriptId] = MonitoringMetrics{};
    limits_[scriptId] = limits;

    // Start monitoring thread
    monitorThreads_[scriptId] = std::make_unique<std::thread>(
        [this, scriptId]() { monitorScript(scriptId); });

    addHistoryEntry("Started monitoring script: " + scriptId);
    spdlog::info("Started monitoring script: {}", scriptId);
}

void ScriptMonitorTask::stopMonitoring(const std::string& scriptId) {
    std::unique_lock lock(metricsMutex_);

    auto threadIt = monitorThreads_.find(scriptId);
    if (threadIt != monitorThreads_.end()) {
        if (threadIt->second->joinable()) {
            threadIt->second->join();
        }
        monitorThreads_.erase(threadIt);
    }

    // Keep metrics for final reporting
    limits_.erase(scriptId);

    addHistoryEntry("Stopped monitoring script: " + scriptId);
    spdlog::info("Stopped monitoring script: {}", scriptId);
}

void ScriptMonitorTask::monitorScript(const std::string& scriptId) {
    auto startTime = std::chrono::steady_clock::now();

    while (!shouldStop_) {
        try {
            updateMetrics(scriptId);
            checkResourceLimits(scriptId);

            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Check if monitoring should continue
            std::shared_lock lock(metricsMutex_);
            auto limitsIt = limits_.find(scriptId);
            if (limitsIt == limits_.end()) {
                break;  // Monitoring stopped externally
            }

            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > limitsIt->second.maxExecutionTime) {
                if (resourceExceededCallback_) {
                    resourceExceededCallback_(scriptId,
                                              "Execution timeout exceeded");
                }
                break;
            }

        } catch (const std::exception& e) {
            spdlog::error("Error monitoring script {}: {}", scriptId, e.what());
            break;
        }
    }

    // Final metrics update
    updateMetrics(scriptId);

    // Call completion callback
    if (completionCallback_) {
        std::shared_lock lock(metricsMutex_);
        auto metricsIt = metrics_.find(scriptId);
        if (metricsIt != metrics_.end()) {
            completionCallback_(scriptId, metricsIt->second);
        }
    }
}

void ScriptMonitorTask::updateMetrics(const std::string& scriptId) {
    std::unique_lock lock(metricsMutex_);

    auto metricsIt = metrics_.find(scriptId);
    if (metricsIt == metrics_.end()) {
        return;
    }

    auto& metrics = metricsIt->second;

    // Update execution time
    static auto scriptStartTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    metrics.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(currentTime -
                                                              scriptStartTime);

    // Update memory usage (simplified - in practice, you'd query the actual
    // process)
    std::ifstream statFile("/proc/self/status");
    std::string line;
    while (std::getline(statFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            size_t value;
            if (sscanf(line.c_str(), "VmRSS: %zu kB", &value) == 1) {
                metrics.memoryUsage = value * 1024;  // Convert to bytes
            }
            break;
        }
    }

    // Update CPU usage (simplified)
    static double lastCpuTime = 0.0;
    std::ifstream cpuStatFile("/proc/self/stat");
    if (cpuStatFile.is_open()) {
        std::string cpuLine;
        std::getline(cpuStatFile, cpuLine);

        // Parse CPU times (simplified)
        std::istringstream iss(cpuLine);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                        std::istream_iterator<std::string>{}};

        if (tokens.size() > 15) {
            double userTime = std::stod(tokens[13]);
            double sysTime = std::stod(tokens[14]);
            double totalTime = userTime + sysTime;

            if (lastCpuTime > 0) {
                double cpuDelta = totalTime - lastCpuTime;
                metrics.cpuUsage =
                    std::min(100.0, cpuDelta);  // Simplified calculation
            }
            lastCpuTime = totalTime;
        }
    }

    // Update I/O operations count (simplified)
    metrics.ioOperations++;
}

void ScriptMonitorTask::checkResourceLimits(const std::string& scriptId) {
    std::shared_lock lock(metricsMutex_);

    auto metricsIt = metrics_.find(scriptId);
    auto limitsIt = limits_.find(scriptId);

    if (metricsIt == metrics_.end() || limitsIt == limits_.end()) {
        return;
    }

    const auto& metrics = metricsIt->second;
    const auto& limits = limitsIt->second;

    // Check memory limit
    if (metrics.memoryUsage > limits.maxMemoryUsage) {
        if (resourceExceededCallback_) {
            resourceExceededCallback_(
                scriptId, "Memory limit exceeded: " +
                              std::to_string(metrics.memoryUsage) + " > " +
                              std::to_string(limits.maxMemoryUsage));
        }
    }

    // Check CPU limit
    if (metrics.cpuUsage > limits.maxCpuUsage) {
        if (resourceExceededCallback_) {
            resourceExceededCallback_(
                scriptId,
                "CPU limit exceeded: " + std::to_string(metrics.cpuUsage) +
                    "% > " + std::to_string(limits.maxCpuUsage) + "%");
        }
    }
}

MonitoringMetrics ScriptMonitorTask::getMetrics(
    const std::string& scriptId) const {
    std::shared_lock lock(metricsMutex_);

    auto it = metrics_.find(scriptId);
    if (it != metrics_.end()) {
        return it->second;
    }

    return MonitoringMetrics{};
}

void ScriptMonitorTask::setResourceLimits(const std::string& scriptId,
                                          const ResourceLimits& limits) {
    std::unique_lock lock(metricsMutex_);
    limits_[scriptId] = limits;
    addHistoryEntry("Updated resource limits for script: " + scriptId);
}

void ScriptMonitorTask::setResourceExceededCallback(
    std::function<void(const std::string&, const std::string&)> callback) {
    resourceExceededCallback_ = callback;
}

void ScriptMonitorTask::setCompletionCallback(
    std::function<void(const std::string&, const MonitoringMetrics&)>
        callback) {
    completionCallback_ = callback;
}

void ScriptMonitorTask::cleanupMonitoring() {
    std::unique_lock lock(metricsMutex_);

    for (auto& [scriptId, thread] : monitorThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    monitorThreads_.clear();
    addHistoryEntry("Monitoring cleanup completed");
}

// Register with factory
namespace {
static auto monitor_task_registrar = TaskRegistrar<ScriptMonitorTask>(
    "script_monitor",
    TaskInfo{.name = "script_monitor",
             .description =
                 "Monitor script execution with resource limits and alerting",
             .category = "monitoring",
             .requiredParameters = {"scriptId"},
             .parameterSchema =
                 json{{"scriptId",
                       {{"type", "string"},
                        {"description", "ID of script to monitor"}}},
                      {"maxExecutionTime",
                       {{"type", "number"},
                        {"description", "Max execution time (seconds)"},
                        {"default", 300}}},
                      {"maxMemoryUsage",
                       {{"type", "number"},
                        {"description", "Max memory usage (bytes)"},
                        {"default", 1073741824}}},
                      {"maxCpuUsage",
                       {{"type", "number"},
                        {"description", "Max CPU usage (%)"},
                        {"default", 80.0}}},
                      {"monitorInterval",
                       {{"type", "number"},
                        {"description", "Monitor interval (seconds)"},
                        {"default", 1}}},
                      {"alertThresholds",
                       {{"type", "object"},
                        {"description", "Custom alert thresholds"}}}},
             .version = "1.0.0",
             .dependencies = {},
             .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<ScriptMonitorTask> {
        return std::make_unique<ScriptMonitorTask>(name);
    });
}  // namespace

}  // namespace lithium::task::script