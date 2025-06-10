/**
 * @file task_manager.hpp
 * @brief Task Manager for custom task integration and execution
 *
 * This file contains the task manager that integrates custom tasks
 * with the sequencer and provides advanced execution capabilities.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_MANAGER_HPP
#define LITHIUM_TASK_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "task.hpp"
#include "factory.hpp"
#include "generator.hpp"

namespace lithium::sequencer {

using json = nlohmann::json;

/**
 * @brief Task execution context
 */
struct TaskExecutionContext {
    std::string taskId;
    std::string taskType;
    std::string targetName;
    json parameters;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    TaskStatus status = TaskStatus::Pending;
    std::vector<std::string> dependencies;
    int priority = 5;
    int retryCount = 0;
    int maxRetries = 3;
    std::chrono::seconds timeout{30};
    std::function<void(const TaskExecutionContext&)> onComplete;
    std::function<void(const TaskExecutionContext&, const std::exception&)> onError;
};

/**
 * @brief Task execution statistics
 */
struct TaskExecutionStats {
    size_t totalExecuted = 0;
    size_t successfulExecutions = 0;
    size_t failedExecutions = 0;
    size_t retriedExecutions = 0;
    double averageExecutionTime = 0.0;
    std::chrono::system_clock::time_point lastExecutionTime;
};

/**
 * @brief Task dependency graph node
 */
struct DependencyNode {
    std::string taskId;
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;
    bool executed = false;
    bool canExecute = false;
};

/**
 * @brief Task Manager for advanced custom task execution
 */
class TaskManager {
public:
    /**
     * @brief Constructor
     */
    TaskManager();

    /**
     * @brief Destructor
     */
    ~TaskManager();

    // Task registration and creation
    
    /**
     * @brief Register a task type with the factory
     */
    template<typename TaskType>
    void registerTaskType(const std::string& taskType, const TaskInfo& info) {
        TaskFactory::getInstance().registerTask<TaskType>(taskType, 
            [](const std::string& name, const json& config) -> std::unique_ptr<TaskType> {
                return std::make_unique<TaskType>(name, config);
            }, info);
    }

    /**
     * @brief Create a task execution context
     */
    std::string createTaskContext(const std::string& taskType,
                                 const std::string& targetName,
                                 const json& parameters);

    /**
     * @brief Add task dependency
     */
    void addTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId);

    /**
     * @brief Remove task dependency
     */
    void removeTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId);

    /**
     * @brief Set task priority
     */
    void setTaskPriority(const std::string& taskId, int priority);

    /**
     * @brief Set task timeout
     */
    void setTaskTimeout(const std::string& taskId, std::chrono::seconds timeout);

    /**
     * @brief Set task retry policy
     */
    void setTaskRetryPolicy(const std::string& taskId, int maxRetries);

    // Task execution

    /**
     * @brief Execute a single task
     */
    void executeTask(const std::string& taskId);

    /**
     * @brief Execute tasks in dependency order
     */
    void executeTasksInOrder(const std::vector<std::string>& taskIds);

    /**
     * @brief Execute all pending tasks
     */
    void executeAllTasks();

    /**
     * @brief Cancel task execution
     */
    void cancelTask(const std::string& taskId);

    /**
     * @brief Cancel all tasks
     */
    void cancelAllTasks();

    // Task monitoring and control

    /**
     * @brief Get task execution context
     */
    std::optional<TaskExecutionContext> getTaskContext(const std::string& taskId) const;

    /**
     * @brief Get task status
     */
    TaskStatus getTaskStatus(const std::string& taskId) const;

    /**
     * @brief Get all task contexts
     */
    std::vector<TaskExecutionContext> getAllTaskContexts() const;

    /**
     * @brief Get tasks by status
     */
    std::vector<std::string> getTasksByStatus(TaskStatus status) const;

    /**
     * @brief Get task execution statistics
     */
    TaskExecutionStats getExecutionStats() const;

    /**
     * @brief Get dependency graph
     */
    std::unordered_map<std::string, DependencyNode> getDependencyGraph() const;

    // Script integration

    /**
     * @brief Execute tasks from script
     */
    void executeFromScript(const std::string& script);

    /**
     * @brief Generate script from task contexts
     */
    std::string generateScript(const std::vector<std::string>& taskIds) const;

    /**
     * @brief Load task definitions from script
     */
    std::vector<std::string> loadTasksFromScript(const std::string& script);

    // Event handling

    /**
     * @brief Set global task completion callback
     */
    void setTaskCompletionCallback(std::function<void(const TaskExecutionContext&)> callback);

    /**
     * @brief Set global task error callback
     */
    void setTaskErrorCallback(std::function<void(const TaskExecutionContext&, const std::exception&)> callback);

    /**
     * @brief Set task status change callback
     */
    void setTaskStatusCallback(std::function<void(const std::string&, TaskStatus, TaskStatus)> callback);

    // Advanced features

    /**
     * @brief Enable parallel execution
     */
    void setParallelExecution(bool enabled, size_t maxConcurrency = 4);

    /**
     * @brief Set execution scheduler
     */
    void setScheduler(std::function<std::vector<std::string>(const std::vector<std::string>&)> scheduler);

    /**
     * @brief Add execution middleware
     */
    void addMiddleware(std::function<bool(TaskExecutionContext&)> middleware);

    /**
     * @brief Clear all middleware
     */
    void clearMiddleware();

    /**
     * @brief Validate task configuration
     */
    bool validateTaskConfiguration(const std::string& taskType, const json& config) const;

    /**
     * @brief Get available task types
     */
    std::vector<std::string> getAvailableTaskTypes() const;

    /**
     * @brief Clear all tasks
     */
    void clearAllTasks();

    /**
     * @brief Start background execution thread
     */
    void startExecutionService();

    /**
     * @brief Stop background execution thread
     */
    void stopExecutionService();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Task execution queue for background processing
 */
class TaskExecutionQueue {
public:
    TaskExecutionQueue();
    ~TaskExecutionQueue();

    void enqueue(const std::string& taskId, int priority = 5);
    std::optional<std::string> dequeue();
    void clear();
    size_t size() const;
    bool empty() const;

private:
    struct QueueItem {
        std::string taskId;
        int priority;
        std::chrono::system_clock::time_point timestamp;

        bool operator<(const QueueItem& other) const {
            if (priority != other.priority) {
                return priority < other.priority; // Higher priority first
            }
            return timestamp > other.timestamp; // Earlier timestamp first
        }
    };

    mutable std::shared_mutex mutex_;
    std::priority_queue<QueueItem> queue_;
    std::atomic<bool> shutdown_{false};
};

/**
 * @brief Helper functions for task management
 */
namespace TaskUtils {
    /**
     * @brief Generate unique task ID
     */
    std::string generateTaskId();

    /**
     * @brief Validate task dependencies for cycles
     */
    bool validateDependencies(const std::unordered_map<std::string, std::vector<std::string>>& dependencies);

    /**
     * @brief Topological sort of tasks based on dependencies
     */
    std::vector<std::string> topologicalSort(const std::unordered_map<std::string, std::vector<std::string>>& dependencies);

    /**
     * @brief Calculate task execution order
     */
    std::vector<std::string> calculateExecutionOrder(const std::vector<DependencyNode>& nodes);

    /**
     * @brief Merge task execution statistics
     */
    TaskExecutionStats mergeStats(const std::vector<TaskExecutionStats>& stats);
}

} // namespace lithium::sequencer

#endif // LITHIUM_TASK_MANAGER_HPP
