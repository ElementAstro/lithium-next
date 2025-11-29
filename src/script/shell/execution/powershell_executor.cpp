/*
 * powershell_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file powershell_executor.cpp
 * @brief PowerShell script executor implementation
 * @date 2024-1-13
 */

#include "powershell_executor.hpp"

#include <atomic>
#include <cstdio>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::shell {

class PowerShellExecutor::Impl {
public:
    std::atomic<bool> abortFlag_{false};
    std::atomic<bool> running_{false};
    std::vector<std::string> importedModules_;

    auto buildSetupScript() const -> std::string {
        std::string setup = "$ErrorActionPreference = 'Stop';\n";
        for (const auto& module : importedModules_) {
            setup += "Import-Module " + module + ";\n";
        }
        return setup;
    }

    auto buildEnvironmentString(
        const std::unordered_map<std::string, std::string>& env) -> std::string {
        std::string envCmd;
        for (const auto& [key, value] : env) {
            envCmd += "$env:" + key + "=\"" + value + "\";";
        }
        return envCmd;
    }

    auto buildArgumentString(
        const std::unordered_map<std::string, std::string>& args) -> std::string {
        std::string argStr;
        for (const auto& [key, value] : args) {
            argStr += " -" + key + " '" + value + "'";
        }
        return argStr;
    }
};

PowerShellExecutor::PowerShellExecutor() : pImpl_(std::make_unique<Impl>()) {}

PowerShellExecutor::~PowerShellExecutor() = default;

PowerShellExecutor::PowerShellExecutor(PowerShellExecutor&&) noexcept = default;
PowerShellExecutor& PowerShellExecutor::operator=(PowerShellExecutor&&) noexcept = default;

auto PowerShellExecutor::execute(const Script& script, const ExecutionContext& ctx)
    -> ScriptExecutionResult {
    spdlog::debug("PowerShellExecutor: executing script ({} chars)", script.size());

    ScriptExecutionResult result;
    result.detectedLanguage = ScriptLanguage::PowerShell;

    auto startTime = std::chrono::steady_clock::now();
    pImpl_->running_ = true;
    pImpl_->abortFlag_ = false;

    try {
        // Build the PowerShell command
        std::string setup = pImpl_->buildSetupScript();
        std::string envCmd = pImpl_->buildEnvironmentString(ctx.environment);
        std::string argStr = pImpl_->buildArgumentString(ctx.arguments);

        std::string command = "powershell.exe -Command \"" +
                              envCmd + setup + script + argStr + "\"";

        // Execute using pipe
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            result.success = false;
            result.exitCode = -1;
            result.errorOutput = "Failed to create pipe for PowerShell execution";
            pImpl_->running_ = false;
            return result;
        }

        std::string output;
        char buffer[128];
        while (!pImpl_->abortFlag_.load() &&
               fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;

            // Progress callback
            if (ctx.progressCallback && output.find("Write-Progress") != std::string::npos) {
                ScriptProgress progressInfo;
                progressInfo.status = "Running";
                progressInfo.timestamp = std::chrono::system_clock::now();
                ctx.progressCallback(progressInfo);
            }
        }

        int status = pclose(pipe);

        if (pImpl_->abortFlag_.load()) {
            result.success = false;
            result.exitCode = -999;
            result.errorOutput = "PowerShell execution aborted";
        } else {
            result.success = (status == 0);
            result.exitCode = status;
            result.output = output;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.exitCode = -1;
        result.errorOutput = e.what();
        result.exception = e.what();
        spdlog::error("PowerShellExecutor: execution failed: {}", e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    pImpl_->running_ = false;
    return result;
}

auto PowerShellExecutor::supports(ScriptLanguage lang) const noexcept -> bool {
    return lang == ScriptLanguage::PowerShell;
}

auto PowerShellExecutor::primaryLanguage() const noexcept -> ScriptLanguage {
    return ScriptLanguage::PowerShell;
}

void PowerShellExecutor::abort() {
    pImpl_->abortFlag_ = true;
}

auto PowerShellExecutor::isRunning() const noexcept -> bool {
    return pImpl_->running_.load();
}

void PowerShellExecutor::importModule(const std::string& moduleName) {
    pImpl_->importedModules_.push_back(moduleName);
    spdlog::debug("PowerShellExecutor: imported module '{}'", moduleName);
}

auto PowerShellExecutor::getImportedModules() const -> std::vector<std::string> {
    return pImpl_->importedModules_;
}

}  // namespace lithium::shell
