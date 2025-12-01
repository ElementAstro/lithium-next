/*
 * server_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Server Manager - Enhanced server lifecycle management

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_SERVER_MANAGER_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_SERVER_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "server_config.hpp"

namespace lithium::client::indi_manager {

/**
 * @brief Server event callback type
 */
using ServerEventCallback =
    std::function<void(ServerState, const std::string&)>;

/**
 * @brief INDI Server Manager
 *
 * Manages the lifecycle of an INDI server process with:
 * - Configurable startup options
 * - FIFO-based control
 * - Health monitoring
 * - Auto-restart capability
 * - Graceful shutdown
 */
class ServerManager {
public:
    /**
     * @brief Construct server manager with configuration
     * @param config Server configuration
     */
    explicit ServerManager(ServerConfig config = {});

    /**
     * @brief Destructor - ensures server is stopped
     */
    ~ServerManager();

    // Non-copyable, movable
    ServerManager(const ServerManager&) = delete;
    ServerManager& operator=(const ServerManager&) = delete;
    ServerManager(ServerManager&&) noexcept;
    ServerManager& operator=(ServerManager&&) noexcept;

    // ==================== Lifecycle ====================

    /**
     * @brief Start the INDI server
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the INDI server
     * @param force Force kill if graceful shutdown fails
     * @return true if stopped successfully
     */
    bool stop(bool force = false);

    /**
     * @brief Restart the server
     * @return true if restarted successfully
     */
    bool restart();

    /**
     * @brief Check if server is running
     * @return true if running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get current server state
     * @return Server state
     */
    [[nodiscard]] ServerState getState() const;

    /**
     * @brief Get server process ID
     * @return PID or -1 if not running
     */
    [[nodiscard]] pid_t getPid() const;

    // ==================== Configuration ====================

    /**
     * @brief Update configuration (only when stopped)
     * @param config New configuration
     * @return true if updated successfully
     */
    bool setConfig(const ServerConfig& config);

    /**
     * @brief Get current configuration
     * @return Configuration reference
     */
    [[nodiscard]] const ServerConfig& getConfig() const;

    /**
     * @brief Get FIFO path
     * @return FIFO path string
     */
    [[nodiscard]] const std::string& getFifoPath() const;

    // ==================== Health & Monitoring ====================

    /**
     * @brief Check server health
     * @return true if healthy
     */
    [[nodiscard]] bool checkHealth() const;

    /**
     * @brief Get server uptime
     * @return Uptime duration or nullopt if not running
     */
    [[nodiscard]] std::optional<std::chrono::seconds> getUptime() const;

    /**
     * @brief Get last error message
     * @return Error message
     */
    [[nodiscard]] const std::string& getLastError() const;

    /**
     * @brief Get restart count
     * @return Number of restarts since creation
     */
    [[nodiscard]] int getRestartCount() const;

    // ==================== Events ====================

    /**
     * @brief Set event callback
     * @param callback Callback function
     */
    void setEventCallback(ServerEventCallback callback);

    // ==================== Static Utilities ====================

    /**
     * @brief Check if indiserver is installed
     * @param binaryPath Path to check (default: "indiserver")
     * @return true if installed
     */
    static bool isInstalled(const std::string& binaryPath = "indiserver");

    /**
     * @brief Get indiserver version
     * @param binaryPath Path to binary
     * @return Version string or empty if not available
     */
    static std::string getVersion(const std::string& binaryPath = "indiserver");

    /**
     * @brief Kill any existing indiserver processes
     * @return Number of processes killed
     */
    static int killExistingServers();

private:
    /**
     * @brief Create FIFO pipe
     * @return true if created successfully
     */
    bool createFifo();

    /**
     * @brief Remove FIFO pipe
     */
    void removeFifo();

    /**
     * @brief Wait for server to start
     * @return true if started within timeout
     */
    bool waitForStartup();

    /**
     * @brief Wait for server to stop
     * @return true if stopped within timeout
     */
    bool waitForShutdown();

    /**
     * @brief Set server state and emit event
     * @param state New state
     * @param message Optional message
     */
    void setState(ServerState state, const std::string& message = "");

    /**
     * @brief Set error state
     * @param error Error message
     */
    void setError(const std::string& error);

    /**
     * @brief Check if process is alive
     * @return true if alive
     */
    bool isProcessAlive() const;

    /**
     * @brief Start health monitoring thread
     */
    void startHealthMonitor();

    /**
     * @brief Stop health monitoring thread
     */
    void stopHealthMonitor();

    ServerConfig config_;
    std::atomic<ServerState> state_{ServerState::Stopped};
    std::atomic<pid_t> pid_{-1};
    std::string lastError_;
    std::chrono::steady_clock::time_point startTime_;
    int restartCount_{0};

    mutable std::mutex mutex_;
    ServerEventCallback eventCallback_;

    std::atomic<bool> healthMonitorRunning_{false};
    std::unique_ptr<std::thread> healthMonitorThread_;
};

// Backward compatibility alias
using INDIServerManager = ServerManager;

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_SERVER_MANAGER_HPP
