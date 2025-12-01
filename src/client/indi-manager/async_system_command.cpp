/*
 * async_system_command.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Async System Command implementation

**************************************************/

#include "async_system_command.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lithium::client::indi_manager {

AsyncSystemCommand::AsyncSystemCommand(const std::string& cmd)
    : cmd_(cmd), pid_(0), running_(false), lastExitStatus_(0) {
    LOG_INFO("AsyncSystemCommand created with command: {}", cmd);
}

AsyncSystemCommand::~AsyncSystemCommand() {
    LOG_INFO("AsyncSystemCommand destructor called");
    terminate();
}

void AsyncSystemCommand::setEnvironmentVariables(
    const std::unordered_map<std::string, std::string>& envVars) {
    std::lock_guard lock(mutex_);
    envVars_ = envVars;
}

bool AsyncSystemCommand::isCommandValid() const {
    return atom::system::isCommandAvailable(cmd_);
}

void AsyncSystemCommand::run() {
    std::lock_guard lock(mutex_);

    if (running_) {
        LOG_WARN("Command already running");
        return;
    }

    if (!isCommandValid()) {
        LOG_ERROR("Command not available: {}", cmd_);
        return;
    }

    auto [pid, output] = atom::system::startProcess(
        !envVars_.empty()
            ? atom::system::executeCommandWithEnv(cmd_, envVars_)
            : cmd_);

    if (pid > 0) {
        pid_ = pid;
        running_ = true;
        LOG_INFO("Started command with PID {}", pid_.load());
    } else {
        LOG_ERROR("Failed to start command");
    }
}

void AsyncSystemCommand::terminate() {
    std::lock_guard lock(mutex_);

    if (!running_) {
        LOG_INFO("No running command to terminate");
        return;
    }

    pid_t pid = pid_.load();
    if (pid <= 0) {
        running_ = false;
        LOG_WARN("Invalid PID: {}", pid);
        return;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        LOG_ERROR("OpenProcess failed: {}", GetLastError());
        return;
    }

    if (!TerminateProcess(hProcess, 0)) {
        LOG_ERROR("TerminateProcess failed: {}", GetLastError());
    } else {
        LOG_INFO("Process {} terminated", pid);
    }

    CloseHandle(hProcess);
#else
    if (kill(-pid, SIGTERM) == 0) {
        int status;
        waitpid(pid, &status, 0);
        LOG_INFO("Process {} terminated", pid);
    } else {
        LOG_ERROR("Failed to terminate process {}: {}", pid, strerror(errno));
    }
#endif

    pid_ = 0;
    running_ = false;
}

bool AsyncSystemCommand::isRunning() const {
    std::lock_guard lock(mutex_);

    if (!running_) {
        return false;
    }

    pid_t pid = pid_.load();
    if (pid <= 0) {
        return false;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    if (exitCode == STILL_ACTIVE) {
        return true;
    }
#else
    if (kill(pid, 0) == 0) {
        return true;
    }
#endif

    const_cast<AsyncSystemCommand*>(this)->running_ = false;
    return false;
}

std::string AsyncSystemCommand::getLastOutput() const {
    std::lock_guard lock(mutex_);
    return lastOutput_;
}

int AsyncSystemCommand::getLastExitStatus() const {
    std::lock_guard lock(mutex_);
    return lastExitStatus_;
}

}  // namespace lithium::client::indi_manager
