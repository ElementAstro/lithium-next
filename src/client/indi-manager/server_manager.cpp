/*
 * server_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Server Manager implementation

**************************************************/

#include "server_manager.hpp"

#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lithium::client::indi_manager {

// ==================== ServerConfig Implementation ====================

std::vector<std::string> ServerConfig::buildCommandArgs() const {
    std::vector<std::string> args;
    args.push_back(binaryPath);

    // Add verbosity flags
    std::string verbosity = getVerbosityFlags();
    if (!verbosity.empty()) {
        args.push_back(verbosity);
    }

    // Port
    args.push_back("-p");
    args.push_back(std::to_string(port));

    // FIFO
    if (enableFifo && !fifoPath.empty()) {
        args.push_back("-f");
        args.push_back(fifoPath);
    }

    // Max clients
    if (maxClients > 0) {
        args.push_back("-m");
        args.push_back(std::to_string(maxClients));
    }

    return args;
}

std::string ServerConfig::buildCommandString() const {
    auto args = buildCommandArgs();
    std::stringstream ss;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            ss << " ";
        ss << args[i];
    }
    return ss.str();
}

std::string ServerConfig::validate() const {
    if (port <= 0 || port > 65535) {
        return "Invalid port number";
    }
    if (binaryPath.empty()) {
        return "Binary path is empty";
    }
    return "";
}

std::string ServerConfig::getVerbosityFlags() const {
    switch (startMode) {
        case ServerStartMode::Verbose:
            return "-v";
        case ServerStartMode::VeryVerbose:
            return "-vv";
        case ServerStartMode::Debug:
            return "-vvv";
        default:
            return "";
    }
}

// ==================== ServerManager Implementation ====================

ServerManager::ServerManager(ServerConfig config)
    : config_(std::move(config)) {
    LOG_INFO("ServerManager created with port: {}", config_.port);
}

ServerManager::~ServerManager() {
    if (isRunning()) {
        stop(true);
    }
    stopHealthMonitor();
}

ServerManager::ServerManager(ServerManager&& other) noexcept
    : config_(std::move(other.config_)),
      state_(other.state_.load()),
      pid_(other.pid_.load()),
      lastError_(std::move(other.lastError_)),
      startTime_(other.startTime_),
      restartCount_(other.restartCount_),
      eventCallback_(std::move(other.eventCallback_)),
      healthMonitorRunning_(other.healthMonitorRunning_.load()),
      healthMonitorThread_(std::move(other.healthMonitorThread_)) {
    other.state_ = ServerState::Stopped;
    other.pid_ = -1;
    other.healthMonitorRunning_ = false;
}

ServerManager& ServerManager::operator=(ServerManager&& other) noexcept {
    if (this != &other) {
        if (isRunning()) {
            stop(true);
        }
        stopHealthMonitor();

        config_ = std::move(other.config_);
        state_ = other.state_.load();
        pid_ = other.pid_.load();
        lastError_ = std::move(other.lastError_);
        startTime_ = other.startTime_;
        restartCount_ = other.restartCount_;
        eventCallback_ = std::move(other.eventCallback_);
        healthMonitorRunning_ = other.healthMonitorRunning_.load();
        healthMonitorThread_ = std::move(other.healthMonitorThread_);

        other.state_ = ServerState::Stopped;
        other.pid_ = -1;
        other.healthMonitorRunning_ = false;
    }
    return *this;
}

bool ServerManager::start() {
    std::lock_guard lock(mutex_);

    if (state_ == ServerState::Running) {
        LOG_INFO("Server already running");
        return true;
    }

    if (state_ == ServerState::Starting) {
        LOG_WARN("Server is already starting");
        return false;
    }

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
            int logFd =
                open(config_.logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (logFd >= 0) {
                dup2(logFd, STDOUT_FILENO);
                dup2(logFd, STDERR_FILENO);
                close(logFd);
            }
        }

        execvp(config_.binaryPath.c_str(), argv.data());
        _exit(127);
    }

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

bool ServerManager::stop(bool force) {
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
        if (kill(currentPid, SIGTERM) == 0) {
            if (!waitForShutdown()) {
                LOG_WARN("Graceful shutdown timed out, forcing kill");
                kill(currentPid, SIGKILL);
                waitpid(currentPid, nullptr, 0);
            }
        }
    } else {
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

bool ServerManager::restart() {
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

bool ServerManager::isRunning() const {
    return state_ == ServerState::Running && isProcessAlive();
}

ServerState ServerManager::getState() const { return state_.load(); }

pid_t ServerManager::getPid() const { return pid_.load(); }

bool ServerManager::setConfig(const ServerConfig& config) {
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

const ServerConfig& ServerManager::getConfig() const { return config_; }

const std::string& ServerManager::getFifoPath() const {
    return config_.fifoPath;
}

bool ServerManager::checkHealth() const {
    if (!isProcessAlive()) {
        return false;
    }
    return true;
}

std::optional<std::chrono::seconds> ServerManager::getUptime() const {
    if (state_ != ServerState::Running) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
}

const std::string& ServerManager::getLastError() const { return lastError_; }

int ServerManager::getRestartCount() const { return restartCount_; }

void ServerManager::setEventCallback(ServerEventCallback callback) {
    std::lock_guard lock(mutex_);
    eventCallback_ = std::move(callback);
}

bool ServerManager::isInstalled(const std::string& binaryPath) {
    return atom::system::checkSoftwareInstalled(binaryPath);
}

std::string ServerManager::getVersion(const std::string& binaryPath) {
    try {
        std::string output =
            atom::system::executeCommand(binaryPath + " --version", false);
        return output;
    } catch (...) {
        return "";
    }
}

int ServerManager::killExistingServers() {
    int killed = 0;

#ifdef _WIN32
    try {
        atom::system::executeCommand("taskkill /F /IM indiserver.exe", false);
        killed++;
    } catch (...) {
    }
#else
    try {
        std::string result =
            atom::system::executeCommand("pkill -c indiserver", false);
        killed = std::stoi(result);
    } catch (...) {
    }
#endif

    return killed;
}

bool ServerManager::createFifo() {
#ifdef _WIN32
    LOG_WARN("FIFO not supported on Windows");
    return true;
#else
    if (atom::io::isFileExists(config_.fifoPath)) {
        if (unlink(config_.fifoPath.c_str()) != 0) {
            LOG_ERROR("Failed to remove existing FIFO: {}", strerror(errno));
            return false;
        }
    }

    if (mkfifo(config_.fifoPath.c_str(), 0666) != 0) {
        LOG_ERROR("Failed to create FIFO: {}", strerror(errno));
        return false;
    }

    LOG_INFO("Created FIFO at {}", config_.fifoPath);
    return true;
#endif
}

void ServerManager::removeFifo() {
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

bool ServerManager::waitForStartup() {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config_.startupTimeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (isProcessAlive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (isProcessAlive()) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

bool ServerManager::waitForShutdown() {
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

void ServerManager::setState(ServerState state, const std::string& message) {
    state_ = state;

    if (!message.empty()) {
        LOG_INFO("Server state: {} - {}", static_cast<int>(state), message);
    }

    if (eventCallback_) {
        eventCallback_(state, message);
    }
}

void ServerManager::setError(const std::string& error) {
    lastError_ = error;
    LOG_ERROR("Server error: {}", error);
}

bool ServerManager::isProcessAlive() const {
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
    return kill(currentPid, 0) == 0;
#endif
}

void ServerManager::startHealthMonitor() {
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

                LOG_INFO("Attempting automatic restart");
                restart();
                failedChecks = 0;
            } else {
                failedChecks = 0;
            }
        }
    });
}

void ServerManager::stopHealthMonitor() {
    healthMonitorRunning_ = false;

    if (healthMonitorThread_ && healthMonitorThread_->joinable()) {
        healthMonitorThread_->join();
    }
    healthMonitorThread_.reset();
}

}  // namespace lithium::client::indi_manager
