/*
 * sheller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-13

Description: System Script Manager

**************************************************/

#include "sheller.hpp"

#include <future>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "atom/log/loguru.hpp"
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

public:
    void registerScript(std::string_view name, const Script& script);
    void registerPowerShellScript(std::string_view name, const Script& script);
    auto getAllScripts() const -> ScriptMap;
    void deleteScript(std::string_view name);
    void updateScript(std::string_view name, const Script& script);

    auto runScript(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        bool safe = true, std::optional<int> timeoutMs = std::nullopt,
        int retryCount = 0) -> std::optional<std::pair<std::string, int>>;
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
        const std::unordered_map<std::string, std::string>& args,
        bool safe) -> std::future<std::optional<std::pair<std::string, int>>>;

    auto getScriptProgress(std::string_view name) const -> float;

    void abortScript(std::string_view name);

    [[nodiscard]] auto getScriptInfo(std::string_view name) const
        -> std::string;
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
    std::optional<int> timeoutMs,
    int retryCount) -> std::optional<std::pair<std::string, int>> {
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
    bool safe,
    int retryCount) -> std::vector<std::optional<std::pair<std::string, int>>> {
    return pImpl_->runScriptsSequentially(scripts, safe, retryCount);
}

auto ScriptManager::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe,
    int retryCount) -> std::vector<std::optional<std::pair<std::string, int>>> {
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
    const std::unordered_map<std::string, std::string>& args,
    bool safe) -> std::future<std::optional<std::pair<std::string, int>>> {
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
    LOG_F(INFO, "Script deleted: {}", nameStr);
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
    std::optional<int> timeoutMs,
    int retryCount) -> std::optional<std::pair<std::string, int>> {
    std::string nameStr(name);

    // 执行前置钩子
    for (const auto& hook : preHooks_[nameStr]) {
        hook(nameStr);
    }

    // 重置进度
    progressTrackers_[nameStr].store(0.0f);
    abortFlags_[nameStr].store(false);

    try {
        // 构建环境变量
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

        // PowerShell特定优化
        std::string powershellSetup;
        if (powerShellScripts_.contains(nameStr)) {
            powershellSetup = "$ErrorActionPreference = 'Stop';\n";
            for (const auto& module : loadedPowerShellModules_) {
                powershellSetup += "Import-Module " + module + ";\n";
            }
        }

        // 执行脚本并监控进度
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

        // 添加参数
        for (const auto& [key, value] : args) {
            scriptCmd += " \"" + key + "=" + value + "\"";
        }

        // 使用管道实现进度跟踪和中止
        FILE* pipe = popen(scriptCmd.c_str(), "r");
        if (!pipe) {
            throw ScriptException("Failed to create pipe for script execution");
        }

        std::string output;
        char buffer[128];
        while (!abortFlags_[nameStr].load() &&
               fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
            // 更新进度(示例实现)
            if (output.find("PROGRESS:") != std::string::npos) {
                float progress = std::stof(output.substr(output.find(":") + 1));
                progressTrackers_[nameStr].store(progress);
            }
        }

        int status = pclose(pipe);

        // 如果被中止,则返回特殊状态码
        if (abortFlags_[nameStr].load()) {
            status = -999;
        }

        // 执行后置钩子
        for (const auto& hook : postHooks_[nameStr]) {
            hook(output, status);
        }

        return std::make_pair(output, status);

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error executing script '{}': {}", nameStr, e.what());
        throw ScriptException(std::string("Script execution failed: ") +
                              e.what());
    }
}

auto ScriptManagerImpl::runScript(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args, bool safe,
    std::optional<int> timeoutMs,
    int retryCount) -> std::optional<std::pair<std::string, int>> {
    try {
        return runScriptImpl(name, args, safe, timeoutMs, retryCount);
    } catch (const ScriptException& e) {
        LOG_F(ERROR, "ScriptException: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during script execution: {}", e.what());
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
    bool safe,
    int retryCount) -> std::vector<std::optional<std::pair<std::string, int>>> {
    std::vector<std::optional<std::pair<std::string, int>>> results;
    results.reserve(scripts.size());
    for (const auto& [name, args] : scripts) {
        try {
            results.emplace_back(
                runScriptImpl(name, args, safe, std::nullopt, retryCount));
        } catch (const ScriptException& e) {
            LOG_F(ERROR, "Error running script '{}': {}", name, e.what());
            results.emplace_back(std::nullopt);
        }
    }
    return results;
}

auto ScriptManagerImpl::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe,
    int retryCount) -> std::vector<std::optional<std::pair<std::string, int>>> {
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
            LOG_F(ERROR, "ScriptException during concurrent execution: {}",
                  e.what());
            results.emplace_back(std::nullopt);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during concurrent execution: {}", e.what());
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
    LOG_F(INFO, "Versioning enabled for all scripts.");
}

auto ScriptManagerImpl::rollbackScript(std::string_view name,
                                       int version) -> bool {
    std::unique_lock lock(mSharedMutex_);
    std::string nameStr(name);
    if (!scriptVersions_.contains(nameStr) || version < 0 ||
        version >= static_cast<int>(scriptVersions_[nameStr].size())) {
        LOG_F(ERROR, "Invalid rollback attempt for script '{}' to version %d.",
              nameStr, version);
        return false;
    }
    if (scripts_.contains(nameStr)) {
        scripts_[nameStr] = scriptVersions_[nameStr][version];
    } else if (powerShellScripts_.contains(nameStr)) {
        powerShellScripts_[nameStr] = scriptVersions_[nameStr][version];
    } else {
        LOG_F(ERROR, "Script '{}' not found for rollback.", nameStr);
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
    LOG_F(INFO, "Max script versions set to %d.", maxVersions_);
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

}  // namespace lithium
