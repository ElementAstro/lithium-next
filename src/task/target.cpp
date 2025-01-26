#include "target.hpp"

#include "task_camera.hpp"

#include <mutex>

#include "atom/async/safetype.hpp"

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/uuid.hpp"

#include "matchit/matchit.h"

#include "constant/constant.hpp"

namespace lithium::sequencer {
class TaskErrorException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_TASK_ERROR_EXCEPTION(...)                                      \
    throw TaskErrorException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                             __VA_ARGS__);

Target::Target(std::string name, std::chrono::seconds cooldown, int maxRetries)
    : name_(std::move(name)),
      uuid_(atom::utils::UUID().toString()),
      cooldown_(cooldown),
      maxRetries_(maxRetries) {
    LOG_F(INFO, "Target created with name: {}, cooldown: {}s, maxRetries: {}",
          name_, cooldown_.count(), maxRetries_);
    if (auto queueOpt =
            GetPtr<atom::async::LockFreeHashTable<std::string, json>>(
                Constants::TASK_QUEUE)) {
        queue_ = queueOpt.value();
    } else {
        THROW_RUNTIME_ERROR("Task queue not found in global shared memory");
    }
}

void Target::addTask(std::unique_ptr<Task> task) {
    if (!task) {
        THROW_INVALID_ARGUMENT("Cannot add a null task");
    }
    std::unique_lock lock(mutex_);
    tasks_.emplace_back(std::move(task));
    totalTasks_ = tasks_.size();
    LOG_F(INFO, "Task added to target: {}, total tasks: {}", name_,
          totalTasks_);
}

void Target::setCooldown(std::chrono::seconds cooldown) {
    std::unique_lock lock(mutex_);
    cooldown_ = cooldown;
    LOG_F(INFO, "Cooldown set to {}s for target: {}", cooldown_.count(), name_);
}

void Target::setEnabled(bool enabled) {
    std::unique_lock lock(mutex_);
    enabled_ = enabled;
    LOG_F(INFO, "Target {} enabled status set to: {}", name_, enabled_);
}

void Target::setMaxRetries(int retries) {
    std::unique_lock lock(mutex_);
    maxRetries_ = retries;
    LOG_F(INFO, "Max retries set to {} for target: {}", retries, name_);
}

void Target::setOnStart(TargetStartCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onStart_ = std::move(callback);
    LOG_F(INFO, "OnStart callback set for target: {}", name_);
}

void Target::setOnEnd(TargetEndCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onEnd_ = std::move(callback);
    LOG_F(INFO, "OnEnd callback set for target: {}", name_);
}

void Target::setOnError(TargetErrorCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onError_ = std::move(callback);
    LOG_F(INFO, "OnError callback set for target: {}", name_);
}

void Target::setStatus(TargetStatus status) {
    std::unique_lock lock(mutex_);
    status_ = status;
    LOG_F(INFO, "Status set to {} for target: {}", static_cast<int>(status),
          name_);
}

const std::string& Target::getName() const { return name_; }

const std::string& Target::getUUID() const { return uuid_; }

TargetStatus Target::getStatus() const { return status_.load(); }

bool Target::isEnabled() const { return enabled_; }

double Target::getProgress() const {
    size_t completed = completedTasks_.load();
    size_t total = totalTasks_;
    if (total == 0) {
        return 100.0;
    }
    return (static_cast<double>(completed) / static_cast<double>(total)) *
           100.0;
}

void Target::notifyStart() {
    TargetStartCallback callbackCopy;
    {
        std::shared_lock lock(callbackMutex_);
        callbackCopy = onStart_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name_);
            LOG_F(INFO, "OnStart callback executed for target: {}", name_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in OnStart callback for target: {}: {}",
                  name_, e.what());
        }
    }
}

void Target::notifyEnd(TargetStatus status) {
    TargetEndCallback callbackCopy;
    {
        std::shared_lock lock(callbackMutex_);
        callbackCopy = onEnd_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name_, status);
            LOG_F(INFO,
                  "OnEnd callback executed for target: {} with status: {}",
                  name_, static_cast<int>(status));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in OnEnd callback for target: {}: {}",
                  name_, e.what());
        }
    }
}

void Target::notifyError(const std::exception& e) {
    TargetErrorCallback callbackCopy;
    {
        std::shared_lock lock(callbackMutex_);
        callbackCopy = onError_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name_, e);
            LOG_F(INFO,
                  "OnError callback executed for target: {} with error: {}",
                  name_, e.what());
        } catch (const std::exception& ex) {
            LOG_F(ERROR, "Exception in OnError callback for target: {}: {}",
                  name_, ex.what());
        }
    }
}

void Target::setTaskParams(const std::string& taskUUID, const json& params) {
    std::unique_lock lock(paramsMutex_);
    taskParams_[taskUUID] = params;
    LOG_F(INFO, "Parameters set for task {}", taskUUID);
}

auto Target::getTaskParams(const std::string& taskUUID) const
    -> std::optional<json> {
    std::shared_lock lock(paramsMutex_);
    auto it = taskParams_.find(taskUUID);
    if (it != taskParams_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Target::createTaskGroup(const std::string& groupName) {
    std::unique_lock lock(groupMutex_);
    if (taskGroups_.find(groupName) == taskGroups_.end()) {
        taskGroups_[groupName] = std::vector<std::string>();
        LOG_F(INFO, "Created task group: {}", groupName);
    }
}

void Target::addTaskToGroup(const std::string& groupName,
                            const std::string& taskUUID) {
    std::unique_lock lock(groupMutex_);
    if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
        it->second.push_back(taskUUID);
        LOG_F(INFO, "Added task {} to group {}", taskUUID, groupName);
    }
}

void Target::executeGroup(const std::string& groupName) {
    std::shared_lock lock(groupMutex_);
    if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
        for (const auto& taskUUID : it->second) {
            if (!checkDependencies(taskUUID)) {
                LOG_F(ERROR, "Dependencies not met for task: {}", taskUUID);
                continue;
            }

            try {
                auto task = std::find_if(tasks_.begin(), tasks_.end(),
                                         [&taskUUID](const auto& t) {
                                             return t->getUUID() == taskUUID;
                                         });

                if (task != tasks_.end()) {
                    (*task)->execute(params_);
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to execute task {}: {}", taskUUID,
                      e.what());
                if (onError_)
                    onError_(name_, e);
            }
        }
    }
}

void Target::addTaskDependency(const std::string& taskUUID,
                               const std::string& dependsOnUUID) {
    std::unique_lock lock(depMutex_);
    taskDependencies_[taskUUID].push_back(dependsOnUUID);
    LOG_F(INFO, "Added dependency: {} depends on {}", taskUUID, dependsOnUUID);
}

bool Target::checkDependencies(const std::string& taskUUID) const {
    std::shared_lock lock(depMutex_);
    if (auto it = taskDependencies_.find(taskUUID);
        it != taskDependencies_.end()) {
        for (const auto& depUUID : it->second) {
            auto depTask = std::find_if(
                tasks_.begin(), tasks_.end(),
                [&depUUID](const auto& t) { return t->getUUID() == depUUID; });

            if (depTask == tasks_.end() ||
                (*depTask)->getStatus() != TaskStatus::Completed) {
                return false;
            }
        }
    }
    return true;
}

void Target::execute() {
    if (!isEnabled()) {
        status_ = TargetStatus::Skipped;
        if (onEnd_)
            onEnd_(name_, status_);
        return;
    }

    status_ = TargetStatus::InProgress;
    if (onStart_)
        onStart_(name_);

    std::vector<std::string> executedGroups;
    bool hasFailure = false;

    // 先执行没有分组的任务
    for (auto& task : tasks_) {
        if (!checkDependencies(task->getUUID()))
            continue;

        try {
            task->execute(params_);
            completedTasks_++;

            if (task->getStatus() == TaskStatus::Failed) {
                hasFailure = true;
                break;
            }
        } catch (const std::exception& e) {
            if (onError_)
                onError_(name_, e);
            hasFailure = true;
            break;
        }
    }

    // 再执行任务组
    if (!hasFailure) {
        std::shared_lock lock(groupMutex_);
        for (const auto& [groupName, _] : taskGroups_) {
            executeGroup(groupName);
            executedGroups.push_back(groupName);
        }
    }

    status_ = hasFailure ? TargetStatus::Failed : TargetStatus::Completed;
    if (onEnd_)
        onEnd_(name_, status_);
}

void Target::setParams(const json& params) {
    std::unique_lock lock(paramsMutex_);
    params_ = params;
    LOG_F(INFO, "Parameters set for target {}: {}", name_, params.dump());
}

auto Target::getParams() const -> const json& {
    std::shared_lock lock(paramsMutex_);
    return params_;
}

void Target::loadTasksFromJson(const json& tasksJson) {
    for (const auto& taskJson : tasksJson) {
        std::string taskName = taskJson.at("name").get<std::string>();
        using namespace matchit;
        auto task = match(taskName)(
            pattern | "TakeExposure" = [&]() -> std::unique_ptr<Task> {
                auto task = TaskCreator<task::TakeExposureTask>::createTask();
                return task;
            },
            pattern | "TakeManyExposure" = [&]() -> std::unique_ptr<Task> {
                auto task =
                    TaskCreator<task::TakeManyExposureTask>::createTask();
                return task;
            },
            pattern | "SubframeExposure" = [&]() -> std::unique_ptr<Task> {
                auto task =
                    TaskCreator<task::SubframeExposureTask>::createTask();
                return task;
            },
            pattern | _ = [&]() -> std::unique_ptr<Task> {
                THROW_TASK_ERROR_EXCEPTION("Unknown task type: {}", taskName);
            });
        addTask(std::move(task));
    }
}

auto Target::getDependencies() -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return dependencies_;
}

auto Target::getTasks() -> const std::vector<std::unique_ptr<Task>>& {
    std::shared_lock lock(mutex_);
    return tasks_;
}

auto Target::toJson() const -> json {
    std::shared_lock lock(paramsMutex_);
    json j;
    j["name"] = name_;
    j["uuid"] = uuid_;
    j["cooldown"] = cooldown_.count();
    j["maxRetries"] = maxRetries_;
    j["enabled"] = enabled_;
    j["status"] = static_cast<int>(status_.load());
    j["progress"] = getProgress();
    j["params"] = params_;
    j["tasks"] = json::array();
    for (const auto& task : tasks_) {
        j["tasks"].push_back(task->toJson());
    }
    return j;
}

auto Target::fromJson(const json& data) -> void {
    std::unique_lock lock(paramsMutex_);
    name_ = data["name"].get<std::string>();
    uuid_ = data["uuid"].get<std::string>();
    cooldown_ = std::chrono::seconds(data["cooldown"].get<int>());
    maxRetries_ = data["maxRetries"].get<int>();
    enabled_ = data["enabled"].get<bool>();
    status_ = static_cast<TargetStatus>(data["status"].get<int>());
    params_ = data["params"];
    tasks_.clear();
    if (data.contains("tasks") && data["tasks"].is_array()) {
        loadTasksFromJson(data["tasks"]);
    }
}
}  // namespace lithium::sequencer
