#ifndef LITHIUM_TASK_CORE_SEQUENCER_HPP
#define LITHIUM_TASK_CORE_SEQUENCER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../../database/orm.hpp"
#include "generator.hpp"
#include "target.hpp"

namespace lithium::task {
using namespace lithium::database;

/**
 * @enum SequenceState
 * @brief Represents the current state of a sequence.
 */
enum class SequenceState { Idle, Running, Paused, Stopping, Stopped };

/**
 * @class SequenceModel
 * @brief Database model for sequence storage and retrieval.
 */
class SequenceModel {
public:
    static std::string tableName() { return "sequences"; }

    static std::vector<std::unique_ptr<ColumnBase>> columns() {
        std::vector<std::unique_ptr<ColumnBase>> cols;
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "uuid", &SequenceModel::uuid));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "name", &SequenceModel::name));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "data", &SequenceModel::data));
        cols.push_back(std::make_unique<Column<std::string, SequenceModel>>(
            "created_at", &SequenceModel::createdAt));
        return cols;
    }

    std::string uuid;       ///< Unique identifier of the sequence
    std::string name;       ///< Name of the sequence
    std::string data;       ///< JSON data representing the sequence
    std::string createdAt;  ///< Creation timestamp
};

/**
 * @class ExposureSequence
 * @brief Manages and executes a sequence of targets with tasks.
 */
class ExposureSequence {
public:
    // Callback function type definitions
    using SequenceCallback = std::function<void()>;
    using TargetCallback =
        std::function<void(const std::string& targetName, TargetStatus status)>;
    using ErrorCallback = std::function<void(const std::string& targetName,
                                             const std::exception& e)>;
    using ProgressCallback = std::function<void(const json& progress)>;
    using TaskCallback =
        std::function<void(const std::string& targetName,
                           const std::string& taskName, const json& taskInfo)>;

    /**
     * @brief Constructor that initializes database and task generator.
     */
    ExposureSequence();

    /**
     * @brief Destructor that ensures sequence execution is stopped.
     */
    ~ExposureSequence();

    // Delete copy constructor and assignment operator
    ExposureSequence(const ExposureSequence&) = delete;
    ExposureSequence& operator=(const ExposureSequence&) = delete;

    // Target management methods

    /**
     * @brief Adds a target to the sequence.
     * @param target The target to add.
     */
    void addTarget(std::unique_ptr<Target> target);

    /**
     * @brief Removes a target from the sequence by name.
     * @param name The name of the target to remove.
     */
    void removeTarget(const std::string& name);

    /**
     * @brief Modifies a target using a modifier function.
     * @param name The name of the target to modify.
     * @param modifier The function to modify the target.
     */
    void modifyTarget(const std::string& name, const TargetModifier& modifier);

    // Execution control methods

    /**
     * @brief Checks if the sequence is currently running.
     * @return True if the sequence is running.
     */
    [[nodiscard]] auto isRunning() const -> bool;

    /**
     * @brief Executes all targets in the sequence.
     */
    void executeAll();

    /**
     * @brief Stops the execution of the sequence.
     */
    void stop();

    /**
     * @brief Pauses the execution of the sequence.
     */
    void pause();

    /**
     * @brief Resumes a paused sequence.
     */
    void resume();

    // Serialization methods

    /**
     * @brief Saves the sequence to a file.
     * @param filename The name of the file to save to.
     */
    void saveSequence(const std::string& filename) const;

    /**
     * @brief Loads a sequence from a file.
     * @param filename The name of the file to load from.
     */
    void loadSequence(const std::string& filename);

    // Query methods

    /**
     * @brief Gets the names of all targets in the sequence.
     * @return A vector of target names.
     */
    std::vector<std::string> getTargetNames() const;

    /**
     * @brief Gets the status of a target by name.
     * @param name The name of the target.
     * @return The status of the target.
     */
    TargetStatus getTargetStatus(const std::string& name) const;

    /**
     * @brief Gets the overall progress of the sequence.
     * @return The progress as a percentage.
     */
    double getProgress() const;

    // Callback setter methods

    /**
     * @brief Sets the callback for sequence start.
     * @param callback The callback function.
     */
    void setOnSequenceStart(SequenceCallback callback);

    /**
     * @brief Sets the callback for sequence end.
     * @param callback The callback function.
     */
    void setOnSequenceEnd(SequenceCallback callback);

    /**
     * @brief Sets the callback for target start.
     * @param callback The callback function.
     */
    void setOnTargetStart(TargetCallback callback);

    /**
     * @brief Sets the callback for target end.
     * @param callback The callback function.
     */
    void setOnTargetEnd(TargetCallback callback);

    /**
     * @brief Sets the callback for error handling.
     * @param callback The callback function.
     */
    void setOnError(ErrorCallback callback);

    /**
     * @brief Sets the callback for progress updates.
     * @param callback The callback function receiving progress JSON.
     *
     * Progress JSON format:
     * {
     *   "sequenceId": "uuid",
     *   "state": "running|paused|stopping",
     *   "progress": 0.0-100.0,
     *   "completedTargets": 0,
     *   "totalTargets": 0,
     *   "currentTarget": "targetName",
     *   "currentTask": "taskName",
     *   "elapsedTime": 0,
     *   "estimatedRemaining": 0
     * }
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Sets the callback for task start events.
     * @param callback The callback function.
     */
    void setOnTaskStart(TaskCallback callback);

    /**
     * @brief Sets the callback for task end events.
     * @param callback The callback function.
     */
    void setOnTaskEnd(TaskCallback callback);

    /**
     * @brief Handles errors that occur during target execution.
     * @param target The target in which the error occurred.
     * @param e The exception that was thrown.
     */
    void handleTargetError(Target* target, const std::exception& e);

    /**
     * @enum SchedulingStrategy
     * @brief Defines how targets are scheduled for execution.
     */
    enum class SchedulingStrategy {
        FIFO,         ///< First In, First Out
        Priority,     ///< Based on priority values
        Dependencies  ///< Based on dependency relationships
    };

    /**
     * @enum ExecutionStrategy
     * @brief Defines how tasks in a sequence are executed
     */
    enum class ExecutionStrategy {
        Sequential,  ///< Execute tasks one after another
        Parallel,    ///< Execute independent tasks simultaneously
        Adaptive,    ///< Dynamically select strategy based on resources
        Priority     ///< Execute based on priority with preemption
    };

    /**
     * @enum RecoveryStrategy
     * @brief Defines how to recover from errors.
     */
    enum class RecoveryStrategy {
        Stop,        ///< Stop the sequence
        Skip,        ///< Skip failed tasks
        Retry,       ///< Retry failed tasks
        Alternative  ///< Use alternative tasks
    };

    // Strategy setting methods

    /**
     * @brief Sets the scheduling strategy.
     * @param strategy The strategy to use.
     */
    void setSchedulingStrategy(SchedulingStrategy strategy);

    /**
     * @brief Sets the error recovery strategy.
     * @param strategy The strategy to use.
     */
    void setRecoveryStrategy(RecoveryStrategy strategy);

    /**
     * @brief Adds an alternative target for error recovery.
     * @param targetName The name of the primary target.
     * @param alternative The alternative target.
     */
    void addAlternativeTarget(const std::string& targetName,
                              std::unique_ptr<Target> alternative);

    // Performance monitoring methods

    /**
     * @brief Gets the average execution time of targets.
     * @return The average execution time.
     */
    [[nodiscard]] auto getAverageExecutionTime() const
        -> std::chrono::milliseconds;

    /**
     * @brief Gets the total memory usage of the sequence.
     * @return The total memory usage in bytes.
     */
    [[nodiscard]] auto getTotalMemoryUsage() const -> size_t;

    // Dependency management methods

    /**
     * @brief Adds a dependency between targets.
     * @param targetName The dependent target.
     * @param dependsOn The target that targetName depends on.
     */
    void addTargetDependency(const std::string& targetName,
                             const std::string& dependsOn);

    /**
     * @brief Removes a dependency between targets.
     * @param targetName The dependent target.
     * @param dependsOn The target to remove dependency from.
     */
    void removeTargetDependency(const std::string& targetName,
                                const std::string& dependsOn);

    /**
     * @brief Sets the priority of a target.
     * @param targetName The name of the target.
     * @param priority The priority value.
     */
    void setTargetPriority(const std::string& targetName, int priority);

    /**
     * @brief Checks if a target is ready to execute.
     * @param targetName The name of the target.
     * @return True if the target is ready, false otherwise.
     */
    [[nodiscard]] auto isTargetReady(const std::string& targetName) const
        -> bool;

    /**
     * @brief Gets the dependencies of a target.
     * @param targetName The name of the target.
     * @return A vector of dependency names.
     */
    [[nodiscard]] auto getTargetDependencies(
        const std::string& targetName) const -> std::vector<std::string>;

    // Monitoring and control methods

    /**
     * @brief Sets the maximum number of targets to execute concurrently.
     * @param max The maximum number.
     */
    void setMaxConcurrentTargets(size_t max);

    /**
     * @brief Sets a timeout for a target.
     * @param name The name of the target.
     * @param timeout The timeout duration.
     */
    void setTargetTimeout(const std::string& name,
                          std::chrono::seconds timeout);

    /**
     * @brief Sets a global timeout for the sequence.
     * @param timeout The timeout duration.
     */
    void setGlobalTimeout(std::chrono::seconds timeout);

    // Status query methods

    /**
     * @brief Gets the names of failed targets.
     * @return A vector of failed target names.
     */
    [[nodiscard]] auto getFailedTargets() const -> std::vector<std::string>;

    /**
     * @brief Gets execution statistics.
     * @return JSON object with execution statistics.
     */
    [[nodiscard]] auto getExecutionStats() const -> json;

    /**
     * @brief Gets resource usage information.
     * @return JSON object with resource usage information.
     */
    [[nodiscard]] auto getResourceUsage() const -> json;

    // Error recovery methods

    /**
     * @brief Retries all failed targets.
     */
    void retryFailedTargets();

    /**
     * @brief Skips all failed targets.
     */
    void skipFailedTargets();

    /**
     * @brief Sets parameters for a specific task in a target.
     * @param targetName The name of the target.
     * @param taskUUID The UUID of the task.
     * @param params The parameters in JSON format.
     */
    void setTargetTaskParams(const std::string& targetName,
                             const std::string& taskUUID, const json& params);

    /**
     * @brief Gets parameters for a specific task in a target.
     * @param targetName The name of the target.
     * @param taskUUID The UUID of the task.
     * @return The parameters if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID) const
        -> std::optional<json>;

    /**
     * @brief Sets parameters for a target.
     * @param targetName The name of the target.
     * @param params The parameters in JSON format.
     */
    void setTargetParams(const std::string& targetName, const json& params);

    /**
     * @brief Gets parameters for a target.
     * @param targetName The name of the target.
     * @return The parameters if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getTargetParams(const std::string& targetName) const
        -> std::optional<json>;

    /**
     * @brief Saves the sequence to the database.
     */
    void saveToDatabase();

    /**
     * @brief Loads a sequence from the database.
     * @param uuid The UUID of the sequence.
     */
    void loadFromDatabase(const std::string& uuid);

    /**
     * @brief Lists all sequences in the database.
     * @return A vector of sequence models.
     */
    std::vector<SequenceModel> listSequences();

    /**
     * @brief Deletes a sequence from the database.
     * @param uuid The UUID of the sequence to delete.
     */
    void deleteFromDatabase(const std::string& uuid);

    /**
     * @brief Sets the task generator.
     * @param generator The task generator to use.
     */
    void setTaskGenerator(std::shared_ptr<TaskGenerator> generator);

    /**
     * @brief Gets the current task generator.
     * @return The current task generator.
     */
    auto getTaskGenerator() const -> std::shared_ptr<TaskGenerator>;

    /**
     * @brief Processes a target with macros.
     * @param targetName The name of the target.
     */
    void processTargetWithMacros(const std::string& targetName);

    /**
     * @brief Processes all targets with macros.
     */
    void processAllTargetsWithMacros();

    /**
     * @brief Adds a macro to the sequence.
     * @param name The name of the macro.
     * @param value The value of the macro.
     */
    void addMacro(const std::string& name, MacroValue value);

    /**
     * @brief Removes a macro from the sequence.
     * @param name The name of the macro.
     */
    void removeMacro(const std::string& name);

    /**
     * @brief Lists all macros in the sequence.
     * @return A vector of macro names.
     */
    auto listMacros() const -> std::vector<std::string>;

    /**
     * @brief Sets the execution strategy.
     * @param strategy The execution strategy to use.
     */
    void setExecutionStrategy(ExecutionStrategy strategy);

    /**
     * @brief Gets the current execution strategy.
     * @return The current execution strategy.
     */
    [[nodiscard]] auto getExecutionStrategy() const -> ExecutionStrategy;

    /**
     * @brief Sets the concurrency limit for parallel execution.
     * @param limit Maximum number of concurrent tasks.
     */
    void setConcurrencyLimit(size_t limit);

    /**
     * @brief Gets the current concurrency limit.
     * @return The concurrency limit.
     */
    [[nodiscard]] auto getConcurrencyLimit() const -> size_t;

    /**
     * @brief Enables or disables monitoring.
     * @param enabled True to enable monitoring.
     */
    void enableMonitoring(bool enabled);

    /**
     * @brief Checks if monitoring is enabled.
     * @return True if monitoring is enabled.
     */
    [[nodiscard]] auto isMonitoringEnabled() const -> bool;

    /**
     * @brief Enables script integration.
     * @param enabled True to enable script execution.
     */
    void enableScriptIntegration(bool enabled);

    /**
     * @brief Sets resource limits for execution.
     * @param maxCpuUsage Maximum CPU usage percentage.
     * @param maxMemoryUsage Maximum memory usage in bytes.
     */
    void setResourceLimits(double maxCpuUsage, size_t maxMemoryUsage);

    /**
     * @brief Enables performance optimization.
     * @param enabled True to enable optimization.
     */
    void enablePerformanceOptimization(bool enabled);

    /**
     * @brief Gets optimization suggestions.
     * @return JSON object with optimization suggestions.
     */
    [[nodiscard]] auto getOptimizationSuggestions() const -> json;

    /**
     * @brief Gets real-time metrics.
     * @return JSON object with current metrics.
     */
    [[nodiscard]] auto getMetrics() const -> json;

    // ========================================================================
    // Astronomical Scheduling Methods
    // ========================================================================

    /**
     * @brief Sets the observer location for astronomical calculations.
     * @param location The observer's geographic location.
     */
    void setObserverLocation(const ObserverLocation& location);

    /**
     * @brief Gets the current observer location.
     * @return The observer location.
     */
    [[nodiscard]] const ObserverLocation& getObserverLocation() const;

    /**
     * @brief Sorts targets by observability (best observable first).
     */
    void sortTargetsByObservability();

    /**
     * @brief Gets the next best observable target.
     * @return Pointer to the best target, or nullptr if none available.
     */
    [[nodiscard]] Target* getNextObservableTarget();

    /**
     * @brief Updates observability windows for all targets.
     */
    void updateTargetObservability();

    /**
     * @brief Checks if any target needs a meridian flip.
     * @return Name of target needing flip, or empty if none.
     */
    [[nodiscard]] std::string checkMeridianFlips() const;

    /**
     * @brief Gets a summary of tonight's observable targets.
     * @return JSON array with target observability info.
     */
    [[nodiscard]] json getObservabilitySummary() const;

    /**
     * @brief Sets the minimum altitude constraint for all targets.
     * @param altitude Minimum altitude in degrees.
     */
    void setMinimumAltitude(double altitude);

    /**
     * @brief Gets estimated completion time for sequence.
     * @return Estimated completion timestamp.
     */
    [[nodiscard]] std::chrono::system_clock::time_point
    getEstimatedCompletionTime() const;

    /**
     * @brief Checks if sequence can complete before dawn.
     * @param dawnTime Time of astronomical dawn.
     * @return True if sequence can complete in time.
     */
    [[nodiscard]] bool canCompleteBeforeDawn(
        std::chrono::system_clock::time_point dawnTime) const;

    /**
     * @brief Gets targets that can be completed before dawn.
     * @param dawnTime Time of astronomical dawn.
     * @return Vector of target names that can complete in time.
     */
    [[nodiscard]] std::vector<std::string> getTargetsCompletableBeforeDawn(
        std::chrono::system_clock::time_point dawnTime) const;

private:
    std::vector<std::unique_ptr<Target>>
        targets_;                      ///< The targets in the sequence
    mutable std::shared_mutex mutex_;  ///< Mutex for thread safety
    std::atomic<SequenceState> state_{
        SequenceState::Idle};      ///< Current state of the sequence
    std::jthread sequenceThread_;  ///< Thread for executing the sequence

    // Progress tracking
    std::atomic<size_t> completedTargets_{0};  ///< Number of completed targets
    size_t totalTargets_{0};                   ///< Total number of targets

    // Callback functions
    SequenceCallback onSequenceStart_;  ///< Called when sequence starts
    SequenceCallback onSequenceEnd_;    ///< Called when sequence ends
    TargetCallback onTargetStart_;      ///< Called when a target starts
    TargetCallback onTargetEnd_;        ///< Called when a target ends
    ErrorCallback onError_;             ///< Called when an error occurs
    ProgressCallback onProgress_;       ///< Called for progress updates
    TaskCallback onTaskStart_;          ///< Called when a task starts
    TaskCallback onTaskEnd_;            ///< Called when a task ends

    // Current execution tracking
    std::string currentTargetName_;  ///< Name of currently executing target
    std::string currentTaskName_;    ///< Name of currently executing task
    std::chrono::steady_clock::time_point
        executionStartTime_;  ///< When execution started

    SchedulingStrategy schedulingStrategy_{
        SchedulingStrategy::FIFO};  ///< Current scheduling strategy
    RecoveryStrategy recoveryStrategy_{
        RecoveryStrategy::Stop};  ///< Current recovery strategy
    std::map<std::string, std::unique_ptr<Target>>
        alternativeTargets_;  ///< Alternative targets for recovery

    // Helper methods

    /**
     * @brief Executes the sequence of targets.
     */
    void executeSequence();

    /**
     * @brief Notifies that the sequence has started.
     */
    void notifySequenceStart();

    /**
     * @brief Notifies that the sequence has ended.
     */
    void notifySequenceEnd();

    /**
     * @brief Notifies that a target has started.
     * @param name The name of the target.
     */
    void notifyTargetStart(const std::string& name);

    /**
     * @brief Notifies that a target has ended.
     * @param name The name of the target.
     * @param status The status of the target.
     */
    void notifyTargetEnd(const std::string& name, TargetStatus status);

    /**
     * @brief Notifies that an error has occurred.
     * @param name The name of the target.
     * @param e The exception that was thrown.
     */
    void notifyError(const std::string& name, const std::exception& e);

    /**
     * @brief Notifies progress update.
     */
    void notifyProgress();

    /**
     * @brief Notifies that a task has started.
     * @param targetName The name of the target.
     * @param taskName The name of the task.
     * @param taskInfo Additional task information.
     */
    void notifyTaskStart(const std::string& targetName,
                         const std::string& taskName, const json& taskInfo);

    /**
     * @brief Notifies that a task has ended.
     * @param targetName The name of the target.
     * @param taskName The name of the task.
     * @param taskInfo Task result information.
     */
    void notifyTaskEnd(const std::string& targetName,
                       const std::string& taskName, const json& taskInfo);

    /**
     * @brief Builds progress JSON object.
     * @return JSON with current progress information.
     */
    [[nodiscard]] json buildProgressJson() const;

    // Scheduling helper methods

    /**
     * @brief Reorders targets based on dependencies.
     */
    void reorderTargetsByDependencies();

    /**
     * @brief Gets the next target that is ready to execute.
     * @return Pointer to the next executable target, or nullptr if none.
     */
    [[nodiscard]] auto getNextExecutableTarget() -> Target*;

    /**
     * @brief Updates the ready status of all targets.
     */
    void updateTargetReadyStatus();

    /**
     * @brief Finds any cyclic dependencies in the target graph.
     * @return The name of a target in a cycle, or std::nullopt if none.
     */
    auto findCyclicDependency() const -> std::optional<std::string>;

    /**
     * @brief Reorders targets based on priority.
     */
    void reorderTargetsByPriority();

    std::unordered_map<std::string, std::vector<std::string>>
        targetDependencies_;  ///< Maps targets to their dependencies
    std::unordered_map<std::string, bool>
        targetReadyStatus_;  ///< Ready status of each target

    size_t maxConcurrentTargets_{
        1};  ///< Maximum targets to execute concurrently
    std::chrono::seconds globalTimeout_{
        0};                                 ///< Global timeout for the sequence
    std::atomic<size_t> failedTargets_{0};  ///< Number of failed targets
    std::vector<std::string> failedTargetNames_;  ///< Names of failed targets

    // Execution statistics
    struct ExecutionStats {
        std::chrono::steady_clock::time_point
            startTime;                     ///< When execution started
        size_t totalExecutions{0};         ///< Total targets executed
        size_t successfulExecutions{0};    ///< Successfully executed targets
        size_t failedExecutions{0};        ///< Failed targets
        double averageExecutionTime{0.0};  ///< Average execution time
    } stats_;

    std::string uuid_;  ///< Unique identifier for the sequence
    std::shared_ptr<database::Database> db_;  ///< Database connection
    std::unique_ptr<database::Table<SequenceModel>>
        sequenceTable_;  ///< Database table

    // Serialization helper methods

    /**
     * @brief Converts the sequence to JSON.
     * @return JSON representation of the sequence.
     */
    json serializeToJson() const;

    /**
     * @brief Initializes the sequence from JSON.
     * @param data The JSON data.
     */
    void deserializeFromJson(const json& data);

    std::shared_ptr<TaskGenerator>
        taskGenerator_;  ///< Task generator for processing macros

    // Execution strategy and monitoring
    ExecutionStrategy executionStrategy_{
        ExecutionStrategy::Sequential};     ///< Current execution strategy
    size_t concurrencyLimit_{4};            ///< Maximum concurrent tasks
    bool monitoringEnabled_{false};         ///< Monitoring flag
    bool scriptIntegrationEnabled_{false};  ///< Script integration flag
    bool performanceOptimizationEnabled_{
        false};  ///< Performance optimization flag

    struct ResourceLimits {
        double maxCpuUsage{80.0};  ///< Maximum CPU usage percentage
        size_t maxMemoryUsage{1024 * 1024 *
                              1024};  ///< Maximum memory usage (1GB default)
    } resourceLimits_;                ///< Resource limits

    // Helper methods

    /**
     * @brief Initializes default macros for the task generator.
     */
    void initializeDefaultMacros();

    /**
     * @brief Processes JSON with the task generator.
     * @param data The JSON data to process.
     */
    void processJsonWithGenerator(json& data);

    /**
     * @brief Executes sequence with sequential strategy.
     * @param targets Vector of targets to execute.
     */
    void executeSequential(const std::vector<Target*>& targets);

    /**
     * @brief Executes sequence with parallel strategy.
     * @param targets Vector of targets to execute.
     */
    void executeParallel(const std::vector<Target*>& targets);

    /**
     * @brief Executes sequence with adaptive strategy.
     * @param targets Vector of targets to execute.
     */
    void executeAdaptive(const std::vector<Target*>& targets);

    /**
     * @brief Executes sequence with priority strategy.
     * @param targets Vector of targets to execute.
     */
    void executePriority(const std::vector<Target*>& targets);

    /**
     * @brief Updates resource metrics.
     */
    void updateResourceMetrics();

    /**
     * @brief Checks if resources are available.
     * @return True if resources are available.
     */
    [[nodiscard]] auto checkResourceAvailability() const -> bool;

    /**
     * @brief Determines optimal execution strategy.
     * @return Optimal execution strategy.
     */
    [[nodiscard]] auto determineOptimalStrategy() const -> ExecutionStrategy;

    /**
     * @brief Analyzes performance and generates suggestions.
     * @return Optimization suggestions.
     */
    [[nodiscard]] auto analyzePerformance() const -> json;

    // Astronomical scheduling data
    ObserverLocation observerLocation_;  ///< Observer's geographic location
    double minimumAltitude_{15.0};       ///< Minimum target altitude (degrees)
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_CORE_SEQUENCER_HPP
