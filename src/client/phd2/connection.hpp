/*
 * connection.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 TCP connection management with modern C++ features

*************************************************/

#pragma once

#include "event_handler.hpp"
#include "types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

namespace phd2 {

/**
 * @brief Connection configuration
 */
struct ConnectionConfig {
    std::string host{"localhost"};
    int port{4400};
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds rpcTimeout{10000};
    size_t receiveBufferSize{65536};
    bool autoReconnect{false};
    int maxReconnectAttempts{3};
    std::chrono::milliseconds reconnectDelay{1000};
};

/**
 * @brief Connection state
 */
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

/**
 * @brief Manages the TCP connection to PHD2 using libcurl
 */
class Connection {
public:
    using EventCallback = std::function<void(const Event&)>;
    using ErrorCallback = std::function<void(std::string_view)>;

    /**
     * @brief Construct a new Connection object
     * @param config Connection configuration
     * @param eventHandler Optional event handler
     */
    explicit Connection(
        ConnectionConfig config = {},
        std::shared_ptr<EventHandler> eventHandler = nullptr);

    /**
     * @brief Construct with host/port (backward compatible)
     */
    Connection(std::string host, int port,
               std::shared_ptr<EventHandler> eventHandler = nullptr);

    /**
     * @brief Destroy the Connection object
     */
    ~Connection();

    // Non-copyable, movable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    /**
     * @brief Connect to PHD2
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection was successful
     */
    [[nodiscard]] auto connect(int timeoutMs = 5000) -> bool;

    /**
     * @brief Connect with std::chrono duration
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto connect(std::chrono::duration<Rep, Period> timeout) -> bool {
        return connect(static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
    }

    /**
     * @brief Disconnect from PHD2
     */
    void disconnect();

    /**
     * @brief Check if currently connected
     */
    [[nodiscard]] auto isConnected() const noexcept -> bool;

    /**
     * @brief Get current connection state
     */
    [[nodiscard]] auto getState() const noexcept -> ConnectionState;

    /**
     * @brief Send an RPC method call to PHD2
     * @param method The method name
     * @param params The parameters (as a JSON object)
     * @param timeoutMs Timeout for the response
     * @return RpcResponse The response from PHD2
     */
    [[nodiscard]] auto sendRpc(
        std::string_view method,
        const json& params = json::object(),
        int timeoutMs = 10000) -> RpcResponse;

    /**
     * @brief Send RPC with std::chrono duration
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto sendRpc(
        std::string_view method,
        const json& params,
        std::chrono::duration<Rep, Period> timeout) -> RpcResponse {
        return sendRpc(method, params, static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
    }

    /**
     * @brief Send RPC asynchronously
     */
    [[nodiscard]] auto sendRpcAsync(
        std::string_view method,
        const json& params = json::object()) -> std::future<RpcResponse>;

    /**
     * @brief Set event handler
     */
    void setEventHandler(std::shared_ptr<EventHandler> handler);

    /**
     * @brief Set event callback (alternative to handler)
     */
    void setEventCallback(EventCallback callback);

    /**
     * @brief Set error callback
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Get connection configuration
     */
    [[nodiscard]] auto getConfig() const noexcept -> const ConnectionConfig&;

    /**
     * @brief Get connection statistics
     */
    struct Stats {
        size_t messagesSent{0};
        size_t messagesReceived{0};
        size_t bytesReceived{0};
        size_t errors{0};
        std::chrono::steady_clock::time_point connectedSince{};
    };
    [[nodiscard]] auto getStats() const noexcept -> Stats;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief RAII wrapper for CURL global initialization
 */
class CurlGlobalInit {
public:
    CurlGlobalInit() {
        if (refCount_++ == 0) {
            curl_global_init(CURL_GLOBAL_ALL);
            spdlog::debug("CURL global initialized");
        }
    }

    ~CurlGlobalInit() {
        if (--refCount_ == 0) {
            curl_global_cleanup();
            spdlog::debug("CURL global cleanup");
        }
    }

    CurlGlobalInit(const CurlGlobalInit&) = delete;
    CurlGlobalInit& operator=(const CurlGlobalInit&) = delete;

private:
    static inline std::atomic<int> refCount_{0};
};

}  // namespace phd2
