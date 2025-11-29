/*
 * process_runner.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Process execution utilities implementation

*************************************************/

#include "process_runner.hpp"

#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

#include <spdlog/spdlog.h>

#include <sstream>

namespace lithium::client {

// ============================================================================
// ProcessRunner Implementation
// ============================================================================

class ProcessRunner::Impl {
public:
    auto execute(const ProcessConfig& config, std::atomic<bool>& abortFlag,
                 std::atomic<int>& pidOut)
        -> atom::type::expected<ProcessResult, ProcessError> {
        if (!validateExecutable(config.executable)) {
            return atom::type::unexpected(ProcessError::NotFound);
        }

        auto startTime = std::chrono::steady_clock::now();
        ProcessResult result;

        try {
            std::string cmdLine = buildCommandLine(config);
            spdlog::debug("Executing: {}", cmdLine);

            // Execute command
            result.stdOut = atom::system::executeCommand(cmdLine, false);
            result.exitCode = 0;  // executeCommand throws on failure

        } catch (const std::exception& ex) {
            spdlog::error("Process execution failed: {}", ex.what());
            result.stdErr = ex.what();
            result.exitCode = -1;
            return atom::type::unexpected(ProcessError::ExecutionFailed);
        }

        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (abortFlag.load()) {
            result.aborted = true;
        }

        return result;
    }

    static auto buildCommandLine(const ProcessConfig& config) -> std::string {
        std::ostringstream cmd;

        // Quote executable path if needed
        auto exePath = config.executable.string();
        if (exePath.find(' ') != std::string::npos) {
            cmd << "\"" << exePath << "\"";
        } else {
            cmd << exePath;
        }

        // Add arguments
        for (const auto& arg : config.arguments) {
            cmd << " ";
            if (arg.find(' ') != std::string::npos && arg.front() != '"') {
                cmd << "\"" << arg << "\"";
            } else {
                cmd << arg;
            }
        }

        return cmd.str();
    }

    static auto validateExecutable(const std::filesystem::path& path) -> bool {
        if (path.empty()) {
            return false;
        }

        // Check if it's an absolute path that exists
        if (path.is_absolute()) {
            return std::filesystem::exists(path);
        }

        // Check if it's in PATH
        return atom::system::checkSoftwareInstalled(path.string());
    }
};

ProcessRunner::~ProcessRunner() {
    if (running_.load()) {
        abort();
    }
}

auto ProcessRunner::execute(const ProcessConfig& config)
    -> atom::type::expected<ProcessResult, ProcessError> {
    if (running_.load()) {
        return atom::type::unexpected(ProcessError::ExecutionFailed);
    }

    running_.store(true);
    abortRequested_.store(false);

    Impl impl;
    auto result = impl.execute(config, abortRequested_, currentPid_);

    running_.store(false);
    currentPid_.store(0);

    return result;
}

auto ProcessRunner::executeAsync(const ProcessConfig& config)
    -> std::future<atom::type::expected<ProcessResult, ProcessError>> {
    return std::async(std::launch::async,
                      [this, config]() { return execute(config); });
}

auto ProcessRunner::executeWithCallback(const ProcessConfig& config,
                                        OutputCallback callback)
    -> atom::type::expected<ProcessResult, ProcessError> {
    // For now, execute normally and call callback with full output
    auto result = execute(config);
    if (result.has_value() && callback) {
        // Split output into lines and call callback
        std::istringstream stream(result.value().stdOut);
        std::string line;
        while (std::getline(stream, line)) {
            callback(line, false);
        }
        if (!result.value().stdErr.empty()) {
            std::istringstream errStream(result.value().stdErr);
            while (std::getline(errStream, line)) {
                callback(line, true);
            }
        }
    }
    return result;
}

void ProcessRunner::abort() {
    abortRequested_.store(true);
    auto pid = currentPid_.load();
    if (pid > 0) {
        try {
            atom::system::killProcessByPID(pid, 15);
        } catch (...) {
            // Ignore errors during abort
        }
    }
}

auto ProcessRunner::isRunning() const noexcept -> bool {
    return running_.load();
}

auto ProcessRunner::getProcessId() const noexcept -> std::optional<int> {
    auto pid = currentPid_.load();
    return pid > 0 ? std::optional{pid} : std::nullopt;
}

auto ProcessRunner::buildCommandLine(const ProcessConfig& config)
    -> std::string {
    return Impl::buildCommandLine(config);
}

auto ProcessRunner::validateExecutable(const std::filesystem::path& path)
    -> bool {
    return Impl::validateExecutable(path);
}

// ============================================================================
// CommandBuilder Implementation
// ============================================================================

CommandBuilder::CommandBuilder(std::string_view executable) {
    config_.executable = executable;
}

auto CommandBuilder::addFlag(std::string_view flag) -> CommandBuilder& {
    config_.arguments.emplace_back(flag);
    return *this;
}

auto CommandBuilder::addFlagIf(bool condition, std::string_view flag)
    -> CommandBuilder& {
    if (condition) {
        config_.arguments.emplace_back(flag);
    }
    return *this;
}

auto CommandBuilder::addOption(std::string_view option, std::string_view value)
    -> CommandBuilder& {
    config_.arguments.emplace_back(option);
    config_.arguments.emplace_back(value);
    return *this;
}

auto CommandBuilder::addOptionIf(bool condition, std::string_view option,
                                 std::string_view value) -> CommandBuilder& {
    if (condition) {
        config_.arguments.emplace_back(option);
        config_.arguments.emplace_back(value);
    }
    return *this;
}

auto CommandBuilder::addArg(std::string_view arg) -> CommandBuilder& {
    config_.arguments.emplace_back(arg);
    return *this;
}

auto CommandBuilder::addArgs(std::span<const std::string> args)
    -> CommandBuilder& {
    for (const auto& arg : args) {
        config_.arguments.push_back(arg);
    }
    return *this;
}

auto CommandBuilder::setWorkingDirectory(const std::filesystem::path& path)
    -> CommandBuilder& {
    config_.workingDirectory = path;
    return *this;
}

auto CommandBuilder::setTimeout(std::chrono::milliseconds timeout)
    -> CommandBuilder& {
    config_.timeout = timeout;
    return *this;
}

auto CommandBuilder::setEnv(std::string_view key, std::string_view value)
    -> CommandBuilder& {
    config_.environment[std::string(key)] = std::string(value);
    return *this;
}

auto CommandBuilder::build() const -> ProcessConfig { return config_; }

auto CommandBuilder::toString() const -> std::string {
    return ProcessRunner::buildCommandLine(config_);
}

// ============================================================================
// ProcessGuard Implementation
// ============================================================================

ProcessGuard::ProcessGuard(ProcessRunner& runner) : runner_(&runner) {}

ProcessGuard::~ProcessGuard() {
    if (!released_ && runner_ && runner_->isRunning()) {
        runner_->abort();
    }
}

void ProcessGuard::release() noexcept { released_ = true; }

}  // namespace lithium::client
