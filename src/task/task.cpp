#include "task.hpp"
#include "exception.hpp"

#include "atom/async/packaged_task.hpp"
#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/uuid.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <numeric>

namespace lithium::task {

// Using the exception class defined in exception.hpp

Task::Task(std::string name, std::function<void(const json&)> action)
    : name_(std::move(name)),
      uuid_(atom::utils::UUID().toString()),
      taskType_("generic"),
      action_(std::move(action)) {}

Task::Task(std::string name, std::string taskType,
           std::function<void(const json&)> action)
    : name_(std::move(name)),
      uuid_(atom::utils::UUID().toString()),
      taskType_(std::move(taskType)),
      action_(std::move(action)) {}

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
                throw TaskTimeoutException(
                    "Task '" + name_ + "' execution timed out after " +
                        std::to_string(timeout_.count()) + " seconds",
                    name_, timeout_);
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

void Task::setTaskType(const std::string& taskType) { taskType_ = taskType; }

auto Task::getTaskType() const -> const std::string& { return taskType_; }

json Task::toJson(bool includeRuntime) const {
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

    json j = {
        {"version", "2.0.0"},  // Version information for schema compatibility
        {"name", name_},
        {"uuid", uuid_},
        {"taskType", taskType_},
        {"status", static_cast<int>(status_)},
        {"error", error_.value_or("")},
        {"priority", priority_},
        {"dependencies", dependencies_},
        {"paramDefinitions", paramDefs},
        {"timeout", timeout_.count()}};

    if (includeRuntime) {
        j["executionTime"] = executionTime_.count();
        j["memoryUsage"] = memoryUsage_;
        j["logLevel"] = logLevel_;
        j["errorType"] = static_cast<int>(errorType_);
        j["errorDetails"] = errorDetails_;
        j["cpuUsage"] = cpuUsage_;
        j["taskHistory"] = taskHistory_;
    }

    // Serialize pre and post tasks (only UUIDs to avoid circular references)
    json preTasks = json::array();
    for (const auto& task : preTasks_) {
        preTasks.push_back(task->getUUID());
    }
    j["preTasks"] = preTasks;

    json postTasks = json::array();
    for (const auto& task : postTasks_) {
        postTasks.push_back(task->getUUID());
    }
    j["postTasks"] = postTasks;

    return j;
}

void Task::fromJson(const json& data) {
    try {
        // Required fields
        name_ = data.at("name").get<std::string>();

        // Optional fields with defaults
        if (data.contains("uuid")) {
            uuid_ = data.at("uuid").get<std::string>();
        } else {
            uuid_ = atom::utils::UUID().toString();
        }

        if (data.contains("taskType")) {
            taskType_ = data.at("taskType").get<std::string>();
        } else {
            taskType_ = "generic";
        }

        if (data.contains("status")) {
            status_ = static_cast<TaskStatus>(data.at("status").get<int>());
        } else {
            status_ = TaskStatus::Pending;
        }

        if (data.contains("error") &&
            !data.at("error").get<std::string>().empty()) {
            error_ = data.at("error").get<std::string>();
        }

        if (data.contains("priority")) {
            priority_ = data.at("priority").get<int>();
        }

        if (data.contains("dependencies")) {
            dependencies_ =
                data.at("dependencies").get<std::vector<std::string>>();
        }

        if (data.contains("timeout")) {
            timeout_ = std::chrono::seconds(data.at("timeout").get<int64_t>());
        }

        if (data.contains("paramDefinitions") &&
            data.at("paramDefinitions").is_array()) {
            paramDefinitions_.clear();
            for (const auto& defJson : data.at("paramDefinitions")) {
                ParamDefinition def;
                def.name = defJson.at("name").get<std::string>();
                def.type = defJson.at("type").get<std::string>();
                def.required = defJson.at("required").get<bool>();
                def.defaultValue = defJson.at("defaultValue");
                def.description = defJson.at("description").get<std::string>();
                paramDefinitions_.push_back(def);
            }
        }

        if (data.contains("executionTime")) {
            executionTime_ = std::chrono::milliseconds(
                data.at("executionTime").get<int64_t>());
        }

        if (data.contains("memoryUsage")) {
            memoryUsage_ = data.at("memoryUsage").get<size_t>();
        }

        if (data.contains("logLevel")) {
            logLevel_ = data.at("logLevel").get<int>();
        }

        if (data.contains("errorType")) {
            errorType_ =
                static_cast<TaskErrorType>(data.at("errorType").get<int>());
        }

        if (data.contains("errorDetails")) {
            errorDetails_ = data.at("errorDetails").get<std::string>();
        }

        if (data.contains("cpuUsage")) {
            cpuUsage_ = data.at("cpuUsage").get<double>();
        }

        if (data.contains("taskHistory") && data.at("taskHistory").is_array()) {
            taskHistory_ =
                data.at("taskHistory").get<std::vector<std::string>>();
        }

        // Pre-tasks and post-tasks are handled elsewhere to resolve references

    } catch (const json::exception& e) {
        spdlog::error("Failed to deserialize task from JSON: {}", e.what());
        throw std::runtime_error(
            std::string("Failed to deserialize task from JSON: ") + e.what());
    }
}

std::unique_ptr<Task> Task::createFromJson(const json& data) {
    try {
        std::string name = data.at("name").get<std::string>();
        std::string taskType = data.value("taskType", "generic");

        // Create a task with a placeholder action
        auto task = std::make_unique<Task>(name, taskType, [](const json&) {
            // This will be replaced when loading the sequence
        });

        task->fromJson(data);
        return task;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create task from JSON: {}", e.what());
        throw std::runtime_error(
            std::string("Failed to create task from JSON: ") + e.what());
    }
}
}  // namespace lithium::task