/*
 * process_spawning_unix.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef _WIN32

#include "process_spawning.hpp"

#include <spdlog/spdlog.h>

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace lithium::isolated {

Result<int> ProcessSpawner::spawn(
    const std::filesystem::path& pythonPath,
    const std::filesystem::path& executorPath,
    const IsolationConfig& config,
    std::pair<int, int> subprocessFds) {

    auto [readFd, writeFd] = subprocessFds;

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("Fork failed");
        return std::unexpected(RunnerError::ProcessSpawnFailed);
    }

    if (pid == 0) {
        // Child process

        // Set resource limits if sandboxed
        if (config.level == IsolationLevel::Sandboxed) {
            if (config.maxMemoryMB > 0) {
                struct rlimit limit;
                limit.rlim_cur = config.maxMemoryMB * 1024 * 1024;
                limit.rlim_max = config.maxMemoryMB * 1024 * 1024;
                setrlimit(RLIMIT_AS, &limit);
            }
        }

        // Change working directory
        if (!config.workingDirectory.empty()) {
            chdir(config.workingDirectory.c_str());
        }

        // Execute Python
        std::string readFdStr = std::to_string(readFd);
        std::string writeFdStr = std::to_string(writeFd);

        execl(pythonPath.c_str(),
              pythonPath.c_str(),
              executorPath.c_str(),
              readFdStr.c_str(),
              writeFdStr.c_str(),
              nullptr);

        // If we get here, exec failed
        _exit(127);
    }

    // Parent process
    spdlog::debug("Spawned Python process with PID {}", pid);
    return static_cast<int>(pid);
}

Result<int> ProcessSpawner::waitForProcess(int processId, int timeoutMs) {
    if (timeoutMs == 0) {
        int status;
        waitpid(processId, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return std::unexpected(RunnerError::ProcessCrashed);
    }

    // Non-blocking wait with timeout
    int elapsed = 0;
    while (elapsed < timeoutMs) {
        int status;
        pid_t result = waitpid(processId, &status, WNOHANG);
        if (result == processId) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            return std::unexpected(RunnerError::ProcessCrashed);
        } else if (result < 0) {
            return std::unexpected(RunnerError::ProcessCrashed);
        }
        usleep(10000);  // 10ms
        elapsed += 10;
    }

    return std::unexpected(RunnerError::Timeout);
}

Result<void> ProcessSpawner::killProcess(int processId) {
    if (kill(processId, SIGKILL) != 0) {
        return std::unexpected(RunnerError::ProcessKilled);
    }

    int status;
    waitpid(processId, &status, 0);
    return {};
}

bool ProcessSpawner::isProcessRunning(int processId) {
    return kill(processId, 0) == 0;
}

}  // namespace lithium::isolated

#endif  // !_WIN32
