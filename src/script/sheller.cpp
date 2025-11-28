/*
 * sheller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file sheller.cpp
 * @brief System Script Manager implementation
 * @date 2024-1-13
 */

#include "sheller.hpp"
#include "python_caller.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/sysinfo/os.hpp"
#include "atom/type/json.hpp"

namespace lithium {

/**
 * @brief Custom exception for script-related errors.
 */
class ScriptException : public std::runtime_error {
public:
    explicit ScriptException(const std::string& message)
        : std::runtime_error(message) {}
};

class ScriptManagerImpl {
public:
    using ScriptMap = std::unordered_map<std::string, Script>;
    ScriptMap scripts_;
    ScriptMap powerShellScripts_;
    std::unordered_map<std::string, std::vector<Script>> scriptVersions_;
    std::unordered_map<std::string, std::function<bool()>> scriptConditions_;
    std::unordered_map<std::string, std::string> executionEnvironments_;
    std::unordered_map<std::string, std::vector<std::string>> scriptLogs_;

    std::unordered_map<std::string, std::string> scriptOutputs_;
    std::unordered_map<std::string, int> scriptStatus_;
    mutable std::shared_mutex mSharedMutex_;

    int maxVersions_ = 10;

    std::unordered_map<std::string, std::atomic<bool>> abortFlags_;
    std::unordered_map<std::string, std::atomic<float>> progressTrackers_;
    std::unordered_map<std::string,
                       std::vector<std::function<void(const std::string&)>>>
        preHooks_;
    std::unordered_map<
        std::string, std::vector<std::function<void(const std::string&, int)>>>
        postHooks_;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>>
        environmentVars_;
    std::vector<std::string> loadedPowerShellModules_;

    auto runScriptImpl(std::string_view name,
                       const std::unordered_map<std::string, std::string>& args,
                       bool safe, std::optional<int> timeoutMs, int retryCount)
        -> std::optional<std::pair<std::string, int>>;

    // Additional member variables
    mutable std::shared_mutex mMetadataMutex_;
    std::unordered_map<std::string, ScriptMetadata> scriptMetadata_;
    std::unordered_map<std::string, std::jthread> runningScripts_;
    std::unordered_map<std::string, std::function<void()>> timeoutHandlers_;
    std::unordered_map<std::string, RetryStrategy> retryStrategies_;

    // Use C++20 atomic waiting
    std::atomic_flag scriptAborted_;

    std::osyncstream syncOut_{std::cout.rdbuf()};

    // Python integration
    std::shared_ptr<class PythonWrapper> pythonWrapper_;
    std::unordered_map<std::string, PythonScriptConfig> pythonConfigs_;

    // Resource management
    ScriptResourceLimits resourceLimits_;
    std::atomic<size_t> currentMemoryUsage_{0};

    // Statistics tracking
    struct ScriptStatistics {
        size_t executionCount{0};
        size_t successCount{0};
        size_t failureCount{0};
        std::chrono::milliseconds totalExecutionTime{0};
    };
    std::unordered_map<std::string, ScriptStatistics> scriptStatistics_;

    // Helper method to update statistics
    void updateStatistics(const std::string& name,
                          const ScriptExecutionResult& result) {
        std::unique_lock lock(mSharedMutex_);
        auto& stats = scriptStatistics_[name];
        stats.executionCount++;
        if (result.success) {
            stats.successCount++;
        } else {
            stats.failureCount++;
        }
        stats.totalExecutionTime += result.executionTime;
    }

public:
    void registerScript(std::string_view name, const Script& script);
    void registerPowerShellScript(std::string_view name, const Script& script);
    auto getAllScripts() const -> ScriptMap;
    void deleteScript(std::string_view name);
    void updateScript(std::string_view name, const Script& script);

    auto runScript(std::string_view name,
                   const std::unordered_map<std::string, std::string>& args,
                   bool safe = true,
                   std::optional<int> timeoutMs = std::nullopt,
                   int retryCount = 0)
        -> std::optional<std::pair<std::string, int>>;
    auto getScriptOutput(std::string_view name) const
        -> std::optional<std::string>;
    auto getScriptStatus(std::string_view name) const -> std::optional<int>;

    auto runScriptsSequentially(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>&
            scripts,
        bool safe = true, int retryCount = 0)
        -> std::vector<std::optional<std::pair<std::string, int>>>;
    auto runScriptsConcurrently(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>&
            scripts,
        bool safe = true, int retryCount = 0)
        -> std::vector<std::optional<std::pair<std::string, int>>>;

    void enableVersioning();
    auto rollbackScript(std::string_view name, int version) -> bool;

    void setScriptCondition(std::string_view name,
                            std::function<bool()> condition);
    void setExecutionEnvironment(std::string_view name,
                                 const std::string& environment);
    void setMaxScriptVersions(int maxVersions);
    auto getScriptLogs(std::string_view name) const -> std::vector<std::string>;

    auto runScriptAsync(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args, bool safe)
        -> std::future<std::optional<std::pair<std::string, int>>>;

    auto getScriptProgress(std::string_view name) const -> float;

    void abortScript(std::string_view name);

    [[nodiscard]] auto getScriptInfo(std::string_view name) const
        -> std::string;

    // Additional method implementations
    template <typename T>
        requires std::ranges::range<T>
    void importScripts(T&& scripts) {
        std::unique_lock lock(mSharedMutex_);
        for (const auto& [name, script] : scripts) {
            registerScript(name, script);
        }
    }

    auto getScriptMetadata(std::string_view name) const
        -> std::optional<ScriptMetadata> {
        std::shared_lock lock(mMetadataMutex_);
        if (auto it = scriptMetadata_.find(std::string(name));
            it != scriptMetadata_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    // Helper method for retry handling
    auto handleRetry(std::string_view name,
                     const std::unordered_map<std::string, std::string>& args,
                     bool safe, std::optional<int> timeoutMs, int retryCount)
        -> std::optional<std::pair<std::string, int>> {
        auto strategy = retryStrategies_[std::string(name)];
        std::chrono::milliseconds delay(100);

        for (int i = 0; i < retryCount; ++i) {
            switch (strategy) {
                case RetryStrategy::Linear:
                    delay += std::chrono::milliseconds(100);
                    break;
                case RetryStrategy::Exponential:
                    delay *= 2;
                    break;
                default:
                    break;
            }

            std::this_thread::sleep_for(delay);

            try {
                return runScriptImpl(name, args, safe, timeoutMs, 0);
            } catch (...) {
                if (i == retryCount - 1)
                    throw;
            }
        }
        return std::nullopt;
    }
};

ScriptManager::ScriptManager()
    : pImpl_(std::make_unique<ScriptManagerImpl>()) {}
ScriptManager::~ScriptManager() = default;

void ScriptManager::registerScript(std::string_view name,
                                   const Script& script) {
    pImpl_->registerScript(name, script);
}

void ScriptManager::registerPowerShellScript(std::string_view name,
                                             const Script& script) {
    pImpl_->registerPowerShellScript(name, script);
}

auto ScriptManager::getAllScripts() const
    -> std::unordered_map<std::string, Script> {
    return pImpl_->getAllScripts();
}

void ScriptManager::deleteScript(std::string_view name) {
    pImpl_->deleteScript(name);
}

void ScriptManager::updateScript(std::string_view name, const Script& script) {
    pImpl_->updateScript(name, script);
}

auto ScriptManager::runScript(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args, bool safe,
    std::optional<int> timeoutMs, int retryCount)
    -> std::optional<std::pair<std::string, int>> {
    return pImpl_->runScript(name, args, safe, timeoutMs, retryCount);
}

auto ScriptManager::getScriptOutput(std::string_view name) const
    -> std::optional<std::string> {
    return pImpl_->getScriptOutput(name);
}

auto ScriptManager::getScriptStatus(std::string_view name) const
    -> std::optional<int> {
    return pImpl_->getScriptStatus(name);
}

auto ScriptManager::runScriptsSequentially(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe, int retryCount)
    -> std::vector<std::optional<std::pair<std::string, int>>> {
    return pImpl_->runScriptsSequentially(scripts, safe, retryCount);
}

auto ScriptManager::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe, int retryCount)
    -> std::vector<std::optional<std::pair<std::string, int>>> {
    return pImpl_->runScriptsConcurrently(scripts, safe, retryCount);
}

void ScriptManager::enableVersioning() { pImpl_->enableVersioning(); }

auto ScriptManager::rollbackScript(std::string_view name, int version) -> bool {
    return pImpl_->rollbackScript(name, version);
}

void ScriptManager::setScriptCondition(std::string_view name,
                                       std::function<bool()> condition) {
    pImpl_->setScriptCondition(name, condition);
}

void ScriptManager::setExecutionEnvironment(std::string_view name,
                                            const std::string& environment) {
    pImpl_->setExecutionEnvironment(name, environment);
}

void ScriptManager::setMaxScriptVersions(int maxVersions) {
    pImpl_->setMaxScriptVersions(maxVersions);
}

auto ScriptManager::getScriptLogs(std::string_view name) const
    -> std::vector<std::string> {
    return pImpl_->getScriptLogs(name);
}

auto ScriptManager::runScriptAsync(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args, bool safe)
    -> std::future<std::optional<std::pair<std::string, int>>> {
    return std::async(std::launch::async, [this, name, args, safe]() {
        return this->runScript(name, args, safe);
    });
}

auto ScriptManager::getScriptProgress(std::string_view name) const -> float {
    return pImpl_->progressTrackers_[std::string(name)].load();
}

void ScriptManager::abortScript(std::string_view name) {
    pImpl_->abortFlags_[std::string(name)].store(true);
}

auto ScriptManager::getScriptInfo(std::string_view name) const -> std::string {
    return pImpl_->getScriptInfo(name);
}

// Implementation of ScriptManagerImpl

void ScriptManagerImpl::registerScript(std::string_view name,
                                       const Script& script) {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    scripts_[nameStr] = script;
    if (scriptVersions_.contains(nameStr)) {
        scriptVersions_[nameStr].push_back(script);
        if (scriptVersions_[nameStr].size() >
            static_cast<size_t>(maxVersions_)) {
            scriptVersions_[nameStr].erase(scriptVersions_[nameStr].begin());
        }
    } else {
        scriptVersions_[nameStr] = {script};
    }
    scriptLogs_[nameStr].emplace_back("Script registered/updated.");
}

void ScriptManagerImpl::registerPowerShellScript(std::string_view name,
                                                 const Script& script) {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    powerShellScripts_[nameStr] = script;
    if (scriptVersions_.contains(nameStr)) {
        scriptVersions_[nameStr].push_back(script);
        if (scriptVersions_[nameStr].size() >
            static_cast<size_t>(maxVersions_)) {
            scriptVersions_[nameStr].erase(scriptVersions_[nameStr].begin());
        }
    } else {
        scriptVersions_[nameStr] = {script};
    }
    scriptLogs_[nameStr].emplace_back("PowerShell script registered/updated.");
}

auto ScriptManagerImpl::getAllScripts() const -> ScriptMap {
    std::shared_lock lock(mSharedMutex_);
    ScriptMap allScripts = scripts_;
    allScripts.insert(powerShellScripts_.begin(), powerShellScripts_.end());
    return allScripts;
}

void ScriptManagerImpl::deleteScript(std::string_view name) {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    auto erased = scripts_.erase(nameStr) + powerShellScripts_.erase(nameStr);
    if (erased == 0) {
        throw ScriptException("Script not found: " + nameStr);
    }
    scriptOutputs_.erase(nameStr);
    scriptStatus_.erase(nameStr);
    scriptVersions_.erase(nameStr);
    scriptConditions_.erase(nameStr);
    executionEnvironments_.erase(nameStr);
    scriptLogs_.erase(nameStr);
    spdlog::info("Script deleted: {}", nameStr);
}

void ScriptManagerImpl::updateScript(std::string_view name,
                                     const Script& script) {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (scripts_.contains(nameStr)) {
        scripts_[nameStr] = script;
    } else if (powerShellScripts_.contains(nameStr)) {
        powerShellScripts_[nameStr] = script;
    } else {
        throw ScriptException("Script not found for update: " + nameStr);
    }
    if (scriptVersions_.contains(nameStr)) {
        scriptVersions_[nameStr].push_back(script);
        if (scriptVersions_[nameStr].size() >
            static_cast<size_t>(maxVersions_)) {
            scriptVersions_[nameStr].erase(scriptVersions_[nameStr].begin());
        }
    } else {
        scriptVersions_[nameStr] = {script};
    }
    scriptOutputs_[nameStr] = "";
    scriptStatus_[nameStr] = 0;
    scriptLogs_[nameStr].emplace_back("Script updated.");
}

auto ScriptManagerImpl::runScriptImpl(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args, bool safe,
    std::optional<int> timeoutMs, int retryCount)
    -> std::optional<std::pair<std::string, int>> {
    std::string nameStr(name);

    // Execute pre-execution hooks
    for (const auto& hook : preHooks_[nameStr]) {
        hook(nameStr);
    }

    // Reset progress
    progressTrackers_[nameStr].store(0.0f);
    abortFlags_[nameStr].store(false);

    try {
        // Build environment variables
        std::string envVarsCmd;
        if (environmentVars_.contains(nameStr)) {
            for (const auto& [key, value] : environmentVars_[nameStr]) {
                if (atom::system::isWsl()) {
                    envVarsCmd += "$env:" + key + "=\"" + value + "\";";
                } else {
                    envVarsCmd += "export " + key + "=\"" + value + "\";";
                }
            }
        }

        // PowerShell specific optimizations
        std::string powershellSetup;
        if (powerShellScripts_.contains(nameStr)) {
            powershellSetup = "$ErrorActionPreference = 'Stop';\n";
            for (const auto& module : loadedPowerShellModules_) {
                powershellSetup += "Import-Module " + module + ";\n";
            }
        }

        // Execute script and monitor progress
        std::string scriptCmd;
        {
            std::shared_lock lock(mSharedMutex_);
            if (scripts_.contains(nameStr)) {
                scriptCmd = envVarsCmd + "sh -c \"" + scripts_[nameStr] + "\"";
            } else if (powerShellScripts_.contains(nameStr)) {
                scriptCmd = "powershell.exe -Command \"" + envVarsCmd +
                            powershellSetup + powerShellScripts_[nameStr] +
                            "\"";
            } else {
                throw ScriptException("Script not found: " + nameStr);
            }
        }

        // Add parameters
        for (const auto& [key, value] : args) {
            scriptCmd += " \"" + key + "=" + value + "\"";
        }

        // Use pipe for progress tracking and abort capability
        FILE* pipe = popen(scriptCmd.c_str(), "r");
        if (!pipe) {
            throw ScriptException("Failed to create pipe for script execution");
        }

        std::string output;
        char buffer[128];
        while (!abortFlags_[nameStr].load() &&
               fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
            // Update progress (example implementation)
            if (output.find("PROGRESS:") != std::string::npos) {
                float progress = std::stof(output.substr(output.find(":") + 1));
                progressTrackers_[nameStr].store(progress);
            }
        }

        int status = pclose(pipe);

        // If aborted, return special status code
        if (abortFlags_[nameStr].load()) {
            status = -999;
        }

        // Execute post-execution hooks
        for (const auto& hook : postHooks_[nameStr]) {
            hook(output, status);
        }

        return std::make_pair(output, status);

    } catch (const std::exception& e) {
        spdlog::error("Error executing script '{}': {}", nameStr, e.what());
        throw ScriptException(std::string("Script execution failed: ") +
                              e.what());
    }
}

auto ScriptManagerImpl::runScript(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args, bool safe,
    std::optional<int> timeoutMs, int retryCount)
    -> std::optional<std::pair<std::string, int>> {
    try {
        // Use stop_token for cancellation support
        std::stop_source stopSource;
        auto future = std::async(
            std::launch::async,
            [this, name, args, &stopSource, safe, timeoutMs, retryCount]() {
                return runScriptImpl(name, args, safe, timeoutMs, retryCount);
            });

        // Handle timeout
        if (timeoutMs) {
            if (future.wait_for(std::chrono::milliseconds(*timeoutMs)) ==
                std::future_status::timeout) {
                stopSource.request_stop();
                if (auto handler = timeoutHandlers_.find(std::string(name));
                    handler != timeoutHandlers_.end()) {
                    handler->second();
                }
                return std::nullopt;
            }
        }

        try {
            return future.get();
        } catch (const std::exception& e) {
            // Retry logic
            if (retryCount > 0) {
                return handleRetry(name, args, safe, timeoutMs, retryCount);
            }
            throw;
        }
    } catch (const ScriptException& e) {
        spdlog::error("ScriptException: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Exception during script execution: {}", e.what());
        throw ScriptException("Unknown error during script execution.");
    }
}

auto ScriptManagerImpl::getScriptOutput(std::string_view name) const
    -> std::optional<std::string> {
    std::shared_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (scriptOutputs_.contains(nameStr)) {
        return scriptOutputs_.at(nameStr);
    }
    return std::nullopt;
}

auto ScriptManagerImpl::getScriptStatus(std::string_view name) const
    -> std::optional<int> {
    std::shared_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (scriptStatus_.contains(nameStr)) {
        return scriptStatus_.at(nameStr);
    }
    return std::nullopt;
}

auto ScriptManagerImpl::runScriptsSequentially(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe, int retryCount)
    -> std::vector<std::optional<std::pair<std::string, int>>> {
    std::vector<std::optional<std::pair<std::string, int>>> results;
    results.reserve(scripts.size());
    for (const auto& [name, args] : scripts) {
        try {
            results.emplace_back(
                runScriptImpl(name, args, safe, std::nullopt, retryCount));
        } catch (const ScriptException& e) {
            spdlog::error("Error running script '{}': {}", name, e.what());
            results.emplace_back(std::nullopt);
        }
    }
    return results;
}

auto ScriptManagerImpl::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe, int retryCount)
    -> std::vector<std::optional<std::pair<std::string, int>>> {
    std::vector<std::future<std::optional<std::pair<std::string, int>>>>
        futures;
    futures.reserve(scripts.size());
    for (const auto& [name, args] : scripts) {
        futures.emplace_back(
            std::async(std::launch::async, &ScriptManagerImpl::runScriptImpl,
                       this, name, args, safe, std::nullopt, retryCount));
    }
    std::vector<std::optional<std::pair<std::string, int>>> results;
    results.reserve(futures.size());
    for (auto& future : futures) {
        try {
            results.emplace_back(future.get());
        } catch (const ScriptException& e) {
            spdlog::error("ScriptException during concurrent execution: {}",
                          e.what());
            results.emplace_back(std::nullopt);
        } catch (const std::exception& e) {
            spdlog::error("Exception during concurrent execution: {}",
                          e.what());
            results.emplace_back(std::nullopt);
        }
    }
    return results;
}

void ScriptManagerImpl::enableVersioning() {
    std::unique_lock lock(mSharedMutex_);
    for (auto& [name, script] : scripts_) {
        scriptVersions_[name].push_back(script);
        if (scriptVersions_[name].size() > static_cast<size_t>(maxVersions_)) {
            scriptVersions_[name].erase(scriptVersions_[name].begin());
        }
    }
    for (auto& [name, script] : powerShellScripts_) {
        scriptVersions_[name].push_back(script);
        if (scriptVersions_[name].size() > static_cast<size_t>(maxVersions_)) {
            scriptVersions_[name].erase(scriptVersions_[name].begin());
        }
    }
    spdlog::info("Versioning enabled for all scripts.");
}

auto ScriptManagerImpl::rollbackScript(std::string_view name, int version)
    -> bool {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (!scriptVersions_.contains(nameStr) || version < 0 ||
        version >= static_cast<int>(scriptVersions_[nameStr].size())) {
        spdlog::error("Invalid rollback attempt for script '{}' to version {}",
                      nameStr, version);
        return false;
    }
    if (scripts_.contains(nameStr)) {
        scripts_[nameStr] = scriptVersions_[nameStr][version];
    } else if (powerShellScripts_.contains(nameStr)) {
        powerShellScripts_[nameStr] = scriptVersions_[nameStr][version];
    } else {
        spdlog::error("Script '{}' not found for rollback", nameStr);
        return false;
    }
    scriptOutputs_[nameStr] = "";
    scriptStatus_[nameStr] = 0;
    scriptLogs_[nameStr].emplace_back("Script rolled back to version " +
                                      std::to_string(version) + ".");
    return true;
}

void ScriptManagerImpl::setScriptCondition(std::string_view name,
                                           std::function<bool()> condition) {
    std::unique_lock lock(mSharedMutex_);
    scriptConditions_[std::string(name)] = std::move(condition);
    scriptLogs_[std::string(name)].emplace_back("Script condition set.");
}

void ScriptManagerImpl::setExecutionEnvironment(
    std::string_view name, const std::string& environment) {
    std::unique_lock lock(mSharedMutex_);
    executionEnvironments_[std::string(name)] = environment;
    scriptLogs_[std::string(name)].emplace_back("Execution environment set.");
}

void ScriptManagerImpl::setMaxScriptVersions(int maxVersions) {
    std::unique_lock lock(mSharedMutex_);
    maxVersions_ = maxVersions;
    for (auto& [name, versions] : scriptVersions_) {
        while (versions.size() > static_cast<size_t>(maxVersions_)) {
            versions.erase(versions.begin());
        }
    }
    spdlog::info("Max script versions set to {}", maxVersions_);
}

auto ScriptManagerImpl::getScriptLogs(std::string_view name) const
    -> std::vector<std::string> {
    std::shared_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (scriptLogs_.contains(nameStr)) {
        return scriptLogs_.at(nameStr);
    }
    return {};
}

auto ScriptManagerImpl::getScriptInfo(std::string_view name) const
    -> std::string {
    std::shared_lock lock(mSharedMutex_);
    std::string nameStr(name);
    nlohmann::json info;

    if (scripts_.contains(nameStr)) {
        info["Script"] = scripts_.at(nameStr);
    }
    if (powerShellScripts_.contains(nameStr)) {
        info["PowerShell Script"] = powerShellScripts_.at(nameStr);
    }
    if (scriptVersions_.contains(nameStr)) {
        info["Versions"] = scriptVersions_.at(nameStr);
    }
    if (scriptConditions_.contains(nameStr)) {
        info["Condition"] = scriptConditions_.at(nameStr)();
    }
    if (executionEnvironments_.contains(nameStr)) {
        info["Environment"] = executionEnvironments_.at(nameStr);
    }
    if (scriptLogs_.contains(nameStr)) {
        info["Logs"] = scriptLogs_.at(nameStr);
    }

    return info.dump();
}

// Implementation of missing public interfaces
void ScriptManager::importScripts(
    std::span<const std::pair<std::string, Script>> scripts) {
    pImpl_->importScripts(scripts);
}

auto ScriptManager::getScriptMetadata(std::string_view name) const
    -> std::optional<ScriptMetadata> {
    return pImpl_->getScriptMetadata(name);
}

void ScriptManager::addPreExecutionHook(
    std::string_view name, std::function<void(const std::string&)> hook) {
    pImpl_->preHooks_[std::string(name)].push_back(std::move(hook));
}

void ScriptManager::addPostExecutionHook(
    std::string_view name, std::function<void(const std::string&, int)> hook) {
    pImpl_->postHooks_[std::string(name)].push_back(std::move(hook));
}

void ScriptManager::setScriptEnvironmentVars(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& vars) {
    pImpl_->environmentVars_[std::string(name)] = vars;
}

void ScriptManager::importPowerShellModule(std::string_view moduleName) {
    pImpl_->loadedPowerShellModules_.push_back(std::string(moduleName));
}

void ScriptManager::setTimeoutHandler(std::string_view name,
                                      std::function<void()> handler) {
    pImpl_->timeoutHandlers_[std::string(name)] = std::move(handler);
}

void ScriptManager::setRetryStrategy(std::string_view name,
                                     RetryStrategy strategy) {
    pImpl_->retryStrategies_[std::string(name)] = strategy;
}

auto ScriptManager::getRunningScripts() const -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& [name, _] : pImpl_->runningScripts_) {
        result.push_back(name);
    }
    return result;
}

// =============================================================================
// Enhanced Python Integration Methods Implementation
// =============================================================================

void ScriptManager::registerPythonScriptWithConfig(
    std::string_view name, const PythonScriptConfig& config) {
    spdlog::info("Registering Python script '{}' with module '{}'",
                 name, config.moduleName);

    std::unique_lock lock(pImpl_->mSharedMutex_);
    std::string nameStr(name);

    // Store Python configuration
    pImpl_->pythonConfigs_[nameStr] = config;

    // Create metadata for the Python script
    ScriptMetadata metadata;
    metadata.language = ScriptLanguage::Python;
    metadata.isPython = true;
    metadata.createdAt = std::chrono::system_clock::now();
    metadata.lastModified = metadata.createdAt;
    metadata.dependencies = config.requiredPackages;

    pImpl_->scriptMetadata_[nameStr] = metadata;
    pImpl_->scriptLogs_[nameStr].emplace_back(
        "Python script registered with config");

    spdlog::debug("Python script '{}' registered successfully", name);
}

auto ScriptManager::executePythonFunction(
    std::string_view moduleName, std::string_view functionName,
    const std::unordered_map<std::string, std::string>& args)
    -> ScriptExecutionResult {
    spdlog::info("Executing Python function '{}::{}' with {} args",
                 moduleName, functionName, args.size());

    auto startTime = std::chrono::steady_clock::now();
    ScriptExecutionResult result;
    result.detectedLanguage = ScriptLanguage::Python;

    try {
        if (!pImpl_->pythonWrapper_) {
            throw std::runtime_error("Python wrapper not initialized");
        }

        // Load module if not already loaded
        std::string moduleStr(moduleName);
        std::string funcStr(functionName);

        pImpl_->pythonWrapper_->load_script(moduleStr, moduleStr);

        // Call the function
        auto pyResult = pImpl_->pythonWrapper_->call_function<std::string>(
            moduleStr, funcStr);

        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        result.success = true;
        result.exitCode = 0;
        result.output = pyResult;

        spdlog::info("Python function '{}::{}' executed successfully in {}ms",
                     moduleName, functionName, result.executionTime.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        result.success = false;
        result.exitCode = -1;
        result.errorOutput = e.what();
        result.exception = e.what();

        spdlog::error("Python function execution failed: {}", e.what());
    }

    return result;
}

auto ScriptManager::loadPythonScriptsFromDirectory(
    const std::filesystem::path& directory, bool recursive) -> size_t {
    spdlog::info("Loading Python scripts from directory: {} (recursive={})",
                 directory.string(), recursive);

    size_t count = 0;

    if (!std::filesystem::exists(directory)) {
        spdlog::warn("Directory does not exist: {}", directory.string());
        return 0;
    }

    auto processFile = [this, &count](const std::filesystem::path& path) {
        if (path.extension() == ".py") {
            try {
                std::string moduleName = path.stem().string();
                std::string alias = moduleName;

                // Read script content
                std::ifstream file(path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string content = buffer.str();

                    // Register the script
                    registerScript(alias, content);

                    // Update metadata
                    ScriptMetadata metadata;
                    metadata.language = ScriptLanguage::Python;
                    metadata.isPython = true;
                    metadata.sourcePath = path;
                    metadata.createdAt = std::chrono::system_clock::now();
                    metadata.lastModified = metadata.createdAt;

                    setScriptMetadata(alias, metadata);
                    count++;

                    spdlog::debug("Loaded Python script: {}", path.string());
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to load Python script {}: {}",
                            path.string(), e.what());
            }
        }
    };

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
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

    spdlog::info("Loaded {} Python scripts from {}", count, directory.string());
    return count;
}

auto ScriptManager::getPythonWrapper() const
    -> std::shared_ptr<class PythonWrapper> {
    return pImpl_->pythonWrapper_;
}

void ScriptManager::setPythonWrapper(
    std::shared_ptr<class PythonWrapper> wrapper) {
    spdlog::info("Setting Python wrapper instance");
    pImpl_->pythonWrapper_ = std::move(wrapper);
}

auto ScriptManager::isPythonAvailable() const -> bool {
    return pImpl_->pythonWrapper_ != nullptr;
}

void ScriptManager::addPythonSysPath(const std::filesystem::path& path) {
    if (pImpl_->pythonWrapper_) {
        spdlog::debug("Adding Python sys.path: {}", path.string());
        pImpl_->pythonWrapper_->add_sys_path(path.string());
    } else {
        spdlog::warn("Cannot add sys.path: Python wrapper not initialized");
    }
}

// =============================================================================
// Resource Management Methods Implementation
// =============================================================================

void ScriptManager::setResourceLimits(const ScriptResourceLimits& limits) {
    spdlog::info("Setting resource limits: maxMemory={}MB, maxCPU={}%, "
                 "maxTime={}s, maxConcurrent={}",
                 limits.maxMemoryMB, limits.maxCpuPercent,
                 limits.maxExecutionTime.count(), limits.maxConcurrentScripts);

    std::unique_lock lock(pImpl_->mSharedMutex_);
    pImpl_->resourceLimits_ = limits;
}

auto ScriptManager::getResourceLimits() const -> ScriptResourceLimits {
    std::shared_lock lock(pImpl_->mSharedMutex_);
    return pImpl_->resourceLimits_;
}

auto ScriptManager::getResourceUsage() const
    -> std::unordered_map<std::string, double> {
    std::shared_lock lock(pImpl_->mSharedMutex_);

    std::unordered_map<std::string, double> usage;
    usage["running_scripts"] = static_cast<double>(pImpl_->runningScripts_.size());
    usage["total_scripts"] = static_cast<double>(pImpl_->scripts_.size() +
                                                  pImpl_->powerShellScripts_.size());
    usage["memory_usage_percent"] =
        (static_cast<double>(pImpl_->currentMemoryUsage_) /
         static_cast<double>(pImpl_->resourceLimits_.maxMemoryMB)) * 100.0;

    return usage;
}

// =============================================================================
// Script Discovery and Auto-Loading Implementation
// =============================================================================

auto ScriptManager::discoverScripts(
    const std::filesystem::path& directory,
    const std::vector<std::string>& extensions, bool recursive) -> size_t {
    spdlog::info("Discovering scripts in: {} (extensions: {}, recursive: {})",
                 directory.string(), extensions.size(), recursive);

    size_t count = 0;

    if (!std::filesystem::exists(directory)) {
        spdlog::warn("Directory does not exist: {}", directory.string());
        return 0;
    }

    auto processFile = [this, &count, &extensions](
                           const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        if (std::find(extensions.begin(), extensions.end(), ext) !=
            extensions.end()) {
            try {
                std::ifstream file(path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string content = buffer.str();

                    std::string name = path.stem().string();
                    ScriptLanguage lang = detectScriptLanguage(content);

                    if (lang == ScriptLanguage::Python) {
                        registerScript(name, content);
                    } else if (lang == ScriptLanguage::PowerShell) {
                        registerPowerShellScript(name, content);
                    } else {
                        registerScript(name, content);
                    }

                    // Set metadata
                    ScriptMetadata metadata;
                    metadata.language = lang;
                    metadata.isPython = (lang == ScriptLanguage::Python);
                    metadata.sourcePath = path;
                    metadata.createdAt = std::chrono::system_clock::now();
                    metadata.lastModified = metadata.createdAt;

                    setScriptMetadata(name, metadata);
                    count++;

                    spdlog::debug("Discovered script: {} ({})", name,
                                 static_cast<int>(lang));
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to load script {}: {}",
                            path.string(), e.what());
            }
        }
    };

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
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

    spdlog::info("Discovered {} scripts in {}", count, directory.string());
    return count;
}

auto ScriptManager::detectScriptLanguage(std::string_view content)
    -> ScriptLanguage {
    std::string contentStr(content);

    // Check for Python indicators
    if (contentStr.find("#!/usr/bin/env python") != std::string::npos ||
        contentStr.find("#!/usr/bin/python") != std::string::npos ||
        (contentStr.find("import ") != std::string::npos &&
         contentStr.find("def ") != std::string::npos)) {
        return ScriptLanguage::Python;
    }

    // Check for PowerShell indicators
    if (contentStr.find("param(") != std::string::npos ||
        contentStr.find("$PSVersionTable") != std::string::npos ||
        contentStr.find("Write-Host") != std::string::npos ||
        contentStr.find("Get-") != std::string::npos) {
        return ScriptLanguage::PowerShell;
    }

    // Check for shell script indicators
    if (contentStr.find("#!/bin/bash") != std::string::npos ||
        contentStr.find("#!/bin/sh") != std::string::npos ||
        contentStr.find("#!/usr/bin/env bash") != std::string::npos) {
        return ScriptLanguage::Shell;
    }

    // Default to shell
    return ScriptLanguage::Shell;
}

auto ScriptManager::getScriptContent(std::string_view name) const
    -> std::optional<std::string> {
    std::shared_lock lock(pImpl_->mSharedMutex_);
    std::string nameStr(name);

    if (pImpl_->scripts_.contains(nameStr)) {
        return pImpl_->scripts_.at(nameStr);
    }
    if (pImpl_->powerShellScripts_.contains(nameStr)) {
        return pImpl_->powerShellScripts_.at(nameStr);
    }

    return std::nullopt;
}

void ScriptManager::setScriptMetadata(std::string_view name,
                                      const ScriptMetadata& metadata) {
    std::unique_lock lock(pImpl_->mMetadataMutex_);
    pImpl_->scriptMetadata_[std::string(name)] = metadata;
    spdlog::debug("Set metadata for script '{}'", name);
}

// =============================================================================
// Enhanced Execution Methods Implementation
// =============================================================================

auto ScriptManager::executeWithConfig(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    const RetryConfig& config,
    const std::optional<ScriptResourceLimits>& resourceLimits)
    -> ScriptExecutionResult {
    spdlog::info("Executing script '{}' with config (retries={}, strategy={})",
                 name, config.maxRetries, static_cast<int>(config.strategy));

    auto startTime = std::chrono::steady_clock::now();
    ScriptExecutionResult result;

    // Apply resource limits if provided
    ScriptResourceLimits limits = resourceLimits.value_or(pImpl_->resourceLimits_);

    int attempts = 0;
    std::chrono::milliseconds delay = config.initialDelay;

    while (attempts <= config.maxRetries) {
        try {
            // Check resource availability
            if (pImpl_->runningScripts_.size() >=
                static_cast<size_t>(limits.maxConcurrentScripts)) {
                spdlog::warn("Max concurrent scripts reached, waiting...");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Execute the script
            auto execResult = runScript(name, args, true,
                                        static_cast<int>(limits.maxExecutionTime.count() * 1000));

            if (execResult) {
                result.success = (execResult->second == 0);
                result.exitCode = execResult->second;
                result.output = execResult->first;
            } else {
                result.success = false;
                result.exitCode = -1;
            }

            // Detect language from metadata
            auto metadata = getScriptMetadata(name);
            if (metadata) {
                result.detectedLanguage = metadata->language;
            }

            auto endTime = std::chrono::steady_clock::now();
            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime);

            // Check if we should retry
            if (result.success || config.strategy == RetryStrategy::None) {
                break;
            }

            if (config.shouldRetry && !config.shouldRetry(attempts, result)) {
                break;
            }

            attempts++;
            if (attempts <= config.maxRetries) {
                spdlog::info("Retrying script '{}' (attempt {}/{})",
                            name, attempts, config.maxRetries);

                // Calculate delay based on strategy
                switch (config.strategy) {
                    case RetryStrategy::Linear:
                        delay += config.initialDelay;
                        break;
                    case RetryStrategy::Exponential:
                        delay = std::chrono::milliseconds(
                            static_cast<int>(delay.count() * config.multiplier));
                        break;
                    default:
                        break;
                }

                delay = std::min(delay, config.maxDelay);
                std::this_thread::sleep_for(delay);
            }

        } catch (const std::exception& e) {
            result.success = false;
            result.exitCode = -1;
            result.exception = e.what();
            result.errorOutput = e.what();

            spdlog::error("Script execution error: {}", e.what());

            if (config.strategy == RetryStrategy::None || attempts >= config.maxRetries) {
                break;
            }
            attempts++;
        }
    }

    // Update statistics
    pImpl_->updateStatistics(std::string(name), result);

    return result;
}

auto ScriptManager::executeAsync(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    std::function<void(const ScriptProgress&)> progressCallback)
    -> std::future<ScriptExecutionResult> {
    spdlog::info("Starting async execution of script '{}'", name);

    std::string nameStr(name);
    auto argsCopy = args;

    return std::async(std::launch::async,
        [this, nameStr, argsCopy, progressCallback]() -> ScriptExecutionResult {
            ScriptProgress progress;
            progress.status = "Starting";
            progress.percentage = 0.0f;
            progress.timestamp = std::chrono::system_clock::now();

            if (progressCallback) {
                progressCallback(progress);
            }

            auto result = executeWithConfig(nameStr, argsCopy);

            progress.status = result.success ? "Completed" : "Failed";
            progress.percentage = 1.0f;
            progress.timestamp = std::chrono::system_clock::now();
            progress.output = result.output;

            if (progressCallback) {
                progressCallback(progress);
            }

            return result;
        });
}

auto ScriptManager::executePipeline(
    const std::vector<std::string>& scripts,
    const std::unordered_map<std::string, std::string>& sharedContext,
    bool stopOnError) -> std::vector<ScriptExecutionResult> {
    spdlog::info("Executing pipeline with {} scripts (stopOnError={})",
                 scripts.size(), stopOnError);

    std::vector<ScriptExecutionResult> results;
    results.reserve(scripts.size());

    auto context = sharedContext;

    for (const auto& scriptName : scripts) {
        spdlog::debug("Pipeline: executing script '{}'", scriptName);

        auto result = executeWithConfig(scriptName, context);
        results.push_back(result);

        if (!result.success && stopOnError) {
            spdlog::warn("Pipeline stopped due to error in script '{}'", scriptName);
            break;
        }

        // Pass output to next script as context
        if (!result.output.empty()) {
            context["_previous_output"] = result.output;
        }
    }

    spdlog::info("Pipeline completed: {}/{} scripts executed",
                 results.size(), scripts.size());
    return results;
}

// =============================================================================
// Statistics and Monitoring Implementation
// =============================================================================

auto ScriptManager::getScriptStatistics(std::string_view name) const
    -> std::unordered_map<std::string, double> {
    std::shared_lock lock(pImpl_->mSharedMutex_);
    std::string nameStr(name);

    std::unordered_map<std::string, double> stats;

    if (pImpl_->scriptStatistics_.contains(nameStr)) {
        const auto& s = pImpl_->scriptStatistics_.at(nameStr);
        stats["execution_count"] = static_cast<double>(s.executionCount);
        stats["success_count"] = static_cast<double>(s.successCount);
        stats["failure_count"] = static_cast<double>(s.failureCount);
        stats["total_execution_time_ms"] = static_cast<double>(s.totalExecutionTime.count());
        stats["average_execution_time_ms"] =
            s.executionCount > 0
                ? static_cast<double>(s.totalExecutionTime.count()) / s.executionCount
                : 0.0;
        stats["success_rate"] =
            s.executionCount > 0
                ? (static_cast<double>(s.successCount) / s.executionCount) * 100.0
                : 0.0;
    }

    return stats;
}

auto ScriptManager::getGlobalStatistics() const
    -> std::unordered_map<std::string, double> {
    std::shared_lock lock(pImpl_->mSharedMutex_);

    std::unordered_map<std::string, double> stats;
    size_t totalExecutions = 0;
    size_t totalSuccesses = 0;
    size_t totalFailures = 0;
    std::chrono::milliseconds totalTime{0};

    for (const auto& [name, s] : pImpl_->scriptStatistics_) {
        totalExecutions += s.executionCount;
        totalSuccesses += s.successCount;
        totalFailures += s.failureCount;
        totalTime += s.totalExecutionTime;
    }

    stats["total_scripts"] = static_cast<double>(pImpl_->scripts_.size() +
                                                  pImpl_->powerShellScripts_.size());
    stats["total_executions"] = static_cast<double>(totalExecutions);
    stats["total_successes"] = static_cast<double>(totalSuccesses);
    stats["total_failures"] = static_cast<double>(totalFailures);
    stats["total_execution_time_ms"] = static_cast<double>(totalTime.count());
    stats["average_execution_time_ms"] =
        totalExecutions > 0
            ? static_cast<double>(totalTime.count()) / totalExecutions
            : 0.0;
    stats["global_success_rate"] =
        totalExecutions > 0
            ? (static_cast<double>(totalSuccesses) / totalExecutions) * 100.0
            : 0.0;

    return stats;
}

void ScriptManager::resetStatistics(std::string_view name) {
    std::unique_lock lock(pImpl_->mSharedMutex_);

    if (name.empty()) {
        spdlog::info("Resetting all script statistics");
        pImpl_->scriptStatistics_.clear();
    } else {
        std::string nameStr(name);
        spdlog::info("Resetting statistics for script '{}'", name);
        pImpl_->scriptStatistics_.erase(nameStr);
    }
}

}  // namespace lithium