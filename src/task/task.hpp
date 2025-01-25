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

namespace lithium::sequencer {
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

// 在 TaskStatus 枚举后添加
enum class TaskErrorType {
    None,
    Timeout,
    InvalidParameter,
    DeviceError,
    SystemError,
    Unknown
};

/**
 * @class Task
 * @brief Represents a task that can be executed with an optional timeout.
 */
class Task {
public:
    using ExceptionCallback = std::function<void(const std::exception&)>;

    // 参数定义结构体
    struct ParamDefinition {
        std::string name;
        std::string type;
        bool required;
        json defaultValue;
        std::string description;
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
    void execute(const json& params);

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

    // 设置任务优先级 (1-10，数字越大优先级越高)
    void setPriority(int priority);
    [[nodiscard]] auto getPriority() const -> int;

    // 添加任务依赖
    void addDependency(const std::string& taskId);
    void removeDependency(const std::string& taskId);
    [[nodiscard]] auto hasDependency(const std::string& taskId) const -> bool;
    [[nodiscard]] auto getDependencies() const
        -> const std::vector<std::string>&;
    [[nodiscard]] auto isDependencySatisfied() const -> bool;
    void setDependencyStatus(const std::string& taskId, bool status);

    // 任务性能监控
    [[nodiscard]] auto getExecutionTime() const -> std::chrono::milliseconds;
    [[nodiscard]] auto getMemoryUsage() const -> size_t;

    // 新增日志级别控制
    void setLogLevel(int level);
    [[nodiscard]] auto getLogLevel() const -> int;

    // 新增错误处理相关
    void setErrorType(TaskErrorType type);
    [[nodiscard]] auto getErrorType() const -> TaskErrorType;
    [[nodiscard]] auto getErrorDetails() const -> std::string;

    // 新增性能监控
    [[nodiscard]] auto getCPUUsage() const -> double;
    [[nodiscard]] auto getTaskHistory() const -> std::vector<std::string>;
    void addHistoryEntry(const std::string& entry);

    // 参数相关方法
    void addParamDefinition(const std::string& name, const std::string& type,
                            bool required, const json& defaultValue = nullptr,
                            const std::string& description = "");
    void removeParamDefinition(const std::string& name);
    [[nodiscard]] auto getParamDefinitions() const
        -> const std::vector<ParamDefinition>&;
    [[nodiscard]] auto validateParams(const json& params) -> bool;
    [[nodiscard]] auto getParamErrors() const
        -> const std::vector<std::string>&;

    // 前置任务相关方法
    void addPreTask(std::unique_ptr<Task> task);
    void removePreTask(std::unique_ptr<Task> task);
    [[nodiscard]] auto getPreTasks() const
        -> const std::vector<std::unique_ptr<Task>>&;
    [[nodiscard]] auto arePreTasksCompleted() const -> bool;

    // 后置任务相关方法
    void addPostTask(std::unique_ptr<Task> task);
    void removePostTask(std::unique_ptr<Task> task);
    [[nodiscard]] auto getPostTasks() const
        -> const std::vector<std::unique_ptr<Task>>&;
    void triggerPostTasks();

    // 异常处理回调
    void setExceptionCallback(ExceptionCallback callback);
    void clearExceptionCallback();

    json toJson() const;

private:
    std::string name_;  ///< The name of the task.
    std::string uuid_;  ///< The unique identifier of the task.
    std::function<void(const json&)>
        action_;  ///< The action to be performed by the task.
    std::chrono::seconds timeout_{0};  ///< The timeout duration for the task.
    TaskStatus status_{
        TaskStatus::Pending};  ///< The current status of the task.
    std::optional<std::string>
        error_;        ///< The error message if the task has failed.
    int priority_{5};  // 默认优先级为5
    std::vector<std::string> dependencies_;  // 任务依赖列表
    std::unordered_map<std::string, bool> dependencyStatus_;  // 依赖项完成状态
    std::chrono::milliseconds executionTime_{0};              // 执行时间
    size_t memoryUsage_{0};                                   // 内存使用量
    int logLevel_{2};  // 默认日志级别
    TaskErrorType errorType_{TaskErrorType::None};
    std::string errorDetails_;
    double cpuUsage_{0.0};
    std::vector<std::string> taskHistory_;
    std::vector<ParamDefinition> paramDefinitions_;
    std::vector<std::string> paramErrors_;
    std::vector<std::unique_ptr<Task>> preTasks_;   // 前置任务列表
    std::vector<std::unique_ptr<Task>> postTasks_;  // 后置任务列表
    ExceptionCallback exceptionCallback_;           // 异常处理回调

    bool validateParamType(const std::string& type, const json& value) const;
};

/**
 * @brief Base class for task creation using static polymorphism.
 */
template <typename Derived>
class TaskCreator {
public:
    static auto createTask() -> std::unique_ptr<Task> {
        return std::make_unique<Task>(Derived::taskName(), Derived::execute);
    }
};

}  // namespace lithium::sequencer

#endif  // TASK_HPP
