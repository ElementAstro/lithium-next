/*
 * server_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Server Manager - Enhanced server lifecycle management

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_SERVER_MANAGER_HPP
#define LITHIUM_CLIENT_INDI_SERVER_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lithium::client::indi {

/**
 * @brief Server startup mode
 */
enum class ServerStartMode {
    Normal,       ///< Normal startup
    Verbose,      ///< Verbose logging (-v)
    VeryVerbose,  ///< Very verbose logging (-vv)
    Debug         ///< Debug mode (-vvv)
};

/**
 * @brief Server state
 */
enum class ServerState {
    Stopped,   ///< Server is not running
    Starting,  ///< Server is starting up
    Running,   ///< Server is running normally
    Stopping,  ///< Server is shutting down
    Error      ///< Server encountered an error
};

/**
 * @brief INDI Server configuration
 */
struct INDIServerConfig {
    // Network settings
    std::string host{"localhost"};
    int port{7624};

    // Paths
    std::string binaryPath{"indiserver"};        ///< Path to indiserver binary
    std::string fifoPath{"/tmp/indi.fifo"};      ///< FIFO pipe path
    std::string logPath{"/tmp/indiserver.log"};  ///< Log file path
    std::string configDir;                       ///< Configuration directory
    std::string dataDir{"/usr/share/indi"};      ///< Data directory

    // Server options
    ServerStartMode startMode{ServerStartMode::Verbose};
    int maxClients{100};        ///< Maximum concurrent clients (-m)
    int bufferSize{512};        ///< Buffer size in KB
    bool enableFifo{true};      ///< Enable FIFO control
    bool enableLogging{true};   ///< Enable log file
    bool autoRestart{false};    ///< Auto restart on crash
    int restartDelayMs{1000};   ///< Delay before restart
    int maxRestartAttempts{3};  ///< Maximum restart attempts

    // Timeouts
    int startupTimeoutMs{5000};       ///< Startup timeout
    int shutdownTimeoutMs{3000};      ///< Shutdown timeout
    int healthCheckIntervalMs{1000};  ///< Health check interval

    // Environment variables
    std::unordered_map<std::string, std::string> envVars;

    /**
     * @brief Build command line arguments
     * @return Vector of command line arguments
     */
    [[nodiscard]] std::vector<std::string> buildCommandArgs() const;

    /**
     * @brief Build full command string
     * @return Complete command string
     */
    [[nodiscard]] std::string buildCommandString() const;

    /**
     * @brief Validate configuration
     * @return Error message if invalid, empty if valid
     */
    [[nodiscard]] std::string validate() const;

    /**
     * @brief Get verbosity flags based on start mode
     * @return Verbosity flag string
     */
    [[nodiscard]] std::string getVerbosityFlags() const;
};

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
class INDIServerManager {
public:
    /**
     * @brief Construct server manager with configuration
     * @param config Server configuration
     */
    explicit INDIServerManager(INDIServerConfig config = {});

    /**
     * @brief Destructor - ensures server is stopped
     */
    ~INDIServerManager();

    // Non-copyable, movable
    INDIServerManager(const INDIServerManager&) = delete;
    INDIServerManager& operator=(const INDIServerManager&) = delete;
    INDIServerManager(INDIServerManager&&) noexcept;
    INDIServerManager& operator=(INDIServerManager&&) noexcept;

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
    bool setConfig(const INDIServerConfig& config);

    /**
     * @brief Get current configuration
     * @return Configuration reference
     */
    [[nodiscard]] const INDIServerConfig& getConfig() const;

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

    INDIServerConfig config_;
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

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_SERVER_MANAGER_HPP
