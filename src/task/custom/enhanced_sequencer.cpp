/**
 * @file enhanced_sequencer.cpp
 * @brief Enhanced Sequencer implementation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "enhanced_sequencer.hpp"
#include "task_templates.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include <algorithm>
#include <future>
#include <mutex>
#include <unordered_set>

namespace lithium::sequencer {

class EnhancedSequencer::Impl {
public:
    Impl();
    ~Impl();

    void executeSequence(const SequenceExecutionContext& context);
    void executeTargetsWithCustomTasks();
    void addCustomTaskToTarget(const std::string& targetName, 
                              const std::string& taskType,
                              const json& taskParameters);
    void removeCustomTaskFromTarget(const std::string& targetName, 
                                   const std::string& taskId);
    std::string createSequenceFromScript(const std::string& script);
    std::string generateSequenceScript() const;
    void optimizeSequence();

    TaskManager& getTaskManager();
    TaskGenerator& getTaskGenerator();

    void setExecutionStrategy(ExecutionStrategy strategy);
    ExecutionStrategy getExecutionStrategy() const;
    void setMaxConcurrency(size_t maxConcurrency);
    size_t getMaxConcurrency() const;

    void addTargetDependency(const std::string& targetName, 
                            const std::string& dependsOnTarget);
    void removeTargetDependency(const std::string& targetName, 
                               const std::string& dependsOnTarget);
    std::vector<std::string> getTargetExecutionOrder() const;
    bool validateSequenceDependencies() const;

    json getExecutionStatistics() const;
    double getExecutionProgress() const;
    std::chrono::system_clock::time_point getEstimatedCompletionTime() const;

    void pauseExecution();
    void resumeExecution();
    void cancelExecution();
    json getExecutionStatus() const;

    void loadSequenceFromScript(const std::string& script);
    void saveSequenceToScript(const std::string& filename) const;
    bool validateSequenceScript(const std::string& script) const;
    void applyScriptTemplate(const std::string& templateName, const json& parameters);

    void setOnCustomTaskComplete(std::function<void(const std::string&, const TaskExecutionContext&)> callback);
    void setOnCustomTaskError(std::function<void(const std::string&, const TaskExecutionContext&, const std::exception&)> callback);
    void setOnSequenceOptimization(std::function<void(const json&)> callback);

    void createImagingSequence(const json& config);
    void createCalibrationSequence(const json& config);
    void createFocusSequence(const json& config);
    void autoScheduleTargets();
    std::vector<std::string> suggestOptimizations() const;
    void setAutoRetry(bool enabled, int maxRetries = 3);
    void setDefaultTimeout(std::chrono::seconds timeout);

private:
    std::unique_ptr<TaskManager> taskManager_;
    std::unique_ptr<TaskGenerator> taskGenerator_;
    
    ExecutionStrategy strategy_ = ExecutionStrategy::Sequential;
    size_t maxConcurrency_ = 4;
    bool autoRetry_ = true;
    int maxRetries_ = 3;
    std::chrono::seconds defaultTimeout_{300};
    
    mutable std::shared_mutex mutex_;
    std::atomic<bool> executionPaused_{false};
    std::atomic<bool> executionCancelled_{false};
    
    // Target dependencies
    std::unordered_map<std::string, std::vector<std::string>> targetDependencies_;
    std::unordered_map<std::string, std::vector<std::string>> targetTasks_;
    
    // Callbacks
    std::function<void(const std::string&, const TaskExecutionContext&)> onTaskComplete_;
    std::function<void(const std::string&, const TaskExecutionContext&, const std::exception&)> onTaskError_;
    std::function<void(const json&)> onSequenceOptimization_;
    
    // Statistics
    mutable std::atomic<std::chrono::system_clock::time_point> executionStartTime_;
    mutable std::atomic<std::chrono::system_clock::time_point> executionEndTime_;
    mutable std::atomic<size_t> completedTasks_{0};
    mutable std::atomic<size_t> totalTasks_{0};
    
    // Helper methods
    void executeWithStrategy(const std::vector<std::string>& targets, const SequenceExecutionContext& context);
    void executeSequential(const std::vector<std::string>& targets, const SequenceExecutionContext& context);
    void executeParallel(const std::vector<std::string>& targets, const SequenceExecutionContext& context);
    void executeAdaptive(const std::vector<std::string>& targets, const SequenceExecutionContext& context);
    void executePriority(const std::vector<std::string>& targets, const SequenceExecutionContext& context);
    
    std::vector<std::string> calculateTargetExecutionOrder() const;
    bool hasCircularDependencies() const;
    void updateExecutionStats();
    json createTargetTaskSequence(const std::string& targetName) const;
};

EnhancedSequencer::Impl::Impl() 
    : taskManager_(std::make_unique<TaskManager>()),
      taskGenerator_(std::make_unique<TaskGenerator>()) {
    
    executionStartTime_.store(std::chrono::system_clock::now());
    executionEndTime_.store(std::chrono::system_clock::now());
    
    LOG_F(INFO, "Enhanced Sequencer initialized");
}

EnhancedSequencer::Impl::~Impl() {
    if (!executionCancelled_) {
        cancelExecution();
    }
    LOG_F(INFO, "Enhanced Sequencer destroyed");
}

void EnhancedSequencer::Impl::executeSequence(const SequenceExecutionContext& context) {
    std::unique_lock lock(mutex_);
    
    if (executionCancelled_) {
        LOG_F(WARNING, "Cannot execute sequence: execution was cancelled");
        return;
    }
    
    executionStartTime_.store(std::chrono::system_clock::now());
    executionPaused_ = false;
    strategy_ = context.strategy;
    maxConcurrency_ = context.maxConcurrency;
    maxRetries_ = context.maxRetries;
    defaultTimeout_ = context.defaultTimeout;
    
    LOG_F(INFO, "Starting sequence execution: {} (strategy: {})", 
          context.sequenceName, static_cast<int>(context.strategy));
    
    // Calculate execution order
    auto executionOrder = calculateTargetExecutionOrder();
    if (executionOrder.empty()) {
        LOG_F(WARNING, "No targets to execute");
        return;
    }
    
    totalTasks_ = executionOrder.size();
    completedTasks_ = 0;
    
    lock.unlock();
    
    // Execute with selected strategy
    executeWithStrategy(executionOrder, context);
    
    executionEndTime_.store(std::chrono::system_clock::now());
    updateExecutionStats();
    
    LOG_F(INFO, "Sequence execution completed: {}", context.sequenceName);
}

void EnhancedSequencer::Impl::executeTargetsWithCustomTasks() {
    std::shared_lock lock(mutex_);
    
    for (const auto& [targetName, taskIds] : targetTasks_) {
        if (executionCancelled_ || executionPaused_) {
            break;
        }
        
        LOG_F(INFO, "Executing custom tasks for target: {}", targetName);
        
        for (const auto& taskId : taskIds) {
            while (executionPaused_ && !executionCancelled_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            if (executionCancelled_) {
                break;
            }
            
            taskManager_->executeTask(taskId);
            completedTasks_++;
        }
    }
}

void EnhancedSequencer::Impl::addCustomTaskToTarget(const std::string& targetName, 
                                                    const std::string& taskType,
                                                    const json& taskParameters) {
    std::unique_lock lock(mutex_);
    
    std::string taskId = taskManager_->createTaskContext(taskType, targetName, taskParameters);
    
    // Set default timeout and retry policy
    taskManager_->setTaskTimeout(taskId, defaultTimeout_);
    if (autoRetry_) {
        taskManager_->setTaskRetryPolicy(taskId, maxRetries_);
    }
    
    targetTasks_[targetName].push_back(taskId);
    
    LOG_F(INFO, "Added custom task {} to target {}: {}", taskType, targetName, taskId);
}

void EnhancedSequencer::Impl::removeCustomTaskFromTarget(const std::string& targetName, 
                                                         const std::string& taskId) {
    std::unique_lock lock(mutex_);
    
    auto it = targetTasks_.find(targetName);
    if (it != targetTasks_.end()) {
        auto& tasks = it->second;
        tasks.erase(std::remove(tasks.begin(), tasks.end(), taskId), tasks.end());
        
        taskManager_->cancelTask(taskId);
        
        LOG_F(INFO, "Removed custom task {} from target {}", taskId, targetName);
    }
}

std::string EnhancedSequencer::Impl::createSequenceFromScript(const std::string& script) {
    try {
        auto scriptJson = json::parse(script);
        
        if (!scriptJson.contains("sequence") || !scriptJson["sequence"].is_object()) {
            throw std::invalid_argument("Invalid script format: missing sequence object");
        }
        
        auto sequence = scriptJson["sequence"];
        std::string sequenceId = sequence.value("id", "generated_sequence");
        
        // Parse targets and their tasks
        if (sequence.contains("targets") && sequence["targets"].is_array()) {
            for (const auto& target : sequence["targets"]) {
                std::string targetName = target["name"];
                
                if (target.contains("tasks") && target["tasks"].is_array()) {
                    for (const auto& task : target["tasks"]) {
                        std::string taskType = task["type"];
                        json taskParams = task.value("parameters", json::object());
                        
                        addCustomTaskToTarget(targetName, taskType, taskParams);
                    }
                }
                
                // Handle dependencies
                if (target.contains("dependencies") && target["dependencies"].is_array()) {
                    for (const auto& dep : target["dependencies"]) {
                        addTargetDependency(targetName, dep);
                    }
                }
            }
        }
        
        LOG_F(INFO, "Created sequence from script: {}", sequenceId);
        return sequenceId;
        
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to create sequence from script: {}", e.what());
        throw;
    }
}

std::string EnhancedSequencer::Impl::generateSequenceScript() const {
    std::shared_lock lock(mutex_);
    
    json script;
    script["sequence"]["id"] = "generated_sequence";
    script["sequence"]["strategy"] = static_cast<int>(strategy_);
    script["sequence"]["maxConcurrency"] = maxConcurrency_;
    script["sequence"]["targets"] = json::array();
    
    for (const auto& [targetName, taskIds] : targetTasks_) {
        json targetJson;
        targetJson["name"] = targetName;
        targetJson["tasks"] = json::array();
        
        for (const auto& taskId : taskIds) {
            auto context = taskManager_->getTaskContext(taskId);
            if (context) {
                json taskJson;
                taskJson["id"] = taskId;
                taskJson["type"] = context->taskType;
                taskJson["parameters"] = context->parameters;
                targetJson["tasks"].push_back(taskJson);
            }
        }
        
        // Add dependencies
        auto depIt = targetDependencies_.find(targetName);
        if (depIt != targetDependencies_.end() && !depIt->second.empty()) {
            targetJson["dependencies"] = depIt->second;
        }
        
        script["sequence"]["targets"].push_back(targetJson);
    }
    
    return script.dump(2);
}

void EnhancedSequencer::Impl::optimizeSequence() {
    std::unique_lock lock(mutex_);
    
    LOG_F(INFO, "Optimizing sequence execution order");
    
    // Optimization strategies
    json optimizationResult;
    
    // 1. Dependency optimization
    auto executionOrder = calculateTargetExecutionOrder();
    optimizationResult["executionOrder"] = executionOrder;
    
    // 2. Resource optimization
    std::unordered_map<std::string, int> resourceUsage;
    for (const auto& [targetName, taskIds] : targetTasks_) {
        for (const auto& taskId : taskIds) {
            auto context = taskManager_->getTaskContext(taskId);
            if (context) {
                resourceUsage[context->taskType]++;
            }
        }
    }
    optimizationResult["resourceUsage"] = resourceUsage;
    
    // 3. Suggest parallel execution opportunities
    std::vector<std::string> parallelizableTargets;
    for (const auto& targetName : executionOrder) {
        auto depIt = targetDependencies_.find(targetName);
        if (depIt == targetDependencies_.end() || depIt->second.empty()) {
            parallelizableTargets.push_back(targetName);
        }
    }
    optimizationResult["parallelizableTargets"] = parallelizableTargets;
    
    if (onSequenceOptimization_) {
        onSequenceOptimization_(optimizationResult);
    }
    
    LOG_F(INFO, "Sequence optimization completed");
}

TaskManager& EnhancedSequencer::Impl::getTaskManager() {
    return *taskManager_;
}

TaskGenerator& EnhancedSequencer::Impl::getTaskGenerator() {
    return *taskGenerator_;
}

void EnhancedSequencer::Impl::setExecutionStrategy(ExecutionStrategy strategy) {
    strategy_ = strategy;
}

ExecutionStrategy EnhancedSequencer::Impl::getExecutionStrategy() const {
    return strategy_;
}

void EnhancedSequencer::Impl::setMaxConcurrency(size_t maxConcurrency) {
    maxConcurrency_ = maxConcurrency;
}

size_t EnhancedSequencer::Impl::getMaxConcurrency() const {
    return maxConcurrency_;
}

void EnhancedSequencer::Impl::addTargetDependency(const std::string& targetName, 
                                                  const std::string& dependsOnTarget) {
    std::unique_lock lock(mutex_);
    
    targetDependencies_[targetName].push_back(dependsOnTarget);
    
    // Validate for circular dependencies
    if (hasCircularDependencies()) {
        // Remove the dependency that created the cycle
        auto& deps = targetDependencies_[targetName];
        deps.erase(std::remove(deps.begin(), deps.end(), dependsOnTarget), deps.end());
        
        LOG_F(ERROR, "Circular dependency detected, removing dependency: {} -> {}", 
              targetName, dependsOnTarget);
        throw std::invalid_argument("Circular dependency detected");
    }
    
    LOG_F(INFO, "Added target dependency: {} depends on {}", targetName, dependsOnTarget);
}

void EnhancedSequencer::Impl::removeTargetDependency(const std::string& targetName, 
                                                     const std::string& dependsOnTarget) {
    std::unique_lock lock(mutex_);
    
    auto it = targetDependencies_.find(targetName);
    if (it != targetDependencies_.end()) {
        auto& deps = it->second;
        deps.erase(std::remove(deps.begin(), deps.end(), dependsOnTarget), deps.end());
        
        LOG_F(INFO, "Removed target dependency: {} no longer depends on {}", 
              targetName, dependsOnTarget);
    }
}

std::vector<std::string> EnhancedSequencer::Impl::getTargetExecutionOrder() const {
    return calculateTargetExecutionOrder();
}

bool EnhancedSequencer::Impl::validateSequenceDependencies() const {
    return !hasCircularDependencies();
}

json EnhancedSequencer::Impl::getExecutionStatistics() const {
    json stats;
    
    auto taskStats = taskManager_->getExecutionStats();
    stats["taskStats"] = {
        {"totalExecuted", taskStats.totalExecuted},
        {"successfulExecutions", taskStats.successfulExecutions},
        {"failedExecutions", taskStats.failedExecutions},
        {"retriedExecutions", taskStats.retriedExecutions},
        {"averageExecutionTime", taskStats.averageExecutionTime}
    };
    
    auto startTime = executionStartTime_.load();
    auto endTime = executionEndTime_.load();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    stats["sequenceStats"] = {
        {"totalTargets", totalTasks_.load()},
        {"completedTargets", completedTasks_.load()},
        {"executionTime", duration.count()},
        {"strategy", static_cast<int>(strategy_)},
        {"maxConcurrency", maxConcurrency_}
    };
    
    return stats;
}

double EnhancedSequencer::Impl::getExecutionProgress() const {
    size_t total = totalTasks_.load();
    size_t completed = completedTasks_.load();
    
    if (total == 0) return 0.0;
    return static_cast<double>(completed) / static_cast<double>(total);
}

std::chrono::system_clock::time_point EnhancedSequencer::Impl::getEstimatedCompletionTime() const {
    auto progress = getExecutionProgress();
    if (progress <= 0.0) {
        return std::chrono::system_clock::now();
    }
    
    auto startTime = executionStartTime_.load();
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - startTime;
    
    auto totalEstimated = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed / progress);
    return startTime + totalEstimated;
}

void EnhancedSequencer::Impl::pauseExecution() {
    executionPaused_ = true;
    LOG_F(INFO, "Sequence execution paused");
}

void EnhancedSequencer::Impl::resumeExecution() {
    executionPaused_ = false;
    LOG_F(INFO, "Sequence execution resumed");
}

void EnhancedSequencer::Impl::cancelExecution() {
    executionCancelled_ = true;
    taskManager_->cancelAllTasks();
    LOG_F(INFO, "Sequence execution cancelled");
}

json EnhancedSequencer::Impl::getExecutionStatus() const {
    json status;
    status["paused"] = executionPaused_.load();
    status["cancelled"] = executionCancelled_.load();
    status["progress"] = getExecutionProgress();
    status["estimatedCompletion"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        getEstimatedCompletionTime().time_since_epoch()).count();
    
    return status;
}

void EnhancedSequencer::Impl::loadSequenceFromScript(const std::string& script) {
    createSequenceFromScript(script);
}

void EnhancedSequencer::Impl::saveSequenceToScript(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    file << generateSequenceScript();
    file.close();
    
    LOG_F(INFO, "Saved sequence to script: {}", filename);
}

bool EnhancedSequencer::Impl::validateSequenceScript(const std::string& script) const {
    try {
        auto scriptJson = json::parse(script);
        
        // Check required fields
        if (!scriptJson.contains("sequence")) {
            return false;
        }
        
        auto sequence = scriptJson["sequence"];
        if (!sequence.contains("targets") || !sequence["targets"].is_array()) {
            return false;
        }
        
        // Validate each target
        for (const auto& target : sequence["targets"]) {
            if (!target.contains("name") || !target["name"].is_string()) {
                return false;
            }
            
            if (target.contains("tasks") && target["tasks"].is_array()) {
                for (const auto& task : target["tasks"]) {
                    if (!task.contains("type") || !task["type"].is_string()) {
                        return false;
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Script validation failed: {}", e.what());
        return false;
    }
}

void EnhancedSequencer::Impl::applyScriptTemplate(const std::string& templateName, const json& parameters) {
    auto templateDef = templates::TemplateManager::applyTemplate(
        templates::TemplateManager::getImagingSequenceTemplate(), // Default to imaging template
        parameters
    );
    
    loadSequenceFromScript(templateDef.dump());
    
    LOG_F(INFO, "Applied script template: {}", templateName);
}

void EnhancedSequencer::Impl::setOnCustomTaskComplete(std::function<void(const std::string&, const TaskExecutionContext&)> callback) {
    onTaskComplete_ = std::move(callback);
    
    // Set up task manager callback
    taskManager_->setTaskCompletionCallback([this](const TaskExecutionContext& context) {
        if (onTaskComplete_) {
            onTaskComplete_(context.targetName, context);
        }
    });
}

void EnhancedSequencer::Impl::setOnCustomTaskError(std::function<void(const std::string&, const TaskExecutionContext&, const std::exception&)> callback) {
    onTaskError_ = std::move(callback);
    
    // Set up task manager callback
    taskManager_->setTaskErrorCallback([this](const TaskExecutionContext& context, const std::exception& e) {
        if (onTaskError_) {
            onTaskError_(context.targetName, context, e);
        }
    });
}

void EnhancedSequencer::Impl::setOnSequenceOptimization(std::function<void(const json&)> callback) {
    onSequenceOptimization_ = std::move(callback);
}

void EnhancedSequencer::Impl::createImagingSequence(const json& config) {
    auto templateDef = templates::TemplateManager::getImagingSequenceTemplate();
    auto appliedTemplate = templates::TemplateManager::applyTemplate(templateDef, config);
    loadSequenceFromScript(appliedTemplate.dump());
    
    LOG_F(INFO, "Created imaging sequence");
}

void EnhancedSequencer::Impl::createCalibrationSequence(const json& config) {
    auto templateDef = templates::TemplateManager::getCalibrationSequenceTemplate();
    auto appliedTemplate = templates::TemplateManager::applyTemplate(templateDef, config);
    loadSequenceFromScript(appliedTemplate.dump());
    
    LOG_F(INFO, "Created calibration sequence");
}

void EnhancedSequencer::Impl::createFocusSequence(const json& config) {
    auto templateDef = templates::TemplateManager::getFocusSequenceTemplate();
    auto appliedTemplate = templates::TemplateManager::applyTemplate(templateDef, config);
    loadSequenceFromScript(appliedTemplate.dump());
    
    LOG_F(INFO, "Created focus sequence");
}

void EnhancedSequencer::Impl::autoScheduleTargets() {
    // Auto-schedule based on optimal execution order and resource availability
    std::unique_lock lock(mutex_);
    
    auto executionOrder = calculateTargetExecutionOrder();
    
    // Set priorities based on execution order
    for (size_t i = 0; i < executionOrder.size(); ++i) {
        const auto& targetName = executionOrder[i];
        int priority = static_cast<int>(executionOrder.size() - i);
        
        auto it = targetTasks_.find(targetName);
        if (it != targetTasks_.end()) {
            for (const auto& taskId : it->second) {
                taskManager_->setTaskPriority(taskId, priority);
            }
        }
    }
    
    LOG_F(INFO, "Auto-scheduled {} targets", executionOrder.size());
}

std::vector<std::string> EnhancedSequencer::Impl::suggestOptimizations() const {
    std::vector<std::string> suggestions;
    
    // Analyze current configuration and suggest improvements
    if (strategy_ == ExecutionStrategy::Sequential && targetTasks_.size() > 1) {
        suggestions.push_back("Consider using parallel execution for better performance");
    }
    
    if (maxConcurrency_ < 2 && targetTasks_.size() > 2) {
        suggestions.push_back("Increase max concurrency for parallel execution");
    }
    
    // Check for potential dependency optimizations
    size_t independentTargets = 0;
    for (const auto& [targetName, _] : targetTasks_) {
        auto depIt = targetDependencies_.find(targetName);
        if (depIt == targetDependencies_.end() || depIt->second.empty()) {
            independentTargets++;
        }
    }
    
    if (independentTargets > 1) {
        suggestions.push_back("Multiple independent targets can be executed in parallel");
    }
    
    return suggestions;
}

void EnhancedSequencer::Impl::setAutoRetry(bool enabled, int maxRetries) {
    autoRetry_ = enabled;
    maxRetries_ = maxRetries;
    
    LOG_F(INFO, "Auto-retry: {} (max retries: {})", enabled ? "enabled" : "disabled", maxRetries);
}

void EnhancedSequencer::Impl::setDefaultTimeout(std::chrono::seconds timeout) {
    defaultTimeout_ = timeout;
    
    LOG_F(INFO, "Default timeout set to {} seconds", timeout.count());
}

// Helper methods implementation

void EnhancedSequencer::Impl::executeWithStrategy(const std::vector<std::string>& targets, const SequenceExecutionContext& context) {
    switch (context.strategy) {
        case ExecutionStrategy::Sequential:
            executeSequential(targets, context);
            break;
        case ExecutionStrategy::Parallel:
            executeParallel(targets, context);
            break;
        case ExecutionStrategy::Adaptive:
            executeAdaptive(targets, context);
            break;
        case ExecutionStrategy::Priority:
            executePriority(targets, context);
            break;
    }
}

void EnhancedSequencer::Impl::executeSequential(const std::vector<std::string>& targets, const SequenceExecutionContext& context) {
    for (const auto& targetName : targets) {
        if (executionCancelled_) break;
        
        while (executionPaused_ && !executionCancelled_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto it = targetTasks_.find(targetName);
        if (it != targetTasks_.end()) {
            for (const auto& taskId : it->second) {
                taskManager_->executeTask(taskId);
            }
        }
        
        completedTasks_++;
    }
}

void EnhancedSequencer::Impl::executeParallel(const std::vector<std::string>& targets, const SequenceExecutionContext& context) {
    std::vector<std::future<void>> futures;
    
    for (const auto& targetName : targets) {
        if (executionCancelled_) break;
        
        futures.emplace_back(std::async(std::launch::async, [this, targetName]() {
            auto it = targetTasks_.find(targetName);
            if (it != targetTasks_.end()) {
                for (const auto& taskId : it->second) {
                    if (executionCancelled_) break;
                    
                    while (executionPaused_ && !executionCancelled_) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    
                    taskManager_->executeTask(taskId);
                }
            }
            completedTasks_++;
        }));
        
        // Limit concurrency
        if (futures.size() >= maxConcurrency_) {
            futures.front().wait();
            futures.erase(futures.begin());
        }
    }
    
    // Wait for remaining tasks
    for (auto& future : futures) {
        future.wait();
    }
}

void EnhancedSequencer::Impl::executeAdaptive(const std::vector<std::string>& targets, const SequenceExecutionContext& context) {
    // Adaptive strategy: use parallel for independent targets, sequential for dependent ones
    std::unordered_set<std::string> executedTargets;
    std::vector<std::string> readyTargets;
    
    // Find initially ready targets (no dependencies)
    for (const auto& targetName : targets) {
        auto depIt = targetDependencies_.find(targetName);
        if (depIt == targetDependencies_.end() || depIt->second.empty()) {
            readyTargets.push_back(targetName);
        }
    }
    
    while (!readyTargets.empty() && !executionCancelled_) {
        // Execute ready targets in parallel
        std::vector<std::future<void>> futures;
        
        for (const auto& targetName : readyTargets) {
            futures.emplace_back(std::async(std::launch::async, [this, targetName]() {
                auto it = targetTasks_.find(targetName);
                if (it != targetTasks_.end()) {
                    for (const auto& taskId : it->second) {
                        if (executionCancelled_) break;
                        
                        while (executionPaused_ && !executionCancelled_) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        
                        taskManager_->executeTask(taskId);
                    }
                }
            }));
            
            if (futures.size() >= maxConcurrency_) {
                break;
            }
        }
        
        // Wait for completion
        for (auto& future : futures) {
            future.wait();
        }
        
        // Mark targets as executed
        for (size_t i = 0; i < std::min(readyTargets.size(), futures.size()); ++i) {
            executedTargets.insert(readyTargets[i]);
            completedTasks_++;
        }
        
        // Find next ready targets
        readyTargets.clear();
        for (const auto& targetName : targets) {
            if (executedTargets.count(targetName)) continue;
            
            auto depIt = targetDependencies_.find(targetName);
            if (depIt != targetDependencies_.end()) {
                bool allDepsExecuted = true;
                for (const auto& dep : depIt->second) {
                    if (!executedTargets.count(dep)) {
                        allDepsExecuted = false;
                        break;
                    }
                }
                if (allDepsExecuted) {
                    readyTargets.push_back(targetName);
                }
            }
        }
    }
}

void EnhancedSequencer::Impl::executePriority(const std::vector<std::string>& targets, const SequenceExecutionContext& context) {
    // Priority strategy: execute based on task priorities
    taskManager_->setParallelExecution(true, maxConcurrency_);
    
    std::vector<std::string> allTaskIds;
    for (const auto& targetName : targets) {
        auto it = targetTasks_.find(targetName);
        if (it != targetTasks_.end()) {
            allTaskIds.insert(allTaskIds.end(), it->second.begin(), it->second.end());
        }
    }
    
    taskManager_->executeTasksInOrder(allTaskIds);
    completedTasks_ = targets.size();
}

std::vector<std::string> EnhancedSequencer::Impl::calculateTargetExecutionOrder() const {
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visiting;
    
    std::function<void(const std::string&)> dfs = [&](const std::string& target) {
        if (visiting.count(target)) {
            // Circular dependency detected
            return;
        }
        if (visited.count(target)) {
            return;
        }
        
        visiting.insert(target);
        
        auto depIt = targetDependencies_.find(target);
        if (depIt != targetDependencies_.end()) {
            for (const auto& dep : depIt->second) {
                dfs(dep);
            }
        }
        
        visiting.erase(target);
        visited.insert(target);
        result.push_back(target);
    };
    
    // Process all targets
    for (const auto& [targetName, _] : targetTasks_) {
        if (!visited.count(targetName)) {
            dfs(targetName);
        }
    }
    
    return result;
}

bool EnhancedSequencer::Impl::hasCircularDependencies() const {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visiting;
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& target) -> bool {
        if (visiting.count(target)) {
            return true; // Circular dependency detected
        }
        if (visited.count(target)) {
            return false;
        }
        
        visiting.insert(target);
        
        auto depIt = targetDependencies_.find(target);
        if (depIt != targetDependencies_.end()) {
            for (const auto& dep : depIt->second) {
                if (dfs(dep)) {
                    return true;
                }
            }
        }
        
        visiting.erase(target);
        visited.insert(target);
        return false;
    };
    
    for (const auto& [targetName, _] : targetDependencies_) {
        if (!visited.count(targetName)) {
            if (dfs(targetName)) {
                return true;
            }
        }
    }
    
    return false;
}

void EnhancedSequencer::Impl::updateExecutionStats() {
    // Update statistics after execution
    LOG_F(INFO, "Execution statistics updated");
}

json EnhancedSequencer::Impl::createTargetTaskSequence(const std::string& targetName) const {
    json sequence;
    sequence["target"] = targetName;
    sequence["tasks"] = json::array();
    
    auto it = targetTasks_.find(targetName);
    if (it != targetTasks_.end()) {
        for (const auto& taskId : it->second) {
            auto context = taskManager_->getTaskContext(taskId);
            if (context) {
                json task;
                task["id"] = taskId;
                task["type"] = context->taskType;
                task["status"] = static_cast<int>(context->status);
                sequence["tasks"].push_back(task);
            }
        }
    }
    
    return sequence;
}

// EnhancedSequencer public interface

EnhancedSequencer::EnhancedSequencer() : impl_(std::make_unique<Impl>()) {}
EnhancedSequencer::~EnhancedSequencer() = default;

void EnhancedSequencer::executeSequence(const SequenceExecutionContext& context) {
    impl_->executeSequence(context);
}

void EnhancedSequencer::executeTargetsWithCustomTasks() {
    impl_->executeTargetsWithCustomTasks();
}

void EnhancedSequencer::addCustomTaskToTarget(const std::string& targetName, 
                                             const std::string& taskType,
                                             const json& taskParameters) {
    impl_->addCustomTaskToTarget(targetName, taskType, taskParameters);
}

void EnhancedSequencer::removeCustomTaskFromTarget(const std::string& targetName, 
                                                   const std::string& taskId) {
    impl_->removeCustomTaskFromTarget(targetName, taskId);
}

std::string EnhancedSequencer::createSequenceFromScript(const std::string& script) {
    return impl_->createSequenceFromScript(script);
}

std::string EnhancedSequencer::generateSequenceScript() const {
    return impl_->generateSequenceScript();
}

void EnhancedSequencer::optimizeSequence() {
    impl_->optimizeSequence();
}

TaskManager& EnhancedSequencer::getTaskManager() {
    return impl_->getTaskManager();
}

TaskGenerator& EnhancedSequencer::getTaskGenerator() {
    return impl_->getTaskGenerator();
}

void EnhancedSequencer::setExecutionStrategy(ExecutionStrategy strategy) {
    impl_->setExecutionStrategy(strategy);
}

ExecutionStrategy EnhancedSequencer::getExecutionStrategy() const {
    return impl_->getExecutionStrategy();
}

void EnhancedSequencer::setMaxConcurrency(size_t maxConcurrency) {
    impl_->setMaxConcurrency(maxConcurrency);
}

size_t EnhancedSequencer::getMaxConcurrency() const {
    return impl_->getMaxConcurrency();
}

void EnhancedSequencer::addTargetDependency(const std::string& targetName, 
                                           const std::string& dependsOnTarget) {
    impl_->addTargetDependency(targetName, dependsOnTarget);
}

void EnhancedSequencer::removeTargetDependency(const std::string& targetName, 
                                              const std::string& dependsOnTarget) {
    impl_->removeTargetDependency(targetName, dependsOnTarget);
}

std::vector<std::string> EnhancedSequencer::getTargetExecutionOrder() const {
    return impl_->getTargetExecutionOrder();
}

bool EnhancedSequencer::validateSequenceDependencies() const {
    return impl_->validateSequenceDependencies();
}

json EnhancedSequencer::getExecutionStatistics() const {
    return impl_->getExecutionStatistics();
}

double EnhancedSequencer::getExecutionProgress() const {
    return impl_->getExecutionProgress();
}

std::chrono::system_clock::time_point EnhancedSequencer::getEstimatedCompletionTime() const {
    return impl_->getEstimatedCompletionTime();
}

void EnhancedSequencer::pauseExecution() {
    impl_->pauseExecution();
}

void EnhancedSequencer::resumeExecution() {
    impl_->resumeExecution();
}

void EnhancedSequencer::cancelExecution() {
    impl_->cancelExecution();
}

json EnhancedSequencer::getExecutionStatus() const {
    return impl_->getExecutionStatus();
}

void EnhancedSequencer::loadSequenceFromScript(const std::string& script) {
    impl_->loadSequenceFromScript(script);
}

void EnhancedSequencer::saveSequenceToScript(const std::string& filename) const {
    impl_->saveSequenceToScript(filename);
}

bool EnhancedSequencer::validateSequenceScript(const std::string& script) const {
    return impl_->validateSequenceScript(script);
}

void EnhancedSequencer::applyScriptTemplate(const std::string& templateName, const json& parameters) {
    impl_->applyScriptTemplate(templateName, parameters);
}

void EnhancedSequencer::setOnCustomTaskComplete(std::function<void(const std::string&, const TaskExecutionContext&)> callback) {
    impl_->setOnCustomTaskComplete(std::move(callback));
}

void EnhancedSequencer::setOnCustomTaskError(std::function<void(const std::string&, const TaskExecutionContext&, const std::exception&)> callback) {
    impl_->setOnCustomTaskError(std::move(callback));
}

void EnhancedSequencer::setOnSequenceOptimization(std::function<void(const json&)> callback) {
    impl_->setOnSequenceOptimization(std::move(callback));
}

void EnhancedSequencer::createImagingSequence(const json& config) {
    impl_->createImagingSequence(config);
}

void EnhancedSequencer::createCalibrationSequence(const json& config) {
    impl_->createCalibrationSequence(config);
}

void EnhancedSequencer::createFocusSequence(const json& config) {
    impl_->createFocusSequence(config);
}

void EnhancedSequencer::autoScheduleTargets() {
    impl_->autoScheduleTargets();
}

std::vector<std::string> EnhancedSequencer::suggestOptimizations() const {
    return impl_->suggestOptimizations();
}

void EnhancedSequencer::setAutoRetry(bool enabled, int maxRetries) {
    impl_->setAutoRetry(enabled, maxRetries);
}

void EnhancedSequencer::setDefaultTimeout(std::chrono::seconds timeout) {
    impl_->setDefaultTimeout(timeout);
}

// SequenceBuilder implementation

class SequenceBuilder::BuilderImpl {
public:
    std::vector<std::string> targets_;
    std::unordered_map<std::string, std::vector<json>> targetTasks_;
    std::unordered_map<std::string, std::vector<std::string>> targetDependencies_;
    std::unordered_map<std::string, int> targetPriorities_;
    ExecutionStrategy strategy_ = ExecutionStrategy::Sequential;
};

SequenceBuilder::SequenceBuilder() : impl_(std::make_unique<BuilderImpl>()) {}
SequenceBuilder::~SequenceBuilder() = default;

SequenceBuilder& SequenceBuilder::addTarget(const std::string& name, double ra, double dec) {
    impl_->targets_.push_back(name);
    return *this;
}

SequenceBuilder& SequenceBuilder::addTask(const std::string& taskType, const json& parameters) {
    if (!impl_->targets_.empty()) {
        const auto& currentTarget = impl_->targets_.back();
        json task;
        task["type"] = taskType;
        task["parameters"] = parameters;
        impl_->targetTasks_[currentTarget].push_back(task);
    }
    return *this;
}

SequenceBuilder& SequenceBuilder::addDependency(const std::string& dependsOn) {
    if (!impl_->targets_.empty()) {
        const auto& currentTarget = impl_->targets_.back();
        impl_->targetDependencies_[currentTarget].push_back(dependsOn);
    }
    return *this;
}

SequenceBuilder& SequenceBuilder::setPriority(int priority) {
    if (!impl_->targets_.empty()) {
        const auto& currentTarget = impl_->targets_.back();
        impl_->targetPriorities_[currentTarget] = priority;
    }
    return *this;
}

SequenceBuilder& SequenceBuilder::setStrategy(ExecutionStrategy strategy) {
    impl_->strategy_ = strategy;
    return *this;
}

std::unique_ptr<EnhancedSequencer> SequenceBuilder::build() {
    auto sequencer = std::make_unique<EnhancedSequencer>();
    
    sequencer->setExecutionStrategy(impl_->strategy_);
    
    // Add targets and tasks
    for (const auto& targetName : impl_->targets_) {
        auto taskIt = impl_->targetTasks_.find(targetName);
        if (taskIt != impl_->targetTasks_.end()) {
            for (const auto& task : taskIt->second) {
                sequencer->addCustomTaskToTarget(targetName, task["type"], task["parameters"]);
            }
        }
        
        // Add dependencies
        auto depIt = impl_->targetDependencies_.find(targetName);
        if (depIt != impl_->targetDependencies_.end()) {
            for (const auto& dep : depIt->second) {
                sequencer->addTargetDependency(targetName, dep);
            }
        }
    }
    
    return sequencer;
}

std::string SequenceBuilder::generateScript() const {
    json script;
    script["sequence"]["strategy"] = static_cast<int>(impl_->strategy_);
    script["sequence"]["targets"] = json::array();
    
    for (const auto& targetName : impl_->targets_) {
        json target;
        target["name"] = targetName;
        
        auto taskIt = impl_->targetTasks_.find(targetName);
        if (taskIt != impl_->targetTasks_.end()) {
            target["tasks"] = taskIt->second;
        }
        
        auto depIt = impl_->targetDependencies_.find(targetName);
        if (depIt != impl_->targetDependencies_.end()) {
            target["dependencies"] = depIt->second;
        }
        
        auto priIt = impl_->targetPriorities_.find(targetName);
        if (priIt != impl_->targetPriorities_.end()) {
            target["priority"] = priIt->second;
        }
        
        script["sequence"]["targets"].push_back(target);
    }
    
    return script.dump(2);
}

// Validation utilities implementation

namespace SequenceValidation {

bool validateSequenceConfig(const json& config) {
    try {
        if (!config.contains("sequence")) {
            return false;
        }
        
        auto sequence = config["sequence"];
        if (!sequence.contains("targets") || !sequence["targets"].is_array()) {
            return false;
        }
        
        for (const auto& target : sequence["targets"]) {
            if (!target.contains("name") || !target["name"].is_string()) {
                return false;
            }
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool checkCircularDependencies(const std::unordered_map<std::string, std::vector<std::string>>& dependencies) {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visiting;
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        if (visiting.count(node)) {
            return true; // Circular dependency
        }
        if (visited.count(node)) {
            return false;
        }
        
        visiting.insert(node);
        
        auto it = dependencies.find(node);
        if (it != dependencies.end()) {
            for (const auto& dep : it->second) {
                if (dfs(dep)) {
                    return true;
                }
            }
        }
        
        visiting.erase(node);
        visited.insert(node);
        return false;
    };
    
    for (const auto& [node, _] : dependencies) {
        if (!visited.count(node)) {
            if (dfs(node)) {
                return true;
            }
        }
    }
    
    return false;
}

bool validateTaskParameters(const std::string& taskType, const json& parameters) {
    return TaskFactory::getInstance().validateTaskParameters(taskType, parameters);
}

std::chrono::seconds estimateExecutionTime(const json& sequence) {
    // Simple estimation based on number of tasks and average execution time
    size_t taskCount = 0;
    
    if (sequence.contains("sequence") && sequence["sequence"].contains("targets")) {
        for (const auto& target : sequence["sequence"]["targets"]) {
            if (target.contains("tasks") && target["tasks"].is_array()) {
                taskCount += target["tasks"].size();
            }
        }
    }
    
    // Assume average 30 seconds per task
    return std::chrono::seconds(taskCount * 30);
}

std::vector<std::string> suggestImprovements(const json& sequence) {
    std::vector<std::string> suggestions;
    
    if (!sequence.contains("sequence")) {
        suggestions.push_back("Invalid sequence format");
        return suggestions;
    }
    
    auto seq = sequence["sequence"];
    
    // Check strategy
    if (!seq.contains("strategy") || seq["strategy"] == static_cast<int>(ExecutionStrategy::Sequential)) {
        suggestions.push_back("Consider using parallel execution for better performance");
    }
    
    // Check for dependencies
    bool hasDependencies = false;
    if (seq.contains("targets")) {
        for (const auto& target : seq["targets"]) {
            if (target.contains("dependencies") && !target["dependencies"].empty()) {
                hasDependencies = true;
                break;
            }
        }
    }
    
    if (!hasDependencies) {
        suggestions.push_back("Targets appear independent and can be parallelized");
    }
    
    return suggestions;
}

} // namespace SequenceValidation

// Optimization utilities implementation

namespace SequenceOptimization {

std::vector<std::string> optimizeExecutionOrder(const std::vector<std::string>& tasks,
                                               const std::unordered_map<std::string, std::vector<std::string>>& dependencies) {
    return TaskUtils::topologicalSort(dependencies);
}

json optimizeResourceUsage(const json& sequence) {
    json optimized = sequence;
    
    // Analyze resource usage and suggest optimizations
    if (optimized.contains("sequence") && optimized["sequence"].contains("targets")) {
        auto& targets = optimized["sequence"]["targets"];
        
        // Group similar tasks together
        std::unordered_map<std::string, std::vector<size_t>> taskGroups;
        
        for (size_t i = 0; i < targets.size(); ++i) {
            if (targets[i].contains("tasks")) {
                for (const auto& task : targets[i]["tasks"]) {
                    if (task.contains("type")) {
                        taskGroups[task["type"]].push_back(i);
                    }
                }
            }
        }
        
        optimized["optimization"]["taskGroups"] = taskGroups;
    }
    
    return optimized;
}

json minimizeExecutionTime(const json& sequence) {
    json optimized = sequence;
    
    // Set strategy to parallel for minimum execution time
    if (optimized.contains("sequence")) {
        optimized["sequence"]["strategy"] = static_cast<int>(ExecutionStrategy::Parallel);
        optimized["sequence"]["maxConcurrency"] = 8; // Increase concurrency
    }
    
    return optimized;
}

json balanceQualitySpeed(const json& sequence, double qualityWeight) {
    json optimized = sequence;
    
    // Balance between quality and speed based on weight
    if (qualityWeight > 0.5) {
        // Prefer quality - use sequential or adaptive strategy
        if (optimized.contains("sequence")) {
            optimized["sequence"]["strategy"] = static_cast<int>(ExecutionStrategy::Adaptive);
            optimized["sequence"]["maxConcurrency"] = 4;
        }
    } else {
        // Prefer speed - use parallel strategy
        if (optimized.contains("sequence")) {
            optimized["sequence"]["strategy"] = static_cast<int>(ExecutionStrategy::Parallel);
            optimized["sequence"]["maxConcurrency"] = 8;
        }
    }
    
    return optimized;
}

} // namespace SequenceOptimization

} // namespace lithium::sequencer
