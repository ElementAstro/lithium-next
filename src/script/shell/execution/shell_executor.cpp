/*
 * shell_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file shell_executor.cpp
 * @brief Shell/Bash script executor implementation
 * @date 2024-1-13
 */

#include "shell_executor.hpp"

#include <cstdio>
#include <sstream>

#include <spdlog/spdlog.h>
#include "atom/sysinfo/os.hpp"

namespace lithium::shell {

class ShellExecutor::Impl {
public:
    std::atomic<bool> abortFlag_{false};
    std::atomic<bool> running_{false};

    auto buildEnvironmentString(
        const std::unordered_map<std::string, std::string>& env) -> std::string {
        std::string envCmd;
        for (const auto& [key, value] : env) {
            if (atom::system::isWsl()) {
                envCmd += "$env:" + key + "=\"" + value + "\";";
            } else {
                envCmd += "export " + key + "=\"" + value + "\";";
            }
        }
        return envCmd;
    }

    auto buildArgumentString(
        const std::unordered_map<std::string, std::string>& args) -> std::string {
        std::string argStr;
        for (const auto& [key, value] : args) {
            argStr += " \"" + key + "=" + value + "\"";
        }
        return argStr;
    }
};

ShellExecutor::ShellExecutor() : pImpl_(std::make_unique<Impl>()) {}

ShellExecutor::~ShellExecutor() = default;

ShellExecutor::ShellExecutor(ShellExecutor&&) noexcept = default;
ShellExecutor& ShellExecutor::operator=(ShellExecutor&&) noexcept = default;

auto ShellExecutor::execute(const Script& script, const ExecutionContext& ctx)
    -> ScriptExecutionResult {
    spdlog::debug("ShellExecutor: executing script ({} chars)", script.size());

    ScriptExecutionResult result;
    result.detectedLanguage = ScriptLanguage::Shell;

    auto startTime = std::chrono::steady_clock::now();
    pImpl_->running_ = true;
    pImpl_->abortFlag_ = false;

    try {
        // Build environment and arguments
        std::string envCmd = pImpl_->buildEnvironmentString(ctx.environment);
        std::string argStr = pImpl_->buildArgumentString(ctx.arguments);

        // Build the command
        std::string command = envCmd + "sh -c \"" + script + "\"" + argStr;

        // Execute using pipe
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            result.success = false;
            result.exitCode = -1;
            result.errorOutput = "Failed to create pipe for script execution";
            pImpl_->running_ = false;
            return result;
        }

        std::string output;
        char buffer[128];
        while (!pImpl_->abortFlag_.load() &&
               fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;

            // Parse progress if available
            if (ctx.progressCallback && output.find("PROGRESS:") != std::string::npos) {
                try {
                    float progress = std::stof(output.substr(output.find(":") + 1));
                    ScriptProgress progressInfo;
                    progressInfo.percentage = progress;
                    progressInfo.status = "Running";
                    progressInfo.timestamp = std::chrono::system_clock::now();
                    ctx.progressCallback(progressInfo);
                } catch (...) {
                    // Ignore progress parsing errors
                }
            }
        }

        int status = pclose(pipe);

        if (pImpl_->abortFlag_.load()) {
            result.success = false;
            result.exitCode = -999;
            result.errorOutput = "Script execution aborted";
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
        spdlog::error("ShellExecutor: execution failed: {}", e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    pImpl_->running_ = false;
    return result;
}

auto ShellExecutor::supports(ScriptLanguage lang) const noexcept -> bool {
    return lang == ScriptLanguage::Shell || lang == ScriptLanguage::Auto;
}

auto ShellExecutor::primaryLanguage() const noexcept -> ScriptLanguage {
    return ScriptLanguage::Shell;
}

void ShellExecutor::abort() {
    pImpl_->abortFlag_ = true;
}

auto ShellExecutor::isRunning() const noexcept -> bool {
    return pImpl_->running_.load();
}

}  // namespace lithium::shell
