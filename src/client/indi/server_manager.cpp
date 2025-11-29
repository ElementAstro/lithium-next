/*
 * server_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Server Manager implementation

**************************************************/

#include "server_manager.hpp"

#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

#include <csignal>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lithium::client::indi {

// ==================== INDIServerConfig ====================

std::vector<std::string> INDIServerConfig::buildCommandArgs() const {
    std::vector<std::string> args;

    args.push_back(binaryPath);

    // Port
    args.push_back("-p");
    args.push_back(std::to_string(port));

    // Max clients
    args.push_back("-m");
    args.push_back(std::to_string(maxClients));

    // Verbosity
    std::string verbosity = getVerbosityFlags();
    if (!verbosity.empty()) {
        args.push_back(verbosity);
    }

    // FIFO
    if (enableFifo && !fifoPath.empty()) {
        args.push_back("-f");
        args.push_back(fifoPath);
    }

    return args;
}

std::string INDIServerConfig::buildCommandString() const {
    std::stringstream cmd;

    cmd << binaryPath;
    cmd << " -p " << port;
    cmd << " -m " << maxClients;

    std::string verbosity = getVerbosityFlags();
    if (!verbosity.empty()) {
        cmd << " " << verbosity;
    }

    if (enableFifo && !fifoPath.empty()) {
        cmd << " -f " << fifoPath;
    }

    // Redirect output to log file
    if (enableLogging && !logPath.empty()) {
        cmd << " > " << logPath << " 2>&1";
    }

    return cmd.str();
}

std::string INDIServerConfig::validate() const {
    if (port <= 0 || port > 65535) {
        return "Invalid port number: " + std::to_string(port);
    }

    if (maxClients <= 0) {
        return "Invalid max clients: " + std::to_string(maxClients);
    }

    if (enableFifo && fifoPath.empty()) {
        return "FIFO enabled but path is empty";
    }

    if (startupTimeoutMs <= 0) {
        return "Invalid startup timeout";
    }

    return "";
}

std::string INDIServerConfig::getVerbosityFlags() const {
    switch (startMode) {
        case ServerStartMode::Normal:
            return "";
        case ServerStartMode::Verbose:
            return "-v";
        case ServerStartMode::VeryVerbose:
            return "-vv";
        case ServerStartMode::Debug:
            return "-vvv";
        default:
            return "-v";
    }
}

// ==================== INDIServerManager ====================

INDIServerManager::INDIServerManager(INDIServerConfig config)
    : config_(std::move(config)) {
    LOG_INFO("INDIServerManager created with port {}", config_.port);
}

INDIServerManager::~INDIServerManager() {
    stopHealthMonitor();
    if (isRunning()) {
        stop(true);
    }
}

INDIServerManager::INDIServerManager(INDIServerManager&& other) noexcept
    : config_(std::move(other.config_)),
      state_(other.state_.load()),
      pid_(other.pid_.load()),
      lastError_(std::move(other.lastError_)),
      startTime_(other.startTime_),
      restartCount_(other.restartCount_) {
    other.state_ = ServerState::Stopped;
    other.pid_ = -1;
}

INDIServerManager& INDIServerManager::operator=(
    INDIServerManager&& other) noexcept {
    if (this != &other) {
        stop(true);
        config_ = std::move(other.config_);
        state_ = other.state_.load();
        pid_ = other.pid_.load();
        lastError_ = std::move(other.lastError_);
        startTime_ = other.startTime_;
        restartCount_ = other.restartCount_;
        other.state_ = ServerState::Stopped;
        other.pid_ = -1;
    }
    return *this;
}

bool INDIServerManager::start() {
    std::lock_guard lock(mutex_);

    if (state_ == ServerState::Running) {
        LOG_WARN("Server already running");
        return true;
    }

    if (state_ == ServerState::Starting) {
        LOG_WARN("Server is already starting");
        return false;
    }

    // Validate configuration
    std::string validationError = config_.validate();
    if (!validationError.empty()) {
        setError("Configuration error: " + validationError);
        return false;
    }

    setState(ServerState::Starting, "Starting INDI server");

    // Create FIFO if enabled
    if (config_.enableFifo) {
        if (!createFifo()) {
            setError("Failed to create FIFO pipe");
            setState(ServerState::Error, lastError_);
            return false;
        }
    }

    // Build command
    std::string cmd = config_.buildCommandString();
    LOG_INFO("Starting INDI server with command: {}", cmd);

    // Start process
#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr,
                        nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        setError("Failed to create process: " + std::to_string(GetLastError()));
        setState(ServerState::Error, lastError_);
        return false;
    }

    pid_ = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    pid_t pid = fork();

    if (pid < 0) {
        setError("Fork failed: " + std::string(strerror(errno)));
        setState(ServerState::Error, lastError_);
        return false;
    }

    if (pid == 0) {
        // Child process
        // Create new session to become process group leader
        setsid();

        // Set environment variables
        for (const auto& [key, value] : config_.envVars) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Execute indiserver
        auto args = config_.buildCommandArgs();
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Redirect stdout/stderr to log file
        if (config_.enableLogging && !config_.logPath.empty()) {
            int logFd = open(config_.logPath.c_str(),
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (logFd >= 0) {
                dup2(logFd, STDOUT_FILENO);
                dup2(logFd, STDERR_FILENO);
                close(logFd);
            }
        }

        execvp(config_.binaryPath.c_str(), argv.data());

        // If exec fails
        _exit(127);
    }

    // Parent process
    pid_ = pid;
#endif

    LOG_INFO("INDI server process started with PID {}", pid_.load());

    // Wait for startup
    if (!waitForStartup()) {
        setError("Server failed to start within timeout");
        stop(true);
        setState(ServerState::Error, lastError_);
        return false;
    }

    startTime_ = std::chrono::steady_clock::now();
    setState(ServerState::Running, "Server started successfully");

    // Start health monitor if auto-restart is enabled
    if (config_.autoRestart) {
        startHealthMonitor();
    }

    return true;
}

bool INDIServerManager::stop(bool force) {
    std::lock_guard lock(mutex_);

    if (state_ == ServerState::Stopped) {
        return true;
    }

    if (state_ == ServerState::Stopping) {
        LOG_WARN("Server is already stopping");
        return false;
    }

    stopHealthMonitor();
    setState(ServerState::Stopping, "Stopping INDI server");

    pid_t currentPid = pid_.load();
    if (currentPid <= 0) {
        setState(ServerState::Stopped, "Server stopped");
        return true;
    }

    LOG_INFO("Stopping INDI server (PID: {})", currentPid);

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, currentPid);
    if (hProcess != nullptr) {
        if (!force) {
            // Try graceful shutdown first
            // Windows doesn't have SIGTERM equivalent, so we just wait
            if (WaitForSingleObject(hProcess, config_.shutdownTimeoutMs) ==
                WAIT_TIMEOUT) {
                TerminateProcess(hProcess, 0);
            }
        } else {
            TerminateProcess(hProcess, 0);
        }
        CloseHandle(hProcess);
    }
#else
    if (!force) {
        // Send SIGTERM for graceful shutdown
        if (kill(currentPid, SIGTERM) == 0) {
            if (!waitForShutdown()) {
                LOG_WARN("Graceful shutdown timed out, forcing kill");
                kill(currentPid, SIGKILL);
                waitpid(currentPid, nullptr, 0);
            }
        }
    } else {
        // Force kill
        kill(currentPid, SIGKILL);
        waitpid(currentPid, nullptr, 0);
    }
#endif

    pid_ = -1;

    // Clean up FIFO
    if (config_.enableFifo) {
        removeFifo();
    }

    setState(ServerState::Stopped, "Server stopped");
    return true;
}

bool INDIServerManager::restart() {
    LOG_INFO("Restarting INDI server");

    if (!stop(false)) {
        LOG_WARN("Failed to stop server gracefully, forcing");
        stop(true);
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.restartDelayMs));

    restartCount_++;
    return start();
}

bool INDIServerManager::isRunning() const {
    return state_ == ServerState::Running && isProcessAlive();
}

ServerState INDIServerManager::getState() const { return state_.load(); }

pid_t INDIServerManager::getPid() const { return pid_.load(); }

bool INDIServerManager::setConfig(const INDIServerConfig& config) {
    std::lock_guard lock(mutex_);

    if (state_ != ServerState::Stopped) {
        LOG_ERROR("Cannot change config while server is running");
        return false;
    }

    std::string validationError = config.validate();
    if (!validationError.empty()) {
        LOG_ERROR("Invalid configuration: {}", validationError);
        return false;
    }

    config_ = config;
    return true;
}

const INDIServerConfig& INDIServerManager::getConfig() const { return config_; }

const std::string& INDIServerManager::getFifoPath() const {
    return config_.fifoPath;
}

bool INDIServerManager::checkHealth() const {
    if (!isProcessAlive()) {
        return false;
    }

    // Additional health checks could be added here:
    // - Check if server is responding to connections
    // - Check FIFO is writable
    // - Check log file for errors

    return true;
}

std::optional<std::chrono::seconds> INDIServerManager::getUptime() const {
    if (state_ != ServerState::Running) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
}

const std::string& INDIServerManager::getLastError() const {
    return lastError_;
}

int INDIServerManager::getRestartCount() const { return restartCount_; }

void INDIServerManager::setEventCallback(ServerEventCallback callback) {
    std::lock_guard lock(mutex_);
    eventCallback_ = std::move(callback);
}

bool INDIServerManager::isInstalled(const std::string& binaryPath) {
    return atom::system::checkSoftwareInstalled(binaryPath);
}

std::string INDIServerManager::getVersion(const std::string& binaryPath) {
    try {
        std::string output =
            atom::system::executeCommand(binaryPath + " --version", false);
        // Parse version from output
        // INDI server typically outputs: "INDI Library: X.Y.Z"
        return output;
    } catch (...) {
        return "";
    }
}

int INDIServerManager::killExistingServers() {
    int killed = 0;

#ifdef _WIN32
    // Windows: Use taskkill
    try {
        atom::system::executeCommand("taskkill /F /IM indiserver.exe", false);
        killed++;
    } catch (...) {
    }
#else
    // Unix: Use pkill
    try {
        std::string result =
            atom::system::executeCommand("pkill -c indiserver", false);
        killed = std::stoi(result);
    } catch (...) {
        // pkill might fail if no processes found
    }
#endif

    return killed;
}

bool INDIServerManager::createFifo() {
#ifdef _WIN32
    // Windows doesn't support FIFO pipes in the same way
    // We could use named pipes, but for now just skip
    LOG_WARN("FIFO not supported on Windows");
    return true;
#else
    // Remove existing FIFO
    if (atom::io::isFileExists(config_.fifoPath)) {
        if (unlink(config_.fifoPath.c_str()) != 0) {
            LOG_ERROR("Failed to remove existing FIFO: {}", strerror(errno));
            return false;
        }
    }

    // Create new FIFO
    if (mkfifo(config_.fifoPath.c_str(), 0666) != 0) {
        LOG_ERROR("Failed to create FIFO: {}", strerror(errno));
        return false;
    }

    LOG_INFO("Created FIFO at {}", config_.fifoPath);
    return true;
#endif
}

void INDIServerManager::removeFifo() {
#ifndef _WIN32
    if (atom::io::isFileExists(config_.fifoPath)) {
        if (unlink(config_.fifoPath.c_str()) != 0) {
            LOG_WARN("Failed to remove FIFO: {}", strerror(errno));
        } else {
            LOG_INFO("Removed FIFO at {}", config_.fifoPath);
        }
    }
#endif
}

bool INDIServerManager::waitForStartup() {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config_.startupTimeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (isProcessAlive()) {
            // Additional check: try to connect to the port
            // For now, just check if process is alive
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (isProcessAlive()) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

bool INDIServerManager::waitForShutdown() {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config_.shutdownTimeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (!isProcessAlive()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

void INDIServerManager::setState(ServerState state,
                                 const std::string& message) {
    state_ = state;

    if (!message.empty()) {
        LOG_INFO("Server state: {} - {}", static_cast<int>(state), message);
    }

    if (eventCallback_) {
        eventCallback_(state, message);
    }
}

void INDIServerManager::setError(const std::string& error) {
    lastError_ = error;
    LOG_ERROR("Server error: {}", error);
}

bool INDIServerManager::isProcessAlive() const {
    pid_t currentPid = pid_.load();
    if (currentPid <= 0) {
        return false;
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, currentPid);
    if (hProcess == nullptr) {
        return false;
    }

    DWORD exitCode;
    bool alive =
        GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProcess);
    return alive;
#else
    // Check if process exists
    return kill(currentPid, 0) == 0;
#endif
}

void INDIServerManager::startHealthMonitor() {
    if (healthMonitorRunning_) {
        return;
    }

    healthMonitorRunning_ = true;
    healthMonitorThread_ = std::make_unique<std::thread>([this]() {
        int failedChecks = 0;

        while (healthMonitorRunning_) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.healthCheckIntervalMs));

            if (!healthMonitorRunning_) {
                break;
            }

            if (!checkHealth()) {
                failedChecks++;
                LOG_WARN("Health check failed ({}/{})", failedChecks,
                         config_.maxRestartAttempts);

                if (failedChecks >= config_.maxRestartAttempts) {
                    LOG_ERROR("Max restart attempts reached, giving up");
                    healthMonitorRunning_ = false;
                    setState(ServerState::Error,
                             "Server crashed and max restarts exceeded");
                    break;
                }

                // Attempt restart
                LOG_INFO("Attempting automatic restart");
                restart();
                failedChecks = 0;
            } else {
                failedChecks = 0;
            }
        }
    });
}

void INDIServerManager::stopHealthMonitor() {
    healthMonitorRunning_ = false;

    if (healthMonitorThread_ && healthMonitorThread_->joinable()) {
        healthMonitorThread_->join();
    }
    healthMonitorThread_.reset();
}

}  // namespace lithium::client::indi
