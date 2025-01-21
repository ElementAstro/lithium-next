#include "target.hpp"

#include "task_camera.hpp"

#include <mutex>
#include <stdexcept>
#include <thread>

#include "atom/async/safetype.hpp"
#include "config/configor.hpp"

#include "atom/async/message_bus.hpp"
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

void Target::execute() {
    if (!isEnabled()) {
        status_ = TargetStatus::Skipped;
        LOG_F(WARNING, "Target {} is disabled, skipping execution", name_);
        return;
    }

    status_ = TargetStatus::InProgress;
    LOG_F(INFO, "Target {} execution started", name_);

    std::shared_lock paramsLock(paramsMutex_);
    const json& currentParams = params_;
    paramsLock.unlock();

    for (auto& task : tasks_) {
        if (status_ == TargetStatus::Failed ||
            status_ == TargetStatus::Skipped) {
            break;
        }

        try {
            LOG_F(INFO, "Executing task {} in target {}", task->getName(),
                  name_);
            // 使用存储的参数直接执行任务
            task->execute(currentParams);

            if (task->getStatus() == TaskStatus::Failed) {
                status_ = TargetStatus::Failed;
                break;
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Task {} failed in target {}: {}", task->getName(),
                  name_, e.what());
            status_ = TargetStatus::Failed;
            break;
        }
    }

    if (status_ != TargetStatus::Failed) {
        status_ = TargetStatus::Completed;
        LOG_F(INFO, "Target {} execution completed successfully", name_);
    }
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
}  // namespace lithium::sequencer