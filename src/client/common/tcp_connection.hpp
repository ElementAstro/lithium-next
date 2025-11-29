/*
 * tcp_connection.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Generic TCP connection utilities for client components

*************************************************/

#pragma once

#include "atom/type/expected.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace lithium::client {

/**
 * @brief TCP connection state
 */
enum class TcpConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

/**
 * @brief TCP connection error types
 */
enum class TcpError {
    None,
    ConnectionRefused,
    Timeout,
    HostNotFound,
    NetworkError,
    Disconnected,
    SendFailed,
    ReceiveFailed
};

/**
 * @brief TCP connection configuration
 */
struct TcpConfig {
    std::string host{"localhost"};
    int port{0};
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{10000};
    std::chrono::milliseconds writeTimeout{5000};
    size_t receiveBufferSize{65536};
    bool autoReconnect{false};
    int maxReconnectAttempts{3};
    std::chrono::milliseconds reconnectDelay{1000};
    bool keepAlive{true};
    bool noDelay{true};  // TCP_NODELAY
};

/**
 * @brief Connection statistics
 */
struct TcpStats {
    size_t bytesSent{0};
    size_t bytesReceived{0};
    size_t messagesSent{0};
    size_t messagesReceived{0};
    size_t errors{0};
    size_t reconnects{0};
    std::chrono::steady_clock::time_point connectedSince{};
    std::chrono::steady_clock::time_point lastActivity{};
};

/**
 * @brief Callback types
 */
using DataCallback = std::function<void(std::string_view data)>;
using ErrorCallback = std::function<void(TcpError error, std::string_view message)>;
using StateCallback = std::function<void(TcpConnectionState oldState,
                                         TcpConnectionState newState)>;

/**
 * @brief Generic TCP connection class with modern C++ features
 *
 * Provides a reusable TCP connection implementation that can be used
 * by various client components (PHD2, INDI, etc.)
 */
class TcpConnection {
public:
    /**
     * @brief Construct with configuration
     */
    explicit TcpConnection(TcpConfig config = {});

    /**
     * @brief Construct with host and port
     */
    TcpConnection(std::string host, int port);

    /**
     * @brief Destructor - ensures clean disconnect
     */
    ~TcpConnection();

    // Non-copyable, movable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) noexcept;
    TcpConnection& operator=(TcpConnection&&) noexcept;

    // ==================== Connection Management ====================

    /**
     * @brief Connect to the configured host:port
     * @return true if connection successful
     */
    [[nodiscard]] auto connect() -> bool;

    /**
     * @brief Connect with timeout
     */
    [[nodiscard]] auto connect(std::chrono::milliseconds timeout) -> bool;

    /**
     * @brief Disconnect from server
     */
    void disconnect();

    /**
     * @brief Check if currently connected
     */
    [[nodiscard]] auto isConnected() const noexcept -> bool;

    /**
     * @brief Get current connection state
     */
    [[nodiscard]] auto getState() const noexcept -> TcpConnectionState;

    /**
     * @brief Get last error
     */
    [[nodiscard]] auto getLastError() const noexcept -> TcpError;

    // ==================== Data Transfer ====================

    /**
     * @brief Send data synchronously
     * @param data Data to send
     * @return Number of bytes sent or error
     */
    [[nodiscard]] auto send(std::string_view data)
        -> atom::type::expected<size_t, TcpError>;

    /**
     * @brief Send line (appends newline)
     */
    [[nodiscard]] auto sendLine(std::string_view line)
        -> atom::type::expected<size_t, TcpError>;

    /**
     * @brief Receive data synchronously
     * @param maxBytes Maximum bytes to receive
     * @return Received data or error
     */
    [[nodiscard]] auto receive(size_t maxBytes = 0)
        -> atom::type::expected<std::string, TcpError>;

    /**
     * @brief Receive until delimiter
     * @param delimiter Delimiter string
     * @param timeout Timeout for operation
     * @return Received data or error
     */
    [[nodiscard]] auto receiveUntil(
        std::string_view delimiter,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> atom::type::expected<std::string, TcpError>;

    /**
     * @brief Receive a complete line (until \n)
     */
    [[nodiscard]] auto receiveLine(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> atom::type::expected<std::string, TcpError>;

    // ==================== Async Operations ====================

    /**
     * @brief Start async receive loop
     * @param callback Callback for received data
     */
    void startAsyncReceive(DataCallback callback);

    /**
     * @brief Stop async receive loop
     */
    void stopAsyncReceive();

    /**
     * @brief Check if async receive is active
     */
    [[nodiscard]] auto isAsyncReceiveActive() const noexcept -> bool;

    // ==================== Callbacks ====================

    /**
     * @brief Set error callback
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Set state change callback
     */
    void setStateCallback(StateCallback callback);

    // ==================== Configuration ====================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] auto getConfig() const noexcept -> const TcpConfig&;

    /**
     * @brief Update configuration (must be disconnected)
     */
    auto setConfig(const TcpConfig& config) -> bool;

    /**
     * @brief Get connection statistics
     */
    [[nodiscard]] auto getStats() const noexcept -> TcpStats;

    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief RAII connection guard
 */
class TcpConnectionGuard {
public:
    explicit TcpConnectionGuard(TcpConnection& conn);
    ~TcpConnectionGuard();

    TcpConnectionGuard(const TcpConnectionGuard&) = delete;
    TcpConnectionGuard& operator=(const TcpConnectionGuard&) = delete;

    void release() noexcept;

private:
    TcpConnection* conn_;
    bool released_{false};
};

/**
 * @brief Line-based protocol helper
 *
 * Wraps TcpConnection for line-based protocols (JSON-RPC, etc.)
 */
class LineProtocol {
public:
    explicit LineProtocol(TcpConnection& connection);

    /**
     * @brief Send a line and wait for response
     */
    [[nodiscard]] auto sendAndReceive(
        std::string_view request,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> atom::type::expected<std::string, TcpError>;

    /**
     * @brief Set line delimiter (default: \n)
     */
    void setDelimiter(std::string_view delimiter);

private:
    TcpConnection& conn_;
    std::string delimiter_{"\n"};
};

}  // namespace lithium::client
