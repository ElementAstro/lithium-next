/**
 * @file sequencer.cpp
 * @brief Implementation of the task sequencer for managing target execution.
 */

#include "sequencer.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <stack>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

#include "constant/constant.hpp"
#include "registration.hpp"

namespace lithium::task {

using json = nlohmann::json;

ExposureSequence::ExposureSequence() {
    // Initialize database connection
    try {
        db_ = std::make_shared<database::Database>("sequences.db");
        sequenceTable_ = std::make_unique<database::Table<SequenceModel>>(*db_);
        sequenceTable_->createTable();
        spdlog::info("Database initialized successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to initialize database: " +
                            std::string(e.what()));
    }

    AddPtr(
        Constants::TASK_QUEUE,
        std::make_shared<atom::async::LockFreeHashTable<std::string, json>>());

    taskGenerator_ = TaskGenerator::createShared();
    
    // Register built-in tasks with the factory
    registerBuiltInTasks();
    spdlog::info("Built-in tasks registered with factory");
    
    initializeDefaultMacros();
}

ExposureSequence::~ExposureSequence() { stop(); }

void ExposureSequence::addTarget(std::unique_ptr<Target> target) {
    if (!target) {
        spdlog::error("Cannot add a null target");
        throw std::invalid_argument("Cannot add a null target");
    }

    std::unique_lock lock(mutex_);
    auto it = std::find_if(targets_.begin(), targets_.end(),
                           [&](const std::unique_ptr<Target>& t) {
                               return t->getUUID() == target->getUUID();
                           });
    if (it != targets_.end()) {
        spdlog::error("Target with UUID '{}' already exists",
                      target->getUUID());
        THROW_RUNTIME_ERROR("Target with UUID '" + target->getUUID() +
                            "' already exists");
    }

    spdlog::info("Adding target: {}", target->getName());
    targets_.push_back(std::move(target));
    totalTargets_ = targets_.size();
    spdlog::info("Total targets: {}", totalTargets_);
}

void ExposureSequence::removeTarget(const std::string& name) {
    std::unique_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });

    if (it == targets_.end()) {
        spdlog::error("Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }

    spdlog::info("Removing target: {}", name);
    targets_.erase(it);
    totalTargets_ = targets_.size();
    spdlog::info("Total targets: {}", totalTargets_);
}

void ExposureSequence::modifyTarget(const std::string& name,
                                    const TargetModifier& modifier) {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });

    if (it != targets_.end()) {
        try {
            spdlog::info("Modifying target: {}", name);
            modifier(**it);
            spdlog::info("Target '{}' modified successfully", name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to modify target '{}': {}", name, e.what());
            THROW_RUNTIME_ERROR("Failed to modify target '" + name +
                                "': " + e.what());
        }
    } else {
        spdlog::error("Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }
}

void ExposureSequence::executeAll() {
    SequenceState expected = SequenceState::Idle;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        spdlog::error("Cannot start sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Sequence is not in Idle state");
    }

    completedTargets_.store(0);
    spdlog::info("Starting sequence execution");
    notifySequenceStart();

    // Start sequence execution in a separate thread
    sequenceThread_ = std::jthread([this] { executeSequence(); });
}

void ExposureSequence::stop() {
    SequenceState current = state_.load();
    if (current == SequenceState::Idle || current == SequenceState::Stopped) {
        spdlog::info("Sequence is already in Idle or Stopped state");
        return;
    }

    spdlog::info("Stopping sequence execution");
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
        spdlog::error("Cannot pause sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot pause sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    spdlog::info("Sequence paused");
}

void ExposureSequence::resume() {
    SequenceState expected = SequenceState::Paused;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        spdlog::error("Cannot resume sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot resume sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    spdlog::info("Sequence resumed");
}

void ExposureSequence::saveSequence(const std::string& filename) const {
    json j;
    std::shared_lock lock(mutex_);

    j["targets"] = json::array();
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
        spdlog::error("Failed to open file '{}' for writing", filename);
        THROW_RUNTIME_ERROR("Failed to open file '" + filename +
                            "' for writing");
    }

    file << j.dump(4);
    spdlog::info("Sequence saved to file: {}", filename);
}

void ExposureSequence::loadSequence(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open file '{}' for reading", filename);
        THROW_RUNTIME_ERROR("Failed to open file '" + filename +
                            "' for reading");
    }

    json j;
    file >> j;

    std::unique_lock lock(mutex_);
    targets_.clear();

    if (!j.contains("targets") || !j["targets"].is_array()) {
        spdlog::error("Invalid sequence file format: 'targets' array missing");
        THROW_RUNTIME_ERROR(
            "Invalid sequence file format: 'targets' array missing");
    }

    for (const auto& targetJson : j["targets"]) {
        // Process JSON with generator before creating the target
        json processedJson = targetJson;
        processJsonWithGenerator(processedJson);

        std::string name = processedJson["name"].get<std::string>();
        bool enabled = processedJson["enabled"].get<bool>();
        auto target = std::make_unique<Target>(name);
        target->setEnabled(enabled);

        // Load tasks using the improved loadTasksFromJson method
        if (processedJson.contains("tasks") &&
            processedJson["tasks"].is_array()) {
            target->loadTasksFromJson(processedJson["tasks"]);
        }

        targets_.push_back(std::move(target));
    }

    totalTargets_ = targets_.size();
    spdlog::info("Sequence loaded from file: {}", filename);
    spdlog::info("Total targets loaded: {}", totalTargets_);
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

    return TargetStatus::Skipped;  // Default if target not found
}

auto ExposureSequence::getProgress() const -> double {
    size_t completed = completedTargets_.load();
    size_t total;
    {
        std::shared_lock lock(mutex_);
        total = totalTargets_;
    }

    if (total == 0) {
        return 100.0;
    }

    return (static_cast<double>(completed) / static_cast<double>(total)) *
           100.0;
}

// Callback setter methods

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

void ExposureSequence::setProgressCallback(ProgressCallback callback) {
    std::unique_lock lock(mutex_);
    onProgress_ = std::move(callback);
}

void ExposureSequence::setOnTaskStart(TaskCallback callback) {
    std::unique_lock lock(mutex_);
    onTaskStart_ = std::move(callback);
}

void ExposureSequence::setOnTaskEnd(TaskCallback callback) {
    std::unique_lock lock(mutex_);
    onTaskEnd_ = std::move(callback);
}

// Notification helper methods

void ExposureSequence::notifySequenceStart() {
    SequenceCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onSequenceStart_;
    }

    if (callbackCopy) {
        try {
            callbackCopy();
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence start callback: {}", e.what());
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
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence end callback: {}", e.what());
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
        } catch (const std::exception& e) {
            spdlog::error("Exception in target start callback for {}: {}", name,
                          e.what());
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
        } catch (const std::exception& e) {
            spdlog::error("Exception in target end callback for {}: {}", name,
                          e.what());
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
        } catch (const std::exception& ex) {
            spdlog::error("Exception in error callback for {}: {}", name,
                          ex.what());
        }
    }
}

void ExposureSequence::notifyProgress() {
    ProgressCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onProgress_;
    }

    if (callbackCopy) {
        try {
            json progressJson = buildProgressJson();
            callbackCopy(progressJson);
        } catch (const std::exception& e) {
            spdlog::error("Exception in progress callback: {}", e.what());
        }
    }
}

void ExposureSequence::notifyTaskStart(const std::string& targetName,
                                       const std::string& taskName,
                                       const json& taskInfo) {
    // Update current tracking
    currentTargetName_ = targetName;
    currentTaskName_ = taskName;

    TaskCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTaskStart_;
    }

    if (callbackCopy) {
        try {
            callbackCopy(targetName, taskName, taskInfo);
        } catch (const std::exception& e) {
            spdlog::error("Exception in task start callback for {}/{}: {}",
                          targetName, taskName, e.what());
        }
    }

    // Also notify progress
    notifyProgress();
}

void ExposureSequence::notifyTaskEnd(const std::string& targetName,
                                     const std::string& taskName,
                                     const json& taskInfo) {
    TaskCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTaskEnd_;
    }

    if (callbackCopy) {
        try {
            callbackCopy(targetName, taskName, taskInfo);
        } catch (const std::exception& e) {
            spdlog::error("Exception in task end callback for {}/{}: {}",
                          targetName, taskName, e.what());
        }
    }

    // Also notify progress
    notifyProgress();
}

json ExposureSequence::buildProgressJson() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats_.startTime).count();

    // Calculate estimated remaining time
    double progress = getProgress();
    int64_t estimatedRemaining = 0;
    if (progress > 0.0 && progress < 100.0) {
        estimatedRemaining = static_cast<int64_t>(
            (elapsed / progress) * (100.0 - progress));
    }

    // Get state string
    std::string stateStr;
    switch (state_.load()) {
        case SequenceState::Idle: stateStr = "idle"; break;
        case SequenceState::Running: stateStr = "running"; break;
        case SequenceState::Paused: stateStr = "paused"; break;
        case SequenceState::Stopping: stateStr = "stopping"; break;
        case SequenceState::Stopped: stateStr = "stopped"; break;
    }

    return json{
        {"sequenceId", uuid_},
        {"state", stateStr},
        {"progress", progress},
        {"completedTargets", completedTargets_.load()},
        {"totalTargets", totalTargets_},
        {"currentTarget", currentTargetName_},
        {"currentTask", currentTaskName_},
        {"elapsedTime", elapsed},
        {"estimatedRemaining", estimatedRemaining},
        {"failedTargets", failedTargets_.load()}
    };
}

void ExposureSequence::executeSequence() {
    stats_.startTime = std::chrono::steady_clock::now();

    while (auto* target = getNextExecutableTarget()) {
        if (state_.load() == SequenceState::Stopping) {
            break;
        }

        try {
            spdlog::info("Executing target: {} ({}/{} completed)",
                         target->getName(), completedTargets_.load(),
                         totalTargets_);

            stats_.totalExecutions++;
            notifyTargetStart(target->getName());
            target->execute();

            if (target->getStatus() == TargetStatus::Completed) {
                stats_.successfulExecutions++;
                completedTargets_.fetch_add(1);
            } else if (target->getStatus() == TargetStatus::Failed) {
                stats_.failedExecutions++;
                failedTargets_.fetch_add(1);

                std::unique_lock lock(mutex_);
                failedTargetNames_.push_back(target->getName());
            }

            notifyTargetEnd(target->getName(), target->getStatus());

        } catch (const std::exception& e) {
            spdlog::error("Target execution failed: {} - {}", target->getName(),
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

    // Check concurrent execution limits
    size_t runningTargets = 0;
    {
        std::shared_lock lock(mutex_);
        if (maxConcurrentTargets_ > 0) {
            for (const auto& target : targets_) {
                if (target->getStatus() == TargetStatus::InProgress) {
                    runningTargets++;
                }
            }

            if (runningTargets >= maxConcurrentTargets_) {
                return nullptr;
            }
        }

        // Find next ready target
        for (const auto& target : targets_) {
            if (target->getStatus() == TargetStatus::Pending &&
                isTargetReady(target->getName())) {
                return target.get();
            }
        }
    }

    return nullptr;
}

void ExposureSequence::handleTargetError(Target* target,
                                         const std::exception& e) {
    RecoveryStrategy strategy;
    {
        std::shared_lock lock(mutex_);
        strategy = recoveryStrategy_;
    }

    switch (strategy) {
        case RecoveryStrategy::Stop:
            state_.store(SequenceState::Stopping);
            break;

        case RecoveryStrategy::Skip:
            target->setStatus(TargetStatus::Skipped);
            notifyTargetEnd(target->getName(), TargetStatus::Skipped);
            break;

        case RecoveryStrategy::Retry:
            // Implementation would handle retry logic
            spdlog::info("Retry strategy selected for target: {}",
                         target->getName());
            break;

        case RecoveryStrategy::Alternative: {
            std::shared_lock lock(mutex_);
            // Try to execute alternative target if available
            if (auto it = alternativeTargets_.find(target->getName());
                it != alternativeTargets_.end()) {
                spdlog::info("Executing alternative target for: {}",
                             target->getName());
                it->second->execute();
            }
            break;
        }
    }

    notifyError(target->getName(), e);
}

void ExposureSequence::setSchedulingStrategy(SchedulingStrategy strategy) {
    std::unique_lock lock(mutex_);
    schedulingStrategy_ = strategy;

    // Reorder targets based on new strategy
    switch (strategy) {
        case SchedulingStrategy::Dependencies:
            reorderTargetsByDependencies();
            break;
        case SchedulingStrategy::Priority:
            reorderTargetsByPriority();
            break;
        default:
            // No reordering needed for FIFO
            break;
    }
}

void ExposureSequence::setRecoveryStrategy(RecoveryStrategy strategy) {
    std::unique_lock lock(mutex_);
    recoveryStrategy_ = strategy;
    spdlog::info("Recovery strategy set to: {}", static_cast<int>(strategy));
}

void ExposureSequence::addAlternativeTarget(
    const std::string& targetName, std::unique_ptr<Target> alternative) {
    if (!alternative) {
        spdlog::error("Cannot add null alternative target for {}", targetName);
        throw std::invalid_argument("Cannot add null alternative target");
    }

    std::unique_lock lock(mutex_);
    alternativeTargets_[targetName] = std::move(alternative);
    spdlog::info("Alternative target added for: {}", targetName);
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

        // Use std::move with care - we're manipulating a const reference
        auto& mutableTarget = const_cast<std::unique_ptr<Target>&>(target);
        stack.push(std::move(mutableTarget));
    };

    // Create a copy of targets since we'll be moving them
    auto targetsCopy = std::move(targets_);
    targets_.clear();

    for (auto& target : targetsCopy) {
        if (!visited[target->getName()]) {
            visit(target, visit);
        }
    }

    while (!stack.empty()) {
        targets_.push_back(std::move(stack.top()));
        stack.pop();
    }

    spdlog::info("Targets reordered by dependencies");
}

void ExposureSequence::reorderTargetsByPriority() {
    // Sort targets by priority using targetDependencies_ count as a simple
    // proxy for priority (targets with fewer dependencies execute first)
    std::sort(
        targets_.begin(), targets_.end(),
        [this](const std::unique_ptr<Target>& a, const std::unique_ptr<Target>& b) {
            auto itA = targetDependencies_.find(a->getName());
            auto itB = targetDependencies_.find(b->getName());
            size_t depsA = (itA != targetDependencies_.end()) ? itA->second.size() : 0;
            size_t depsB = (itB != targetDependencies_.end()) ? itB->second.size() : 0;
            return depsA < depsB;  // Fewer dependencies = higher priority
        });
    spdlog::info("Targets reordered by priority");
}

void ExposureSequence::addTargetDependency(const std::string& targetName,
                                           const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    // Add dependency if not already present
    auto& dependencies = targetDependencies_[targetName];
    if (std::find(dependencies.begin(), dependencies.end(), dependsOn) ==
        dependencies.end()) {
        dependencies.push_back(dependsOn);
    }

    // Check for cyclic dependencies
    if (auto cycle = findCyclicDependency()) {
        // Remove the dependency we just added
        dependencies.pop_back();
        THROW_RUNTIME_ERROR("Cyclic dependency detected in target: " +
                            cycle.value());
    }

    updateTargetReadyStatus();
    spdlog::info("Added dependency: {} depends on {}", targetName, dependsOn);
}

void ExposureSequence::removeTargetDependency(const std::string& targetName,
                                              const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    auto& deps = targetDependencies_[targetName];
    deps.erase(std::remove(deps.begin(), deps.end(), dependsOn), deps.end());
    updateTargetReadyStatus();
    spdlog::info("Removed dependency: {} no longer depends on {}", targetName,
                 dependsOn);
}

auto ExposureSequence::isTargetReady(const std::string& targetName) const
    -> bool {
    std::shared_lock lock(mutex_);
    auto it = targetReadyStatus_.find(targetName);
    return it != targetReadyStatus_.end() && it->second;
}

auto ExposureSequence::getTargetDependencies(
    const std::string& targetName) const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    auto it = targetDependencies_.find(targetName);
    if (it != targetDependencies_.end()) {
        return it->second;
    }
    return {};
}

void ExposureSequence::updateTargetReadyStatus() {
    // First mark all targets as ready
    for (const auto& target : targets_) {
        targetReadyStatus_[target->getName()] = true;
    }

    // Check dependencies for each target
    bool changed;
    do {
        changed = false;
        for (const auto& [targetName, dependencies] : targetDependencies_) {
            // If any dependency is not ready, the target is also not ready
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
    spdlog::info("Maximum concurrent targets set to: {}", max);
}

void ExposureSequence::setTargetTimeout(const std::string& name,
                                        std::chrono::seconds timeout) {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });
    if (it != targets_.end()) {
        // Implementation depends on Target interface
        spdlog::info("Set timeout for target {}: {} seconds", name,
                     timeout.count());
        // (*it)->setTimeout(timeout);  // Assuming Target has setTimeout method
    } else {
        spdlog::error("Target not found: {}", name);
        THROW_RUNTIME_ERROR("Target not found: " + name);
    }
}

void ExposureSequence::setGlobalTimeout(std::chrono::seconds timeout) {
    std::unique_lock lock(mutex_);
    globalTimeout_ = timeout;
    spdlog::info("Global timeout set to: {}s", timeout.count());
}

auto ExposureSequence::getFailedTargets() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return failedTargetNames_;
}

auto ExposureSequence::getExecutionStats() const -> json {
    std::shared_lock lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - stats_.startTime)
            .count();

    return json{{"totalExecutions", stats_.totalExecutions},
                {"successfulExecutions", stats_.successfulExecutions},
                {"failedExecutions", stats_.failedExecutions},
                {"averageExecutionTime", stats_.averageExecutionTime},
                {"uptime", uptime}};
}

auto ExposureSequence::getResourceUsage() const -> json {
    // Implementation would collect resource usage metrics
    return json{
        {"memoryUsage", getTotalMemoryUsage()},
        {"cpuUsage",
         0.0},  // Would be implemented with actual CPU usage tracking
        {"diskUsage", 0}
        // Would be implemented with actual disk usage tracking
    };
}

void ExposureSequence::retryFailedTargets() {
    std::vector<std::string> targetsToRetry;
    {
        std::unique_lock lock(mutex_);
        targetsToRetry = failedTargetNames_;
        failedTargetNames_.clear();
        failedTargets_.store(0);
    }

    for (const auto& targetName : targetsToRetry) {
        std::shared_lock lock(mutex_);
        auto it = std::find_if(targets_.begin(), targets_.end(),
                               [&](const auto& target) {
                                   return target->getName() == targetName;
                               });
        if (it != targets_.end()) {
            (*it)->setStatus(TargetStatus::Pending);
            spdlog::info("Retrying failed target: {}", targetName);
        }
    }
}

void ExposureSequence::skipFailedTargets() {
    std::vector<std::string> targetsToSkip;
    {
        std::unique_lock lock(mutex_);
        targetsToSkip = failedTargetNames_;
        failedTargetNames_.clear();
        failedTargets_.store(0);
    }

    for (const auto& targetName : targetsToSkip) {
        std::shared_lock lock(mutex_);
        auto it = std::find_if(targets_.begin(), targets_.end(),
                               [&](const auto& target) {
                                   return target->getName() == targetName;
                               });
        if (it != targets_.end()) {
            (*it)->setStatus(TargetStatus::Skipped);
            spdlog::info("Skipping failed target: {}", targetName);
        }
    }
}

void ExposureSequence::setTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID,
                                           const json& params) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        spdlog::error("Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }

    (*target)->setTaskParams(taskUUID, params);
    spdlog::info("Set parameters for task {} in target {}", taskUUID,
                 targetName);
}

auto ExposureSequence::getTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        spdlog::warn("Target not found when getting task params: {}",
                     targetName);
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
        spdlog::info("Set parameters for target {}", targetName);
    } else {
        spdlog::error("Target not found: {}", targetName);
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

    spdlog::warn("Target not found when getting params: {}", targetName);
    return std::nullopt;
}

void ExposureSequence::saveToDatabase() {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database not initialized");
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
        spdlog::info("Sequence saved to database with UUID: {}", uuid_);
    } catch (const std::exception& e) {
        db_->rollback();
        spdlog::error("Failed to save sequence to database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to save sequence to database: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::loadFromDatabase(const std::string& uuid) {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        auto results = sequenceTable_->query("uuid = '" + uuid + "'", 1);
        if (results.empty()) {
            spdlog::error("Sequence with UUID {} not found", uuid);
            THROW_RUNTIME_ERROR("Sequence not found: " + uuid);
        }

        const auto& model = results[0];
        uuid_ = model.uuid;
        json data = json::parse(model.data);
        deserializeFromJson(data);

        spdlog::info("Sequence loaded from database: {}", uuid);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load sequence from database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to load sequence from database: " +
                            std::string(e.what()));
    }
}

auto ExposureSequence::listSequences() -> std::vector<SequenceModel> {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        return sequenceTable_->query();
    } catch (const std::exception& e) {
        spdlog::error("Failed to list sequences: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to list sequences: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::deleteFromDatabase(const std::string& uuid) {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database not initialized");
        THROW_RUNTIME_ERROR("Database not initialized");
    }

    try {
        db_->beginTransaction();
        sequenceTable_->remove("uuid = '" + uuid + "'");
        db_->commit();
        spdlog::info("Sequence deleted from database: {}", uuid);
    } catch (const std::exception& e) {
        db_->rollback();
        spdlog::error("Failed to delete sequence from database: {}", e.what());
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
    spdlog::info("Sequence deserialized from JSON data");
}

void ExposureSequence::initializeDefaultMacros() {
    // Initialize built-in macros (date, time, math, string, etc.)
    taskGenerator_->initializeBuiltInMacros();
    
    // Register default task templates
    taskGenerator_->registerDefaultTemplates();

    // Add sequence-specific macros
    taskGenerator_->addMacro(
        "target.uuid",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty())
                return "";

            std::shared_lock lock(mutex_);
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

    taskGenerator_->addMacro(
        "sequence.uuid",
        [this](const std::vector<std::string>&) -> std::string {
            return uuid_;
        });

    taskGenerator_->addMacro(
        "sequence.target_count",
        [this](const std::vector<std::string>&) -> std::string {
            std::shared_lock lock(mutex_);
            return std::to_string(targets_.size());
        });

    spdlog::info("Default macros initialized");
}

void ExposureSequence::setTaskGenerator(
    std::shared_ptr<TaskGenerator> generator) {
    if (!generator) {
        spdlog::error("Cannot set null task generator");
        throw std::invalid_argument("Cannot set null task generator");
    }

    std::unique_lock lock(mutex_);
    taskGenerator_ = std::move(generator);
    spdlog::info("Task generator set");
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
        spdlog::error("Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }

    try {
        json targetData = (*target)->toJson();
        taskGenerator_->processJsonWithJsonMacros(targetData);
        (*target)->fromJson(targetData);
        spdlog::info("Successfully processed target {} with macros",
                     targetName);
    } catch (const std::exception& e) {
        spdlog::error("Failed to process target {} with macros: {}", targetName,
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
            spdlog::error("Failed to process target {} with macros: {}",
                          target->getName(), e.what());
            THROW_RUNTIME_ERROR("Failed to process target with macros: " +
                                std::string(e.what()));
        }
    }
    spdlog::info("Successfully processed all targets with macros");
}

void ExposureSequence::processJsonWithGenerator(json& data) {
    try {
        taskGenerator_->processJsonWithJsonMacros(data);
    } catch (const std::exception& e) {
        spdlog::error("Failed to process JSON with generator: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to process JSON with generator: " +
                            std::string(e.what()));
    }
}

void ExposureSequence::addMacro(const std::string& name, MacroValue value) {
    std::unique_lock lock(mutex_);
    taskGenerator_->addMacro(name, std::move(value));
    spdlog::info("Macro added: {}", name);
}

void ExposureSequence::removeMacro(const std::string& name) {
    std::unique_lock lock(mutex_);
    taskGenerator_->removeMacro(name);
    spdlog::info("Macro removed: {}", name);
}

auto ExposureSequence::listMacros() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return taskGenerator_->listMacros();
}

auto ExposureSequence::getAverageExecutionTime() const
    -> std::chrono::milliseconds {
    std::shared_lock lock(mutex_);
    return std::chrono::milliseconds(
        static_cast<int64_t>(stats_.averageExecutionTime));
}

auto ExposureSequence::getTotalMemoryUsage() const -> size_t {
    std::shared_lock lock(mutex_);
    size_t totalMemory = 0;

    for (const auto& target : targets_) {
        for (const auto& task : target->getTasks()) {
            totalMemory += task->getMemoryUsage();
        }
    }

    return totalMemory;
}

void ExposureSequence::setTargetPriority(const std::string& targetName,
                                         int priority) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });

    if (target != targets_.end()) {
        // Implementation would set priority on the target
        spdlog::info("Set priority {} for target {}", priority, targetName);
    } else {
        spdlog::error("Target not found for priority setting: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }
}

// Execution Strategy Methods

void ExposureSequence::setExecutionStrategy(ExecutionStrategy strategy) {
    std::unique_lock lock(mutex_);
    executionStrategy_ = strategy;
    spdlog::info("Execution strategy set to: {}", static_cast<int>(strategy));
}

auto ExposureSequence::getExecutionStrategy() const -> ExecutionStrategy {
    std::shared_lock lock(mutex_);
    return executionStrategy_;
}

void ExposureSequence::setConcurrencyLimit(size_t limit) {
    std::unique_lock lock(mutex_);
    concurrencyLimit_ = limit;
    spdlog::info("Concurrency limit set to: {}", limit);
}

auto ExposureSequence::getConcurrencyLimit() const -> size_t {
    std::shared_lock lock(mutex_);
    return concurrencyLimit_;
}

void ExposureSequence::enableMonitoring(bool enabled) {
    monitoringEnabled_ = enabled;
    spdlog::info("Monitoring {}", enabled ? "enabled" : "disabled");
}

auto ExposureSequence::isMonitoringEnabled() const -> bool {
    return monitoringEnabled_;
}

void ExposureSequence::enableScriptIntegration(bool enabled) {
    scriptIntegrationEnabled_ = enabled;
    spdlog::info("Script integration {}", enabled ? "enabled" : "disabled");
}

void ExposureSequence::setResourceLimits(double maxCpuUsage, size_t maxMemoryUsage) {
    std::unique_lock lock(mutex_);
    resourceLimits_.maxCpuUsage = maxCpuUsage;
    resourceLimits_.maxMemoryUsage = maxMemoryUsage;
    spdlog::info("Resource limits set: CPU {}%, Memory {}MB",
                 maxCpuUsage, maxMemoryUsage / (1024 * 1024));
}

void ExposureSequence::enablePerformanceOptimization(bool enabled) {
    performanceOptimizationEnabled_ = enabled;
    spdlog::info("Performance optimization {}", enabled ? "enabled" : "disabled");
}

auto ExposureSequence::getOptimizationSuggestions() const -> json {
    return analyzePerformance();
}

auto ExposureSequence::getMetrics() const -> json {
    std::shared_lock lock(mutex_);
    
    json metrics = {
        {"totalTargets", totalTargets_},
        {"completedTargets", completedTargets_.load()},
        {"failedTargets", failedTargets_.load()},
        {"progressPercentage", getProgress()},
        {"state", static_cast<int>(state_.load())},
        {"executionStrategy", static_cast<int>(executionStrategy_)},
        {"concurrencyLimit", concurrencyLimit_},
        {"monitoringEnabled", monitoringEnabled_},
        {"statistics", {
            {"totalExecutions", stats_.totalExecutions},
            {"successfulExecutions", stats_.successfulExecutions},
            {"failedExecutions", stats_.failedExecutions},
            {"averageExecutionTime", stats_.averageExecutionTime}
        }},
        {"resourceLimits", {
            {"maxCpuUsage", resourceLimits_.maxCpuUsage},
            {"maxMemoryUsage", resourceLimits_.maxMemoryUsage}
        }}
    };
    
    return metrics;
}

// Strategy Execution Methods

void ExposureSequence::executeSequential(const std::vector<Target*>& targets) {
    spdlog::info("Executing {} targets sequentially", targets.size());
    
    for (auto* target : targets) {
        if (state_.load() == SequenceState::Stopping) {
            break;
        }
        
        while (state_.load() == SequenceState::Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        try {
            notifyTargetStart(target->getName());
            target->execute();
            notifyTargetEnd(target->getName(), target->getStatus());
            
            if (target->getStatus() == TargetStatus::Completed) {
                completedTargets_.fetch_add(1);
            } else if (target->getStatus() == TargetStatus::Failed) {
                failedTargets_.fetch_add(1);
            }
        } catch (const std::exception& e) {
            spdlog::error("Target execution failed: {} - {}", target->getName(), e.what());
            handleTargetError(target, e);
        }
        
        if (monitoringEnabled_) {
            updateResourceMetrics();
        }
    }
}

void ExposureSequence::executeParallel(const std::vector<Target*>& targets) {
    spdlog::info("Executing {} targets in parallel with concurrency limit: {}",
                 targets.size(), concurrencyLimit_);
    
    std::vector<std::future<void>> futures;
    size_t activeCount = 0;
    
    for (auto* target : targets) {
        // Wait if concurrency limit reached
        while (activeCount >= concurrencyLimit_ && state_.load() != SequenceState::Stopping) {
            futures.erase(
                std::remove_if(futures.begin(), futures.end(),
                    [](std::future<void>& f) {
                        return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
                    }),
                futures.end()
            );
            activeCount = futures.size();
            
            if (activeCount >= concurrencyLimit_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        if (state_.load() == SequenceState::Stopping) {
            break;
        }
        
        futures.push_back(std::async(std::launch::async, [this, target]() {
            try {
                notifyTargetStart(target->getName());
                target->execute();
                notifyTargetEnd(target->getName(), target->getStatus());
                
                if (target->getStatus() == TargetStatus::Completed) {
                    completedTargets_.fetch_add(1);
                } else if (target->getStatus() == TargetStatus::Failed) {
                    failedTargets_.fetch_add(1);
                }
            } catch (const std::exception& e) {
                spdlog::error("Parallel target execution failed: {} - {}",
                             target->getName(), e.what());
                handleTargetError(target, e);
            }
        }));
        
        activeCount = futures.size();
    }
    
    // Wait for all remaining tasks
    for (auto& future : futures) {
        future.wait();
    }
}

void ExposureSequence::executeAdaptive(const std::vector<Target*>& targets) {
    spdlog::info("Executing targets with adaptive strategy");
    
    // Start with sequential, switch to parallel if resources allow
    if (targets.size() <= 3) {
        executeSequential(targets);
    } else {
        updateResourceMetrics();
        if (checkResourceAvailability()) {
            executeParallel(targets);
        } else {
            executeSequential(targets);
        }
    }
}

void ExposureSequence::executePriority(const std::vector<Target*>& targets) {
    spdlog::info("Executing targets with priority strategy");
    
    // Sort targets by priority (would need priority field in Target)
    std::vector<Target*> sortedTargets = targets;
    // For now, use parallel execution
    executeParallel(sortedTargets);
}

void ExposureSequence::updateResourceMetrics() {
    // Update resource metrics - simplified implementation
    spdlog::debug("Updating resource metrics");
    // Would implement actual resource monitoring here
}

auto ExposureSequence::checkResourceAvailability() const -> bool {
    // Check if resources are available based on limits
    // Simplified implementation - always return true for now
    return true;
}

auto ExposureSequence::determineOptimalStrategy() const -> ExecutionStrategy {
    // updateResourceMetrics() is not const, so we skip it here
    
    if (!checkResourceAvailability()) {
        return ExecutionStrategy::Sequential;
    }
    
    // If we have many targets and good resources, use parallel
    if (totalTargets_ > 5) {
        return ExecutionStrategy::Parallel;
    }
    
    return ExecutionStrategy::Sequential;
}

auto ExposureSequence::analyzePerformance() const -> json {
    json suggestions = json::array();
    
    std::shared_lock lock(mutex_);
    
    // Analyze failure rate
    double failureRate = totalTargets_ > 0 ? 
        (static_cast<double>(failedTargets_.load()) / totalTargets_) * 100.0 : 0.0;
    
    if (failureRate > 10.0) {
        suggestions.push_back({
            {"type", "high_failure_rate"},
            {"message", "High target failure rate detected"},
            {"recommendation", "Review target dependencies and error handling"}
        });
    }
    
    // Analyze execution time
    if (executionStrategy_ == ExecutionStrategy::Sequential && totalTargets_ > 10) {
        suggestions.push_back({
            {"type", "slow_sequential_execution"},
            {"message", "Sequential execution with many targets may be slow"},
            {"recommendation", "Consider using parallel or adaptive execution strategy"}
        });
    }
    
    // Check concurrency
    if (executionStrategy_ == ExecutionStrategy::Parallel && concurrencyLimit_ == 1) {
        suggestions.push_back({
            {"type", "low_concurrency"},
            {"message", "Parallel execution with concurrency limit of 1"},
            {"recommendation", "Increase concurrency limit to utilize parallel execution"}
        });
    }
    
    return {
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
        {"suggestions", suggestions},
        {"currentMetrics", getMetrics()}
    };
}

// ============================================================================
// Astronomical Scheduling Methods Implementation
// ============================================================================

void ExposureSequence::setObserverLocation(const ObserverLocation& location) {
    std::unique_lock lock(mutex_);
    observerLocation_ = location;
    spdlog::info("Observer location set: lat={:.4f}° lon={:.4f}°",
                 location.latitude, location.longitude);
}

const ObserverLocation& ExposureSequence::getObserverLocation() const {
    std::shared_lock lock(mutex_);
    return observerLocation_;
}

void ExposureSequence::sortTargetsByObservability() {
    std::unique_lock lock(mutex_);
    
    std::sort(targets_.begin(), targets_.end(),
        [](const std::unique_ptr<Target>& a, const std::unique_ptr<Target>& b) {
            // Sort by: 1) Currently observable, 2) Priority, 3) Remaining time
            bool aObs = a->isObservable();
            bool bObs = b->isObservable();
            
            if (aObs != bObs) return aObs > bObs;  // Observable first
            
            int aPri = a->getPriority();
            int bPri = b->getPriority();
            if (aPri != bPri) return aPri > bPri;  // Higher priority first
            
            // Less remaining time = more urgent
            return a->getRemainingExposureTime() < b->getRemainingExposureTime();
        });
    
    spdlog::info("Targets sorted by observability");
}

Target* ExposureSequence::getNextObservableTarget() {
    std::shared_lock lock(mutex_);
    
    for (const auto& target : targets_) {
        if (target->getStatus() == TargetStatus::Pending &&
            target->isObservable() &&
            !target->areExposurePlansComplete()) {
            return target.get();
        }
    }
    
    return nullptr;
}

void ExposureSequence::updateTargetObservability() {
    std::shared_lock lock(mutex_);
    
    for (const auto& target : targets_) {
        // TODO: Calculate actual observability based on observer location
        // This would integrate with an astronomical calculation library
        // For now, just log that we're updating
        spdlog::debug("Updating observability for target: {}", target->getName());
    }
}

std::string ExposureSequence::checkMeridianFlips() const {
    std::shared_lock lock(mutex_);
    
    for (const auto& target : targets_) {
        if (target->getStatus() == TargetStatus::InProgress &&
            target->needsMeridianFlip()) {
            return target->getName();
        }
    }
    
    return "";
}

json ExposureSequence::getObservabilitySummary() const {
    std::shared_lock lock(mutex_);
    
    json summary = json::array();
    
    for (const auto& target : targets_) {
        const auto& config = target->getAstroConfig();
        const auto& obs = target->getObservabilityWindow();
        const auto& altAz = target->getHorizontalCoordinates();
        
        summary.push_back({
            {"name", target->getName()},
            {"catalogName", config.catalogName},
            {"coordinates", config.coordinates.toJson()},
            {"currentAltitude", altAz.altitude},
            {"currentAzimuth", altAz.azimuth},
            {"isObservable", target->isObservable()},
            {"remainingTime", obs.remainingSeconds()},
            {"maxAltitude", obs.maxAltitude},
            {"priority", config.priority},
            {"exposureProgress", target->getExposureProgress()},
            {"remainingExposureTime", target->getRemainingExposureTime()}
        });
    }
    
    return summary;
}

void ExposureSequence::setMinimumAltitude(double altitude) {
    std::unique_lock lock(mutex_);
    minimumAltitude_ = std::clamp(altitude, 0.0, 90.0);
    spdlog::info("Minimum altitude set to: {:.1f}°", minimumAltitude_);
}

std::chrono::system_clock::time_point ExposureSequence::getEstimatedCompletionTime() const {
    std::shared_lock lock(mutex_);
    
    double totalRemainingTime = 0.0;
    for (const auto& target : targets_) {
        if (target->getStatus() != TargetStatus::Completed &&
            target->getStatus() != TargetStatus::Skipped) {
            totalRemainingTime += target->getRemainingExposureTime();
        }
    }
    
    // Add overhead for slewing, focusing, etc. (estimate 5 min per target)
    size_t remainingTargets = 0;
    for (const auto& target : targets_) {
        if (target->getStatus() == TargetStatus::Pending) {
            remainingTargets++;
        }
    }
    totalRemainingTime += remainingTargets * 300;  // 5 minutes per target
    
    return std::chrono::system_clock::now() + 
           std::chrono::seconds(static_cast<int64_t>(totalRemainingTime));
}

bool ExposureSequence::canCompleteBeforeDawn(
    std::chrono::system_clock::time_point dawnTime) const {
    auto estimatedCompletion = getEstimatedCompletionTime();
    bool canComplete = estimatedCompletion <= dawnTime;
    
    if (!canComplete) {
        auto diff = std::chrono::duration_cast<std::chrono::minutes>(
            estimatedCompletion - dawnTime);
        spdlog::warn("Sequence will NOT complete before dawn. "
                     "Estimated completion is {} minutes after dawn",
                     diff.count());
    }
    
    return canComplete;
}

std::vector<std::string> ExposureSequence::getTargetsCompletableBeforeDawn(
    std::chrono::system_clock::time_point dawnTime) const {
    std::shared_lock lock(mutex_);
    
    std::vector<std::string> completableTargets;
    auto now = std::chrono::system_clock::now();
    auto availableTime = std::chrono::duration_cast<std::chrono::seconds>(
        dawnTime - now).count();
    
    double accumulatedTime = 0.0;
    
    for (const auto& target : targets_) {
        if (target->getStatus() == TargetStatus::Completed ||
            target->getStatus() == TargetStatus::Skipped) {
            continue;
        }
        
        // Estimate time for this target (exposure + 5 min overhead)
        double targetTime = target->getRemainingExposureTime() + 300;
        
        // Check if target is observable and can complete before dawn
        if (target->isObservable()) {
            // Also check if target will still be observable
            const auto& obs = target->getObservabilityWindow();
            auto targetSetTime = obs.setTime;
            
            // Target must complete before it sets or before dawn
            auto effectiveEndTime = std::min(targetSetTime, dawnTime);
            auto effectiveAvailableTime = 
                std::chrono::duration_cast<std::chrono::seconds>(
                    effectiveEndTime - now).count();
            
            if (accumulatedTime + targetTime <= effectiveAvailableTime) {
                completableTargets.push_back(target->getName());
                accumulatedTime += targetTime;
            }
        }
    }
    
    spdlog::info("Found {} targets completable before dawn (of {} total)",
                 completableTargets.size(), targets_.size());
    
    return completableTargets;
}

}  // namespace lithium::task