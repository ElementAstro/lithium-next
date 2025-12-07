/*
 * server_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Server Configuration

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_SERVER_CONFIG_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_SERVER_CONFIG_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::client::indi_manager {

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
struct ServerConfig {
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
    int maxClients{10};  ///< Maximum number of clients
    ServerStartMode startMode{ServerStartMode::Verbose};

    // FIFO options
    bool enableFifo{true};     ///< Enable FIFO control
    bool enableLogging{true};  ///< Enable logging

    // Timeouts
    int startupTimeoutMs{5000};   ///< Startup timeout
    int shutdownTimeoutMs{3000};  ///< Shutdown timeout
    int restartDelayMs{1000};     ///< Delay between restart

    // Health monitoring
    bool autoRestart{false};          ///< Auto-restart on crash
    int healthCheckIntervalMs{5000};  ///< Health check interval
    int maxRestartAttempts{3};        ///< Max restart attempts

    // Environment variables
    std::unordered_map<std::string, std::string> envVars;

    /**
     * @brief Build command line arguments
     * @return Vector of command line arguments
     */
    [[nodiscard]] std::vector<std::string> buildCommandArgs() const;

    /**
     * @brief Build full command string
     * @return Command string
     */
    [[nodiscard]] std::string buildCommandString() const;

    /**
     * @brief Validate configuration
     * @return Empty string if valid, error message otherwise
     */
    [[nodiscard]] std::string validate() const;

    /**
     * @brief Get verbosity flags
     * @return Verbosity flag string
     */
    [[nodiscard]] std::string getVerbosityFlags() const;
};

// Backward compatibility alias
using INDIServerConfig = ServerConfig;

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_SERVER_CONFIG_HPP
