/**
 * @file enhanced_sequencer.hpp
 * @brief Enhanced Sequencer with Custom Task Integration
 *
 * This file contains an enhanced sequencer that integrates with the custom
 * task system for advanced execution capabilities.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_ENHANCED_SEQUENCER_HPP
#define LITHIUM_ENHANCED_SEQUENCER_HPP

#include "sequencer.hpp"
#include "custom/task_manager.hpp"
#include "custom/factory.hpp"
#include "generator.hpp"
#include <memory>
#include <functional>

namespace lithium::sequencer {

/**
 * @brief Enhanced execution strategy
 */
enum class ExecutionStrategy {
    Sequential,     ///< Execute tasks one by one
    Parallel,       ///< Execute compatible tasks in parallel  
    Adaptive,       ///< Dynamically choose based on dependencies
    Priority        ///< Execute based on priority order
};

/**
 * @brief Sequence execution context
 */
struct SequenceExecutionContext {
    std::string sequenceId;
    std::string sequenceName;
    ExecutionStrategy strategy = ExecutionStrategy::Sequential;
    size_t maxConcurrency = 4;
    bool enableRetry = true;
    int maxRetries = 3;
    std::chrono::seconds defaultTimeout{300};
    bool enableOptimization = true;
    json metadata;
};

/**
 * @brief Enhanced Sequencer with custom task integration
 */
class EnhancedSequencer : public ExposureSequence {
public:
    /**
     * @brief Constructor
     */
    EnhancedSequencer();

    /**
     * @brief Destructor
     */
    ~EnhancedSequencer();

    // Enhanced execution methods

    /**
     * @brief Execute sequence with custom strategy
     */
    void executeSequence(const SequenceExecutionContext& context);

    /**
     * @brief Execute targets with custom tasks
     */
    void executeTargetsWithCustomTasks();

    /**
     * @brief Add custom task to target
     */
    void addCustomTaskToTarget(const std::string& targetName, 
                              const std::string& taskType,
                              const json& taskParameters);

    /**
     * @brief Remove custom task from target
     */
    void removeCustomTaskFromTarget(const std::string& targetName, 
                                   const std::string& taskId);

    /**
     * @brief Create task sequence from script
     */
    std::string createSequenceFromScript(const std::string& script);

    /**
     * @brief Generate script from current sequence
     */
    std::string generateSequenceScript() const;

    /**
     * @brief Optimize sequence for execution
     */
    void optimizeSequence();

    // Task management integration

    /**
     * @brief Get task manager instance
     */
    TaskManager& getTaskManager();

    /**
     * @brief Get task generator instance
     */
    TaskGenerator& getTaskGenerator();

    /**
     * @brief Set execution strategy
     */
    void setExecutionStrategy(ExecutionStrategy strategy);

    /**
     * @brief Get execution strategy
     */
    ExecutionStrategy getExecutionStrategy() const;

    /**
     * @brief Set maximum concurrency
     */
    void setMaxConcurrency(size_t maxConcurrency);

    /**
     * @brief Get maximum concurrency
     */
    size_t getMaxConcurrency() const;

    // Advanced scheduling

    /**
     * @brief Add dependency between targets
     */
    void addTargetDependency(const std::string& targetName, 
                            const std::string& dependsOnTarget);

    /**
     * @brief Remove dependency between targets
     */
    void removeTargetDependency(const std::string& targetName, 
                               const std::string& dependsOnTarget);

    /**
     * @brief Get target execution order
     */
    std::vector<std::string> getTargetExecutionOrder() const;

    /**
     * @brief Validate sequence dependencies
     */
    bool validateSequenceDependencies() const;

    // Monitoring and control

    /**
     * @brief Get detailed execution statistics
     */
    json getExecutionStatistics() const;

    /**
     * @brief Get task execution progress
     */
    double getExecutionProgress() const;

    /**
     * @brief Get estimated completion time
     */
    std::chrono::system_clock::time_point getEstimatedCompletionTime() const;

    /**
     * @brief Pause sequence execution
     */
    void pauseExecution();

    /**
     * @brief Resume sequence execution
     */
    void resumeExecution();

    /**
     * @brief Cancel sequence execution
     */
    void cancelExecution();

    /**
     * @brief Get current execution status
     */
    json getExecutionStatus() const;

    // Script integration

    /**
     * @brief Load sequence from JSON script
     */
    void loadSequenceFromScript(const std::string& script);

    /**
     * @brief Save sequence to JSON script
     */
    void saveSequenceToScript(const std::string& filename) const;

    /**
     * @brief Validate sequence script
     */
    bool validateSequenceScript(const std::string& script) const;

    /**
     * @brief Apply script template to sequence
     */
    void applyScriptTemplate(const std::string& templateName, const json& parameters);

    // Event handling enhancements

    /**
     * @brief Set custom task completion callback
     */
    void setOnCustomTaskComplete(std::function<void(const std::string&, const TaskExecutionContext&)> callback);

    /**
     * @brief Set custom task error callback
     */
    void setOnCustomTaskError(std::function<void(const std::string&, const TaskExecutionContext&, const std::exception&)> callback);

    /**
     * @brief Set sequence optimization callback
     */
    void setOnSequenceOptimization(std::function<void(const json&)> callback);

    // Quality of life improvements

    /**
     * @brief Create standard imaging sequence
     */
    void createImagingSequence(const json& config);

    /**
     * @brief Create calibration sequence
     */
    void createCalibrationSequence(const json& config);

    /**
     * @brief Create focus sequence
     */
    void createFocusSequence(const json& config);

    /**
     * @brief Auto-schedule optimal execution times
     */
    void autoScheduleTargets();

    /**
     * @brief Suggest sequence optimizations
     */
    std::vector<std::string> suggestOptimizations() const;

    /**
     * @brief Enable/disable automatic retry
     */
    void setAutoRetry(bool enabled, int maxRetries = 3);

    /**
     * @brief Set default task timeout
     */
    void setDefaultTimeout(std::chrono::seconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Sequence builder for easy sequence creation
 */
class SequenceBuilder {
public:
    SequenceBuilder();
    ~SequenceBuilder();

    /**
     * @brief Add target to sequence
     */
    SequenceBuilder& addTarget(const std::string& name, double ra, double dec);

    /**
     * @brief Add task to current target
     */
    SequenceBuilder& addTask(const std::string& taskType, const json& parameters);

    /**
     * @brief Add dependency to current target
     */
    SequenceBuilder& addDependency(const std::string& dependsOn);

    /**
     * @brief Set target priority
     */
    SequenceBuilder& setPriority(int priority);

    /**
     * @brief Set execution strategy
     */
    SequenceBuilder& setStrategy(ExecutionStrategy strategy);

    /**
     * @brief Build and return the sequence
     */
    std::unique_ptr<EnhancedSequencer> build();

    /**
     * @brief Generate sequence script
     */
    std::string generateScript() const;

private:
    class BuilderImpl;
    std::unique_ptr<BuilderImpl> impl_;
};

/**
 * @brief Sequence validation utilities
 */
namespace SequenceValidation {
    /**
     * @brief Validate sequence configuration
     */
    bool validateSequenceConfig(const json& config);

    /**
     * @brief Check for circular dependencies
     */
    bool checkCircularDependencies(const std::unordered_map<std::string, std::vector<std::string>>& dependencies);

    /**
     * @brief Validate task parameters
     */
    bool validateTaskParameters(const std::string& taskType, const json& parameters);

    /**
     * @brief Estimate sequence execution time
     */
    std::chrono::seconds estimateExecutionTime(const json& sequence);

    /**
     * @brief Suggest sequence improvements
     */
    std::vector<std::string> suggestImprovements(const json& sequence);
}

/**
 * @brief Sequence optimization utilities
 */
namespace SequenceOptimization {
    /**
     * @brief Optimize task execution order
     */
    std::vector<std::string> optimizeExecutionOrder(const std::vector<std::string>& tasks,
                                                    const std::unordered_map<std::string, std::vector<std::string>>& dependencies);

    /**
     * @brief Optimize resource usage
     */
    json optimizeResourceUsage(const json& sequence);

    /**
     * @brief Minimize execution time
     */
    json minimizeExecutionTime(const json& sequence);

    /**
     * @brief Balance quality and speed
     */
    json balanceQualitySpeed(const json& sequence, double qualityWeight = 0.7);
}

} // namespace lithium::sequencer

#endif // LITHIUM_ENHANCED_SEQUENCER_HPP
