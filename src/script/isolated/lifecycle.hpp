/*
 * lifecycle.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_LIFECYCLE_HPP
#define LITHIUM_SCRIPT_ISOLATED_LIFECYCLE_HPP

#include "../ipc/channel.hpp"
#include "types.hpp"

#include <atomic>
#include <memory>

namespace lithium::isolated {

/**
 * @brief Process lifecycle management
 *
 * Handles process cancellation, termination, and cleanup operations
 * for isolated Python subprocesses.
 */
class ProcessLifecycle {
public:
    ProcessLifecycle();
    ~ProcessLifecycle();

    // Non-copyable
    ProcessLifecycle(const ProcessLifecycle&) = delete;
    ProcessLifecycle& operator=(const ProcessLifecycle&) = delete;

    // Movable
    ProcessLifecycle(ProcessLifecycle&&) noexcept;
    ProcessLifecycle& operator=(ProcessLifecycle&&) noexcept;

    /**
     * @brief Set the IPC channel for communication
     */
    void setChannel(std::shared_ptr<ipc::BidirectionalChannel> channel);

    /**
     * @brief Set the current process ID
     */
    void setProcessId(int processId);

    /**
     * @brief Mark the process as running
     */
    void setRunning(bool running);

    /**
     * @brief Check if process is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get current process ID
     */
    [[nodiscard]] int getProcessId() const;

    /**
     * @brief Request cancellation of current execution
     * @return True if cancellation request was sent
     */
    bool cancel();

    /**
     * @brief Check if cancellation was requested
     */
    [[nodiscard]] bool isCancelled() const;

    /**
     * @brief Reset cancellation flag
     */
    void resetCancellation();

    /**
     * @brief Kill the subprocess forcefully
     */
    void kill();

    /**
     * @brief Wait for process to exit with optional timeout
     * @param timeoutMs Timeout in milliseconds (0 = default 5000ms)
     */
    void waitForExit(int timeoutMs = 5000);

    /**
     * @brief Clean up resources (channel, handles, etc.)
     */
    void cleanup();

private:
    std::shared_ptr<ipc::BidirectionalChannel> channel_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    int processId_{-1};

#ifdef _WIN32
    void* processHandle_{nullptr};
#endif
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_LIFECYCLE_HPP
