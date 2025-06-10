/**
 * @file target.cpp
 * @brief Implementation of the Target class.
 */
#include "target.hpp"

#include "task_camera.hpp"

#include <mutex>

#include "atom/async/safetype.hpp"
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/uuid.hpp"
#include "spdlog/spdlog.h"

#include "matchit/matchit.h"

#include "constant/constant.hpp"

namespace lithium::sequencer {

/**
 * @class TaskErrorException
 * @brief Exception thrown when a task error occurs.
 */
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
    spdlog::info("Target created with name: {}, cooldown: {}s, maxRetries: {}",
                 name_, cooldown_.count(), maxRetries_);
    if (auto queueOpt =
            GetPtr<atom::async::LockFreeHashTable<std::string, json>>(
                Constants::TASK_QUEUE)) {
        queue_ = queueOpt.value();
    } else {
        throw std::runtime_error(
            "Task queue not found in global shared memory");
    }
}

void Target::addTask(std::unique_ptr<Task> task) {
    if (!task) {
        throw std::invalid_argument("Cannot add a null task");
    }
    std::unique_lock lock(mutex_);
    tasks_.emplace_back(std::move(task));
    totalTasks_ = tasks_.size();
    spdlog::info("Task added to target: {}, total tasks: {}", name_,
                 totalTasks_);
}

void Target::setCooldown(std::chrono::seconds cooldown) {
    std::unique_lock lock(mutex_);
    cooldown_ = cooldown;
    spdlog::info("Cooldown set to {}s for target: {}", cooldown_.count(),
                 name_);
}

void Target::setEnabled(bool enabled) {
    std::unique_lock lock(mutex_);
    enabled_ = enabled;
    spdlog::info("Target {} enabled status set to: {}", name_, enabled_);
}

void Target::setMaxRetries(int retries) {
    std::unique_lock lock(mutex_);
    maxRetries_ = retries;
    spdlog::info("Max retries set to {} for target: {}", retries, name_);
}

void Target::setOnStart(TargetStartCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onStart_ = std::move(callback);
    spdlog::info("OnStart callback set for target: {}", name_);
}

void Target::setOnEnd(TargetEndCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onEnd_ = std::move(callback);
    spdlog::info("OnEnd callback set for target: {}", name_);
}

void Target::setOnError(TargetErrorCallback callback) {
    std::unique_lock lock(callbackMutex_);
    onError_ = std::move(callback);
    spdlog::info("OnError callback set for target: {}", name_);
}

void Target::setStatus(TargetStatus status) {
    status_ = status;
    spdlog::info("Status set to {} for target: {}", static_cast<int>(status),
                 name_);
}

const std::string& Target::getName() const { return name_; }

const std::string& Target::getUUID() const { return uuid_; }

TargetStatus Target::getStatus() const { return status_.load(); }

bool Target::isEnabled() const {
    std::shared_lock lock(mutex_);
    return enabled_;
}

double Target::getProgress() const {
    size_t completed = completedTasks_.load();
    size_t total;
    {
        std::shared_lock lock(mutex_);
        total = totalTasks_;
    }

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
            spdlog::info("OnStart callback executed for target: {}", name_);
        } catch (const std::exception& e) {
            spdlog::error("Exception in OnStart callback for target: {}: {}",
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
            spdlog::info(
                "OnEnd callback executed for target: {} with status: {}", name_,
                static_cast<int>(status));
        } catch (const std::exception& e) {
            spdlog::error("Exception in OnEnd callback for target: {}: {}",
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
            spdlog::info(
                "OnError callback executed for target: {} with error: {}",
                name_, e.what());
        } catch (const std::exception& ex) {
            spdlog::error("Exception in OnError callback for target: {}: {}",
                          name_, ex.what());
        }
    }
}

void Target::setTaskParams(const std::string& taskUUID, const json& params) {
    std::unique_lock lock(paramsMutex_);
    taskParams_[taskUUID] = params;
    spdlog::info("Parameters set for task {}", taskUUID);
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
        spdlog::info("Created task group: {}", groupName);
    }
}

void Target::addTaskToGroup(const std::string& groupName,
                            const std::string& taskUUID) {
    std::unique_lock lock(groupMutex_);
    if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
        // Check if task already exists in group
        if (std::find(it->second.begin(), it->second.end(), taskUUID) ==
            it->second.end()) {
            it->second.push_back(taskUUID);
            spdlog::info("Added task {} to group {}", taskUUID, groupName);
        }
    }
}

void Target::removeTaskFromGroup(const std::string& groupName,
                                 const std::string& taskUUID) {
    std::unique_lock lock(groupMutex_);
    if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
        auto& tasks = it->second;
        auto taskIt = std::remove(tasks.begin(), tasks.end(), taskUUID);
        if (taskIt != tasks.end()) {
            tasks.erase(taskIt, tasks.end());
            spdlog::info("Removed task {} from group {}", taskUUID, groupName);
        }
    }
}

std::vector<std::string> Target::getTaskGroup(
    const std::string& groupName) const {
    std::shared_lock lock(groupMutex_);
    if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
        return it->second;
    }
    return {};
}

void Target::executeGroup(const std::string& groupName) {
    std::vector<std::string> taskUUIDs;
    {
        std::shared_lock lock(groupMutex_);
        if (auto it = taskGroups_.find(groupName); it != taskGroups_.end()) {
            taskUUIDs = it->second;
        } else {
            spdlog::warn("Task group not found: {}", groupName);
            return;
        }
    }

    for (const auto& taskUUID : taskUUIDs) {
        if (!checkDependencies(taskUUID)) {
            spdlog::error("Dependencies not met for task: {}", taskUUID);
            continue;
        }

        try {
            std::shared_lock lock(mutex_);
            auto task = std::find_if(tasks_.begin(), tasks_.end(),
                                     [&taskUUID](const auto& t) {
                                         return t->getUUID() == taskUUID;
                                     });

            if (task != tasks_.end()) {
                std::shared_lock paramsLock(paramsMutex_);
                (*task)->execute(params_);
                completedTasks_.fetch_add(1);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to execute task {}: {}", taskUUID, e.what());
            notifyError(e);
        }
    }
}

void Target::abortGroup(const std::string& groupName) {
    spdlog::info("Aborting task group: {}", groupName);
    // Implementation would depend on specifics of how tasks can be aborted
}

void Target::addTaskDependency(const std::string& taskUUID,
                               const std::string& dependsOnUUID) {
    std::unique_lock lock(depMutex_);
    auto& deps = taskDependencies_[taskUUID];
    if (std::find(deps.begin(), deps.end(), dependsOnUUID) == deps.end()) {
        deps.push_back(dependsOnUUID);
        spdlog::info("Added dependency: {} depends on {}", taskUUID,
                     dependsOnUUID);
    }
}

void Target::removeTaskDependency(const std::string& taskUUID,
                                  const std::string& dependsOnUUID) {
    std::unique_lock lock(depMutex_);
    if (auto it = taskDependencies_.find(taskUUID);
        it != taskDependencies_.end()) {
        auto& deps = it->second;
        auto depIt = std::remove(deps.begin(), deps.end(), dependsOnUUID);
        if (depIt != deps.end()) {
            deps.erase(depIt, deps.end());
            spdlog::info("Removed dependency: {} no longer depends on {}",
                         taskUUID, dependsOnUUID);
        }
    }
}

bool Target::checkDependencies(const std::string& taskUUID) const {
    std::shared_lock depLock(depMutex_);
    if (auto it = taskDependencies_.find(taskUUID);
        it != taskDependencies_.end()) {
        std::shared_lock lock(mutex_);
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
    bool isTargetEnabled;
    {
        std::shared_lock lock(mutex_);
        isTargetEnabled = enabled_;
    }

    if (!isTargetEnabled) {
        setStatus(TargetStatus::Skipped);
        notifyEnd(TargetStatus::Skipped);
        return;
    }

    setStatus(TargetStatus::InProgress);
    notifyStart();

    bool hasFailure = false;

    // First execute ungrouped tasks
    {
        std::shared_lock lock(mutex_);
        for (auto& task : tasks_) {
            std::string taskUUID = task->getUUID();

            // Skip tasks that are part of groups
            bool inGroup = false;
            {
                std::shared_lock groupLock(groupMutex_);
                for (const auto& [_, taskUUIDs] : taskGroups_) {
                    if (std::find(taskUUIDs.begin(), taskUUIDs.end(),
                                  taskUUID) != taskUUIDs.end()) {
                        inGroup = true;
                        break;
                    }
                }
            }

            if (inGroup) {
                continue;
            }

            if (!checkDependencies(taskUUID)) {
                continue;
            }

            try {
                std::shared_lock paramsLock(paramsMutex_);
                task->execute(params_);
                completedTasks_.fetch_add(1);

                if (task->getStatus() == TaskStatus::Failed) {
                    hasFailure = true;
                    break;
                }
            } catch (const std::exception& e) {
                notifyError(e);
                hasFailure = true;
                break;
            }
        }
    }

    // Then execute task groups if no failure occurred
    if (!hasFailure) {
        std::vector<std::string> groupNames;
        {
            std::shared_lock lock(groupMutex_);
            for (const auto& [groupName, _] : taskGroups_) {
                groupNames.push_back(groupName);
            }
        }

        for (const auto& groupName : groupNames) {
            executeGroup(groupName);
        }
    }

    TargetStatus finalStatus =
        hasFailure ? TargetStatus::Failed : TargetStatus::Completed;
    setStatus(finalStatus);
    notifyEnd(finalStatus);
}

void Target::setParams(const json& params) {
    std::unique_lock lock(paramsMutex_);
    params_ = params;
    spdlog::info("Parameters set for target {}", name_);
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
                return TaskCreator<task::TakeExposureTask>::createTask();
            },
            pattern | "TakeManyExposure" = [&]() -> std::unique_ptr<Task> {
                return TaskCreator<task::TakeManyExposureTask>::createTask();
            },
            pattern | "SubframeExposure" = [&]() -> std::unique_ptr<Task> {
                return TaskCreator<task::SubframeExposureTask>::createTask();
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
    json j = {{"name", name_},
              {"uuid", uuid_},
              {"enabled", isEnabled()},
              {"status", static_cast<int>(getStatus())},
              {"progress", getProgress()},
              {"tasks", json::array()}};

    {
        std::shared_lock lock(mutex_);
        j["cooldown"] = cooldown_.count();
        j["maxRetries"] = maxRetries_;

        for (const auto& task : tasks_) {
            j["tasks"].push_back(task->toJson());
        }
    }

    {
        std::shared_lock lock(paramsMutex_);
        j["params"] = params_;
    }

    {
        std::shared_lock lock(groupMutex_);
        j["taskGroups"] = json::object();
        for (const auto& [groupName, tasks] : taskGroups_) {
            j["taskGroups"][groupName] = tasks;
        }
    }

    {
        std::shared_lock lock(depMutex_);
        j["taskDependencies"] = json::object();
        for (const auto& [taskUUID, deps] : taskDependencies_) {
            j["taskDependencies"][taskUUID] = deps;
        }
    }

    return j;
}

auto Target::fromJson(const json& data) -> void {
    name_ = data["name"].get<std::string>();
    uuid_ = data["uuid"].get<std::string>();

    {
        std::unique_lock lock(mutex_);
        cooldown_ = std::chrono::seconds(data["cooldown"].get<int>());
        maxRetries_ = data["maxRetries"].get<int>();
        enabled_ = data["enabled"].get<bool>();
    }

    setStatus(static_cast<TargetStatus>(data["status"].get<int>()));

    {
        std::unique_lock lock(paramsMutex_);
        if (data.contains("params")) {
            params_ = data["params"];
        }
    }

    {
        std::unique_lock lock(mutex_);
        tasks_.clear();
    }

    if (data.contains("tasks") && data["tasks"].is_array()) {
        loadTasksFromJson(data["tasks"]);
    }

    {
        std::unique_lock lock(groupMutex_);
        taskGroups_.clear();
        if (data.contains("taskGroups") && data["taskGroups"].is_object()) {
            for (const auto& [groupName, tasks] : data["taskGroups"].items()) {
                taskGroups_[groupName] = tasks.get<std::vector<std::string>>();
            }
        }
    }

    {
        std::unique_lock lock(depMutex_);
        taskDependencies_.clear();
        if (data.contains("taskDependencies") &&
            data["taskDependencies"].is_object()) {
            for (const auto& [taskUUID, deps] :
                 data["taskDependencies"].items()) {
                taskDependencies_[taskUUID] =
                    deps.get<std::vector<std::string>>();
            }
        }
    }
}

}  // namespace lithium::sequencer