#include "task.hpp"

#include "atom/async/packaged_task.hpp"
#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/uuid.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <numeric>

namespace lithium::task {

/**
 * @class TaskTimeoutException
 * @brief Exception thrown when a task times out.
 */
class TaskTimeoutException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_TASK_TIMEOUT_EXCEPTION(...)                                      \
    throw TaskTimeoutException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                               __VA_ARGS__);

Task::Task(std::string name, std::function<void(const json&)> action)
    : name_(std::move(name)),
      uuid_(atom::utils::UUID().toString()),
      action_(std::move(action)) {
    spdlog::info("Task created with name: {}, uuid: {}", name_, uuid_);
}

void Task::execute(const json& params) {
    auto start = std::chrono::high_resolution_clock::now();

    try {
        // Check if pre-tasks are completed
        if (!arePreTasksCompleted()) {
            throw std::runtime_error("Pre-tasks not completed");
        }

        // Validate parameters
        if (!validateParams(params)) {
            status_ = TaskStatus::Failed;
            errorType_ = TaskErrorType::InvalidParameter;
            error_ =
                "Parameter validation failed: " +
                std::accumulate(std::next(paramErrors_.begin()),
                                paramErrors_.end(), paramErrors_[0],
                                [](const std::string& a, const std::string& b) {
                                    return a + "; " + b;
                                });
            return;
        }

        spdlog::info("Task {} executing with params: {}", name_, params.dump());
        status_ = TaskStatus::InProgress;
        error_.reset();
        errorType_ = TaskErrorType::None;

        if (timeout_ > std::chrono::seconds{0}) {
            spdlog::info("Task {} with uuid {} executing with timeout {}s",
                         name_, uuid_, timeout_.count());
            atom::async::EnhancedPackagedTask<void, const json&> task(action_);
            auto future = task.getEnhancedFuture();
            task(params);
            if (!future.waitFor(timeout_)) {
                THROW_TASK_TIMEOUT_EXCEPTION("Task timed out");
            }
        } else {
            spdlog::info("Task {} with uuid {} executing without timeout",
                         name_, uuid_);
            action_(params);
        }
        status_ = TaskStatus::Completed;
        addHistoryEntry("Task executed successfully");

        // Trigger post-tasks
        triggerPostTasks();

    } catch (const std::exception& e) {
        status_ = TaskStatus::Failed;
        error_ = e.what();

        // Call exception callback
        if (exceptionCallback_) {
            try {
                exceptionCallback_(e);
            } catch (const std::exception& callbackEx) {
                spdlog::error("Exception in callback handler: {}",
                              callbackEx.what());
            }
        }

        // Set appropriate error type
        if (dynamic_cast<const TaskTimeoutException*>(&e)) {
            setErrorType(TaskErrorType::Timeout);
        } else if (dynamic_cast<const std::invalid_argument*>(&e)) {
            setErrorType(TaskErrorType::InvalidParameter);
        } else {
            setErrorType(TaskErrorType::Unknown);
        }
        errorDetails_ = e.what();
        spdlog::error("Task {} with uuid {} failed: {}", name_, uuid_,
                      e.what());
    }
    spdlog::info("Task {} with uuid {} completed", name_, uuid_);

    auto end = std::chrono::high_resolution_clock::now();
    executionTime_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

void Task::setTimeout(std::chrono::seconds timeout) { timeout_ = timeout; }

auto Task::getName() const -> const std::string& { return name_; }

auto Task::getUUID() const -> const std::string& { return uuid_; }

auto Task::getStatus() const -> TaskStatus { return status_; }

auto Task::getError() const -> std::optional<std::string> { return error_; }

void Task::setPriority(int priority) {
    if (priority < 1 || priority > 10) {
        throw std::invalid_argument("Priority must be between 1 and 10");
    }
    priority_ = priority;
}

auto Task::getPriority() const -> int { return priority_; }

void Task::addDependency(const std::string& taskId) {
    if (std::find(dependencies_.begin(), dependencies_.end(), taskId) ==
        dependencies_.end()) {
        dependencies_.push_back(taskId);
        dependencyStatus_[taskId] = false;
    }
}

void Task::removeDependency(const std::string& taskId) {
    auto it = std::remove(dependencies_.begin(), dependencies_.end(), taskId);
    if (it != dependencies_.end()) {
        dependencies_.erase(it, dependencies_.end());
        dependencyStatus_.erase(taskId);
    }
}

auto Task::hasDependency(const std::string& taskId) const -> bool {
    return std::find(dependencies_.begin(), dependencies_.end(), taskId) !=
           dependencies_.end();
}

auto Task::getDependencies() const -> const std::vector<std::string>& {
    return dependencies_;
}

void Task::setDependencyStatus(const std::string& taskId, bool status) {
    if (hasDependency(taskId)) {
        dependencyStatus_[taskId] = status;
    }
}

auto Task::isDependencySatisfied() const -> bool {
    for (const auto& dep : dependencies_) {
        if (!dependencyStatus_.at(dep)) {
            return false;
        }
    }
    return true;
}

auto Task::getExecutionTime() const -> std::chrono::milliseconds {
    return executionTime_;
}

auto Task::getMemoryUsage() const -> size_t { return memoryUsage_; }

void Task::setLogLevel(int level) {
    if (level < 0 || level > 4) {
        throw std::invalid_argument("Log level must be between 0 and 4");
    }
    logLevel_ = level;
}

auto Task::getLogLevel() const -> int { return logLevel_; }

void Task::setErrorType(TaskErrorType type) {
    errorType_ = type;
    spdlog::error("Task {} error type set to: {}", name_,
                  static_cast<int>(type));
}

auto Task::getErrorType() const -> TaskErrorType { return errorType_; }

auto Task::getErrorDetails() const -> std::string { return errorDetails_; }

auto Task::getCPUUsage() const -> double { return cpuUsage_; }

void Task::addHistoryEntry(const std::string& entry) {
    taskHistory_.push_back(entry);
    spdlog::info("Task {} history entry added: {}", name_, entry);
}

auto Task::getTaskHistory() const -> std::vector<std::string> {
    return taskHistory_;
}

void Task::addParamDefinition(const std::string& name, const std::string& type,
                              bool required, const json& defaultValue,
                              const std::string& description) {
    paramDefinitions_.push_back(
        {name, type, required, defaultValue, description});
    spdlog::info("Parameter definition added for task {}: {} ({})", name_, name,
                 type);
}

void Task::removeParamDefinition(const std::string& name) {
    auto it = std::remove_if(
        paramDefinitions_.begin(), paramDefinitions_.end(),
        [&name](const ParamDefinition& def) { return def.name == name; });
    if (it != paramDefinitions_.end()) {
        paramDefinitions_.erase(it, paramDefinitions_.end());
        spdlog::info("Parameter definition removed from task {}: {}", name_,
                     name);
    }
}

auto Task::getParamDefinitions() const -> const std::vector<ParamDefinition>& {
    return paramDefinitions_;
}

auto Task::validateParamType(const std::string& type, const json& value) const
    -> bool {
    if (type == "string" && !value.is_string()) {
        return false;
    }
    if (type == "number" && !value.is_number()) {
        return false;
    }
    if (type == "boolean" && !value.is_boolean()) {
        return false;
    }
    if (type == "array" && !value.is_array()) {
        return false;
    }
    if (type == "object" && !value.is_object()) {
        return false;
    }
    return true;
}

auto Task::validateParams(const json& params) -> bool {
    paramErrors_.clear();

    for (const auto& def : paramDefinitions_) {
        auto it = params.find(def.name);

        if (it == params.end()) {
            if (def.required) {
                paramErrors_.push_back("Missing required parameter: " +
                                       def.name);
                continue;
            }
            continue;
        }

        if (!validateParamType(def.type, *it)) {
            paramErrors_.push_back("Invalid type for parameter " + def.name +
                                   ": expected " + def.type);
        }
    }

    return paramErrors_.empty();
}

auto Task::getParamErrors() const -> const std::vector<std::string>& {
    return paramErrors_;
}

void Task::addPreTask(std::unique_ptr<Task> task) {
    if (task && std::find_if(preTasks_.begin(), preTasks_.end(),
                             [&task](const std::unique_ptr<Task>& t) {
                                 return t->getUUID() == task->getUUID();
                             }) == preTasks_.end()) {
        preTasks_.push_back(std::move(task));
        spdlog::info("Pre-task added to task {}", name_);
    }
}

void Task::removePreTask(std::unique_ptr<Task> task) {
    auto it = std::find_if(preTasks_.begin(), preTasks_.end(),
                           [&task](const std::unique_ptr<Task>& t) {
                               return t->getUUID() == task->getUUID();
                           });
    if (it != preTasks_.end()) {
        preTasks_.erase(it);
        spdlog::info("Pre-task removed from task {}", name_);
    }
}

auto Task::getPreTasks() const -> const std::vector<std::unique_ptr<Task>>& {
    return preTasks_;
}

auto Task::arePreTasksCompleted() const -> bool {
    return std::all_of(preTasks_.begin(), preTasks_.end(),
                       [](const std::unique_ptr<Task>& task) {
                           return task->getStatus() == TaskStatus::Completed;
                       });
}

void Task::addPostTask(std::unique_ptr<Task> task) {
    if (task && std::find_if(postTasks_.begin(), postTasks_.end(),
                             [&task](const std::unique_ptr<Task>& t) {
                                 return t->getUUID() == task->getUUID();
                             }) == postTasks_.end()) {
        postTasks_.push_back(std::move(task));
        spdlog::info("Post-task added to task {}", name_);
    }
}

void Task::removePostTask(std::unique_ptr<Task> task) {
    auto it = std::find_if(postTasks_.begin(), postTasks_.end(),
                           [&task](const std::unique_ptr<Task>& t) {
                               return t->getUUID() == task->getUUID();
                           });
    if (it != postTasks_.end()) {
        postTasks_.erase(it);
        spdlog::info("Post-task removed from task {}", name_);
    }
}

auto Task::getPostTasks() const -> const std::vector<std::unique_ptr<Task>>& {
    return postTasks_;
}

void Task::triggerPostTasks() {
    if (!postTasks_.empty()) {
        spdlog::info("Triggering {} post-tasks for task {}", postTasks_.size(),
                     name_);
        for (auto& postTask : postTasks_) {
            if (postTask->getStatus() == TaskStatus::Pending) {
                spdlog::info("Post-task {} is ready to be triggered",
                             postTask->getUUID());
                postTask->execute({});
            }
        }
    }
}

void Task::setExceptionCallback(ExceptionCallback callback) {
    exceptionCallback_ = std::move(callback);
    spdlog::info("Exception callback set for task {}", name_);
}

void Task::clearExceptionCallback() {
    exceptionCallback_ = nullptr;
    spdlog::info("Exception callback cleared for task {}", name_);
}

void Task::setTaskType(const std::string& type) {
    taskType_ = type;
    spdlog::info("Task '{}' type set to '{}'", name_, type);
}

auto Task::getTaskType() const -> const std::string& {
    return taskType_;
}

json Task::toJson() const {
    auto paramDefs = json::array();
    for (const auto& def : paramDefinitions_) {
        paramDefs.push_back({
            {"name", def.name},
            {"type", def.type},
            {"required", def.required},
            {"defaultValue", def.defaultValue},
            {"description", def.description},
        });
    }
    return {
        {"name", name_},
        {"uuid", uuid_},
        {"taskType", taskType_},
        {"status", static_cast<int>(status_)},
        {"error", error_.value_or("")},
        {"priority", priority_},
        {"dependencies", dependencies_},
        {"executionTime", executionTime_.count()},
        {"memoryUsage", memoryUsage_},
        {"logLevel", logLevel_},
        {"errorType", static_cast<int>(errorType_)},
        {"errorDetails", errorDetails_},
        {"cpuUsage", cpuUsage_},
        {"taskHistory", taskHistory_},
        {"paramDefinitions", paramDefs},
        {"preTasks", json::array()},
        {"postTasks", json::array()},
    };
}
}  // namespace lithium::task