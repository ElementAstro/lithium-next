/*
 * device_task_scheduler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Advanced Device Task Scheduler with optimizations

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace lithium {

// Forward declaration
class AtomDriver;

// Task priority levels
enum class TaskPriority {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3,
    BACKGROUND = 4
};

// Task execution state
enum class TaskState {
    PENDING,
    QUEUED,
    RUNNING,
    SUSPENDED,
    COMPLETED,
    FAILED,
    CANCELLED,
    TIMEOUT
};

// Task execution mode
enum class ExecutionMode {
    SYNCHRONOUS,
    ASYNCHRONOUS,
    DEFERRED,
    PERIODIC,
    CONDITIONAL
};

// Task scheduling policy
enum class SchedulingPolicy {
    FIFO,              // First In, First Out
    PRIORITY,          // Priority-based
    ROUND_ROBIN,       // Round-robin
    SHORTEST_JOB,      // Shortest job first
    DEADLINE,          // Earliest deadline first
    ADAPTIVE           // Adaptive based on load
};

// Task dependency type
enum class DependencyType {
    HARD,              // Must complete successfully
    SOFT,              // Should complete, but failure is acceptable
    CONDITIONAL,       // Conditional execution based on result
    ORDERING           // Just for ordering, no result dependency
};

// Device task definition
struct DeviceTask {
    std::string task_id;
    std::string device_name;
    std::string task_name;
    std::string description;

    TaskPriority priority{TaskPriority::NORMAL};
    ExecutionMode execution_mode{ExecutionMode::ASYNCHRONOUS};
    TaskState state{TaskState::PENDING};

    std::function<bool(std::shared_ptr<AtomDriver>)> task_function;
    std::function<void(const std::string&, TaskState, const std::string&)> completion_callback;
    std::function<void(const std::string&, double)> progress_callback;

    // Timing constraints
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point scheduled_at;
    std::chrono::system_clock::time_point deadline;
    std::chrono::milliseconds estimated_duration{0};
    std::chrono::milliseconds max_execution_time{300000}; // 5 minutes default

    // Resource requirements
    double cpu_requirement{1.0};
    size_t memory_requirement{100}; // MB
    bool requires_exclusive_access{false};
    std::vector<std::string> required_capabilities;

    // Retry configuration
    size_t max_retries{3};
    size_t retry_count{0};
    std::chrono::milliseconds retry_delay{1000};
    double retry_backoff_factor{2.0};

    // Dependencies
    std::vector<std::pair<std::string, DependencyType>> dependencies;
    std::vector<std::string> dependents;

    // Execution context
    std::string execution_context;
    std::unordered_map<std::string, std::string> parameters;

    // Statistics
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds actual_duration{0};
    std::string error_message;
    double progress{0.0};
};

// Task execution result
struct TaskResult {
    std::string task_id;
    TaskState final_state;
    bool success;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    std::chrono::system_clock::time_point completed_at;
    std::unordered_map<std::string, std::string> output_data;
};

// Scheduler configuration
struct SchedulerConfig {
    SchedulingPolicy policy{SchedulingPolicy::PRIORITY};
    size_t max_concurrent_tasks{10};
    size_t max_queue_size{1000};
    size_t worker_thread_count{4};

    std::chrono::milliseconds scheduling_interval{100};
    std::chrono::milliseconds health_check_interval{30000};
    std::chrono::milliseconds task_timeout{300000};

    bool enable_task_preemption{false};
    bool enable_load_balancing{true};
    bool enable_task_migration{false};
    bool enable_priority_aging{true};

    double cpu_threshold{0.8};
    double memory_threshold{0.8};
    size_t queue_threshold{800};

    // Advanced features
    bool enable_task_prediction{true};
    bool enable_adaptive_scheduling{true};
    bool enable_resource_aware_scheduling{true};
    bool enable_deadline_awareness{true};
};

// Scheduler statistics
struct SchedulerStatistics {
    size_t total_tasks{0};
    size_t completed_tasks{0};
    size_t failed_tasks{0};
    size_t cancelled_tasks{0};
    size_t timeout_tasks{0};

    size_t queued_tasks{0};
    size_t running_tasks{0};
    size_t pending_tasks{0};

    std::chrono::milliseconds average_wait_time{0};
    std::chrono::milliseconds average_execution_time{0};
    std::chrono::milliseconds total_processing_time{0};

    double throughput{0.0}; // tasks per second
    double utilization{0.0}; // percentage
    double success_rate{0.0}; // percentage

    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;

    std::unordered_map<TaskPriority, size_t> tasks_by_priority;
    std::unordered_map<std::string, size_t> tasks_by_device;
};

class DeviceTaskScheduler {
public:
    DeviceTaskScheduler();
    explicit DeviceTaskScheduler(const SchedulerConfig& config);
    ~DeviceTaskScheduler();

    // Configuration
    void setConfiguration(const SchedulerConfig& config);
    SchedulerConfig getConfiguration() const;

    // Scheduler lifecycle
    void start();
    void stop();
    void pause();
    void resume();
    bool isRunning() const;

    // Task submission
    std::string submitTask(const DeviceTask& task);
    std::vector<std::string> submitTaskBatch(const std::vector<DeviceTask>& tasks);

    // Task management
    bool cancelTask(const std::string& task_id);
    bool suspendTask(const std::string& task_id);
    bool resumeTask(const std::string& task_id);
    bool rescheduleTask(const std::string& task_id, std::chrono::system_clock::time_point new_time);

    // Task dependency management
    void addTaskDependency(const std::string& task_id, const std::string& dependency_id, DependencyType type);
    void removeTaskDependency(const std::string& task_id, const std::string& dependency_id);
    std::vector<std::string> getTaskDependencies(const std::string& task_id) const;
    std::vector<std::string> getTaskDependents(const std::string& task_id) const;

    // Task querying
    DeviceTask getTask(const std::string& task_id) const;
    std::vector<DeviceTask> getAllTasks() const;
    std::vector<DeviceTask> getTasksByState(TaskState state) const;
    std::vector<DeviceTask> getTasksByDevice(const std::string& device_name) const;
    std::vector<DeviceTask> getTasksByPriority(TaskPriority priority) const;

    // Task execution control
    void setTaskPriority(const std::string& task_id, TaskPriority priority);
    TaskPriority getTaskPriority(const std::string& task_id) const;

    void setMaxConcurrentTasks(size_t max_tasks);
    size_t getMaxConcurrentTasks() const;

    // Device management
    void registerDevice(const std::string& device_name, std::shared_ptr<AtomDriver> device);
    void unregisterDevice(const std::string& device_name);
    bool isDeviceRegistered(const std::string& device_name) const;

    void setDeviceCapacity(const std::string& device_name, size_t max_concurrent_tasks);
    size_t getDeviceCapacity(const std::string& device_name) const;

    // Load balancing
    void enableLoadBalancing(bool enable);
    bool isLoadBalancingEnabled() const;

    std::string selectOptimalDevice(const DeviceTask& task) const;
    void redistributeLoad();

    // Resource management
    void setResourceLimit(const std::string& resource_type, double limit);
    double getResourceLimit(const std::string& resource_type) const;
    double getCurrentResourceUsage(const std::string& resource_type) const;

    // Scheduling policies
    void setSchedulingPolicy(SchedulingPolicy policy);
    SchedulingPolicy getSchedulingPolicy() const;

    // Performance optimization
    void enableAdaptiveScheduling(bool enable);
    bool isAdaptiveSchedulingEnabled() const;

    void enableTaskPrediction(bool enable);
    bool isTaskPredictionEnabled() const;

    struct OptimizationSuggestion {
        std::string category;
        std::string suggestion;
        std::string rationale;
        double expected_improvement;
        int priority;
    };

    std::vector<OptimizationSuggestion> getOptimizationSuggestions() const;
    void applyOptimization(const OptimizationSuggestion& suggestion);

    // Statistics and monitoring
    SchedulerStatistics getStatistics() const;
    SchedulerStatistics getDeviceStatistics(const std::string& device_name) const;

    TaskResult getTaskResult(const std::string& task_id) const;
    std::vector<TaskResult> getCompletedTaskResults(size_t limit = 100) const;

    // Event callbacks
    using TaskEventCallback = std::function<void(const std::string&, TaskState, const std::string&)>;
    using SchedulerEventCallback = std::function<void(const std::string&, const std::string&)>;

    void setTaskStateChangedCallback(TaskEventCallback callback);
    void setTaskCompletedCallback(TaskEventCallback callback);
    void setSchedulerEventCallback(SchedulerEventCallback callback);

    // Workflow support
    std::string createWorkflow(const std::string& workflow_name, const std::vector<DeviceTask>& tasks);
    bool executeWorkflow(const std::string& workflow_id);
    void cancelWorkflow(const std::string& workflow_id);

    // Advanced scheduling features

    // Deadline-aware scheduling
    void enableDeadlineAwareness(bool enable);
    bool isDeadlineAwarenessEnabled() const;
    std::vector<std::string> getTasksNearDeadline(std::chrono::milliseconds threshold) const;

    // Task preemption
    void enableTaskPreemption(bool enable);
    bool isTaskPreemptionEnabled() const;
    void preemptTask(const std::string& task_id);

    // Task migration
    void enableTaskMigration(bool enable);
    bool isTaskMigrationEnabled() const;
    bool migrateTask(const std::string& task_id, const std::string& target_device);

    // Priority aging
    void enablePriorityAging(bool enable);
    bool isPriorityAgingEnabled() const;
    void setAgingFactor(double factor);

    // Batch processing
    void enableBatchProcessing(bool enable);
    bool isBatchProcessingEnabled() const;
    void setBatchSize(size_t size);
    void setBatchTimeout(std::chrono::milliseconds timeout);

    // Debugging and diagnostics
    std::string getSchedulerStatus() const;
    std::string getTaskInfo(const std::string& task_id) const;
    void dumpSchedulerState(const std::string& output_path) const;

    // Maintenance
    void runMaintenance();
    void cleanupCompletedTasks(std::chrono::milliseconds age_threshold);
    void resetStatistics();
    void validateTaskIntegrity();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Internal scheduling methods
    void schedulingLoop();
    void selectAndExecuteNextTask();
    std::string selectNextTask();

    // Task execution
    void executeTask(const DeviceTask& task);
    void handleTaskCompletion(const std::string& task_id, const TaskResult& result);
    void handleTaskFailure(const std::string& task_id, const std::string& error);

    // Dependency management
    bool areDependenciesSatisfied(const std::string& task_id) const;
    void updateDependentTasks(const std::string& completed_task_id, bool success);
    std::vector<std::string> topologicalSort(const std::vector<std::string>& task_ids) const;

    // Resource management
    bool checkResourceAvailability(const DeviceTask& task) const;
    void allocateResources(const DeviceTask& task);
    void releaseResources(const DeviceTask& task);

    // Performance analysis
    void updatePerformanceMetrics();
    void predictTaskDuration(DeviceTask& task) const;
    void analyzeSchedulingEfficiency();

    // Adaptive scheduling
    void adjustSchedulingParameters();
    void updateLoadBalancingWeights();
    void optimizeQueueManagement();
};

// Utility functions
namespace scheduler_utils {
    std::string generateTaskId();
    std::string generateWorkflowId();

    std::string formatTaskInfo(const DeviceTask& task);
    std::string formatSchedulerStatistics(const SchedulerStatistics& stats);

    double calculateTaskUrgency(const DeviceTask& task);
    double calculateTaskComplexity(const DeviceTask& task);

    // Task planning utilities
    std::vector<DeviceTask> createTaskChain(const std::vector<std::function<bool(std::shared_ptr<AtomDriver>)>>& functions,
                                          const std::string& device_name);

    std::vector<DeviceTask> createParallelTasks(const std::vector<std::function<bool(std::shared_ptr<AtomDriver>)>>& functions,
                                               const std::vector<std::string>& device_names);

    // Scheduling analysis
    double calculateSchedulingEfficiency(const SchedulerStatistics& stats);
    double calculateResourceUtilization(const SchedulerStatistics& stats);
    std::vector<std::string> identifyBottlenecks(const SchedulerStatistics& stats);
}

} // namespace lithium
