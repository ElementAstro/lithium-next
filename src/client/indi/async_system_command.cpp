#include "async_system_command.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#endif

AsyncSystemCommand::AsyncSystemCommand(const std::string& cmd)
    : cmd_(cmd), pid_(0), running_(false), last_exit_status_(0) {
    LOG_F(INFO, "AsyncSystemCommand created with command: {}", cmd);
}

AsyncSystemCommand::~AsyncSystemCommand() {
    LOG_F(INFO, "AsyncSystemCommand destructor called");
    terminate();
}

void AsyncSystemCommand::setEnvironmentVariables(
    const std::unordered_map<std::string, std::string>& envVars) {
    std::lock_guard lock(mutex_);
    env_vars_ = envVars;
}

bool AsyncSystemCommand::isCommandValid() const {
    return atom::system::isCommandAvailable(cmd_);
}

void AsyncSystemCommand::run() {
    std::lock_guard lock(mutex_);

    if (running_) {
        LOG_F(WARNING, "Command already running");
        return;
    }

    if (!isCommandValid()) {
        LOG_F(ERROR, "Command not available: {}", cmd_);
        return;
    }

    auto [pid, output] = atom::system::startProcess(
        !env_vars_.empty()
            ? atom::system::executeCommandWithEnv(cmd_, env_vars_)
            : cmd_);

    if (pid > 0) {
        pid_ = pid;
        running_ = true;
        LOG_F(INFO, "Started command with PID {}", pid_.load());
    } else {
        LOG_F(ERROR, "Failed to start command");
    }
}

void AsyncSystemCommand::terminate() {
    std::lock_guard lock(mutex_);

    if (!running_) {
        LOG_F(INFO, "No running command to terminate");
        return;
    }

    pid_t pid = pid_.load();
    if (pid <= 0) {
        running_ = false;
        LOG_F(WARNING, "Invalid PID: {}", pid);
        return;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "OpenProcess failed: {}", GetLastError());
        return;
    }

    if (!TerminateProcess(hProcess, 0)) {
        LOG_F(ERROR, "TerminateProcess failed: {}", GetLastError());
    } else {
        LOG_F(INFO, "Process {} terminated", pid);
    }

    CloseHandle(hProcess);
#else
    // Kill entire process group
    if (kill(-pid, SIGTERM) == 0) {
        int status;
        waitpid(pid, &status, 0);
        LOG_F(INFO, "Process {} terminated", pid);
    } else {
        LOG_F(ERROR, "Failed to terminate process {}: {}", pid,
              strerror(errno));
    }
#endif

    pid_ = 0;
    running_ = false;
}

bool AsyncSystemCommand::isRunning() const {
    std::lock_guard lock(mutex_);

    if (!running_) {
        LOG_F(INFO, "No running command");
        return false;
    }

    pid_t pid = pid_.load();
    if (pid <= 0) {
        LOG_F(WARNING, "Invalid PID: {}", pid);
        return false;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        LOG_F(ERROR, "OpenProcess failed: {}", GetLastError());
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        LOG_F(ERROR, "GetExitCodeProcess failed: {}", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    if (exitCode == STILL_ACTIVE) {
        LOG_F(INFO, "Process {} is still running", pid);
        return true;
    }
#else
    // Check if process exists
    if (kill(pid, 0) == 0) {
        LOG_F(INFO, "Process {} is still running", pid);
        return true;
    }
#endif

    // Process no longer exists
    const_cast<AsyncSystemCommand*>(this)->running_ = false;
    LOG_F(INFO, "Process {} is no longer running", pid);
    return false;
}

std::string AsyncSystemCommand::getLastOutput() const {
    std::lock_guard lock(mutex_);
    return last_output_;
}

int AsyncSystemCommand::getLastExitStatus() const {
    std::lock_guard lock(mutex_);
    return last_exit_status_;
}
