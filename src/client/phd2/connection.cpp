/*
 * connection.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 TCP connection implementation

*************************************************/

#include "connection.hpp"
#include "exceptions.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>
#include <ranges>
#include <sstream>

namespace phd2 {

// ============================================================================
// Connection Implementation
// ============================================================================

class Connection::Impl {
public:
    Impl(ConnectionConfig config, std::shared_ptr<EventHandler> eventHandler)
        : config_(std::move(config)), eventHandler_(std::move(eventHandler)) {
        curlInit_ = std::make_unique<CurlGlobalInit>();
    }

    ~Impl() { disconnect(); }

    auto connect(int timeoutMs) -> bool {
        std::lock_guard lock(mutex_);

        if (state_ == ConnectionState::Connected) {
            return true;
        }

        state_ = ConnectionState::Connecting;

        // Create curl easy handle
        curlEasy_ = curl_easy_init();
        if (!curlEasy_) {
            spdlog::error("Failed to create CURL handle");
            state_ = ConnectionState::Error;
            return false;
        }

        // Build URL
        auto url = std::format("http://{}:{}/", config_.host, config_.port);

        // Set options
        curl_easy_setopt(curlEasy_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlEasy_, CURLOPT_CONNECT_ONLY, 1L);
        curl_easy_setopt(curlEasy_, CURLOPT_CONNECTTIMEOUT_MS,
                         static_cast<long>(timeoutMs));
        curl_easy_setopt(curlEasy_, CURLOPT_NOSIGNAL, 1L);

        // Attempt connection
        auto result = curl_easy_perform(curlEasy_);
        if (result != CURLE_OK) {
            spdlog::error("Failed to connect to PHD2: {}",
                          curl_easy_strerror(result));
            curl_easy_cleanup(curlEasy_);
            curlEasy_ = nullptr;
            state_ = ConnectionState::Error;
            return false;
        }

        // Get socket
        curl_socket_t sockfd;
        result = curl_easy_getinfo(curlEasy_, CURLINFO_ACTIVESOCKET, &sockfd);
        if (result != CURLE_OK || sockfd == CURL_SOCKET_BAD) {
            spdlog::error("Failed to get socket");
            curl_easy_cleanup(curlEasy_);
            curlEasy_ = nullptr;
            state_ = ConnectionState::Error;
            return false;
        }

        socket_ = sockfd;
        state_ = ConnectionState::Connected;
        stopping_ = false;
        stats_.connectedSince = std::chrono::steady_clock::now();

        // Start receive thread
        receiveThread_ = std::make_unique<std::jthread>(
            [this](std::stop_token st) { receiveLoop(st); });

        spdlog::info("Connected to PHD2 at {}:{}", config_.host, config_.port);

        if (eventHandler_) {
            eventHandler_->onConnectionStateChanged(true);
        }

        return true;
    }

    void disconnect() {
        std::lock_guard lock(mutex_);

        if (state_ == ConnectionState::Disconnected) {
            return;
        }

        stopping_ = true;
        state_ = ConnectionState::Disconnected;

        // Stop receive thread
        if (receiveThread_) {
            receiveThread_->request_stop();
            receiveThread_->join();
            receiveThread_.reset();
        }

        // Cleanup curl
        if (curlEasy_) {
            curl_easy_cleanup(curlEasy_);
            curlEasy_ = nullptr;
        }

        // Clear pending RPCs
        {
            std::lock_guard rpcLock(rpcMutex_);
            for (auto& [id, promise] : pendingRpcs_) {
                promise.set_value(
                    RpcResponse{.success = false,
                                .errorCode = -1,
                                .errorMessage = "Connection closed"});
            }
            pendingRpcs_.clear();
        }

        spdlog::info("Disconnected from PHD2");

        if (eventHandler_) {
            eventHandler_->onConnectionStateChanged(false);
        }
    }

    [[nodiscard]] auto isConnected() const noexcept -> bool {
        return state_ == ConnectionState::Connected;
    }

    [[nodiscard]] auto getState() const noexcept -> ConnectionState {
        return state_;
    }

    auto sendRpc(std::string_view method, const json& params, int timeoutMs)
        -> RpcResponse {
        if (!isConnected()) {
            throw ConnectionException("Not connected to PHD2");
        }

        int rpcId;
        std::future<RpcResponse> future;

        {
            std::lock_guard lock(rpcMutex_);
            rpcId = nextRpcId_++;

            // Create promise/future pair
            std::promise<RpcResponse> promise;
            future = promise.get_future();
            pendingRpcs_[rpcId] = std::move(promise);
        }

        // Build request
        json request = {{"jsonrpc", "2.0"}, {"method", method}, {"id", rpcId}};

        if (!params.empty() && !params.is_null()) {
            request["params"] = params;
        }

        auto requestStr = request.dump() + "\r\n";

        // Send request
        {
            std::lock_guard lock(mutex_);
            size_t sent = 0;
            auto result = curl_easy_send(curlEasy_, requestStr.data(),
                                         requestStr.size(), &sent);
            if (result != CURLE_OK) {
                std::lock_guard rpcLock(rpcMutex_);
                pendingRpcs_.erase(rpcId);
                throw ConnectionException(std::format(
                    "Failed to send RPC: {}", curl_easy_strerror(result)));
            }
            stats_.messagesSent++;
        }

        spdlog::debug("Sent RPC {}: {}", rpcId, method);

        // Wait for response
        auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout) {
            std::lock_guard lock(rpcMutex_);
            pendingRpcs_.erase(rpcId);
            throw TimeoutException(std::format("RPC {} timed out", method),
                                   timeoutMs);
        }

        return future.get();
    }

    auto sendRpcAsync(std::string_view method, const json& params)
        -> std::future<RpcResponse> {
        return std::async(std::launch::async, [this, m = std::string(method),
                                               p = params]() {
            return sendRpc(m, p, static_cast<int>(config_.rpcTimeout.count()));
        });
    }

    void setEventHandler(std::shared_ptr<EventHandler> handler) {
        std::lock_guard lock(mutex_);
        eventHandler_ = std::move(handler);
    }

    void setEventCallback(EventCallback callback) {
        eventCallback_ = std::move(callback);
    }

    void setErrorCallback(ErrorCallback callback) {
        errorCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getConfig() const noexcept -> const ConnectionConfig& {
        return config_;
    }

    [[nodiscard]] auto getStats() const noexcept -> Stats { return stats_; }

private:
    void receiveLoop(std::stop_token st) {
        std::vector<char> buffer(config_.receiveBufferSize);

        while (!st.stop_requested() && !stopping_) {
            size_t received = 0;

            {
                std::lock_guard lock(mutex_);
                if (!curlEasy_)
                    break;

                auto result = curl_easy_recv(curlEasy_, buffer.data(),
                                             buffer.size(), &received);

                if (result == CURLE_AGAIN) {
                    // No data available, wait a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                if (result != CURLE_OK) {
                    if (!stopping_) {
                        spdlog::error("Receive error: {}",
                                      curl_easy_strerror(result));
                        if (eventHandler_) {
                            eventHandler_->onConnectionError(
                                curl_easy_strerror(result));
                        }
                        if (errorCallback_) {
                            errorCallback_(curl_easy_strerror(result));
                        }
                    }
                    break;
                }
            }

            if (received > 0) {
                stats_.bytesReceived += received;
                receiveBuffer_.append(buffer.data(), received);
                processBuffer();
            }
        }
    }

    void processBuffer() {
        // Process complete lines
        size_t pos;
        while ((pos = receiveBuffer_.find('\n')) != std::string::npos) {
            auto line = receiveBuffer_.substr(0, pos);
            receiveBuffer_.erase(0, pos + 1);

            // Remove \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (!line.empty()) {
                processLine(line);
            }
        }
    }

    void processLine(std::string_view line) {
        try {
            auto j = json::parse(line);

            if (j.contains("jsonrpc")) {
                // RPC response
                if (j.contains("id") && !j["id"].is_null()) {
                    processRpcResponse(j);
                }
            } else if (j.contains("Event")) {
                // Event message
                processEvent(j);
            }
        } catch (const json::parse_error& e) {
            spdlog::warn("Failed to parse JSON: {}", e.what());
        }
    }

    void processRpcResponse(const json& j) {
        int id = j["id"].get<int>();

        RpcResponse response;
        if (j.contains("error")) {
            response.success = false;
            response.errorCode = j["error"].value("code", -1);
            response.errorMessage =
                j["error"].value("message", "Unknown error");
        } else {
            response.success = true;
            response.result = j.value("result", json{});
        }

        std::lock_guard lock(rpcMutex_);
        auto it = pendingRpcs_.find(id);
        if (it != pendingRpcs_.end()) {
            it->second.set_value(std::move(response));
            pendingRpcs_.erase(it);
            stats_.messagesReceived++;
        }
    }

    void processEvent(const json& j) {
        auto event = parseEvent(j);

        if (eventHandler_) {
            eventHandler_->onEvent(event);
        }

        if (eventCallback_) {
            eventCallback_(event);
        }

        stats_.messagesReceived++;
    }

    auto parseEvent(const json& j) -> Event {
        auto eventName = j.value("Event", "");
        auto timestamp = j.value("Timestamp", 0.0);
        auto host = j.value("Host", "");
        auto instance = j.value("Inst", 0);

        auto type = eventTypeFromString(eventName);

        switch (type) {
            case EventType::Version:
                return VersionEvent{
                    .type = type,
                    .timestamp = timestamp,
                    .host = host,
                    .instance = instance,
                    .phdVersion = j.value("PHDVersion", ""),
                    .phdSubver = j.value("PHDSubver", ""),
                    .msgVersion = j.value("MsgVersion", 0),
                    .overlapSupport = j.value("OverlapSupport", false)};

            case EventType::AppState:
                return AppStateEvent{.type = type,
                                     .timestamp = timestamp,
                                     .host = host,
                                     .instance = instance,
                                     .state = j.value("State", "")};

            case EventType::GuideStep:
                return GuideStepEvent{
                    .type = type,
                    .timestamp = timestamp,
                    .host = host,
                    .instance = instance,
                    .frame = j.value("Frame", 0),
                    .time = j.value("Time", 0.0),
                    .mount = j.value("Mount", ""),
                    .dx = j.value("dx", 0.0),
                    .dy = j.value("dy", 0.0),
                    .raDistanceRaw = j.value("RADistanceRaw", 0.0),
                    .decDistanceRaw = j.value("DECDistanceRaw", 0.0),
                    .raDistanceGuide = j.value("RADistanceGuide", 0.0),
                    .decDistanceGuide = j.value("DECDistanceGuide", 0.0),
                    .raDuration = j.value("RADuration", 0),
                    .raDirection = j.value("RADirection", ""),
                    .decDuration = j.value("DECDuration", 0),
                    .decDirection = j.value("DECDirection", ""),
                    .starMass = j.value("StarMass", 0.0),
                    .snr = j.value("SNR", 0.0),
                    .hfd = j.value("HFD", 0.0),
                    .avgDist = j.value("AvgDist", 0.0)};

            case EventType::SettleDone:
                return SettleDoneEvent{
                    .type = type,
                    .timestamp = timestamp,
                    .host = host,
                    .instance = instance,
                    .status = j.value("Status", 0),
                    .error = j.value("Error", ""),
                    .totalFrames = j.value("TotalFrames", 0),
                    .droppedFrames = j.value("DroppedFrames", 0)};

            case EventType::StarLost:
                return StarLostEvent{.type = type,
                                     .timestamp = timestamp,
                                     .host = host,
                                     .instance = instance,
                                     .frame = j.value("Frame", 0),
                                     .time = j.value("Time", 0.0),
                                     .starMass = j.value("StarMass", 0.0),
                                     .snr = j.value("SNR", 0.0),
                                     .avgDist = j.value("AvgDist", 0.0),
                                     .errorCode = j.value("ErrorCode", 0),
                                     .status = j.value("Status", "")};

            case EventType::StartGuiding:
                return StartGuidingEvent{.type = type,
                                         .timestamp = timestamp,
                                         .host = host,
                                         .instance = instance};

            case EventType::GuidingStopped:
                return GuidingStoppedEvent{.type = type,
                                           .timestamp = timestamp,
                                           .host = host,
                                           .instance = instance};

            case EventType::Paused:
                return PausedEvent{.type = type,
                                   .timestamp = timestamp,
                                   .host = host,
                                   .instance = instance};

            case EventType::Resumed:
                return ResumedEvent{.type = type,
                                    .timestamp = timestamp,
                                    .host = host,
                                    .instance = instance};

            default:
                return GenericEvent{.type = EventType::Generic,
                                    .timestamp = timestamp,
                                    .host = host,
                                    .instance = instance,
                                    .data = j};
        }
    }

    ConnectionConfig config_;
    std::shared_ptr<EventHandler> eventHandler_;
    EventCallback eventCallback_;
    ErrorCallback errorCallback_;

    std::unique_ptr<CurlGlobalInit> curlInit_;
    CURL* curlEasy_{nullptr};
    curl_socket_t socket_{CURL_SOCKET_BAD};

    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> stopping_{false};

    mutable std::mutex mutex_;
    std::unique_ptr<std::jthread> receiveThread_;
    std::string receiveBuffer_;

    std::mutex rpcMutex_;
    int nextRpcId_{1};
    std::unordered_map<int, std::promise<RpcResponse>> pendingRpcs_;

    Stats stats_;
};

// ============================================================================
// Connection Public Interface
// ============================================================================

Connection::Connection(ConnectionConfig config,
                       std::shared_ptr<EventHandler> eventHandler)
    : impl_(
          std::make_unique<Impl>(std::move(config), std::move(eventHandler))) {}

Connection::Connection(std::string host, int port,
                       std::shared_ptr<EventHandler> eventHandler)
    : impl_(std::make_unique<Impl>(
          ConnectionConfig{.host = std::move(host), .port = port},
          std::move(eventHandler))) {}

Connection::~Connection() = default;

Connection::Connection(Connection&&) noexcept = default;
Connection& Connection::operator=(Connection&&) noexcept = default;

auto Connection::connect(int timeoutMs) -> bool {
    return impl_->connect(timeoutMs);
}

void Connection::disconnect() { impl_->disconnect(); }

auto Connection::isConnected() const noexcept -> bool {
    return impl_->isConnected();
}

auto Connection::getState() const noexcept -> ConnectionState {
    return impl_->getState();
}

auto Connection::sendRpc(std::string_view method, const json& params,
                         int timeoutMs) -> RpcResponse {
    return impl_->sendRpc(method, params, timeoutMs);
}

auto Connection::sendRpcAsync(std::string_view method, const json& params)
    -> std::future<RpcResponse> {
    return impl_->sendRpcAsync(method, params);
}

void Connection::setEventHandler(std::shared_ptr<EventHandler> handler) {
    impl_->setEventHandler(std::move(handler));
}

void Connection::setEventCallback(EventCallback callback) {
    impl_->setEventCallback(std::move(callback));
}

void Connection::setErrorCallback(ErrorCallback callback) {
    impl_->setErrorCallback(std::move(callback));
}

auto Connection::getConfig() const noexcept -> const ConnectionConfig& {
    return impl_->getConfig();
}

auto Connection::getStats() const noexcept -> Stats {
    return impl_->getStats();
}

}  // namespace phd2
