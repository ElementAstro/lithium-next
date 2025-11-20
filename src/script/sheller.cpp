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

#include <future>
#include <iostream>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
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

}  // namespace lithium