/*
 * channel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "channel.hpp"
#include "message.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define pipe(fds) _pipe(fds, 65536, _O_BINARY)
#define read(fd, buf, size) _read(fd, buf, static_cast<unsigned int>(size))
#define write(fd, buf, size) _write(fd, buf, static_cast<unsigned int>(size))
#define close(fd) _close(fd)
#else
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#endif

namespace lithium::ipc {

// ============================================================================
// PipeChannel::Impl Implementation
// ============================================================================

class PipeChannel::Impl {
public:
    Impl() : readFd_(-1), writeFd_(-1), sequenceId_(0) {}

    ~Impl() {
        close();
    }

    IPCResult<void> create() {
#ifdef _WIN32
        int fds[2];
        if (_pipe(fds, 65536, _O_BINARY) != 0) {
            spdlog::error("Failed to create pipe");
            return std::unexpected(IPCError::PipeError);
        }
        readFd_ = fds[0];
        writeFd_ = fds[1];
#else
        int fds[2];
        if (pipe(fds) != 0) {
            spdlog::error("Failed to create pipe");
            return std::unexpected(IPCError::PipeError);
        }
        readFd_ = fds[0];
        writeFd_ = fds[1];
#endif
        return {};
    }

    void close() {
        if (readFd_ >= 0) {
            ::close(readFd_);
            readFd_ = -1;
        }
        if (writeFd_ >= 0) {
            ::close(writeFd_);
            writeFd_ = -1;
        }
    }

    bool isOpen() const noexcept {
        return readFd_ >= 0 || writeFd_ >= 0;
    }

    IPCResult<void> send(const Message& message) {
        if (writeFd_ < 0) {
            return std::unexpected(IPCError::ChannelClosed);
        }

        auto data = message.serialize();

        std::lock_guard<std::mutex> lock(writeMutex_);

        size_t totalWritten = 0;
        while (totalWritten < data.size()) {
            auto written = ::write(writeFd_, data.data() + totalWritten,
                                   data.size() - totalWritten);
            if (written <= 0) {
                spdlog::error("Write failed");
                return std::unexpected(IPCError::PipeError);
            }
            totalWritten += written;
        }

        return {};
    }

    IPCResult<Message> receive(std::chrono::milliseconds timeout) {
        if (readFd_ < 0) {
            return std::unexpected(IPCError::ChannelClosed);
        }

#ifndef _WIN32
        // Use poll for timeout on Unix
        struct pollfd pfd;
        pfd.fd = readFd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
        if (ret == 0) {
            return std::unexpected(IPCError::Timeout);
        } else if (ret < 0) {
            return std::unexpected(IPCError::PipeError);
        }
#else
        // Windows doesn't have poll for pipes, use a simple loop with small reads
        auto deadline = std::chrono::steady_clock::now() + timeout;
#endif

        // Read header
        std::vector<uint8_t> headerData(MessageHeader::SIZE);
        size_t headerRead = 0;

        while (headerRead < MessageHeader::SIZE) {
#ifdef _WIN32
            if (std::chrono::steady_clock::now() > deadline) {
                return std::unexpected(IPCError::Timeout);
            }
#endif
            auto bytesRead = ::read(readFd_, headerData.data() + headerRead,
                                    MessageHeader::SIZE - headerRead);
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    return std::unexpected(IPCError::ChannelClosed);
                }
                return std::unexpected(IPCError::PipeError);
            }
            headerRead += bytesRead;
        }

        auto headerResult = MessageHeader::deserialize(headerData);
        if (!headerResult) {
            return std::unexpected(headerResult.error());
        }

        // Read payload
        std::vector<uint8_t> payload(headerResult->payloadSize);
        size_t payloadRead = 0;

        while (payloadRead < headerResult->payloadSize) {
#ifdef _WIN32
            if (std::chrono::steady_clock::now() > deadline) {
                return std::unexpected(IPCError::Timeout);
            }
#endif
            auto bytesRead = ::read(readFd_, payload.data() + payloadRead,
                                    headerResult->payloadSize - payloadRead);
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    return std::unexpected(IPCError::ChannelClosed);
                }
                return std::unexpected(IPCError::PipeError);
            }
            payloadRead += bytesRead;
        }

        Message msg;
        msg.header = *headerResult;
        msg.payload = std::move(payload);

        return msg;
    }

    bool hasData() const {
#ifndef _WIN32
        struct pollfd pfd;
        pfd.fd = readFd_;
        pfd.events = POLLIN;
        return poll(&pfd, 1, 0) > 0;
#else
        return true;  // Windows pipes don't have easy peek
#endif
    }

    int getReadFd() const noexcept { return readFd_; }
    int getWriteFd() const noexcept { return writeFd_; }

    void closeRead() {
        if (readFd_ >= 0) {
            ::close(readFd_);
            readFd_ = -1;
        }
    }

    void closeWrite() {
        if (writeFd_ >= 0) {
            ::close(writeFd_);
            writeFd_ = -1;
        }
    }

    IPCResult<void> setNonBlocking(bool nonBlocking) {
#ifndef _WIN32
        if (readFd_ >= 0) {
            int flags = fcntl(readFd_, F_GETFL, 0);
            if (nonBlocking) {
                fcntl(readFd_, F_SETFL, flags | O_NONBLOCK);
            } else {
                fcntl(readFd_, F_SETFL, flags & ~O_NONBLOCK);
            }
        }
        if (writeFd_ >= 0) {
            int flags = fcntl(writeFd_, F_GETFL, 0);
            if (nonBlocking) {
                fcntl(writeFd_, F_SETFL, flags | O_NONBLOCK);
            } else {
                fcntl(writeFd_, F_SETFL, flags & ~O_NONBLOCK);
            }
        }
#endif
        return {};
    }

    uint32_t nextSequenceId() {
        return sequenceId_++;
    }

private:
    int readFd_;
    int writeFd_;
    std::atomic<uint32_t> sequenceId_;
    std::mutex writeMutex_;
};

// ============================================================================
// PipeChannel Implementation
// ============================================================================

PipeChannel::PipeChannel() : pImpl_(std::make_unique<Impl>()) {}
PipeChannel::~PipeChannel() = default;

PipeChannel::PipeChannel(PipeChannel&& other) noexcept = default;
PipeChannel& PipeChannel::operator=(PipeChannel&& other) noexcept = default;

IPCResult<void> PipeChannel::create() { return pImpl_->create(); }
void PipeChannel::close() { pImpl_->close(); }
bool PipeChannel::isOpen() const noexcept { return pImpl_->isOpen(); }
IPCResult<void> PipeChannel::send(const Message& message) { return pImpl_->send(message); }
IPCResult<void> PipeChannel::send(MessageType type, const json& payload) {
    return send(Message::create(type, payload, nextSequenceId()));
}
IPCResult<Message> PipeChannel::receive(std::chrono::milliseconds timeout) {
    return pImpl_->receive(timeout);
}
bool PipeChannel::hasData() const { return pImpl_->hasData(); }
int PipeChannel::getReadFd() const noexcept { return pImpl_->getReadFd(); }
int PipeChannel::getWriteFd() const noexcept { return pImpl_->getWriteFd(); }
void PipeChannel::closeRead() { pImpl_->closeRead(); }
void PipeChannel::closeWrite() { pImpl_->closeWrite(); }
IPCResult<void> PipeChannel::setNonBlocking(bool nonBlocking) {
    return pImpl_->setNonBlocking(nonBlocking);
}
uint32_t PipeChannel::nextSequenceId() { return pImpl_->nextSequenceId(); }

// ============================================================================
// BidirectionalChannel Implementation
// ============================================================================

BidirectionalChannel::BidirectionalChannel() = default;
BidirectionalChannel::~BidirectionalChannel() = default;

IPCResult<void> BidirectionalChannel::create() {
    auto result1 = parentToChild_.create();
    if (!result1) return result1;

    auto result2 = childToParent_.create();
    if (!result2) {
        parentToChild_.close();
        return result2;
    }

    return {};
}

void BidirectionalChannel::close() {
    parentToChild_.close();
    childToParent_.close();
}

bool BidirectionalChannel::isOpen() const noexcept {
    return parentToChild_.isOpen() && childToParent_.isOpen();
}

IPCResult<void> BidirectionalChannel::send(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return parentToChild_.send(message);
}

IPCResult<Message> BidirectionalChannel::receive(std::chrono::milliseconds timeout) {
    return childToParent_.receive(timeout);
}

std::pair<int, int> BidirectionalChannel::getSubprocessFds() const noexcept {
    return {parentToChild_.getReadFd(), childToParent_.getWriteFd()};
}

void BidirectionalChannel::setupParent() {
    // Parent keeps: write end of parentToChild, read end of childToParent
    parentToChild_.closeRead();
    childToParent_.closeWrite();
}

void BidirectionalChannel::setupChild() {
    // Child keeps: read end of parentToChild, write end of childToParent
    parentToChild_.closeWrite();
    childToParent_.closeRead();
}

IPCResult<HandshakePayload> BidirectionalChannel::performHandshake(
    std::chrono::milliseconds timeout) {

    // Send handshake request
    HandshakePayload request;
    request.version = "1.0";
#ifdef _WIN32
    request.pid = GetCurrentProcessId();
#else
    request.pid = getpid();
#endif
    request.capabilities = {"execute", "progress", "cancel"};

    auto sendResult = send(Message::create(MessageType::Handshake,
                                           request.toJson(), sequenceId_++));
    if (!sendResult) {
        return std::unexpected(sendResult.error());
    }

    // Wait for handshake acknowledgment
    auto response = receive(timeout);
    if (!response) {
        return std::unexpected(response.error());
    }

    if (response->header.type != MessageType::HandshakeAck) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    auto payloadResult = response->getPayloadAsJson();
    if (!payloadResult) {
        return std::unexpected(payloadResult.error());
    }

    return HandshakePayload::fromJson(*payloadResult);
}

IPCResult<void> BidirectionalChannel::respondToHandshake(const HandshakePayload& payload) {
    auto msg = Message::create(MessageType::HandshakeAck, payload.toJson(), sequenceId_++);
    return childToParent_.send(msg);
}

}  // namespace lithium::ipc
