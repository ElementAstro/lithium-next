/*
 * script_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file script_manager.cpp
 * @brief Unified script management facade implementation
 * @date 2024-1-13
 */

#include "script_manager.hpp"

#include <fstream>
#include <future>
#include <shared_mutex>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::shell {

class ScriptManager::Impl {
public:
    // Script storage
    std::unordered_map<std::string, Script> scripts_;
    mutable std::shared_mutex scriptsMutex_;

    // Component instances
    MetadataManager metadataManager_;
    HookManager hookManager_;
    VersionManager versionManager_;
    ResourceManager resourceManager_;
    RetryExecutor retryExecutor_;

    // Execution tracking
    std::unordered_map<std::string, std::atomic<float>> progressTrackers_;
    std::unordered_map<std::string, std::atomic<bool>> abortFlags_;

    // Statistics
    struct Statistics {
        size_t executionCount{0};
        size_t successCount{0};
        size_t failureCount{0};
        std::chrono::milliseconds totalTime{0};
    };
    std::unordered_map<std::string, Statistics> statistics_;
    mutable std::shared_mutex statsMutex_;

    // Create appropriate executor for a script
    auto createExecutor(std::string_view scriptName) -> std::unique_ptr<IScriptExecutor> {
        std::shared_lock lock(scriptsMutex_);
        auto it = scripts_.find(std::string(scriptName));
        if (it == scripts_.end()) {
            return nullptr;
        }
        return ScriptExecutorFactory::createForScript(it->second);
    }

    void updateStats(const std::string& name, const ScriptExecutionResult& result) {
        std::unique_lock lock(statsMutex_);
        auto& stats = statistics_[name];
        stats.executionCount++;
        if (result.success) {
            stats.successCount++;
        } else {
            stats.failureCount++;
        }
        stats.totalTime += result.executionTime;
    }
};

ScriptManager::ScriptManager() : pImpl_(std::make_unique<Impl>()) {}

ScriptManager::~ScriptManager() = default;

ScriptManager::ScriptManager(ScriptManager&&) noexcept = default;
ScriptManager& ScriptManager::operator=(ScriptManager&&) noexcept = default;

// =========================================================================
// Script Registration
// =========================================================================

void ScriptManager::registerScript(std::string_view name, const Script& script) {
    std::unique_lock lock(pImpl_->scriptsMutex_);
    std::string nameStr(name);

    pImpl_->scripts_[nameStr] = script;

    // Save version
    pImpl_->versionManager_.saveVersion(nameStr, script);

    // Set default metadata
    auto meta = ScriptMetadata::create();
    meta.language = ScriptExecutorFactory::detectLanguage(script);
    pImpl_->metadataManager_.setMetadata(nameStr, meta);

    spdlog::debug("ScriptManager: registered script '{}'", name);
}

void ScriptManager::deleteScript(std::string_view name) {
    std::unique_lock lock(pImpl_->scriptsMutex_);
    std::string nameStr(name);

    if (pImpl_->scripts_.erase(nameStr) == 0) {
        spdlog::warn("ScriptManager: script '{}' not found for deletion", name);
        return;
    }

    pImpl_->metadataManager_.removeMetadata(nameStr);
    pImpl_->versionManager_.clearVersionHistory(nameStr);

    spdlog::debug("ScriptManager: deleted script '{}'", name);
}

void ScriptManager::updateScript(std::string_view name, const Script& script) {
    std::unique_lock lock(pImpl_->scriptsMutex_);
    std::string nameStr(name);

    if (!pImpl_->scripts_.contains(nameStr)) {
        spdlog::warn("ScriptManager: script '{}' not found for update", name);
        return;
    }

    pImpl_->scripts_[nameStr] = script;
    pImpl_->versionManager_.saveVersion(nameStr, script);

    // Update metadata timestamp
    auto meta = pImpl_->metadataManager_.getMetadata(nameStr);
    if (meta) {
        meta->touch();
        pImpl_->metadataManager_.setMetadata(nameStr, *meta);
    }

    spdlog::debug("ScriptManager: updated script '{}'", name);
}

auto ScriptManager::getAllScripts() const
    -> std::unordered_map<std::string, Script> {
    std::shared_lock lock(pImpl_->scriptsMutex_);
    return pImpl_->scripts_;
}

auto ScriptManager::getScriptContent(std::string_view name) const
    -> std::optional<std::string> {
    std::shared_lock lock(pImpl_->scriptsMutex_);
    auto it = pImpl_->scripts_.find(std::string(name));
    if (it != pImpl_->scripts_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ScriptManager::importScripts(
    std::span<const std::pair<std::string, Script>> scripts) {
    for (const auto& [name, script] : scripts) {
        registerScript(name, script);
    }
}

// =========================================================================
// Script Execution
// =========================================================================

auto ScriptManager::runScript(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    bool safe, std::optional<int> timeoutMs)
    -> std::optional<std::pair<std::string, int>> {

    auto executor = pImpl_->createExecutor(name);
    if (!executor) {
        spdlog::error("ScriptManager: script '{}' not found", name);
        return std::nullopt;
    }

    std::string nameStr(name);

    // Check resources
    if (!pImpl_->resourceManager_.acquire()) {
        spdlog::warn("ScriptManager: resources not available for '{}'", name);
        return std::nullopt;
    }

    // Execute hooks
    pImpl_->hookManager_.executePreHooks(nameStr);

    // Build context
    ExecutionContext ctx;
    ctx.arguments = args;
    ctx.safe = safe;
    if (timeoutMs) {
        ctx.timeout = std::chrono::milliseconds(*timeoutMs);
    }

    // Get script content
    Script script;
    {
        std::shared_lock lock(pImpl_->scriptsMutex_);
        script = pImpl_->scripts_[nameStr];
    }

    // Execute
    auto result = executor->execute(script, ctx);

    // Execute post hooks
    pImpl_->hookManager_.executePostHooks(nameStr, result.exitCode);

    // Release resources
    pImpl_->resourceManager_.release();

    // Update statistics
    pImpl_->updateStats(nameStr, result);

    if (result.success) {
        return std::make_pair(result.output, result.exitCode);
    }
    return std::nullopt;
}

auto ScriptManager::runScriptAsync(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    bool safe)
    -> std::future<std::optional<std::pair<std::string, int>>> {
    std::string nameStr(name);
    auto argsCopy = args;

    return std::async(std::launch::async, [this, nameStr, argsCopy, safe]() {
        return this->runScript(nameStr, argsCopy, safe);
    });
}

auto ScriptManager::executeWithConfig(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    const RetryConfig& retryConfig,
    const std::optional<ScriptResourceLimits>& resourceLimits)
    -> ScriptExecutionResult {

    auto executor = pImpl_->createExecutor(name);
    if (!executor) {
        ScriptExecutionResult result;
        result.success = false;
        result.errorOutput = "Script not found";
        return result;
    }

    // Get script content
    Script script;
    {
        std::shared_lock lock(pImpl_->scriptsMutex_);
        auto it = pImpl_->scripts_.find(std::string(name));
        if (it == pImpl_->scripts_.end()) {
            ScriptExecutionResult result;
            result.success = false;
            result.errorOutput = "Script not found";
            return result;
        }
        script = it->second;
    }

    // Build context
    ExecutionContext ctx;
    ctx.arguments = args;
    ctx.safe = true;

    // Execute with retry support
    pImpl_->retryExecutor_.setRetryConfig(retryConfig);

    auto result = pImpl_->retryExecutor_.executeWithRetry(
        [&executor, &script, &ctx]() {
            return executor->execute(script, ctx);
        }
    );

    pImpl_->updateStats(std::string(name), result);
    return result;
}

auto ScriptManager::runScriptsSequentially(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe)
    -> std::vector<std::optional<std::pair<std::string, int>>> {

    std::vector<std::optional<std::pair<std::string, int>>> results;
    results.reserve(scripts.size());

    for (const auto& [name, args] : scripts) {
        results.push_back(runScript(name, args, safe));
    }

    return results;
}

auto ScriptManager::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe)
    -> std::vector<std::optional<std::pair<std::string, int>>> {

    std::vector<std::future<std::optional<std::pair<std::string, int>>>> futures;
    futures.reserve(scripts.size());

    for (const auto& [name, args] : scripts) {
        futures.push_back(runScriptAsync(name, args, safe));
    }

    std::vector<std::optional<std::pair<std::string, int>>> results;
    results.reserve(futures.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

auto ScriptManager::executePipeline(
    const std::vector<std::string>& scripts,
    const std::unordered_map<std::string, std::string>& sharedContext,
    bool stopOnError) -> std::vector<ScriptExecutionResult> {

    std::vector<ScriptExecutionResult> results;
    results.reserve(scripts.size());

    auto context = sharedContext;

    for (const auto& scriptName : scripts) {
        auto result = executeWithConfig(scriptName, context);
        results.push_back(result);

        if (!result.success && stopOnError) {
            break;
        }

        // Pass output to next script
        if (!result.output.empty()) {
            context["_previous_output"] = result.output;
        }
    }

    return results;
}

void ScriptManager::abortScript(std::string_view name) {
    pImpl_->abortFlags_[std::string(name)].store(true);
    spdlog::debug("ScriptManager: abort requested for '{}'", name);
}

auto ScriptManager::getScriptProgress(std::string_view name) const -> float {
    auto it = pImpl_->progressTrackers_.find(std::string(name));
    if (it != pImpl_->progressTrackers_.end()) {
        return it->second.load();
    }
    return 0.0f;
}

// =========================================================================
// Hooks
// =========================================================================

void ScriptManager::addPreExecutionHook(std::string_view name,
                                        PreExecutionHook hook) {
    pImpl_->hookManager_.addPreHook(std::string(name), std::move(hook));
}

void ScriptManager::addPostExecutionHook(std::string_view name,
                                         PostExecutionHook hook) {
    pImpl_->hookManager_.addPostHook(std::string(name), std::move(hook));
}

// =========================================================================
// Versioning
// =========================================================================

void ScriptManager::enableVersioning() {
    // Versioning is always enabled in the new architecture
    spdlog::debug("ScriptManager: versioning enabled");
}

auto ScriptManager::rollbackScript(std::string_view name, int version) -> bool {
    auto content = pImpl_->versionManager_.rollback(std::string(name),
                                                     static_cast<size_t>(version));
    if (content) {
        std::unique_lock lock(pImpl_->scriptsMutex_);
        pImpl_->scripts_[std::string(name)] = *content;
        spdlog::info("ScriptManager: rolled back '{}' to version {}", name, version);
        return true;
    }
    return false;
}

void ScriptManager::setMaxScriptVersions(int maxVersions) {
    pImpl_->versionManager_.setMaxVersions(static_cast<size_t>(maxVersions));
}

// =========================================================================
// Metadata
// =========================================================================

auto ScriptManager::getScriptMetadata(std::string_view name) const
    -> std::optional<ScriptMetadata> {
    return pImpl_->metadataManager_.getMetadata(name);
}

void ScriptManager::setScriptMetadata(std::string_view name,
                                      const ScriptMetadata& metadata) {
    pImpl_->metadataManager_.setMetadata(name, metadata);
}

// =========================================================================
// Resources
// =========================================================================

void ScriptManager::setResourceLimits(const ScriptResourceLimits& limits) {
    pImpl_->resourceManager_.setMaxMemory(limits.maxMemoryMB);
    pImpl_->resourceManager_.setMaxCpuPercent(limits.maxCpuPercent);
    pImpl_->resourceManager_.setMaxExecutionTime(limits.maxExecutionTime);
    pImpl_->resourceManager_.setMaxOutputSize(limits.maxOutputSize);
    pImpl_->resourceManager_.setMaxConcurrent(limits.maxConcurrentScripts);
}

auto ScriptManager::getResourceLimits() const -> ScriptResourceLimits {
    ScriptResourceLimits limits;
    limits.maxMemoryMB = pImpl_->resourceManager_.getMaxMemory();
    limits.maxCpuPercent = pImpl_->resourceManager_.getMaxCpuPercent();
    limits.maxExecutionTime = pImpl_->resourceManager_.getMaxExecutionTime();
    limits.maxOutputSize = pImpl_->resourceManager_.getMaxOutputSize();
    limits.maxConcurrentScripts = pImpl_->resourceManager_.getMaxConcurrent();
    return limits;
}

auto ScriptManager::getResourceUsage() const
    -> std::unordered_map<std::string, double> {
    return pImpl_->resourceManager_.getUsageMap();
}

// =========================================================================
// Discovery
// =========================================================================

auto ScriptManager::discoverScripts(const std::filesystem::path& directory,
                                    const std::vector<std::string>& extensions,
                                    bool recursive) -> size_t {
    size_t count = 0;

    if (!std::filesystem::exists(directory)) {
        spdlog::warn("ScriptManager: directory not found: {}", directory.string());
        return 0;
    }

    auto processFile = [this, &count, &extensions](const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();

                std::string name = path.stem().string();
                registerScript(name, content);

                auto meta = ScriptMetadata::create();
                meta.sourcePath = path;
                setScriptMetadata(name, meta);

                count++;
            }
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                processFile(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                processFile(entry.path());
            }
        }
    }

    spdlog::info("ScriptManager: discovered {} scripts in {}", count, directory.string());
    return count;
}

auto ScriptManager::detectScriptLanguage(std::string_view content)
    -> ScriptLanguage {
    return ScriptExecutorFactory::detectLanguage(content);
}

// =========================================================================
// Statistics
// =========================================================================

auto ScriptManager::getScriptStatistics(std::string_view name) const
    -> std::unordered_map<std::string, double> {
    std::shared_lock lock(pImpl_->statsMutex_);
    std::unordered_map<std::string, double> result;

    auto it = pImpl_->statistics_.find(std::string(name));
    if (it != pImpl_->statistics_.end()) {
        const auto& stats = it->second;
        result["execution_count"] = static_cast<double>(stats.executionCount);
        result["success_count"] = static_cast<double>(stats.successCount);
        result["failure_count"] = static_cast<double>(stats.failureCount);
        result["total_time_ms"] = static_cast<double>(stats.totalTime.count());
        result["average_time_ms"] = stats.executionCount > 0
            ? static_cast<double>(stats.totalTime.count()) / stats.executionCount
            : 0.0;
    }

    return result;
}

auto ScriptManager::getGlobalStatistics() const
    -> std::unordered_map<std::string, double> {
    std::shared_lock lock(pImpl_->statsMutex_);

    size_t totalExec = 0, totalSuccess = 0, totalFailure = 0;
    std::chrono::milliseconds totalTime{0};

    for (const auto& [_, stats] : pImpl_->statistics_) {
        totalExec += stats.executionCount;
        totalSuccess += stats.successCount;
        totalFailure += stats.failureCount;
        totalTime += stats.totalTime;
    }

    std::shared_lock scriptsLock(pImpl_->scriptsMutex_);
    return {
        {"total_scripts", static_cast<double>(pImpl_->scripts_.size())},
        {"total_executions", static_cast<double>(totalExec)},
        {"total_successes", static_cast<double>(totalSuccess)},
        {"total_failures", static_cast<double>(totalFailure)},
        {"total_time_ms", static_cast<double>(totalTime.count())},
        {"success_rate", totalExec > 0
            ? (static_cast<double>(totalSuccess) / totalExec) * 100.0 : 0.0}
    };
}

void ScriptManager::resetStatistics(std::string_view name) {
    std::unique_lock lock(pImpl_->statsMutex_);
    if (name.empty()) {
        pImpl_->statistics_.clear();
    } else {
        pImpl_->statistics_.erase(std::string(name));
    }
}

// =========================================================================
// Component Access
// =========================================================================

auto ScriptManager::metadata() -> MetadataManager& {
    return pImpl_->metadataManager_;
}

auto ScriptManager::hooks() -> HookManager& {
    return pImpl_->hookManager_;
}

auto ScriptManager::versions() -> VersionManager& {
    return pImpl_->versionManager_;
}

auto ScriptManager::resources() -> ResourceManager& {
    return pImpl_->resourceManager_;
}

}  // namespace lithium::shell
