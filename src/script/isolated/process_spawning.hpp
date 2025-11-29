/*
 * process_spawning.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_PROCESS_SPAWNING_HPP
#define LITHIUM_SCRIPT_ISOLATED_PROCESS_SPAWNING_HPP

#include "types.hpp"

#include <filesystem>
#include <tuple>

namespace lithium::isolated {

/**
 * @brief Platform-independent process spawning interface
 */
class ProcessSpawner {
public:
    /**
     * @brief Spawn a Python subprocess
     * @param pythonPath Path to Python executable
     * @param executorPath Path to executor script
     * @param config Isolation configuration
     * @param subprocessFds File descriptors for IPC (read, write)
     * @return Process ID on success, or error
     */
    [[nodiscard]] static Result<int> spawn(
        const std::filesystem::path& pythonPath,
        const std::filesystem::path& executorPath,
        const IsolationConfig& config,
        std::pair<int, int> subprocessFds);

    /**
     * @brief Wait for process to exit
     * @param processId Process ID to wait for
     * @param timeoutMs Timeout in milliseconds (0 = infinite)
     * @return Exit code or error
     */
    [[nodiscard]] static Result<int> waitForProcess(int processId, int timeoutMs = 0);

    /**
     * @brief Kill a running process
     * @param processId Process ID to kill
     * @return Success or error
     */
    [[nodiscard]] static Result<void> killProcess(int processId);

    /**
     * @brief Check if process is still running
     * @param processId Process ID to check
     * @return True if running
     */
    [[nodiscard]] static bool isProcessRunning(int processId);

#ifdef _WIN32
    /**
     * @brief Get native Windows process handle
     * @param processId Process ID
     * @return Handle or nullptr
     */
    [[nodiscard]] static void* getProcessHandle(int processId);

    /**
     * @brief Close Windows process handle
     * @param handle Handle to close
     */
    static void closeProcessHandle(void* handle);
#endif
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_PROCESS_SPAWNING_HPP
