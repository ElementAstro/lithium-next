/*
 * process_runner.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Modern C++ process execution utilities for client components

*************************************************/

#pragma once

#include "atom/type/expected.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lithium::client {

/**
 * @brief Process execution result
 */
struct ProcessResult {
    int exitCode{-1};
    std::string stdOut;
    std::string stdErr;
    std::chrono::milliseconds duration{0};
    bool timedOut{false};
    bool aborted{false};

    [[nodiscard]] auto success() const noexcept -> bool {
        return exitCode == 0 && !timedOut && !aborted;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return success(); }
};

/**
 * @brief Process execution error types
 */
enum class ProcessError {
    NotFound,
    PermissionDenied,
    Timeout,
    Aborted,
    ExecutionFailed,
    InvalidArgument
};

/**
 * @brief Process execution configuration
 */
struct ProcessConfig {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> workingDirectory;
    std::optional<std::chrono::milliseconds> timeout;
    bool captureOutput{true};
    bool mergeStderr{false};
    std::unordered_map<std::string, std::string> environment;
};

/**
 * @brief Output callback for streaming process output
 */
using OutputCallback = std::function<void(std::string_view line, bool isStderr)>;

/**
 * @brief Modern process runner with RAII and async support
 */
class ProcessRunner {
public:
    ProcessRunner() = default;
    ~ProcessRunner();

    // Non-copyable, movable
    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;
    ProcessRunner(ProcessRunner&&) noexcept = default;
    ProcessRunner& operator=(ProcessRunner&&) noexcept = default;

    /**
     * @brief Execute a process synchronously
     * @param config Process configuration
     * @return Expected result or error
     */
    [[nodiscard]] auto execute(const ProcessConfig& config)
        -> atom::type::expected<ProcessResult, ProcessError>;

    /**
     * @brief Execute a process asynchronously
     * @param config Process configuration
     * @return Future containing result
     */
    [[nodiscard]] auto executeAsync(const ProcessConfig& config)
        -> std::future<atom::type::expected<ProcessResult, ProcessError>>;

    /**
     * @brief Execute with streaming output callback
     * @param config Process configuration
     * @param callback Output callback
     * @return Expected result or error
     */
    [[nodiscard]] auto executeWithCallback(const ProcessConfig& config,
                                           OutputCallback callback)
        -> atom::type::expected<ProcessResult, ProcessError>;

    /**
     * @brief Abort currently running process
     */
    void abort();

    /**
     * @brief Check if process is currently running
     */
    [[nodiscard]] auto isRunning() const noexcept -> bool;

    /**
     * @brief Get current process ID (if running)
     */
    [[nodiscard]] auto getProcessId() const noexcept -> std::optional<int>;

    /**
     * @brief Build command line string from config
     */
    [[nodiscard]] static auto buildCommandLine(const ProcessConfig& config)
        -> std::string;

    /**
     * @brief Check if executable exists and is runnable
     */
    [[nodiscard]] static auto validateExecutable(
        const std::filesystem::path& path) -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
    std::atomic<bool> abortRequested_{false};
    std::atomic<int> currentPid_{0};
};

/**
 * @brief Command line argument builder with fluent interface
 */
class CommandBuilder {
public:
    explicit CommandBuilder(std::string_view executable);

    /**
     * @brief Add a flag (e.g., --verbose)
     */
    auto addFlag(std::string_view flag) -> CommandBuilder&;

    /**
     * @brief Add a flag conditionally
     */
    auto addFlagIf(bool condition, std::string_view flag) -> CommandBuilder&;

    /**
     * @brief Add an option with value (e.g., --output file.txt)
     */
    auto addOption(std::string_view option, std::string_view value)
        -> CommandBuilder&;

    /**
     * @brief Add an option with value conditionally
     */
    auto addOptionIf(bool condition, std::string_view option,
                     std::string_view value) -> CommandBuilder&;

    /**
     * @brief Add an optional value (only if has_value)
     */
    template <typename T>
    auto addOptional(std::string_view option, const std::optional<T>& value)
        -> CommandBuilder& {
        if (value) {
            if constexpr (std::is_same_v<T, std::string>) {
                return addOption(option, *value);
            } else {
                return addOption(option, std::to_string(*value));
            }
        }
        return *this;
    }

    /**
     * @brief Add a positional argument
     */
    auto addArg(std::string_view arg) -> CommandBuilder&;

    /**
     * @brief Add multiple positional arguments
     */
    auto addArgs(std::span<const std::string> args) -> CommandBuilder&;

    /**
     * @brief Set working directory
     */
    auto setWorkingDirectory(const std::filesystem::path& path)
        -> CommandBuilder&;

    /**
     * @brief Set timeout
     */
    auto setTimeout(std::chrono::milliseconds timeout) -> CommandBuilder&;

    /**
     * @brief Set environment variable
     */
    auto setEnv(std::string_view key, std::string_view value) -> CommandBuilder&;

    /**
     * @brief Build the process configuration
     */
    [[nodiscard]] auto build() const -> ProcessConfig;

    /**
     * @brief Build command line string
     */
    [[nodiscard]] auto toString() const -> std::string;

private:
    ProcessConfig config_;
};

/**
 * @brief RAII guard for process execution with automatic cleanup
 */
class ProcessGuard {
public:
    explicit ProcessGuard(ProcessRunner& runner);
    ~ProcessGuard();

    ProcessGuard(const ProcessGuard&) = delete;
    ProcessGuard& operator=(const ProcessGuard&) = delete;

    /**
     * @brief Release the guard without aborting
     */
    void release() noexcept;

private:
    ProcessRunner* runner_;
    bool released_{false};
};

}  // namespace lithium::client
