/**
 * @file task_manager.cpp
 * @brief Task Manager implementation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "task_manager.hpp"
#include "atom/log/loguru.hpp"
#include "atom/utils/uuid.hpp"
#include <algorithm>
#include <future>
#include <unordered_set>

namespace lithium::sequencer {

class TaskManager::Impl {
public:
    Impl();
    ~Impl();

    std::string createTaskContext(const std::string& taskType,
                                 const std::string& targetName,
                                 const json& parameters);
    void addTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId);
    void removeTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId);
    void setTaskPriority(const std::string& taskId, int priority);
    void setTaskTimeout(const std::string& taskId, std::chrono::seconds timeout);
    void setTaskRetryPolicy(const std::string& taskId, int maxRetries);

    void executeTask(const std::string& taskId);
    void executeTasksInOrder(const std::vector<std::string>& taskIds);
    void executeAllTasks();
    void cancelTask(const std::string& taskId);
    void cancelAllTasks();

    std::optional<TaskExecutionContext> getTaskContext(const std::string& taskId) const;
    TaskStatus getTaskStatus(const std::string& taskId) const;
    std::vector<TaskExecutionContext> getAllTaskContexts() const;
    std::vector<std::string> getTasksByStatus(TaskStatus status) const;
    TaskExecutionStats getExecutionStats() const;
    std::unordered_map<std::string, DependencyNode> getDependencyGraph() const;

    void executeFromScript(const std::string& script);
    std::string generateScript(const std::vector<std::string>& taskIds) const;
    std::vector<std::string> loadTasksFromScript(const std::string& script);

    void setTaskCompletionCallback(std::function<void(const TaskExecutionContext&)> callback);
    void setTaskErrorCallback(std::function<void(const TaskExecutionContext&, const std::exception&)> callback);
    void setTaskStatusCallback(std::function<void(const std::string&, TaskStatus, TaskStatus)> callback);

    void setParallelExecution(bool enabled, size_t maxConcurrency);
    void setScheduler(std::function<std::vector<std::string>(const std::vector<std::string>&)> scheduler);
    void addMiddleware(std::function<bool(TaskExecutionContext&)> middleware);
    void clearMiddleware();

    bool validateTaskConfiguration(const std::string& taskType, const json& config) const;
    std::vector<std::string> getAvailableTaskTypes() const;
    void clearAllTasks();

    void startExecutionService();
    void stopExecutionService();

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, TaskExecutionContext> taskContexts_;
    std::unordered_map<std::string, DependencyNode> dependencyGraph_;
    TaskExecutionStats stats_;
    
    // Callbacks
    std::function<void(const TaskExecutionContext&)> completionCallback_;
    std::function<void(const TaskExecutionContext&, const std::exception&)> errorCallback_;
    std::function<void(const std::string&, TaskStatus, TaskStatus)> statusCallback_;
    
    // Execution settings
    bool parallelExecution_ = false;
    size_t maxConcurrency_ = 4;
    std::function<std::vector<std::string>(const std::vector<std::string>&)> scheduler_;
    std::vector<std::function<bool(TaskExecutionContext&)>> middleware_;
    
    // Background execution
    std::unique_ptr<TaskExecutionQueue> executionQueue_;
    std::thread executionThread_;
    std::atomic<bool> serviceRunning_{false};
    std::atomic<bool> shouldStop_{false};
    
    // Helper methods
    void executeTaskInternal(TaskExecutionContext& context);
    void updateTaskStatus(const std::string& taskId, TaskStatus oldStatus, TaskStatus newStatus);
    bool canExecuteTask(const std::string& taskId) const;
    void updateDependencyGraph();
    std::vector<std::string> getExecutableTask() const;
    void retryTask(const std::string& taskId);
};

TaskManager::Impl::Impl() 
    : executionQueue_(std::make_unique<TaskExecutionQueue>()) {
}

TaskManager::Impl::~Impl() {
    stopExecutionService();
}

std::string TaskManager::Impl::createTaskContext(const std::string& taskType,
                                                const std::string& targetName,
                                                const json& parameters) {
    std::unique_lock lock(mutex_);
    
    std::string taskId = TaskUtils::generateTaskId();
    
    TaskExecutionContext context;
    context.taskId = taskId;
    context.taskType = taskType;
    context.targetName = targetName;
    context.parameters = parameters;
    context.status = TaskStatus::Pending;
    
    taskContexts_[taskId] = context;
    
    // Initialize dependency node
    DependencyNode node;
    node.taskId = taskId;
    node.executed = false;
    node.canExecute = true; // Will be updated by updateDependencyGraph
    dependencyGraph_[taskId] = node;
    
    LOG_F(INFO, "Created task context: {} (type: {}, target: {})", taskId, taskType, targetName);
    
    return taskId;
}

void TaskManager::Impl::addTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId) {
    std::unique_lock lock(mutex_);
    
    auto taskIt = dependencyGraph_.find(taskId);
    auto depIt = dependencyGraph_.find(dependsOnTaskId);
    
    if (taskIt != dependencyGraph_.end() && depIt != dependencyGraph_.end()) {
        taskIt->second.dependencies.push_back(dependsOnTaskId);
        depIt->second.dependents.push_back(taskId);
        updateDependencyGraph();
        
        LOG_F(INFO, "Added dependency: {} depends on {}", taskId, dependsOnTaskId);
    } else {
        LOG_F(WARNING, "Cannot add dependency: task not found");
    }
}

void TaskManager::Impl::removeTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId) {
    std::unique_lock lock(mutex_);
    
    auto taskIt = dependencyGraph_.find(taskId);
    auto depIt = dependencyGraph_.find(dependsOnTaskId);
    
    if (taskIt != dependencyGraph_.end() && depIt != dependencyGraph_.end()) {
        auto& deps = taskIt->second.dependencies;
        deps.erase(std::remove(deps.begin(), deps.end(), dependsOnTaskId), deps.end());
        
        auto& dependents = depIt->second.dependents;
        dependents.erase(std::remove(dependents.begin(), dependents.end(), taskId), dependents.end());
        
        updateDependencyGraph();
        
        LOG_F(INFO, "Removed dependency: {} no longer depends on {}", taskId, dependsOnTaskId);
    }
}

void TaskManager::Impl::setTaskPriority(const std::string& taskId, int priority) {
    std::unique_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it != taskContexts_.end()) {
        it->second.priority = priority;
        LOG_F(INFO, "Set task {} priority to {}", taskId, priority);
    }
}

void TaskManager::Impl::setTaskTimeout(const std::string& taskId, std::chrono::seconds timeout) {
    std::unique_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it != taskContexts_.end()) {
        it->second.timeout = timeout;
        LOG_F(INFO, "Set task {} timeout to {} seconds", taskId, timeout.count());
    }
}

void TaskManager::Impl::setTaskRetryPolicy(const std::string& taskId, int maxRetries) {
    std::unique_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it != taskContexts_.end()) {
        it->second.maxRetries = maxRetries;
        LOG_F(INFO, "Set task {} max retries to {}", taskId, maxRetries);
    }
}

void TaskManager::Impl::executeTask(const std::string& taskId) {
    std::unique_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it == taskContexts_.end()) {
        LOG_F(ERROR, "Task not found: {}", taskId);
        return;
    }
    
    if (!canExecuteTask(taskId)) {
        LOG_F(WARNING, "Cannot execute task {} - dependencies not satisfied", taskId);
        return;
    }
    
    TaskExecutionContext context = it->second; // Copy for thread safety
    lock.unlock();
    
    executeTaskInternal(context);
}

void TaskManager::Impl::executeTaskInternal(TaskExecutionContext& context) {
    const std::string& taskId = context.taskId;
    
    try {
        updateTaskStatus(taskId, context.status, TaskStatus::InProgress);
        context.startTime = std::chrono::system_clock::now();
        
        // Apply middleware
        for (const auto& middleware : middleware_) {
            if (!middleware(context)) {
                LOG_F(WARNING, "Middleware rejected task execution: {}", taskId);
                updateTaskStatus(taskId, TaskStatus::InProgress, TaskStatus::Failed);
                return;
            }
        }
        
        // Create and execute the task
        auto task = TaskFactory::getInstance().createTask(context.taskType, taskId, context.parameters);
        if (!task) {
            throw std::runtime_error("Failed to create task of type: " + context.taskType);
        }
        
        // Set timeout
        task->setTimeout(context.timeout);
        
        // Execute the task
        task->execute(context.parameters);
        
        context.endTime = std::chrono::system_clock::now();
        updateTaskStatus(taskId, TaskStatus::InProgress, TaskStatus::Completed);
        
        // Update statistics
        {
            std::unique_lock lock(mutex_);
            stats_.totalExecuted++;
            stats_.successfulExecutions++;
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                context.endTime - context.startTime).count();
            stats_.averageExecutionTime = (stats_.averageExecutionTime * (stats_.totalExecuted - 1) + duration) / stats_.totalExecuted;
            stats_.lastExecutionTime = context.endTime;
            
            // Update context
            taskContexts_[taskId] = context;
            
            // Mark as executed in dependency graph
            auto depIt = dependencyGraph_.find(taskId);
            if (depIt != dependencyGraph_.end()) {
                depIt->second.executed = true;
                updateDependencyGraph();
            }
        }
        
        // Call completion callback
        if (completionCallback_) {
            completionCallback_(context);
        }
        
        LOG_F(INFO, "Task {} completed successfully", taskId);
        
    } catch (const std::exception& e) {
        context.endTime = std::chrono::system_clock::now();
        
        LOG_F(ERROR, "Task {} failed: {}", taskId, e.what());
        
        // Handle retry logic
        if (context.retryCount < context.maxRetries) {
            context.retryCount++;
            {
                std::unique_lock lock(mutex_);
                stats_.retriedExecutions++;
                taskContexts_[taskId] = context;
            }
            
            LOG_F(INFO, "Retrying task {} (attempt {}/{})", taskId, context.retryCount, context.maxRetries);
            
            // Schedule retry
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Basic retry delay
            executeTaskInternal(context);
            return;
        }
        
        updateTaskStatus(taskId, TaskStatus::InProgress, TaskStatus::Failed);
        
        // Update statistics
        {
            std::unique_lock lock(mutex_);
            stats_.totalExecuted++;
            stats_.failedExecutions++;
            taskContexts_[taskId] = context;
        }
        
        // Call error callback
        if (errorCallback_) {
            errorCallback_(context, e);
        }
    }
}

void TaskManager::Impl::updateTaskStatus(const std::string& taskId, TaskStatus oldStatus, TaskStatus newStatus) {
    {
        std::unique_lock lock(mutex_);
        auto it = taskContexts_.find(taskId);
        if (it != taskContexts_.end()) {
            it->second.status = newStatus;
        }
    }
    
    if (statusCallback_) {
        statusCallback_(taskId, oldStatus, newStatus);
    }
    
    LOG_F(INFO, "Task {} status changed: {} -> {}", taskId, static_cast<int>(oldStatus), static_cast<int>(newStatus));
}

bool TaskManager::Impl::canExecuteTask(const std::string& taskId) const {
    auto depIt = dependencyGraph_.find(taskId);
    if (depIt == dependencyGraph_.end()) {
        return false;
    }
    
    // Check if all dependencies are completed
    for (const auto& depId : depIt->second.dependencies) {
        auto contextIt = taskContexts_.find(depId);
        if (contextIt == taskContexts_.end() || contextIt->second.status != TaskStatus::Completed) {
            return false;
        }
    }
    
    return true;
}

void TaskManager::Impl::updateDependencyGraph() {
    for (auto& [taskId, node] : dependencyGraph_) {
        node.canExecute = canExecuteTask(taskId);
    }
}

std::vector<std::string> TaskManager::Impl::getExecutableTask() const {
    std::vector<std::string> executable;
    
    for (const auto& [taskId, node] : dependencyGraph_) {
        if (node.canExecute && !node.executed) {
            auto contextIt = taskContexts_.find(taskId);
            if (contextIt != taskContexts_.end() && contextIt->second.status == TaskStatus::Pending) {
                executable.push_back(taskId);
            }
        }
    }
    
    return executable;
}

std::optional<TaskExecutionContext> TaskManager::Impl::getTaskContext(const std::string& taskId) const {
    std::shared_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it != taskContexts_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

TaskStatus TaskManager::Impl::getTaskStatus(const std::string& taskId) const {
    std::shared_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it != taskContexts_.end()) {
        return it->second.status;
    }
    
    return TaskStatus::Failed; // Default for non-existent tasks
}

std::vector<TaskExecutionContext> TaskManager::Impl::getAllTaskContexts() const {
    std::shared_lock lock(mutex_);
    
    std::vector<TaskExecutionContext> contexts;
    contexts.reserve(taskContexts_.size());
    
    for (const auto& [taskId, context] : taskContexts_) {
        contexts.push_back(context);
    }
    
    return contexts;
}

std::vector<std::string> TaskManager::Impl::getTasksByStatus(TaskStatus status) const {
    std::shared_lock lock(mutex_);
    
    std::vector<std::string> taskIds;
    
    for (const auto& [taskId, context] : taskContexts_) {
        if (context.status == status) {
            taskIds.push_back(taskId);
        }
    }
    
    return taskIds;
}

TaskExecutionStats TaskManager::Impl::getExecutionStats() const {
    std::shared_lock lock(mutex_);
    return stats_;
}

std::unordered_map<std::string, DependencyNode> TaskManager::Impl::getDependencyGraph() const {
    std::shared_lock lock(mutex_);
    return dependencyGraph_;
}

void TaskManager::Impl::setTaskCompletionCallback(std::function<void(const TaskExecutionContext&)> callback) {
    completionCallback_ = std::move(callback);
}

void TaskManager::Impl::setTaskErrorCallback(std::function<void(const TaskExecutionContext&, const std::exception&)> callback) {
    errorCallback_ = std::move(callback);
}

void TaskManager::Impl::setTaskStatusCallback(std::function<void(const std::string&, TaskStatus, TaskStatus)> callback) {
    statusCallback_ = std::move(callback);
}

bool TaskManager::Impl::validateTaskConfiguration(const std::string& taskType, const json& config) const {
    return TaskFactory::getInstance().validateTaskParameters(taskType, config);
}

std::vector<std::string> TaskManager::Impl::getAvailableTaskTypes() const {
    return TaskFactory::getInstance().getRegisteredTaskTypes();
}

void TaskManager::Impl::clearAllTasks() {
    std::unique_lock lock(mutex_);
    taskContexts_.clear();
    dependencyGraph_.clear();
    stats_ = TaskExecutionStats{};
    LOG_F(INFO, "Cleared all tasks");
}

void TaskManager::Impl::executeTasksInOrder(const std::vector<std::string>& taskIds) {
    if (parallelExecution_) {
        // Parallel execution with dependency resolution
        std::vector<std::string> orderedTasks = taskIds;
        
        // Apply custom scheduler if available
        if (scheduler_) {
            orderedTasks = scheduler_(taskIds);
        }
        
        // Execute in batches respecting dependencies
        std::unordered_set<std::string> completed;
        
        while (completed.size() < orderedTasks.size()) {
            std::vector<std::future<void>> futures;
            std::vector<std::string> currentBatch;
            
            // Find tasks that can be executed now
            for (const auto& taskId : orderedTasks) {
                if (completed.count(taskId)) continue;
                
                if (canExecuteTask(taskId) && currentBatch.size() < maxConcurrency_) {
                    currentBatch.push_back(taskId);
                }
            }
            
            if (currentBatch.empty() && completed.size() < orderedTasks.size()) {
                // Deadlock detection - shouldn't happen with proper validation
                LOG_F(ERROR, "Potential deadlock detected in task execution");
                break;
            }
            
            // Execute current batch in parallel
            for (const auto& taskId : currentBatch) {
                futures.emplace_back(std::async(std::launch::async, [this, taskId]() {
                    executeTask(taskId);
                }));
            }
            
            // Wait for current batch to complete
            for (auto& future : futures) {
                future.wait();
            }
            
            // Mark completed tasks
            for (const auto& taskId : currentBatch) {
                if (getTaskStatus(taskId) == TaskStatus::Completed) {
                    completed.insert(taskId);
                }
            }
        }
    } else {
        // Sequential execution
        for (const auto& taskId : taskIds) {
            if (shouldStop_) break;
            executeTask(taskId);
        }
    }
}

void TaskManager::Impl::executeAllTasks() {
    auto executableTasks = getExecutableTask();
    
    if (executableTasks.empty()) {
        LOG_F(INFO, "No executable tasks found");
        return;
    }
    
    // Apply topological sorting for dependency resolution
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    for (const auto& [taskId, node] : dependencyGraph_) {
        dependencies[taskId] = node.dependencies;
    }
    
    auto sortedTasks = TaskUtils::topologicalSort(dependencies);
    
    // Filter to only include tasks that are actually in our executable list
    std::vector<std::string> orderedExecutableTasks;
    std::unordered_set<std::string> executableSet(executableTasks.begin(), executableTasks.end());
    
    for (const auto& taskId : sortedTasks) {
        if (executableSet.count(taskId)) {
            orderedExecutableTasks.push_back(taskId);
        }
    }
    
    executeTasksInOrder(orderedExecutableTasks);
}

void TaskManager::Impl::cancelTask(const std::string& taskId) {
    std::unique_lock lock(mutex_);
    
    auto it = taskContexts_.find(taskId);
    if (it == taskContexts_.end()) {
        LOG_F(WARNING, "Cannot cancel task {}: not found", taskId);
        return;
    }
    
    TaskStatus currentStatus = it->second.status;
    
    if (currentStatus == TaskStatus::Completed || currentStatus == TaskStatus::Failed) {
        LOG_F(INFO, "Task {} already finished, cannot cancel", taskId);
        return;
    }
    
    // Mark as cancelled and update dependency graph
    updateTaskStatus(taskId, currentStatus, TaskStatus::Failed);
    
    auto depIt = dependencyGraph_.find(taskId);
    if (depIt != dependencyGraph_.end()) {
        depIt->second.executed = false;
        depIt->second.canExecute = false;
        updateDependencyGraph();
    }
    
    // Update statistics
    stats_.failedExecutions++;
    
    LOG_F(INFO, "Task {} cancelled", taskId);
}

void TaskManager::Impl::cancelAllTasks() {
    shouldStop_ = true;
    
    std::unique_lock lock(mutex_);
    
    std::vector<std::string> tasksToCancel;
    for (const auto& [taskId, context] : taskContexts_) {
        if (context.status == TaskStatus::Pending || context.status == TaskStatus::InProgress) {
            tasksToCancel.push_back(taskId);
        }
    }
    
    lock.unlock();
    
    for (const auto& taskId : tasksToCancel) {
        cancelTask(taskId);
    }
    
    LOG_F(INFO, "Cancelled {} tasks", tasksToCancel.size());
}

void TaskManager::Impl::executeFromScript(const std::string& script) {
    // Parse script and create task contexts
    auto taskIds = loadTasksFromScript(script);
    executeTasksInOrder(taskIds);
}

std::string TaskManager::Impl::generateScript(const std::vector<std::string>& taskIds) const {
    json script = json::array();
    
    std::shared_lock lock(mutex_);
    for (const auto& taskId : taskIds) {
        auto it = taskContexts_.find(taskId);
        if (it != taskContexts_.end()) {
            json taskJson;
            taskJson["id"] = taskId;
            taskJson["type"] = it->second.taskType;
            taskJson["target"] = it->second.targetName;
            taskJson["parameters"] = it->second.parameters;
            script.push_back(taskJson);
        }
    }
    
    return script.dump(2);
}

std::vector<std::string> TaskManager::Impl::loadTasksFromScript(const std::string& script) {
    std::vector<std::string> taskIds;
    
    try {
        auto scriptJson = json::parse(script);
        
        if (scriptJson.is_array()) {
            for (const auto& taskDef : scriptJson) {
                if (taskDef.contains("type") && taskDef.contains("parameters")) {
                    std::string taskId = createTaskContext(
                        taskDef["type"],
                        taskDef.value("target", ""),
                        taskDef["parameters"]
                    );
                    taskIds.push_back(taskId);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to load tasks from script: {}", e.what());
    }
    
    return taskIds;
}

void TaskManager::Impl::setParallelExecution(bool enabled, size_t maxConcurrency) {
    parallelExecution_ = enabled;
    maxConcurrency_ = maxConcurrency;
}

void TaskManager::Impl::setScheduler(std::function<std::vector<std::string>(const std::vector<std::string>&)> scheduler) {
    scheduler_ = std::move(scheduler);
}

void TaskManager::Impl::addMiddleware(std::function<bool(TaskExecutionContext&)> middleware) {
    middleware_.push_back(std::move(middleware));
}

void TaskManager::Impl::clearMiddleware() {
    middleware_.clear();
}

void TaskManager::Impl::startExecutionService() {
    if (serviceRunning_) {
        return;
    }
    
    serviceRunning_ = true;
    shouldStop_ = false;
    
    executionThread_ = std::thread([this]() {
        while (serviceRunning_ && !shouldStop_) {
            auto taskId = executionQueue_->dequeue();
            if (taskId) {
                executeTask(*taskId);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
    
    LOG_F(INFO, "Task execution service started");
}

void TaskManager::Impl::stopExecutionService() {
    if (!serviceRunning_) {
        return;
    }
    
    shouldStop_ = true;
    serviceRunning_ = false;
    
    if (executionThread_.joinable()) {
        executionThread_.join();
    }
    
    LOG_F(INFO, "Task execution service stopped");
}

// TaskManager public interface implementation
TaskManager::TaskManager() : impl_(std::make_unique<Impl>()) {}
TaskManager::~TaskManager() = default;

std::string TaskManager::createTaskContext(const std::string& taskType,
                                         const std::string& targetName,
                                         const json& parameters) {
    return impl_->createTaskContext(taskType, targetName, parameters);
}

void TaskManager::addTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId) {
    impl_->addTaskDependency(taskId, dependsOnTaskId);
}

void TaskManager::removeTaskDependency(const std::string& taskId, const std::string& dependsOnTaskId) {
    impl_->removeTaskDependency(taskId, dependsOnTaskId);
}

void TaskManager::setTaskPriority(const std::string& taskId, int priority) {
    impl_->setTaskPriority(taskId, priority);
}

void TaskManager::setTaskTimeout(const std::string& taskId, std::chrono::seconds timeout) {
    impl_->setTaskTimeout(taskId, timeout);
}

void TaskManager::setTaskRetryPolicy(const std::string& taskId, int maxRetries) {
    impl_->setTaskRetryPolicy(taskId, maxRetries);
}

void TaskManager::executeTask(const std::string& taskId) {
    impl_->executeTask(taskId);
}

void TaskManager::executeTasksInOrder(const std::vector<std::string>& taskIds) {
    impl_->executeTasksInOrder(taskIds);
}

void TaskManager::executeAllTasks() {
    impl_->executeAllTasks();
}

void TaskManager::cancelTask(const std::string& taskId) {
    impl_->cancelTask(taskId);
}

void TaskManager::cancelAllTasks() {
    impl_->cancelAllTasks();
}

std::optional<TaskExecutionContext> TaskManager::getTaskContext(const std::string& taskId) const {
    return impl_->getTaskContext(taskId);
}

TaskStatus TaskManager::getTaskStatus(const std::string& taskId) const {
    return impl_->getTaskStatus(taskId);
}

std::vector<TaskExecutionContext> TaskManager::getAllTaskContexts() const {
    return impl_->getAllTaskContexts();
}

std::vector<std::string> TaskManager::getTasksByStatus(TaskStatus status) const {
    return impl_->getTasksByStatus(status);
}

TaskExecutionStats TaskManager::getExecutionStats() const {
    return impl_->getExecutionStats();
}

std::unordered_map<std::string, DependencyNode> TaskManager::getDependencyGraph() const {
    return impl_->getDependencyGraph();
}

void TaskManager::executeFromScript(const std::string& script) {
    impl_->executeFromScript(script);
}

std::string TaskManager::generateScript(const std::vector<std::string>& taskIds) const {
    return impl_->generateScript(taskIds);
}

std::vector<std::string> TaskManager::loadTasksFromScript(const std::string& script) {
    return impl_->loadTasksFromScript(script);
}

void TaskManager::setTaskCompletionCallback(std::function<void(const TaskExecutionContext&)> callback) {
    impl_->setTaskCompletionCallback(std::move(callback));
}

void TaskManager::setTaskErrorCallback(std::function<void(const TaskExecutionContext&, const std::exception&)> callback) {
    impl_->setTaskErrorCallback(std::move(callback));
}

void TaskManager::setTaskStatusCallback(std::function<void(const std::string&, TaskStatus, TaskStatus)> callback) {
    impl_->setTaskStatusCallback(std::move(callback));
}

void TaskManager::setParallelExecution(bool enabled, size_t maxConcurrency) {
    impl_->setParallelExecution(enabled, maxConcurrency);
}

void TaskManager::setScheduler(std::function<std::vector<std::string>(const std::vector<std::string>&)> scheduler) {
    impl_->setScheduler(std::move(scheduler));
}

void TaskManager::addMiddleware(std::function<bool(TaskExecutionContext&)> middleware) {
    impl_->addMiddleware(std::move(middleware));
}

void TaskManager::clearMiddleware() {
    impl_->clearMiddleware();
}

bool TaskManager::validateTaskConfiguration(const std::string& taskType, const json& config) const {
    return impl_->validateTaskConfiguration(taskType, config);
}

std::vector<std::string> TaskManager::getAvailableTaskTypes() const {
    return impl_->getAvailableTaskTypes();
}

void TaskManager::clearAllTasks() {
    impl_->clearAllTasks();
}

void TaskManager::startExecutionService() {
    impl_->startExecutionService();
}

void TaskManager::stopExecutionService() {
    impl_->stopExecutionService();
}

// TaskExecutionQueue implementation
TaskExecutionQueue::TaskExecutionQueue() = default;
TaskExecutionQueue::~TaskExecutionQueue() = default;

void TaskExecutionQueue::enqueue(const std::string& taskId, int priority) {
    std::unique_lock lock(mutex_);
    if (!shutdown_) {
        queue_.emplace(QueueItem{taskId, priority, std::chrono::system_clock::now()});
    }
}

std::optional<std::string> TaskExecutionQueue::dequeue() {
    std::unique_lock lock(mutex_);
    if (queue_.empty() || shutdown_) {
        return std::nullopt;
    }
    
    auto item = queue_.top();
    queue_.pop();
    return item.taskId;
}

void TaskExecutionQueue::clear() {
    std::unique_lock lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

size_t TaskExecutionQueue::size() const {
    std::shared_lock lock(mutex_);
    return queue_.size();
}

bool TaskExecutionQueue::empty() const {
    std::shared_lock lock(mutex_);
    return queue_.empty();
}

// TaskUtils implementation
namespace TaskUtils {

std::string generateTaskId() {
    return atom::utils::UUID::generateV4().toString();
}

bool validateDependencies(const std::unordered_map<std::string, std::vector<std::string>>& dependencies) {
    // Simple cycle detection using DFS
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recStack;
    
    std::function<bool(const std::string&)> hasCycle = [&](const std::string& node) -> bool {
        visited.insert(node);
        recStack.insert(node);
        
        auto it = dependencies.find(node);
        if (it != dependencies.end()) {
            for (const auto& neighbor : it->second) {
                if (recStack.count(neighbor) || (!visited.count(neighbor) && hasCycle(neighbor))) {
                    return true;
                }
            }
        }
        
        recStack.erase(node);
        return false;
    };
    
    for (const auto& [node, deps] : dependencies) {
        if (!visited.count(node) && hasCycle(node)) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> topologicalSort(const std::unordered_map<std::string, std::vector<std::string>>& dependencies) {
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    
    std::function<void(const std::string&)> dfs = [&](const std::string& node) {
        visited.insert(node);
        
        auto it = dependencies.find(node);
        if (it != dependencies.end()) {
            for (const auto& neighbor : it->second) {
                if (!visited.count(neighbor)) {
                    dfs(neighbor);
                }
            }
        }
        
        result.insert(result.begin(), node);
    };
    
    for (const auto& [node, deps] : dependencies) {
        if (!visited.count(node)) {
            dfs(node);
        }
    }
    
    return result;
}

std::vector<std::string> calculateExecutionOrder(const std::vector<DependencyNode>& nodes) {
    std::unordered_map<std::string, std::vector<std::string>> deps;
    
    for (const auto& node : nodes) {
        deps[node.taskId] = node.dependencies;
    }
    
    return topologicalSort(deps);
}

TaskExecutionStats mergeStats(const std::vector<TaskExecutionStats>& stats) {
    TaskExecutionStats merged;
    
    if (stats.empty()) {
        return merged;
    }
    
    double totalTime = 0.0;
    for (const auto& stat : stats) {
        merged.totalExecuted += stat.totalExecuted;
        merged.successfulExecutions += stat.successfulExecutions;
        merged.failedExecutions += stat.failedExecutions;
        merged.retriedExecutions += stat.retriedExecutions;
        totalTime += stat.averageExecutionTime * stat.totalExecuted;
        
        if (stat.lastExecutionTime > merged.lastExecutionTime) {
            merged.lastExecutionTime = stat.lastExecutionTime;
        }
    }
    
    if (merged.totalExecuted > 0) {
        merged.averageExecutionTime = totalTime / merged.totalExecuted;
    }
    
    return merged;
}

} // namespace TaskUtils

} // namespace lithium::sequencer
