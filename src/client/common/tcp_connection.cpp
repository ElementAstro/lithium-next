/*
 * tcp_connection.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: TCP connection implementation

*************************************************/

#include "tcp_connection.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
#endif

#include <algorithm>
#include <cstring>

namespace lithium::client {

// ============================================================================
// Platform-specific socket helpers
// ============================================================================

namespace {

#ifdef _WIN32
class WinsockInit {
public:
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() { WSACleanup(); }
};

static WinsockInit winsockInit;

inline void closeSocket(socket_t sock) { closesocket(sock); }

inline int getLastError() { return WSAGetLastError(); }

inline bool wouldBlock() {
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

#else

inline void closeSocket(socket_t sock) { close(sock); }

inline int getLastError() { return errno; }

inline bool wouldBlock() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
}

#endif

auto setNonBlocking(socket_t sock, bool nonBlocking) -> bool {
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags) == 0;
#endif
}

auto setSocketOption(socket_t sock, int level, int optname, bool value)
    -> bool {
    int val = value ? 1 : 0;
    return setsockopt(sock, level, optname, reinterpret_cast<const char*>(&val),
                      sizeof(val)) == 0;
}

}  // namespace

// ============================================================================
// TcpConnection Implementation
// ============================================================================

class TcpConnection::Impl {
public:
    explicit Impl(TcpConfig config) : config_(std::move(config)) {}

    ~Impl() { disconnect(); }

    auto connect(std::chrono::milliseconds timeout) -> bool {
        if (state_ == TcpConnectionState::Connected) {
            return true;
        }

        setState(TcpConnectionState::Connecting);

        // Resolve hostname
        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* result = nullptr;
        std::string portStr = std::to_string(config_.port);

        int ret = getaddrinfo(config_.host.c_str(), portStr.c_str(), &hints,
                              &result);
        if (ret != 0) {
            spdlog::error("Failed to resolve host {}: {}", config_.host,
                          gai_strerror(ret));
            setError(TcpError::HostNotFound);
            return false;
        }

        // Try each address
        for (auto* rp = result; rp != nullptr; rp = rp->ai_next) {
            socket_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_ == INVALID_SOCK) {
                continue;
            }

            // Set non-blocking for timeout support
            setNonBlocking(socket_, true);

            // Attempt connection
            ret = ::connect(socket_, rp->ai_addr,
                            static_cast<int>(rp->ai_addrlen));

            if (ret == 0) {
                // Connected immediately
                break;
            }

            if (wouldBlock()) {
                // Wait for connection with timeout
                fd_set writefds;
                FD_ZERO(&writefds);
                FD_SET(socket_, &writefds);

                struct timeval tv;
                tv.tv_sec =
                    static_cast<long>(timeout.count() / 1000);
                tv.tv_usec =
                    static_cast<long>((timeout.count() % 1000) * 1000);

                ret = select(static_cast<int>(socket_) + 1, nullptr, &writefds,
                             nullptr, &tv);

                if (ret > 0) {
                    // Check if connection succeeded
                    int error = 0;
                    socklen_t len = sizeof(error);
                    getsockopt(socket_, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char*>(&error), &len);
                    if (error == 0) {
                        break;  // Success
                    }
                }
            }

            closeSocket(socket_);
            socket_ = INVALID_SOCK;
        }

        freeaddrinfo(result);

        if (socket_ == INVALID_SOCK) {
            spdlog::error("Failed to connect to {}:{}", config_.host,
                          config_.port);
            setError(TcpError::ConnectionRefused);
            return false;
        }

        // Set socket options
        if (config_.keepAlive) {
            setSocketOption(socket_, SOL_SOCKET, SO_KEEPALIVE, true);
        }
        if (config_.noDelay) {
            setSocketOption(socket_, IPPROTO_TCP, TCP_NODELAY, true);
        }

        // Set back to blocking mode for normal operations
        setNonBlocking(socket_, false);

        setState(TcpConnectionState::Connected);
        stats_.connectedSince = std::chrono::steady_clock::now();
        stats_.lastActivity = stats_.connectedSince;

        spdlog::info("Connected to {}:{}", config_.host, config_.port);
        return true;
    }

    void disconnect() {
        if (socket_ != INVALID_SOCK) {
            stopAsyncReceive();
            closeSocket(socket_);
            socket_ = INVALID_SOCK;
        }
        setState(TcpConnectionState::Disconnected);
    }

    [[nodiscard]] auto isConnected() const noexcept -> bool {
        return state_ == TcpConnectionState::Connected &&
               socket_ != INVALID_SOCK;
    }

    [[nodiscard]] auto getState() const noexcept -> TcpConnectionState {
        return state_;
    }

    [[nodiscard]] auto getLastError() const noexcept -> TcpError {
        return lastError_;
    }

    auto send(std::string_view data) -> atom::type::expected<size_t, TcpError> {
        if (!isConnected()) {
            return atom::type::unexpected(TcpError::Disconnected);
        }

        std::lock_guard lock(sendMutex_);

        size_t totalSent = 0;
        while (totalSent < data.size()) {
            auto sent =
                ::send(socket_, data.data() + totalSent,
                       static_cast<int>(data.size() - totalSent), 0);
            if (sent <= 0) {
                setError(TcpError::SendFailed);
                return atom::type::unexpected(TcpError::SendFailed);
            }
            totalSent += static_cast<size_t>(sent);
        }

        stats_.bytesSent += totalSent;
        stats_.messagesSent++;
        stats_.lastActivity = std::chrono::steady_clock::now();

        return totalSent;
    }

    auto sendLine(std::string_view line) -> atom::type::expected<size_t, TcpError> {
        std::string data(line);
        data += "\r\n";
        return send(data);
    }

    auto receive(size_t maxBytes) -> atom::type::expected<std::string, TcpError> {
        if (!isConnected()) {
            return atom::type::unexpected(TcpError::Disconnected);
        }

        size_t bufSize =
            maxBytes > 0 ? maxBytes : config_.receiveBufferSize;
        std::vector<char> buffer(bufSize);

        auto received = ::recv(socket_, buffer.data(),
                               static_cast<int>(buffer.size()), 0);

        if (received < 0) {
            if (wouldBlock()) {
                return "";  // No data available
            }
            setError(TcpError::ReceiveFailed);
            return atom::type::unexpected(TcpError::ReceiveFailed);
        }

        if (received == 0) {
            // Connection closed
            setState(TcpConnectionState::Disconnected);
            return atom::type::unexpected(TcpError::Disconnected);
        }

        stats_.bytesReceived += static_cast<size_t>(received);
        stats_.messagesReceived++;
        stats_.lastActivity = std::chrono::steady_clock::now();

        return std::string(buffer.data(), static_cast<size_t>(received));
    }

    auto receiveUntil(std::string_view delimiter,
                      std::optional<std::chrono::milliseconds> timeout)
        -> atom::type::expected<std::string, TcpError> {
        auto startTime = std::chrono::steady_clock::now();
        auto effectiveTimeout = timeout.value_or(config_.readTimeout);

        while (true) {
            // Check for delimiter in buffer
            auto pos = receiveBuffer_.find(delimiter);
            if (pos != std::string::npos) {
                std::string result = receiveBuffer_.substr(0, pos);
                receiveBuffer_.erase(0, pos + delimiter.size());
                return result;
            }

            // Check timeout
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed >= effectiveTimeout) {
                return atom::type::unexpected(TcpError::Timeout);
            }

            // Receive more data
            auto data = receive(0);
            if (!data.has_value()) {
                return data.error();
            }
            receiveBuffer_ += data.value();
        }
    }

    auto receiveLine(std::optional<std::chrono::milliseconds> timeout)
        -> atom::type::expected<std::string, TcpError> {
        auto result = receiveUntil("\n", timeout);
        if (result.has_value()) {
            // Remove trailing \r if present
            auto& str = result.value();
            if (!str.empty() && str.back() == '\r') {
                str.pop_back();
            }
        }
        return result;
    }

    void startAsyncReceive(DataCallback callback) {
        if (asyncReceiveActive_) {
            return;
        }

        asyncReceiveActive_ = true;
        asyncCallback_ = std::move(callback);

        asyncThread_ = std::jthread([this](std::stop_token st) {
            std::vector<char> buffer(config_.receiveBufferSize);

            while (!st.stop_requested() && isConnected()) {
                auto received = ::recv(socket_, buffer.data(),
                                        static_cast<int>(buffer.size()), 0);

                if (received > 0) {
                    stats_.bytesReceived += static_cast<size_t>(received);
                    stats_.lastActivity = std::chrono::steady_clock::now();

                    if (asyncCallback_) {
                        asyncCallback_(std::string_view(
                            buffer.data(), static_cast<size_t>(received)));
                    }
                } else if (received == 0) {
                    // Connection closed
                    break;
                } else if (!wouldBlock()) {
                    // Error
                    break;
                }
            }

            asyncReceiveActive_ = false;
        });
    }

    void stopAsyncReceive() {
        if (asyncThread_.joinable()) {
            asyncThread_.request_stop();
            asyncThread_.join();
        }
        asyncReceiveActive_ = false;
    }

    [[nodiscard]] auto isAsyncReceiveActive() const noexcept -> bool {
        return asyncReceiveActive_;
    }

    void setErrorCallback(ErrorCallback callback) {
        errorCallback_ = std::move(callback);
    }

    void setStateCallback(StateCallback callback) {
        stateCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getConfig() const noexcept -> const TcpConfig& {
        return config_;
    }

    auto setConfig(const TcpConfig& config) -> bool {
        if (isConnected()) {
            return false;
        }
        config_ = config;
        return true;
    }

    [[nodiscard]] auto getStats() const noexcept -> TcpStats { return stats_; }

    void resetStats() {
        stats_ = TcpStats{};
        if (isConnected()) {
            stats_.connectedSince = std::chrono::steady_clock::now();
        }
    }

private:
    void setState(TcpConnectionState newState) {
        auto oldState = state_.exchange(newState);
        if (oldState != newState && stateCallback_) {
            stateCallback_(oldState, newState);
        }
    }

    void setError(TcpError error) {
        lastError_ = error;
        stats_.errors++;
        if (errorCallback_) {
            errorCallback_(error, "");
        }
    }

    TcpConfig config_;
    socket_t socket_{INVALID_SOCK};
    std::atomic<TcpConnectionState> state_{TcpConnectionState::Disconnected};
    TcpError lastError_{TcpError::None};
    TcpStats stats_;

    std::mutex sendMutex_;
    std::string receiveBuffer_;

    std::atomic<bool> asyncReceiveActive_{false};
    std::jthread asyncThread_;
    DataCallback asyncCallback_;

    ErrorCallback errorCallback_;
    StateCallback stateCallback_;
};

// ============================================================================
// TcpConnection Public Interface
// ============================================================================

TcpConnection::TcpConnection(TcpConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

TcpConnection::TcpConnection(std::string host, int port)
    : impl_(std::make_unique<Impl>(
          TcpConfig{.host = std::move(host), .port = port})) {}

TcpConnection::~TcpConnection() = default;

TcpConnection::TcpConnection(TcpConnection&&) noexcept = default;
TcpConnection& TcpConnection::operator=(TcpConnection&&) noexcept = default;

auto TcpConnection::connect() -> bool {
    return impl_->connect(impl_->getConfig().connectTimeout);
}

auto TcpConnection::connect(std::chrono::milliseconds timeout) -> bool {
    return impl_->connect(timeout);
}

void TcpConnection::disconnect() { impl_->disconnect(); }

auto TcpConnection::isConnected() const noexcept -> bool {
    return impl_->isConnected();
}

auto TcpConnection::getState() const noexcept -> TcpConnectionState {
    return impl_->getState();
}

auto TcpConnection::getLastError() const noexcept -> TcpError {
    return impl_->getLastError();
}

auto TcpConnection::send(std::string_view data)
    -> atom::type::expected<size_t, TcpError> {
    return impl_->send(data);
}

auto TcpConnection::sendLine(std::string_view line)
    -> atom::type::expected<size_t, TcpError> {
    return impl_->sendLine(line);
}

auto TcpConnection::receive(size_t maxBytes)
    -> atom::type::expected<std::string, TcpError> {
    return impl_->receive(maxBytes);
}

auto TcpConnection::receiveUntil(
    std::string_view delimiter,
    std::optional<std::chrono::milliseconds> timeout)
    -> atom::type::expected<std::string, TcpError> {
    return impl_->receiveUntil(delimiter, timeout);
}

auto TcpConnection::receiveLine(
    std::optional<std::chrono::milliseconds> timeout)
    -> atom::type::expected<std::string, TcpError> {
    return impl_->receiveLine(timeout);
}

void TcpConnection::startAsyncReceive(DataCallback callback) {
    impl_->startAsyncReceive(std::move(callback));
}

void TcpConnection::stopAsyncReceive() { impl_->stopAsyncReceive(); }

auto TcpConnection::isAsyncReceiveActive() const noexcept -> bool {
    return impl_->isAsyncReceiveActive();
}

void TcpConnection::setErrorCallback(ErrorCallback callback) {
    impl_->setErrorCallback(std::move(callback));
}

void TcpConnection::setStateCallback(StateCallback callback) {
    impl_->setStateCallback(std::move(callback));
}

auto TcpConnection::getConfig() const noexcept -> const TcpConfig& {
    return impl_->getConfig();
}

auto TcpConnection::setConfig(const TcpConfig& config) -> bool {
    return impl_->setConfig(config);
}

auto TcpConnection::getStats() const noexcept -> TcpStats {
    return impl_->getStats();
}

void TcpConnection::resetStats() { impl_->resetStats(); }

// ============================================================================
// TcpConnectionGuard Implementation
// ============================================================================

TcpConnectionGuard::TcpConnectionGuard(TcpConnection& conn) : conn_(&conn) {}

TcpConnectionGuard::~TcpConnectionGuard() {
    if (!released_ && conn_ && conn_->isConnected()) {
        conn_->disconnect();
    }
}

void TcpConnectionGuard::release() noexcept { released_ = true; }

// ============================================================================
// LineProtocol Implementation
// ============================================================================

LineProtocol::LineProtocol(TcpConnection& connection) : conn_(connection) {}

auto LineProtocol::sendAndReceive(
    std::string_view request,
    std::optional<std::chrono::milliseconds> timeout)
    -> atom::type::expected<std::string, TcpError> {
    auto sendResult = conn_.sendLine(request);
    if (!sendResult.has_value()) {
        return sendResult.error();
    }
    return conn_.receiveLine(timeout);
}

void LineProtocol::setDelimiter(std::string_view delimiter) {
    delimiter_ = delimiter;
}

}  // namespace lithium::client
