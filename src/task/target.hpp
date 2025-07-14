/**
 * @file target.hpp
 * @brief Defines the Target class for managing task execution with
 * dependencies.
 */

#ifndef LITHIUM_TARGET_HPP
#define LITHIUM_TARGET_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/async/safetype.hpp"
#include "task.hpp"

namespace lithium::task {

/**
 * @enum TargetStatus
 * @brief Represents the status of a target.
 */
enum class TargetStatus {
    Pending,     ///< Target is pending and has not started yet.
    InProgress,  ///< Target is currently in progress.
    Completed,   ///< Target has completed successfully.
    Failed,      ///< Target has failed.
    Skipped      ///< Target was skipped.
};

/**
 * @brief Callback function type definitions.
 */
using TargetStartCallback = std::function<void(const std::string&)>;
using TargetEndCallback = std::function<void(const std::string&, TargetStatus)>;
using TargetErrorCallback =
    std::function<void(const std::string&, const std::exception&)>;

class Target;

/**
 * @brief Target modifier function type definition.
 */
using TargetModifier = std::function<void(Target&)>;

/**
 * @class Target
 * @brief Represents a target that can execute a series of tasks with optional
 * retries and cooldown periods.
 */
class Target {
public:
    /**
     * @brief Constructs a Target with a name, cooldown time, and max retries.
     * @param name The name of the target.
     * @param cooldown The cooldown time between retries.
     * @param maxRetries The maximum number of retries.
     */
    explicit Target(std::string name,
                    std::chrono::seconds cooldown = std::chrono::seconds(0),
                    int maxRetries = 0);

    // Disable copy constructor and assignment operator
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;

    // Task Management Methods

    /**
     * @brief Adds a task to the target.
     * @param task The task to add.
     */
    void addTask(std::unique_ptr<Task> task);

    /**
     * @brief Sets the cooldown period for the target.
     * @param cooldown The cooldown period in seconds.
     */
    void setCooldown(std::chrono::seconds cooldown);

    /**
     * @brief Enables or disables the target.
     * @param enabled True to enable, false to disable.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Sets the maximum number of retries for each task.
     * @param retries The maximum number of retries.
     */
    void setMaxRetries(int retries);

    /**
     * @brief Sets the status of the target.
     * @param status The status to be set.
     */
    void setStatus(TargetStatus status);

    /**
     * @brief Sets the callback function to be called when the target starts.
     * @param callback The callback function.
     */
    void setOnStart(TargetStartCallback callback);

    /**
     * @brief Sets the callback function to be called when the target ends.
     * @param callback The callback function.
     */
    void setOnEnd(TargetEndCallback callback);

    /**
     * @brief Sets the callback function to be called when an error occurs.
     * @param callback The callback function.
     */
    void setOnError(TargetErrorCallback callback);

    /**
     * @brief Gets the name of the target.
     * @return The name of the target.
     */
    [[nodiscard]] const std::string& getName() const;

    /**
     * @brief Gets the UUID of the target.
     * @return The UUID of the target.
     */
    [[nodiscard]] const std::string& getUUID() const;

    /**
     * @brief Gets the current status of the target.
     * @return The current status of the target.
     */
    [[nodiscard]] TargetStatus getStatus() const;

    /**
     * @brief Checks if the target is enabled.
     * @return True if the target is enabled, false otherwise.
     */
    [[nodiscard]] bool isEnabled() const;

    /**
     * @brief Gets the progress of the target as a percentage.
     * @return The progress percentage.
     */
    [[nodiscard]] double getProgress() const;

    /**
     * @brief Executes the target.
     */
    void execute();

    /**
     * @brief Loads tasks from a JSON array.
     * @param tasksJson The JSON array containing task definitions.
     */
    void loadTasksFromJson(const json& tasksJson);

    /**
     * @brief Gets the dependencies for this target.
     * @return Vector of dependency strings.
     */
    auto getDependencies() -> std::vector<std::string>;

    /**
     * @brief Gets the tasks in this target.
     * @return Vector of tasks.
     */
    auto getTasks() -> const std::vector<std::unique_ptr<Task>>&;

    /**
     * @brief Sets parameters for a specific task
     * @param taskUUID The UUID of the task
     * @param params The parameters in JSON format
     */
    void setTaskParams(const std::string& taskUUID, const json& params);

    /**
     * @brief Gets parameters for a specific task
     * @param taskUUID The UUID of the task
     * @return The task parameters
     */
    [[nodiscard]] auto getTaskParams(const std::string& taskUUID) const
        -> std::optional<json>;

    /**
     * @brief Sets parameters for all tasks in this target
     * @param params The parameters in JSON format
     */
    void setParams(const json& params);

    /**
     * @brief Gets the parameters for this target's tasks
     * @return The current parameters
     */
    [[nodiscard]] auto getParams() const -> const json&;

    /**
     * @brief Serializes the target to JSON.
     * @param includeRuntime Whether to include runtime information.
     * @return JSON representation of the target.
     */
    [[nodiscard]] auto toJson(bool includeRuntime = true) const -> json;

    /**
     * @brief Initializes a target from JSON.
     * @param data The JSON data.
     */
    auto fromJson(const json& data) -> void;

    /**
     * @brief Creates a target from JSON.
     * @param data The JSON data.
     * @return A new Target instance.
     */
    static std::unique_ptr<Target> createFromJson(const json& data);

    /**
     * @brief Validates the JSON schema for target serialization.
     * @param data The JSON data to validate.
     * @return True if valid, false otherwise.
     */
    static bool validateJson(const json& data);

    /**
     * @brief Creates a new task group.
     * @param groupName The name of the group.
     */
    void createTaskGroup(const std::string& groupName);

    /**
     * @brief Adds a task to a group.
     * @param groupName The name of the group.
     * @param taskUUID The UUID of the task to add.
     */
    void addTaskToGroup(const std::string& groupName,
                        const std::string& taskUUID);

    /**
     * @brief Removes a task from a group.
     * @param groupName The name of the group.
     * @param taskUUID The UUID of the task to remove.
     */
    void removeTaskFromGroup(const std::string& groupName,
                             const std::string& taskUUID);

    /**
     * @brief Gets the tasks in a group.
     * @param groupName The name of the group.
     * @return Vector of task UUIDs in the group.
     */
    std::vector<std::string> getTaskGroup(const std::string& groupName) const;

    /**
     * @brief Adds a dependency between tasks.
     * @param taskUUID The UUID of the task.
     * @param dependsOnUUID The UUID of the task it depends on.
     */
    void addTaskDependency(const std::string& taskUUID,
                           const std::string& dependsOnUUID);

    /**
     * @brief Removes a dependency between tasks.
     * @param taskUUID The UUID of the task.
     * @param dependsOnUUID The UUID of the task it depends on.
     */
    void removeTaskDependency(const std::string& taskUUID,
                              const std::string& dependsOnUUID);

    /**
     * @brief Checks if all dependencies for a task are satisfied.
     * @param taskUUID The UUID of the task.
     * @return True if all dependencies are satisfied.
     */
    bool checkDependencies(const std::string& taskUUID) const;

    /**
     * @brief Executes all tasks in a group.
     * @param groupName The name of the group.
     */
    void executeGroup(const std::string& groupName);

    /**
     * @brief Aborts execution of a task group.
     * @param groupName The name of the group.
     */
    void abortGroup(const std::string& groupName);

private:
    std::string name_;  ///< The name of the target.
    std::string uuid_;  ///< The unique identifier of the target.
    std::vector<std::unique_ptr<Task>>
        tasks_;  ///< The list of tasks to be executed by the target.
    std::vector<std::string> dependencies_;  ///< The list of task dependencies.
    std::chrono::seconds
        cooldown_;        ///< The cooldown period between task executions.
    bool enabled_{true};  ///< Indicates whether the target is enabled.
    std::atomic<TargetStatus> status_{
        TargetStatus::Pending};  ///< The current status of the target.
    std::shared_mutex
        mutex_;  ///< Mutex for thread-safe access to target properties.

    // Progress tracking
    std::atomic<size_t> completedTasks_{0};  ///< The number of completed tasks.
    size_t totalTasks_{0};                   ///< The total number of tasks.

    // Callback functions
    TargetStartCallback
        onStart_;  ///< Callback function to be called when the target starts.
    TargetEndCallback
        onEnd_;  ///< Callback function to be called when the target ends.
    TargetErrorCallback
        onError_;  ///< Callback function to be called when an error occurs.

    // Retry mechanism
    int maxRetries_;  ///< The maximum number of retries for each task.
    mutable std::shared_mutex callbackMutex_;  ///< Mutex for callback access

    std::shared_ptr<atom::async::LockFreeHashTable<std::string, json>>
        queue_;  ///< The task queue.

    // Task parameters storage
    std::unordered_map<std::string, json>
        taskParams_;                         ///< Individual task parameters
    mutable std::shared_mutex paramsMutex_;  ///< Mutex for parameter access

    json params_;  ///< Parameters for all tasks in this target

    // Task group management
    std::unordered_map<std::string, std::vector<std::string>>
        taskGroups_;                ///< Task groups
    std::shared_mutex groupMutex_;  ///< Mutex for group access

    // Task dependency relationships
    std::unordered_map<std::string, std::vector<std::string>>
        taskDependencies_;                ///< Task dependencies
    mutable std::shared_mutex depMutex_;  ///< Mutex for dependency access

    /**
     * @brief Notifies that the target has started.
     */
    void notifyStart();

    /**
     * @brief Notifies that the target has ended.
     * @param status The final status of the target.
     */
    void notifyEnd(TargetStatus status);

    /**
     * @brief Notifies that an error occurred in the target.
     * @param e The exception that occurred.
     */
    void notifyError(const std::exception& e);
};

}  // namespace lithium::task

#endif  // LITHIUM_TARGET_HPP
