/**
 * @file sequencer.cpp
 * @brief Task Sequencer Implementation
 *
 * This file contains the implementation of the ExposureSequence class,
 * which manages and executes a sequence of targets.
 *
 * @date 2023-07-21
 * @modified 2024-04-27
 * @author Max Qian <lightapt.com>
 * @copyright
 */

#include "sequencer.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <stack>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"
#include "task_script.hpp"
#include "task_celestial_search.hpp"
#include "task_config_management.hpp"
#include "task_utility.hpp"
#include "task_combined_script_celestial.hpp"
#include "task_combined_config_utility.hpp"

namespace lithium::sequencer {

using json = nlohmann::json;

// ExposureSequence Implementation

ExposureSequence::ExposureSequence() {
    // 初始化数据库连接
    try {
        db_ = std::make_shared<database::Database>("sequences.db");
        sequenceTable_ = std::make_unique<database::Table<SequenceModel>>(*db_);
        sequenceTable_->createTable();
        LOG_F(INFO, "Database initialized successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to initialize database: " +
                            std::string(e.what()));
    }

    AddPtr(
        Constants::TASK_QUEUE,
        std::make_shared<atom::async::LockFreeHashTable<std::string, json>>());

    taskGenerator_ = TaskGenerator::createShared();
    initializeDefaultMacros();
}

ExposureSequence::~ExposureSequence() { stop(); }

void ExposureSequence::addTarget(std::unique_ptr<Target> target) {
    if (!target) {
        LOG_F(ERROR, "Cannot add a null target");
        THROW_INVALID_ARGUMENT("Cannot add a null target");
    }
    std::unique_lock lock(mutex_);
    auto it = std::find_if(targets_.begin(), targets_.end(),
                           [&](const std::unique_ptr<Target>& t) {
                               return t->getUUID() == target->getUUID();
                           });
    if (it != targets_.end()) {
        LOG_F(ERROR, "Target with UUID '{}' already exists", target->getUUID());
        THROW_RUNTIME_ERROR("Target with UUID '" + target->getUUID() +
                            "' already exists");
    }
    LOG_F(INFO, "Adding target: {}", target->getName());
    targets_.push_back(std::move(target));
    totalTargets_ = targets_.size();
    LOG_F(INFO, "Total targets: {}", totalTargets_);
}

void ExposureSequence::removeTarget(const std::string& name) {
    std::unique_lock lock(mutex_);
    auto it = std::remove_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });
    if (it == targets_.end()) {
        LOG_F(ERROR, "Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }
    LOG_F(INFO, "Removing target: {}", name);
    targets_.erase(it, targets_.end());
    totalTargets_ = targets_.size();
    LOG_F(INFO, "Total targets: {}", totalTargets_);
}

void ExposureSequence::modifyTarget(const std::string& name,
                                    const TargetModifier& modifier) {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });
    if (it != targets_.end()) {
        try {
            LOG_F(INFO, "Modifying target: {}", name);
            modifier(**it);
            LOG_F(INFO, "Target '{}' modified successfully", name);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to modify target '{}': {}", name, e.what());
            THROW_RUNTIME_ERROR("Failed to modify target '" + name +
                                "': " + e.what());
        }
    } else {
        LOG_F(ERROR, "Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }
}

void ExposureSequence::executeAll() {
    SequenceState expected = SequenceState::Idle;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        LOG_F(ERROR, "Cannot start sequence. Current state: {}",
              static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Sequence is not in Idle state");
    }

    completedTargets_.store(0);
    LOG_F(INFO, "Starting sequence execution");
    notifySequenceStart();

    // 在单独的线程中启动序列执行
    sequenceThread_ = std::jthread([this] { executeSequence(); });
}

void ExposureSequence::stop() {
    SequenceState current = state_.load();
    if (current == SequenceState::Idle || current == SequenceState::Stopped) {
        LOG_F(INFO, "Sequence is already in Idle or Stopped state");
        return;
    }

    LOG_F(INFO, "Stopping sequence execution");
    state_.store(SequenceState::Stopping);
    if (sequenceThread_.joinable()) {
        sequenceThread_.request_stop();
        sequenceThread_.join();
    }
    state_.store(SequenceState::Idle);
    notifySequenceEnd();
}

void ExposureSequence::pause() {
    SequenceState expected = SequenceState::Running;
    if (!state_.compare_exchange_strong(expected, SequenceState::Paused)) {
        LOG_F(ERROR, "Cannot pause sequence. Current state: {}",
              static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot pause sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    LOG_F(INFO, "Sequence paused");
}

void ExposureSequence::resume() {
    SequenceState expected = SequenceState::Paused;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        LOG_F(ERROR, "Cannot resume sequence. Current state: {}",
              static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot resume sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    LOG_F(INFO, "Sequence resumed");
}

void ExposureSequence::saveSequence(const std::string& filename) const {
    json j;
    std::shared_lock lock(mutex_);
    for (const auto& target : targets_) {
        json targetJson = {{"name", target->getName()},
                           {"enabled", target->isEnabled()},
                           {"tasks", json::array()}};
        for (const auto& task : target->getTasks()) {
            json taskJson = {{"name", task->getName()},
                             {"status", static_cast<int>(task->getStatus())},
                             {"parameters", json::array()}};
            for (const auto& param : task->getParamDefinitions()) {
                taskJson["parameters"].push_back(
                    {{"name", param.name},
                     {"type", param.type},
                     {"required", param.required},
                     {"defaultValue", param.defaultValue},
                     {"description", param.description}});
            }
            taskJson["errorType"] = static_cast<int>(task->getErrorType());
            taskJson["errorDetails"] = task->getErrorDetails();
            taskJson["executionTime"] = task->getExecutionTime().count();
            taskJson["memoryUsage"] = task->getMemoryUsage();
            taskJson["cpuUsage"] = task->getCPUUsage();
            taskJson["taskHistory"] = task->getTaskHistory();
            targetJson["tasks"].push_back(taskJson);
        }
        j["targets"].push_back(targetJson);
    }
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open file '{}' for writing", filename);
        THROW_RUNTIME_ERROR("Failed to open file '" + filename +
                            "' for writing");
    }
    file << j.dump(4);
    LOG_F(INFO, "Sequence saved to file: {}", filename);
}

void ExposureSequence::loadSequence(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open file '{}' for reading", filename);
        THROW_RUNTIME_ERROR("Failed to open file '" + filename +
                            "' for reading");
    }

    json j;
    file >> j;

    std::unique_lock lock(mutex_);
    targets_.clear();
    if (!j.contains("targets") || !j["targets"].is_array()) {
        LOG_F(ERROR, "Invalid sequence file format: 'targets' array missing");
        THROW_RUNTIME_ERROR(
            "Invalid sequence file format: 'targets' array missing");
    }

    for (const auto& targetJson : j["targets"]) {
        // 在创建目标之前处理 JSON
        json processedJson = targetJson;
        processJsonWithGenerator(processedJson);

        std::string name = processedJson["name"].get<std::string>();
        bool enabled = processedJson["enabled"].get<bool>();
        auto target = std::make_unique<Target>(name);
        target->setEnabled(enabled);
        if (processedJson.contains("tasks") &&
            processedJson["tasks"].is_array()) {
            target->loadTasksFromJson(processedJson["tasks"]);
        }
        targets_.push_back(std::move(target));
    }
    totalTargets_ = targets_.size();
    LOG_F(INFO, "Sequence loaded from file: {}", filename);
    LOG_F(INFO, "Total targets loaded: {}", totalTargets_);
}

auto ExposureSequence::getTargetNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(targets_.size());
    for (const auto& target : targets_) {
        names.emplace_back(target->getName());
    }
    return names;
}

auto ExposureSequence::getTargetStatus(const std::string& name) const
    -> TargetStatus {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });
    if (it != targets_.end()) {
        return (*it)->getStatus();
    }
    return TargetStatus::Skipped;  // 或其他默认值
}

auto ExposureSequence::getProgress() const -> double {
    size_t completed = completedTargets_.load();
    size_t total = totalTargets_;
    if (total == 0) {
        return 100.0;
    }
    return (static_cast<double>(completed) / static_cast<double>(total)) *
           100.0;
}

// 回调设置函数

void ExposureSequence::setOnSequenceStart(SequenceCallback callback) {
    std::unique_lock lock(mutex_);
    onSequenceStart_ = std::move(callback);
}

void ExposureSequence::setOnSequenceEnd(SequenceCallback callback) {
    std::unique_lock lock(mutex_);
    onSequenceEnd_ = std::move(callback);
}

void ExposureSequence::setOnTargetStart(TargetCallback callback) {
    std::unique_lock lock(mutex_);
    onTargetStart_ = std::move(callback);
}

void ExposureSequence::setOnTargetEnd(TargetCallback callback) {
    std::unique_lock lock(mutex_);
    onTargetEnd_ = std::move(callback);
}

void ExposureSequence::setOnError(ErrorCallback callback) {
    std::unique_lock lock(mutex_);
    onError_ = std::move(callback);
}

// 回调通知辅助方法

void ExposureSequence::notifySequenceStart() {
    SequenceCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onSequenceStart_;
    }
    if (callbackCopy) {
        try {
            callbackCopy();
        } catch (...) {
            // 处理或记录回调异常
        }
    }
}

void ExposureSequence::notifySequenceEnd() {
    SequenceCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onSequenceEnd_;
    }
    if (callbackCopy) {
        try {
            callbackCopy();
        } catch (...) {
            // 处理或记录回调异常
        }
    }
}

void ExposureSequence::notifyTargetStart(const std::string& name) {
    TargetCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTargetStart_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name, TargetStatus::InProgress);
        } catch (...) {
            // 处理或记录回调异常
        }
    }
}

void ExposureSequence::notifyTargetEnd(const std::string& name,
                                       TargetStatus status) {
    TargetCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTargetEnd_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name, status);
        } catch (...) {
            // 处理或记录回调异常
        }
    }
}

void ExposureSequence::notifyError(const std::string& name,
                                   const std::exception& e) {
    ErrorCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onError_;
    }
    if (callbackCopy) {
        try {
            callbackCopy(name, e);
        } catch (...) {
            // 处理或记录回调异常
        }
    }
}

void ExposureSequence::executeSequence() {
    stats_.startTime = std::chrono::steady_clock::now();

    while (auto* target = getNextExecutableTarget()) {
        if (state_.load() == SequenceState::Stopping) {
            break;
        }

        try {
            LOG_F(INFO, "Executing target: {} ({}/{} completed)",
                  target->getName(), completedTargets_.load(), totalTargets_);

            stats_.totalExecutions++;
            notifyTargetStart(target->getName());
            target->execute();

            if (target->getStatus() == TargetStatus::Completed) {
                stats_.successfulExecutions++;
                completedTargets_.fetch_add(1);
            } else if (target->getStatus() == TargetStatus::Failed) {
                stats_.failedExecutions++;
                failedTargets_.fetch_add(1);
                failedTargetNames_.push_back(target->getName());
            }

            notifyTargetEnd(target->getName(), target->getStatus());

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Target execution failed: {} - {}", target->getName(),
                  e.what());
            handleTargetError(target, e);
        }
    }
    state_.store(SequenceState::Idle);
    notifySequenceEnd();
}

auto ExposureSequence::getNextExecutableTarget() -> Target* {
    if (state_.load() == SequenceState::Stopping) {
        return nullptr;
    }

    if (maxConcurrentTargets_ > 0) {
        size_t runningTargets = 0;
        for (const auto& target : targets_) {
            if (target->getStatus() == TargetStatus::InProgress) {
                runningTargets++;
            }
        }
        if (runningTargets >= maxConcurrentTargets_) {
            return nullptr;
        }
    }

    for (const auto& target : targets_) {
        if (target->getStatus() == TargetStatus::Pending &&
            isTargetReady(target->getName())) {
            return target.get();
        }
    }
    return nullptr;
}

void ExposureSequence::handleTargetError(Target* target,
                                         const std::exception& e) {
    switch (recoveryStrategy_) {
        case RecoveryStrategy::Stop:
            state_.store(SequenceState::Stopping);
            break;

        case RecoveryStrategy::Skip:
            target->setStatus(TargetStatus::Skipped);
            notifyTargetEnd(target->getName(), TargetStatus::Skipped);
            break;

        case RecoveryStrategy::Retry:
            // 实现重试逻辑
            break;

        case RecoveryStrategy::Alternative:
            // 尝试执行替代任务
            if (auto it = alternativeTargets_.find(target->getName());
                it != alternativeTargets_.end()) {
                it->second->execute();
            }
            break;
    }

    notifyError(target->getName(), e);
}

void ExposureSequence::setSchedulingStrategy(SchedulingStrategy strategy) {
    std::unique_lock lock(mutex_);
    schedulingStrategy_ = strategy;

    // 根据新策略重新排序
    switch (strategy) {
        case SchedulingStrategy::Dependencies:
            reorderTargetsByDependencies();
            break;
        default:
            break;
    }
}

void ExposureSequence::setRecoveryStrategy(RecoveryStrategy strategy) {
    std::unique_lock lock(mutex_);
    recoveryStrategy_ = strategy;
}

void ExposureSequence::addAlternativeTarget(
    const std::string& targetName, std::unique_ptr<Target> alternative) {
    std::unique_lock lock(mutex_);
    alternativeTargets_[targetName] = std::move(alternative);
}

void ExposureSequence::reorderTargetsByDependencies() {
    std::vector<std::unique_ptr<Target>> sorted;
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> inStack;
    std::stack<std::unique_ptr<Target>> stack;

    auto visit = [&](const std::unique_ptr<Target>& target,
                     auto&& visit_ref) -> void {
        const std::string& name = target->getName();
        if (visited[name]) {
            return;
        }
        if (inStack[name]) {
            THROW_RUNTIME_ERROR("Circular dependency detected in target '" +
                                name + "'");
        }

        inStack[name] = true;
        for (const auto& dep : target->getDependencies()) {
            auto it = std::find_if(targets_.begin(), targets_.end(),
                                   [&dep](const std::unique_ptr<Target>& t) {
                                       return t->getName() == dep;
                                   });
            if (it != targets_.end()) {
                visit_ref(*it, visit_ref);
            }
        }
        inStack[name] = false;
        visited[name] = true;
        stack.push(std::move(const_cast<std::unique_ptr<Target>&>(target)));
    };

    for (const auto& target : targets_) {
        visit(target, visit);
    }

    while (!stack.empty()) {
        sorted.push_back(std::move(stack.top()));
        stack.pop();
    }

    targets_ = std::move(sorted);
}

void ExposureSequence::addTargetDependency(const std::string& targetName,
                                           const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    // 添加依赖
    if (std::find(targetDependencies_[targetName].begin(),
                  targetDependencies_[targetName].end(),
                  dependsOn) == targetDependencies_[targetName].end()) {
        targetDependencies_[targetName].push_back(dependsOn);
    }

    // 检查是否存在循环依赖
    if (auto cycle = findCyclicDependency()) {
        targetDependencies_[targetName].pop_back();
        THROW_RUNTIME_ERROR("Cyclic dependency detected in target: " +
                            cycle.value());
    }

    updateTargetReadyStatus();
}

void ExposureSequence::removeTargetDependency(const std::string& targetName,
                                              const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    auto& deps = targetDependencies_[targetName];
    deps.erase(std::remove(deps.begin(), deps.end(), dependsOn), deps.end());
    updateTargetReadyStatus();
}

auto ExposureSequence::isTargetReady(const std::string& targetName) const
    -> bool {
    std::shared_lock lock(mutex_);
    auto it = targetReadyStatus_.find(targetName);
    return it != targetReadyStatus_.end() && it->second;
}

void ExposureSequence::updateTargetReadyStatus() {
    // 首先将所有目标标记为就绪
    for (const auto& target : targets_) {
        targetReadyStatus_[target->getName()] = true;
    }

    // 检查每个目标的依赖
    bool changed;
    do {
        changed = false;
        for (const auto& [targetName, dependencies] : targetDependencies_) {
            // 如果任何依赖项未就绪,则该目标也未就绪
            for (const auto& dep : dependencies) {
                if (!targetReadyStatus_[dep]) {
                    if (targetReadyStatus_[targetName]) {
                        targetReadyStatus_[targetName] = false;
                        changed = true;
                    }
                    break;
                }
            }
        }
    } while (changed);
}

auto ExposureSequence::findCyclicDependency() const
    -> std::optional<std::string> {
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recursionStack;

    std::function<bool(const std::string&)> hasCycle;
    hasCycle = [&](const std::string& targetName) -> bool {
        visited[targetName] = true;
        recursionStack[targetName] = true;

        auto it = targetDependencies_.find(targetName);
        if (it != targetDependencies_.end()) {
            for (const auto& dep : it->second) {
                if (!visited[dep]) {
                    if (hasCycle(dep)) {
                        return true;
                    }
                } else if (recursionStack[dep]) {
                    return true;
                }
            }
        }

        recursionStack[targetName] = false;
        return false;
    };

    for (const auto& [targetName, _] : targetDependencies_) {
        if (!visited[targetName]) {
            if (hasCycle(targetName)) {
                return targetName;
            }
        }
    }

    return std::nullopt;
}

void ExposureSequence::setMaxConcurrentTargets(size_t max) {
    std::unique_lock lock(mutex_);
    maxConcurrentTargets_ = max;
    LOG_F(INFO, "Maximum concurrent targets set to: {}", max);
}

void ExposureSequence::setGlobalTimeout(std::chrono::seconds timeout) {
    std::unique_lock lock(mutex_);
    globalTimeout_ = timeout;
    LOG_F(INFO, "Global timeout set to: {}s", timeout.count());
}

auto ExposureSequence::getFailedTargets() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return failedTargetNames_;
}

void ExposureSequence::retryFailedTargets() {
    std::unique_lock lock(mutex_);
    std::vector<std::string> targetsToRetry = failedTargetNames_;
    failedTargetNames_.clear();
    failedTargets_.store(0);

    for (const auto& targetName : targetsToRetry) {
        auto it = std::find_if(targets_.begin(), targets_.end(),
                               [&](const auto& target) {
                                   return target->getName() == targetName;
                               });
        if (it != targets_.end()) {
            (*it)->setStatus(TargetStatus::Pending);
            LOG_F(INFO, "Retrying failed target: {}", targetName);
        }
    }
}

auto ExposureSequence::getExecutionStats() const -> json {
    std::shared_lock lock(mutex_);
    return json{
        {"totalExecutions", stats_.totalExecutions},
        {"successfulExecutions", stats_.successfulExecutions},
        {"failedExecutions", stats_.failedExecutions},
        {"averageExecutionTime", stats_.averageExecutionTime},
        {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - stats_.startTime)
                       .count()}};
}

void ExposureSequence::setTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID,
                                           const json& params) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }
    (*target)->setTaskParams(taskUUID, params);
}

auto ExposureSequence::getTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        return std::nullopt;
    }
    return (*target)->getTaskParams(taskUUID);
}

void ExposureSequence::setTargetParams(const std::string& targetName,
                                       const json& params) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target != targets_.end()) {
        (*target)->setParams(params);
    } else {
        LOG_F(ERROR, "Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }
}

auto ExposureSequence::getTargetParams(const std::string& targetName) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target != targets_.end()) {
        return (*target)->getParams();
    }
    return std::nullopt;
}

void ExposureSequence::saveToDatabase() {
    if (!db_ || !sequenceTable_) {
        LOG_F(ERROR, "Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        db_->beginTransaction();

        SequenceModel model;
        model.uuid = uuid_;
        model.name = "Sequence_" + uuid_;
        model.data = serializeToJson().dump();
        model.createdAt = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());

        sequenceTable_->insert(model);

        db_->commit();
        LOG_F(INFO, "Sequence saved to database with UUID: {}", uuid_);
    } catch (const std::exception& e) {
        db_->rollback();
        LOG_F(ERROR, "Failed to save sequence to database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to save sequence to database: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::loadFromDatabase(const std::string& uuid) {
    if (!db_ || !sequenceTable_) {
        LOG_F(ERROR, "Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        auto results = sequenceTable_->query("uuid = '" + uuid + "'", 1);
        if (results.empty()) {
            LOG_F(ERROR, "Sequence with UUID {} not found", uuid);
            THROW_RUNTIME_ERROR("Sequence not found: " + uuid);
        }

        const auto& model = results[0];
        uuid_ = model.uuid;
        json data = json::parse(model.data);
        deserializeFromJson(data);

        LOG_F(INFO, "Sequence loaded from database: {}", uuid);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to load sequence from database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to load sequence from database: " +
                            std::string(e.what()));
    }
}

auto ExposureSequence::listSequences() -> std::vector<SequenceModel> {
    if (!db_ || !sequenceTable_) {
        LOG_F(ERROR, "Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        return sequenceTable_->query();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to list sequences: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to list sequences: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::deleteFromDatabase(const std::string& uuid) {
    if (!db_ || !sequenceTable_) {
        LOG_F(ERROR, "Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        db_->beginTransaction();
        sequenceTable_->remove("uuid = '" + uuid + "'");
        db_->commit();
        LOG_F(INFO, "Sequence deleted from database: {}", uuid);
    } catch (const std::exception& e) {
        db_->rollback();
        LOG_F(ERROR, "Failed to delete sequence from database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to delete sequence from database: " +
                            std::string(e.what()));
    }
}

json ExposureSequence::serializeToJson() const {
    json j;
    std::shared_lock lock(mutex_);

    j["uuid"] = uuid_;
    j["state"] = static_cast<int>(state_.load());
    j["maxConcurrentTargets"] = maxConcurrentTargets_;
    j["globalTimeout"] = globalTimeout_.count();

    j["targets"] = json::array();
    for (const auto& target : targets_) {
        j["targets"].push_back(target->toJson());
    }

    j["dependencies"] = targetDependencies_;
    j["executionStats"] = {
        {"totalExecutions", stats_.totalExecutions},
        {"successfulExecutions", stats_.successfulExecutions},
        {"failedExecutions", stats_.failedExecutions},
        {"averageExecutionTime", stats_.averageExecutionTime}};

    return j;
}

void ExposureSequence::deserializeFromJson(const json& data) {
    std::unique_lock lock(mutex_);

    uuid_ = data["uuid"].get<std::string>();
    state_ = static_cast<SequenceState>(data["state"].get<int>());
    maxConcurrentTargets_ = data["maxConcurrentTargets"].get<size_t>();
    globalTimeout_ = std::chrono::seconds(data["globalTimeout"].get<int64_t>());

    targets_.clear();
    for (const auto& targetJson : data["targets"]) {
        auto target =
            std::make_unique<Target>(targetJson["name"].get<std::string>());
        target->fromJson(targetJson);
        targets_.push_back(std::move(target));
    }

    targetDependencies_ =
        data["dependencies"]
            .get<std::unordered_map<std::string, std::vector<std::string>>>();

    const auto& statsJson = data["executionStats"];
    stats_.totalExecutions = statsJson["totalExecutions"].get<size_t>();
    stats_.successfulExecutions =
        statsJson["successfulExecutions"].get<size_t>();
    stats_.failedExecutions = statsJson["failedExecutions"].get<size_t>();
    stats_.averageExecutionTime =
        statsJson["averageExecutionTime"].get<double>();

    updateTargetReadyStatus();
}

void ExposureSequence::initializeDefaultMacros() {
    // 添加一些默认宏用于任务处理
    taskGenerator_->addMacro(
        "target.uuid",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty())
                return "";
            auto target = std::find_if(
                targets_.begin(), targets_.end(),
                [&args](const auto& t) { return t->getName() == args[0]; });
            return target != targets_.end() ? (*target)->getUUID() : "";
        });

    taskGenerator_->addMacro(
        "target.status",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty())
                return "Unknown";
            return std::to_string(static_cast<int>(getTargetStatus(args[0])));
        });

    taskGenerator_->addMacro(
        "sequence.progress",
        [this](const std::vector<std::string>&) -> std::string {
            return std::to_string(getProgress());
        });
}

void ExposureSequence::setTaskGenerator(
    std::shared_ptr<TaskGenerator> generator) {
    if (!generator) {
        LOG_F(ERROR, "Cannot set null task generator");
        THROW_INVALID_ARGUMENT("Cannot set null task generator");
    }
    std::unique_lock lock(mutex_);
    taskGenerator_ = std::move(generator);
}

auto ExposureSequence::getTaskGenerator() const
    -> std::shared_ptr<TaskGenerator> {
    std::shared_lock lock(mutex_);
    return taskGenerator_;
}

void ExposureSequence::processTargetWithMacros(const std::string& targetName) {
    std::shared_lock lock(mutex_);
    auto target = std::find_if(
        targets_.begin(), targets_.end(),
        [&targetName](const auto& t) { return t->getName() == targetName; });

    if (target == targets_.end()) {
        LOG_F(ERROR, "Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }

    try {
        json targetData = (*target)->toJson();
        taskGenerator_->processJsonWithJsonMacros(targetData);
        (*target)->fromJson(targetData);
        LOG_F(INFO, "Successfully processed target {} with macros", targetName);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to process target {} with macros: {}", targetName,
              e.what());
        THROW_RUNTIME_ERROR("Failed to process target with macros: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::processAllTargetsWithMacros() {
    std::shared_lock lock(mutex_);
    for (const auto& target : targets_) {
        try {
            json targetData = target->toJson();
            taskGenerator_->processJsonWithJsonMacros(targetData);
            target->fromJson(targetData);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to process target {} with macros: {}",
                  target->getName(), e.what());
            THROW_RUNTIME_ERROR("Failed to process target with macros: " +
                                std::string(e.what()));
        }
    }
    LOG_F(INFO, "Successfully processed all targets with macros");
}

void ExposureSequence::processJsonWithGenerator(json& data) {
    try {
        taskGenerator_->processJsonWithJsonMacros(data);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to process JSON with generator: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to process JSON with generator: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::addMacro(const std::string& name, MacroValue value) {
    std::unique_lock lock(mutex_);
    taskGenerator_->addMacro(name, std::move(value));
}

void ExposureSequence::removeMacro(const std::string& name) {
    std::unique_lock lock(mutex_);
    taskGenerator_->removeMacro(name);
}

auto ExposureSequence::listMacros() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return taskGenerator_->listMacros();
}

}  // namespace lithium::sequencer
