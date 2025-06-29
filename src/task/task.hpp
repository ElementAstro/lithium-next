/**
 * @file task.hpp
 * @brief Defines the Task class for executing tasks with optional timeout.
 */

#ifndef TASK_HPP
#define TASK_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "atom/type/json.hpp"

namespace lithium::task {
using json = nlohmann::json;

/**
 * @enum TaskStatus
 * @brief Represents the status of a task.
 */
enum class TaskStatus {
    Pending,     ///< Task is pending and has not started yet.
    InProgress,  ///< Task is currently in progress.
    Completed,   ///< Task has completed successfully.
    Failed       ///< Task has failed.
};

/**
 * @enum TaskErrorType
 * @brief Represents the type of error that occurred during task execution.
 */
enum class TaskErrorType {
    None,              ///< No error has occurred.
    Timeout,           ///< Task execution timed out.
    InvalidParameter,  ///< Task parameters were invalid.
    DeviceError,       ///< An error occurred with a device.
    SystemError,       ///< A system error occurred.
    Unknown            ///< An unknown error occurred.
};

/**
 * @class Task
 * @brief Represents a task that can be executed with an optional timeout.
 */
class Task {
public:
    using ExceptionCallback = std::function<void(const std::exception&)>;

    /**
     * @struct ParamDefinition
     * @brief Defines a parameter for a task.
     */
    struct ParamDefinition {
        std::string name;         ///< The name of the parameter.
        std::string type;         ///< The type of the parameter.
        bool required;            ///< Whether the parameter is required.
        json defaultValue;        ///< The default value for the parameter.
        std::string description;  ///< A description of the parameter.
    };

    /**
     * @brief Constructs a Task with a given name and action.
     * @param name The name of the task.
     * @param action The action to be performed by the task.
     */
    Task(std::string name, std::function<void(const json&)> action);

    /**
     * @brief Executes the task with the given parameters.
     * @param params The parameters to be passed to the task action.
     */
    virtual void execute(const json& params);

    /**
     * @brief Sets the timeout for the task.
     * @param timeout The timeout duration in seconds.
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Gets the name of the task.
     * @return The name of the task.
     */
    [[nodiscard]] auto getName() const -> const std::string&;

    /**
     * @brief Gets the UUID of the task.
     * @return The UUID of the task.
     */
    [[nodiscard]] auto getUUID() const -> const std::string&;

    /**
     * @brief Gets the current status of the task.
     * @return The current status of the task.
     */
    [[nodiscard]] auto getStatus() const -> TaskStatus;

    /**
     * @brief Gets the error message if the task has failed.
     * @return An optional string containing the error message if the task has
     * failed, otherwise std::nullopt.
     */
    [[nodiscard]] auto getError() const -> std::optional<std::string>;

    /**
     * @brief Sets the task priority (1-10, higher is more important).
     * @param priority The priority value.
     */
    void setPriority(int priority);

    /**
     * @brief Gets the task priority.
     * @return The priority value.
     */
    [[nodiscard]] auto getPriority() const -> int;

    /**
     * @brief Adds a task dependency.
     * @param taskId The ID of the task this task depends on.
     */
    void addDependency(const std::string& taskId);

    /**
     * @brief Removes a task dependency.
     * @param taskId The ID of the dependency to remove.
     */
    void removeDependency(const std::string& taskId);

    /**
     * @brief Checks if this task has a specific dependency.
     * @param taskId The task ID to check.
     * @return True if the task dependency exists.
     */
    [[nodiscard]] auto hasDependency(const std::string& taskId) const -> bool;

    /**
     * @brief Gets all task dependencies.
     * @return Vector of dependency task IDs.
     */
    [[nodiscard]] auto getDependencies() const
        -> const std::vector<std::string>&;

    /**
     * @brief Checks if all dependencies are satisfied.
     * @return True if all dependencies are satisfied.
     */
    [[nodiscard]] auto isDependencySatisfied() const -> bool;

    /**
     * @brief Sets the status of a dependency.
     * @param taskId The task ID of the dependency.
     * @param status The completion status to set.
     */
    void setDependencyStatus(const std::string& taskId, bool status);

    /**
     * @brief Gets the task execution time.
     * @return The execution time in milliseconds.
     */
    [[nodiscard]] auto getExecutionTime() const -> std::chrono::milliseconds;

    /**
     * @brief Gets the task memory usage.
     * @return The memory usage in bytes.
     */
    [[nodiscard]] auto getMemoryUsage() const -> size_t;

    /**
     * @brief Sets the log level for this task.
     * @param level The log level to set.
     */
    void setLogLevel(int level);

    /**
     * @brief Gets the current log level.
     * @return The current log level.
     */
    [[nodiscard]] auto getLogLevel() const -> int;

    /**
     * @brief Sets the error type for this task.
     * @param type The error type to set.
     */
    void setErrorType(TaskErrorType type);

    /**
     * @brief Gets the current error type.
     * @return The current error type.
     */
    [[nodiscard]] auto getErrorType() const -> TaskErrorType;

    /**
     * @brief Gets detailed error information.
     * @return The error details as a string.
     */
    [[nodiscard]] auto getErrorDetails() const -> std::string;

    /**
     * @brief Gets the CPU usage of this task.
     * @return The CPU usage as a percentage.
     */
    [[nodiscard]] auto getCPUUsage() const -> double;

    /**
     * @brief Gets the task execution history.
     * @return Vector of history entries.
     */
    [[nodiscard]] auto getTaskHistory() const -> std::vector<std::string>;

    /**
     * @brief Adds an entry to the task history.
     * @param entry The history entry to add.
     */
    void addHistoryEntry(const std::string& entry);

    /**
     * @brief Adds a parameter definition to the task.
     * @param name The name of the parameter.
     * @param type The type of the parameter.
     * @param required Whether the parameter is required.
     * @param defaultValue The default value for the parameter.
     * @param description A description of the parameter.
     */
    void addParamDefinition(const std::string& name, const std::string& type,
                            bool required, const json& defaultValue = nullptr,
                            const std::string& description = "");

    /**
     * @brief Removes a parameter definition.
     * @param name The name of the parameter to remove.
     */
    void removeParamDefinition(const std::string& name);

    /**
     * @brief Gets all parameter definitions.
     * @return Vector of parameter definitions.
     */
    [[nodiscard]] auto getParamDefinitions() const
        -> const std::vector<ParamDefinition>&;

    /**
     * @brief Validates parameters against their definitions.
     * @param params The parameters to validate.
     * @return True if all parameters are valid.
     */
    [[nodiscard]] auto validateParams(const json& params) -> bool;

    /**
     * @brief Gets any parameter validation errors.
     * @return Vector of error messages.
     */
    [[nodiscard]] auto getParamErrors() const
        -> const std::vector<std::string>&;

    /**
     * @brief Adds a pre-task that must complete before this task.
     * @param task The task to add.
     */
    void addPreTask(std::unique_ptr<Task> task);

    /**
     * @brief Removes a pre-task.
     * @param task The task to remove.
     */
    void removePreTask(std::unique_ptr<Task> task);

    /**
     * @brief Gets all pre-tasks.
     * @return Vector of pre-tasks.
     */
    [[nodiscard]] auto getPreTasks() const
        -> const std::vector<std::unique_ptr<Task>>&;

    /**
     * @brief Checks if all pre-tasks have completed.
     * @return True if all pre-tasks are completed.
     */
    [[nodiscard]] auto arePreTasksCompleted() const -> bool;

    /**
     * @brief Adds a post-task to execute after this task.
     * @param task The task to add.
     */
    void addPostTask(std::unique_ptr<Task> task);

    /**
     * @brief Removes a post-task.
     * @param task The task to remove.
     */
    void removePostTask(std::unique_ptr<Task> task);

    /**
     * @brief Gets all post-tasks.
     * @return Vector of post-tasks.
     */
    [[nodiscard]] auto getPostTasks() const
        -> const std::vector<std::unique_ptr<Task>>&;

    /**
     * @brief Triggers execution of all post-tasks.
     */
    void triggerPostTasks();

    /**
     * @brief Sets an exception callback function.
     * @param callback The callback function to set.
     */
    void setExceptionCallback(ExceptionCallback callback);

    /**
     * @brief Clears the exception callback function.
     */
    void clearExceptionCallback();

    /**
     * @brief Converts the task to a JSON representation.
     * @return The JSON representation of the task.
     */
    json toJson() const;

    /**
     * @brief Sets the task type for factory-based creation.
     * @param type The task type identifier.
     */
    void setTaskType(const std::string& type);

    /**
     * @brief Gets the task type identifier.
     * @return The task type identifier.
     */
    [[nodiscard]] auto getTaskType() const -> const std::string&;

    void setResult(const json& result) { result_ = result; }

    json getResult() const { return result_; }

private:
    std::string name_;  ///< The name of the task.
    std::string uuid_;  ///< The unique identifier of the task.
    std::string
        taskType_;  ///< The task type identifier for factory-based creation.
    json result_;   ///< The result of the task execution.
    std::function<void(const json&)>
        action_;  ///< The action to be performed by the task.
    std::chrono::seconds timeout_{0};  ///< The timeout duration for the task.
    TaskStatus status_{
        TaskStatus::Pending};  ///< The current status of the task.
    std::optional<std::string>
        error_;        ///< The error message if the task has failed.
    int priority_{5};  ///< Default priority is 5
    std::vector<std::string> dependencies_;  ///< Task dependency list
    std::unordered_map<std::string, bool>
        dependencyStatus_;  ///< Dependency completion status
    std::chrono::milliseconds executionTime_{0};    ///< Execution time
    size_t memoryUsage_{0};                         ///< Memory usage
    int logLevel_{2};                               ///< Default log level
    TaskErrorType errorType_{TaskErrorType::None};  ///< Type of error
    std::string errorDetails_;              ///< Detailed error information
    double cpuUsage_{0.0};                  ///< CPU usage
    std::vector<std::string> taskHistory_;  ///< Task execution history
    std::vector<ParamDefinition> paramDefinitions_;  ///< Parameter definitions
    std::vector<std::string> paramErrors_;  ///< Parameter validation errors
    std::vector<std::unique_ptr<Task>> preTasks_;   ///< Pre-tasks list
    std::vector<std::unique_ptr<Task>> postTasks_;  ///< Post-tasks list
    ExceptionCallback exceptionCallback_;  ///< Exception handler callback

    /**
     * @brief Validates a parameter value against its type.
     * @param type The expected type.
     * @param value The value to validate.
     * @return True if the value matches the type.
     */
    bool validateParamType(const std::string& type, const json& value) const;
};

}  // namespace lithium::task

#endif  // TASK_HPP
