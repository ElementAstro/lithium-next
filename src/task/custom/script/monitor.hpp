#ifndef LITHIUM_TASK_SCRIPT_MONITOR_TASK_HPP
#define LITHIUM_TASK_SCRIPT_MONITOR_TASK_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include "task/task.hpp"

namespace lithium::task::script {

/**
 * @struct MonitoringMetrics
 * @brief Stores real-time monitoring metrics for a script execution.
 *
 * This structure holds various resource usage statistics and outputs
 * collected during the execution of a script, such as execution time,
 * memory usage, CPU usage, I/O operations, and output logs.
 */
struct MonitoringMetrics {
    std::chrono::milliseconds executionTime{
        0};                            ///< Total execution time of the script.
    size_t memoryUsage{0};             ///< Memory usage in bytes.
    double cpuUsage{0.0};              ///< CPU usage as a percentage.
    size_t ioOperations{0};            ///< Number of I/O operations performed.
    std::vector<std::string> outputs;  ///< Collected output logs or messages.
};

/**
 * @struct ResourceLimits
 * @brief Defines resource usage limits for script execution.
 *
 * This structure specifies the maximum allowed values for execution time,
 * memory usage, and CPU usage. If any of these limits are exceeded,
 * the monitor can trigger alerts or take action.
 */
struct ResourceLimits {
    std::chrono::seconds maxExecutionTime{
        300};  ///< Maximum allowed execution time (default: 300s).
    size_t maxMemoryUsage{
        1024 * 1024 *
        1024};  ///< Maximum allowed memory usage in bytes (default: 1GB).
    double maxCpuUsage{
        80.0};  ///< Maximum allowed CPU usage as a percentage (default: 80%).
};

/**
 * @class ScriptMonitorTask
 * @brief Monitors script execution, enforces resource limits, and provides
 * alerting.
 *
 * ScriptMonitorTask runs in parallel with script execution to collect resource
 * usage metrics (such as memory, CPU, and I/O), enforce resource limits, and
 * trigger user-defined callbacks when limits are exceeded or execution
 * completes.
 *
 * Typical usage:
 *   - Start monitoring a script by calling startMonitoring().
 *   - Periodically retrieve metrics with getMetrics().
 *   - Set alert or completion callbacks as needed.
 *   - Stop monitoring with stopMonitoring().
 */
class ScriptMonitorTask : public Task {
public:
    /**
     * @brief Constructs a ScriptMonitorTask with the given name.
     * @param name The name of the monitor task.
     */
    ScriptMonitorTask(const std::string& name);

    /**
     * @brief Destructor. Cleans up monitoring threads and resources.
     */
    ~ScriptMonitorTask();

    /**
     * @brief Executes the monitor task with the provided parameters.
     * @param params JSON object containing monitoring configuration.
     */
    void execute(const json& params) override;

    /**
     * @brief Starts monitoring a script with specified resource limits.
     * @param scriptId The unique identifier of the script to monitor.
     * @param limits ResourceLimits specifying thresholds for monitoring.
     */
    void startMonitoring(const std::string& scriptId,
                         const ResourceLimits& limits);

    /**
     * @brief Stops monitoring the specified script.
     * @param scriptId The unique identifier of the script to stop monitoring.
     */
    void stopMonitoring(const std::string& scriptId);

    /**
     * @brief Retrieves the current monitoring metrics for a script.
     * @param scriptId The unique identifier of the script.
     * @return MonitoringMetrics structure with the latest statistics.
     */
    MonitoringMetrics getMetrics(const std::string& scriptId) const;

    /**
     * @brief Updates the resource limits for a monitored script.
     * @param scriptId The unique identifier of the script.
     * @param limits New resource limits to enforce.
     */
    void setResourceLimits(const std::string& scriptId,
                           const ResourceLimits& limits);

    /**
     * @brief Sets a callback to be invoked when a resource limit is exceeded.
     * @param callback Function to call with scriptId and error message.
     */
    void setResourceExceededCallback(
        std::function<void(const std::string&, const std::string&)> callback);

    /**
     * @brief Sets a callback to be invoked when script execution completes.
     * @param callback Function to call with scriptId and final metrics.
     */
    void setCompletionCallback(
        std::function<void(const std::string&, const MonitoringMetrics&)>
            callback);

protected:
    /**
     * @brief Internal monitoring loop for a script.
     * @param scriptId The unique identifier of the script being monitored.
     */
    void monitorScript(const std::string& scriptId);

    /**
     * @brief Checks if any resource limits have been exceeded for a script.
     * @param scriptId The unique identifier of the script.
     */
    void checkResourceLimits(const std::string& scriptId);

    /**
     * @brief Updates the collected metrics for a script.
     * @param scriptId The unique identifier of the script.
     */
    void updateMetrics(const std::string& scriptId);

private:
    /**
     * @brief Sets up default parameter definitions and monitoring properties.
     */
    void setupMonitoringDefaults();

    /**
     * @brief Cleans up all monitoring threads and resources.
     */
    void cleanupMonitoring();

    std::map<std::string, std::unique_ptr<std::thread>>
        monitorThreads_;  ///< Active monitoring threads per script.
    std::map<std::string, MonitoringMetrics>
        metrics_;  ///< Collected metrics per script.
    std::map<std::string, ResourceLimits>
        limits_;  ///< Resource limits per script.
    std::atomic<bool> shouldStop_{
        false};  ///< Flag to signal monitoring shutdown.

    mutable std::shared_mutex
        metricsMutex_;  ///< Mutex for thread-safe metrics access.

    std::function<void(const std::string&, const std::string&)>
        resourceExceededCallback_;  ///< Callback for resource limit exceeded
                                    ///< events.
    std::function<void(const std::string&, const MonitoringMetrics&)>
        completionCallback_;  ///< Callback for script completion events.
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_SCRIPT_MONITOR_TASK_HPP
