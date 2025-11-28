/*
 * fifo_channel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: FIFO Channel - Communication channel for INDI server control

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_FIFO_CHANNEL_HPP
#define LITHIUM_CLIENT_INDI_FIFO_CHANNEL_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace lithium::client::indi {

/**
 * @brief FIFO command types for INDI server
 */
enum class FifoCommandType {
    Start,      ///< Start a driver: "start <driver> [-s skeleton]"
    Stop,       ///< Stop a driver: "stop <driver>"
    Restart,    ///< Restart a driver: (stop + start)
    Custom      ///< Custom command
};

/**
 * @brief FIFO command structure
 */
struct FifoCommand {
    FifoCommandType type{FifoCommandType::Custom};
    std::string driverName;
    std::string driverBinary;
    std::string skeletonPath;
    std::string customCommand;
    int priority{0};  ///< Higher priority commands are sent first
    
    /**
     * @brief Build command string for FIFO
     * @return Command string to write to FIFO
     */
    [[nodiscard]] std::string build() const;
    
    /**
     * @brief Create a start driver command
     * @param binary Driver binary name
     * @param skeleton Optional skeleton file path
     * @return FifoCommand
     */
    static FifoCommand startDriver(const std::string& binary, 
                                   const std::string& skeleton = "");
    
    /**
     * @brief Create a stop driver command
     * @param binary Driver binary name
     * @return FifoCommand
     */
    static FifoCommand stopDriver(const std::string& binary);
    
    /**
     * @brief Create a custom command
     * @param command Custom command string
     * @return FifoCommand
     */
    static FifoCommand custom(const std::string& command);
};

/**
 * @brief Result of a FIFO operation
 */
struct FifoResult {
    bool success{false};
    std::string errorMessage;
    std::chrono::milliseconds duration{0};
    
    static FifoResult ok() { return {true, "", {}}; }
    static FifoResult error(const std::string& msg) { return {false, msg, {}}; }
};

/**
 * @brief FIFO Channel configuration
 */
struct FifoChannelConfig {
    std::string fifoPath{"/tmp/indi.fifo"};
    int writeTimeoutMs{5000};           ///< Timeout for write operations
    int retryCount{3};                  ///< Number of retries on failure
    int retryDelayMs{100};              ///< Delay between retries
    bool nonBlocking{true};             ///< Use non-blocking I/O
    bool queueCommands{true};           ///< Queue commands if FIFO busy
    int maxQueueSize{100};              ///< Maximum command queue size
};

/**
 * @brief Callback for command completion
 */
using FifoCommandCallback = std::function<void(const FifoCommand&, const FifoResult&)>;

/**
 * @brief FIFO Channel for INDI server communication
 * 
 * Provides reliable communication with INDI server through FIFO pipe:
 * - Thread-safe command sending
 * - Automatic retry on failure
 * - Command queuing
 * - Async command execution
 */
class FifoChannel {
public:
    /**
     * @brief Construct FIFO channel
     * @param config Channel configuration
     */
    explicit FifoChannel(FifoChannelConfig config = {});
    
    /**
     * @brief Destructor
     */
    ~FifoChannel();
    
    // Non-copyable
    FifoChannel(const FifoChannel&) = delete;
    FifoChannel& operator=(const FifoChannel&) = delete;
    
    // ==================== Configuration ====================
    
    /**
     * @brief Set FIFO path
     * @param path Path to FIFO
     */
    void setFifoPath(const std::string& path);
    
    /**
     * @brief Get FIFO path
     * @return FIFO path
     */
    [[nodiscard]] const std::string& getFifoPath() const;
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const FifoChannelConfig& config);
    
    /**
     * @brief Get configuration
     * @return Configuration reference
     */
    [[nodiscard]] const FifoChannelConfig& getConfig() const;
    
    // ==================== FIFO Operations ====================
    
    /**
     * @brief Check if FIFO exists and is writable
     * @return true if FIFO is available
     */
    [[nodiscard]] bool isAvailable() const;
    
    /**
     * @brief Open FIFO for writing
     * @return true if opened successfully
     */
    bool open();
    
    /**
     * @brief Close FIFO
     */
    void close();
    
    /**
     * @brief Check if FIFO is open
     * @return true if open
     */
    [[nodiscard]] bool isOpen() const;
    
    // ==================== Command Sending ====================
    
    /**
     * @brief Send command synchronously
     * @param command Command to send
     * @return Result of operation
     */
    FifoResult send(const FifoCommand& command);
    
    /**
     * @brief Send command asynchronously
     * @param command Command to send
     * @param callback Completion callback
     */
    void sendAsync(const FifoCommand& command, FifoCommandCallback callback = nullptr);
    
    /**
     * @brief Send raw string to FIFO
     * @param data String to send
     * @return Result of operation
     */
    FifoResult sendRaw(const std::string& data);
    
    // ==================== Driver Commands ====================
    
    /**
     * @brief Start a driver
     * @param binary Driver binary name
     * @param skeleton Optional skeleton file
     * @return Result of operation
     */
    FifoResult startDriver(const std::string& binary, 
                          const std::string& skeleton = "");
    
    /**
     * @brief Stop a driver
     * @param binary Driver binary name
     * @return Result of operation
     */
    FifoResult stopDriver(const std::string& binary);
    
    /**
     * @brief Restart a driver
     * @param binary Driver binary name
     * @param skeleton Optional skeleton file
     * @return Result of operation
     */
    FifoResult restartDriver(const std::string& binary,
                            const std::string& skeleton = "");
    
    // ==================== Queue Management ====================
    
    /**
     * @brief Get number of pending commands
     * @return Queue size
     */
    [[nodiscard]] size_t getPendingCount() const;
    
    /**
     * @brief Clear command queue
     */
    void clearQueue();
    
    /**
     * @brief Wait for all pending commands to complete
     * @param timeoutMs Timeout in milliseconds
     * @return true if all completed within timeout
     */
    bool waitForPending(int timeoutMs = 5000);
    
    // ==================== Statistics ====================
    
    /**
     * @brief Get total commands sent
     * @return Command count
     */
    [[nodiscard]] uint64_t getTotalCommandsSent() const;
    
    /**
     * @brief Get total errors
     * @return Error count
     */
    [[nodiscard]] uint64_t getTotalErrors() const;
    
    /**
     * @brief Get last error message
     * @return Error message
     */
    [[nodiscard]] const std::string& getLastError() const;

private:
    /**
     * @brief Write data to FIFO with retry
     * @param data Data to write
     * @return Result of operation
     */
    FifoResult writeToFifo(const std::string& data);
    
    /**
     * @brief Process command queue
     */
    void processQueue();
    
    /**
     * @brief Worker thread function
     */
    void workerThread();
    
    FifoChannelConfig config_;
    int fifoFd_{-1};
    mutable std::mutex mutex_;
    mutable std::mutex queueMutex_;
    
    std::queue<std::pair<FifoCommand, FifoCommandCallback>> commandQueue_;
    std::atomic<bool> workerRunning_{false};
    std::unique_ptr<std::thread> workerThread_;
    
    std::atomic<uint64_t> totalCommandsSent_{0};
    std::atomic<uint64_t> totalErrors_{0};
    std::string lastError_;
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_FIFO_CHANNEL_HPP
