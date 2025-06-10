/**
 * @file factory.cpp
 * @brief Task Factory implementation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "factory.hpp"
#include <spdlog/spdlog.h>  // Use spdlog for logging
#include <algorithm>
#include <mutex>

namespace lithium::sequencer {

TaskFactory& TaskFactory::getInstance() {
    static TaskFactory instance;
    return instance;
}

void TaskFactory::registerTask(const std::string& taskType,
                               std::unique_ptr<TaskCreator> creator,
                               const TaskInfo& info) {
    std::unique_lock lock(mutex_);

    if (creators_.find(taskType) != creators_.end()) {
        spdlog::warn("Task type '{}' is already registered. Overwriting.",
                     taskType);
    }

    creators_[taskType] = std::move(creator);
    taskInfos_[taskType] = info;

    spdlog::info("Registered task type '{}' in category '{}'", taskType,
                 info.category);
}

void TaskFactory::unregisterTask(const std::string& taskType) {
    std::unique_lock lock(mutex_);

    auto creatorIt = creators_.find(taskType);
    auto infoIt = taskInfos_.find(taskType);

    if (creatorIt != creators_.end()) {
        creators_.erase(creatorIt);
    }

    if (infoIt != taskInfos_.end()) {
        taskInfos_.erase(infoIt);
    }

    spdlog::info("Unregistered task type '{}'", taskType);
}

std::unique_ptr<Task> TaskFactory::createTask(const std::string& taskType,
                                              const std::string& name,
                                              const json& config) {
    std::shared_lock lock(mutex_);

    auto it = creators_.find(taskType);
    if (it == creators_.end()) {
        spdlog::error("Task type '{}' not found", taskType);
        return nullptr;
    }

    auto infoIt = taskInfos_.find(taskType);
    if (infoIt != taskInfos_.end() && !infoIt->second.isEnabled) {
        spdlog::error("Task type '{}' is disabled", taskType);
        return nullptr;
    }

    try {
        // Validate parameters if schema is available
        if (infoIt != taskInfos_.end() &&
            !infoIt->second.parameterSchema.empty()) {
            if (!validateTaskParameters(taskType, config)) {
                spdlog::error("Parameter validation failed for task type '{}'",
                              taskType);
                return nullptr;
            }
        }

        auto task = it->second->createTask(name, config);
        spdlog::info("Created task '{}' of type '{}'", name, taskType);
        return task;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create task '{}' of type '{}': {}", name,
                      taskType, e.what());
        return nullptr;
    }
}

bool TaskFactory::isTaskRegistered(const std::string& taskType) const {
    std::shared_lock lock(mutex_);
    return creators_.find(taskType) != creators_.end();
}

std::vector<std::string> TaskFactory::getRegisteredTaskTypes() const {
    std::shared_lock lock(mutex_);

    std::vector<std::string> taskTypes;
    taskTypes.reserve(creators_.size());

    for (const auto& [taskType, _] : creators_) {
        taskTypes.push_back(taskType);
    }

    std::sort(taskTypes.begin(), taskTypes.end());
    return taskTypes;
}

std::optional<TaskInfo> TaskFactory::getTaskInfo(
    const std::string& taskType) const {
    std::shared_lock lock(mutex_);

    auto it = taskInfos_.find(taskType);
    if (it != taskInfos_.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::unordered_map<std::string, std::vector<TaskInfo>>
TaskFactory::getTasksByCategory() const {
    std::shared_lock lock(mutex_);

    std::unordered_map<std::string, std::vector<TaskInfo>> result;

    for (const auto& [taskType, info] : taskInfos_) {
        result[info.category].push_back(info);
    }

    // Sort tasks within each category
    for (auto& [category, tasks] : result) {
        std::sort(tasks.begin(), tasks.end(),
                  [](const TaskInfo& a, const TaskInfo& b) {
                      return a.name < b.name;
                  });
    }

    return result;
}

bool TaskFactory::validateTaskParameters(const std::string& taskType,
                                         const json& params) const {
    auto infoIt = taskInfos_.find(taskType);
    if (infoIt == taskInfos_.end()) {
        return false;
    }

    const auto& info = infoIt->second;

    // Check required parameters
    for (const auto& required : info.requiredParameters) {
        if (!params.contains(required)) {
            spdlog::error("Missing required parameter '{}' for task type '{}'",
                          required, taskType);
            return false;
        }
    }

    // Additional schema validation could be added here
    // For now, we just check required parameters

    return true;
}

void TaskFactory::clear() {
    std::unique_lock lock(mutex_);
    creators_.clear();
    taskInfos_.clear();
    spdlog::info("Cleared all registered tasks");
}

void TaskFactory::setTaskEnabled(const std::string& taskType, bool enabled) {
    std::unique_lock lock(mutex_);

    auto it = taskInfos_.find(taskType);
    if (it != taskInfos_.end()) {
        it->second.isEnabled = enabled;
        spdlog::info("Task type '{}' {}", taskType,
                     enabled ? "enabled" : "disabled");
    } else {
        spdlog::warn("Task type '{}' not found when trying to {}", taskType,
                     enabled ? "enable" : "disable");
    }
}

std::vector<std::string> TaskFactory::checkDependencies(
    const std::string& taskType) const {
    std::shared_lock lock(mutex_);

    std::vector<std::string> missingDeps;

    auto it = taskInfos_.find(taskType);
    if (it == taskInfos_.end()) {
        return missingDeps;
    }

    const auto& dependencies = it->second.dependencies;
    for (const auto& dep : dependencies) {
        if (creators_.find(dep) == creators_.end()) {
            missingDeps.push_back(dep);
        }
    }

    return missingDeps;
}

std::unordered_map<std::string, std::vector<std::string>>
TaskFactory::getDependencyGraph() const {
    std::shared_lock lock(mutex_);

    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (const auto& [taskType, info] : taskInfos_) {
        graph[taskType] = info.dependencies;
    }

    return graph;
}

}  // namespace lithium::sequencer
