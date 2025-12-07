/*
 * python_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file python_executor.cpp
 * @brief Python script executor implementation
 * @date 2024-1-13
 */

#include "python_executor.hpp"

#include <atomic>
#include <cstdio>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::shell {

class PythonExecutor::Impl {
public:
    std::atomic<bool> abortFlag_{false};
    std::atomic<bool> running_{false};
    std::vector<std::filesystem::path> sysPaths_;
    std::filesystem::path pythonExecutable_{"python3"};

    auto buildPythonCommand(const Script& script, const ExecutionContext& ctx)
        -> std::string {
        std::stringstream cmd;

        // Use specified Python executable
        cmd << pythonExecutable_.string();

        // Add sys.path entries via -c option with inline code
        if (!sysPaths_.empty()) {
            cmd << " -c \"import sys; ";
            for (const auto& path : sysPaths_) {
                cmd << "sys.path.insert(0, '" << path.string() << "'); ";
            }
            cmd << "exec(open('<script>').read())\"";
        } else {
            cmd << " -c \"" << script << "\"";
        }

        return cmd.str();
    }

    auto buildEnvironmentPrefix(
        const std::unordered_map<std::string, std::string>& env)
        -> std::string {
        std::string prefix;
        for (const auto& [key, value] : env) {
#ifdef _WIN32
            prefix += "set " + key + "=" + value + " && ";
#else
            prefix += key + "=\"" + value + "\" ";
#endif
        }
        return prefix;
    }
};

PythonExecutor::PythonExecutor() : pImpl_(std::make_unique<Impl>()) {}

PythonExecutor::~PythonExecutor() = default;

PythonExecutor::PythonExecutor(PythonExecutor&&) noexcept = default;
PythonExecutor& PythonExecutor::operator=(PythonExecutor&&) noexcept = default;

auto PythonExecutor::execute(const Script& script, const ExecutionContext& ctx)
    -> ScriptExecutionResult {
    spdlog::debug("PythonExecutor: executing script ({} chars)", script.size());

    ScriptExecutionResult result;
    result.detectedLanguage = ScriptLanguage::Python;

    auto startTime = std::chrono::steady_clock::now();
    pImpl_->running_ = true;
    pImpl_->abortFlag_ = false;

    try {
        // Build environment and command
        std::string envPrefix = pImpl_->buildEnvironmentPrefix(ctx.environment);

        // Create temporary script file for complex scripts
        std::string command;
        if (script.find('\n') != std::string::npos || script.size() > 1000) {
            // For multi-line or long scripts, use -c with proper escaping
            std::string escapedScript = script;
            // Escape quotes
            size_t pos = 0;
            while ((pos = escapedScript.find('"', pos)) != std::string::npos) {
                escapedScript.replace(pos, 1, "\\\"");
                pos += 2;
            }
            command = envPrefix + pImpl_->pythonExecutable_.string() +
                      " -c \"" + escapedScript + "\"";
        } else {
            command = envPrefix + pImpl_->pythonExecutable_.string() +
                      " -c \"" + script + "\"";
        }

        // Execute using pipe
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            result.success = false;
            result.exitCode = -1;
            result.errorOutput = "Failed to create pipe for Python execution";
            pImpl_->running_ = false;
            return result;
        }

        std::string output;
        char buffer[256];
        while (!pImpl_->abortFlag_.load() &&
               fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;

            // Progress callback
            if (ctx.progressCallback) {
                ScriptProgress progressInfo;
                progressInfo.status = "Running";
                progressInfo.output = output;
                progressInfo.timestamp = std::chrono::system_clock::now();
                ctx.progressCallback(progressInfo);
            }
        }

        int status = pclose(pipe);

        if (pImpl_->abortFlag_.load()) {
            result.success = false;
            result.exitCode = -999;
            result.errorOutput = "Python execution aborted";
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
        spdlog::error("PythonExecutor: execution failed: {}", e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    pImpl_->running_ = false;
    return result;
}

auto PythonExecutor::supports(ScriptLanguage lang) const noexcept -> bool {
    return lang == ScriptLanguage::Python;
}

auto PythonExecutor::primaryLanguage() const noexcept -> ScriptLanguage {
    return ScriptLanguage::Python;
}

void PythonExecutor::abort() { pImpl_->abortFlag_ = true; }

auto PythonExecutor::isRunning() const noexcept -> bool {
    return pImpl_->running_.load();
}

auto PythonExecutor::executeWithConfig(const PythonScriptConfig& config,
                                       const ExecutionContext& ctx)
    -> ScriptExecutionResult {
    spdlog::debug("PythonExecutor: executing with config (module='{}')",
                  config.moduleName);

    // Add configured sys.paths
    for (const auto& path : config.sysPaths) {
        addSysPath(path);
    }

    // Build import and call script
    std::stringstream script;
    script << "import " << config.moduleName << "; ";
    if (!config.entryFunction.empty()) {
        script << config.moduleName << "." << config.entryFunction << "()";
    }

    // Merge environment variables
    ExecutionContext mergedCtx = ctx;
    for (const auto& [key, value] : config.envVars) {
        mergedCtx.environment[key] = value;
    }

    return execute(script.str(), mergedCtx);
}

void PythonExecutor::addSysPath(const std::filesystem::path& path) {
    pImpl_->sysPaths_.push_back(path);
    spdlog::debug("PythonExecutor: added sys.path '{}'", path.string());
}

void PythonExecutor::setPythonExecutable(const std::filesystem::path& path) {
    pImpl_->pythonExecutable_ = path;
    spdlog::debug("PythonExecutor: set Python executable to '{}'",
                  path.string());
}

auto PythonExecutor::isPythonAvailable() const -> bool {
    // Try to execute python --version
    std::string cmd = pImpl_->pythonExecutable_.string() + " --version";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    int status = pclose(pipe);
    return status == 0;
}

}  // namespace lithium::shell
