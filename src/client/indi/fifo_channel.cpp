/*
 * fifo_channel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: FIFO Channel implementation

**************************************************/

#include "fifo_channel.hpp"

#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"

#include <chrono>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace lithium::client::indi {

// ==================== FifoCommand ====================

std::string FifoCommand::build() const {
    std::stringstream ss;

    switch (type) {
        case FifoCommandType::Start:
            ss << "start " << driverBinary;
            if (!skeletonPath.empty()) {
                ss << " -s \"" << skeletonPath << "\"";
            }
            break;

        case FifoCommandType::Stop:
            ss << "stop " << driverBinary;
            break;

        case FifoCommandType::Restart:
            // Restart is handled as stop + start in send()
            ss << "stop " << driverBinary;
            break;

        case FifoCommandType::Custom:
            ss << customCommand;
            break;
    }

    return ss.str();
}

FifoCommand FifoCommand::startDriver(const std::string& binary,
                                     const std::string& skeleton) {
    FifoCommand cmd;
    cmd.type = FifoCommandType::Start;
    cmd.driverBinary = binary;
    cmd.skeletonPath = skeleton;
    return cmd;
}

FifoCommand FifoCommand::stopDriver(const std::string& binary) {
    FifoCommand cmd;
    cmd.type = FifoCommandType::Stop;
    cmd.driverBinary = binary;
    return cmd;
}

FifoCommand FifoCommand::custom(const std::string& command) {
    FifoCommand cmd;
    cmd.type = FifoCommandType::Custom;
    cmd.customCommand = command;
    return cmd;
}

// ==================== FifoChannel ====================

FifoChannel::FifoChannel(FifoChannelConfig config)
    : config_(std::move(config)) {
    LOG_INFO("FifoChannel created with path: {}", config_.fifoPath);
}

FifoChannel::~FifoChannel() {
    close();

    // Stop worker thread
    workerRunning_ = false;
    if (workerThread_ && workerThread_->joinable()) {
        workerThread_->join();
    }
}

void FifoChannel::setFifoPath(const std::string& path) {
    std::lock_guard lock(mutex_);
    config_.fifoPath = path;
}

const std::string& FifoChannel::getFifoPath() const { return config_.fifoPath; }

void FifoChannel::setConfig(const FifoChannelConfig& config) {
    std::lock_guard lock(mutex_);
    config_ = config;
}

const FifoChannelConfig& FifoChannel::getConfig() const { return config_; }

bool FifoChannel::isAvailable() const {
#ifdef _WIN32
    // Windows named pipes work differently
    return true;
#else
    struct stat st;
    if (stat(config_.fifoPath.c_str(), &st) != 0) {
        return false;
    }
    return S_ISFIFO(st.st_mode);
#endif
}

bool FifoChannel::open() {
    std::lock_guard lock(mutex_);

    if (fifoFd_ >= 0) {
        return true;  // Already open
    }

#ifdef _WIN32
    // Windows: Use named pipes differently
    // For now, we'll use a file-based approach
    LOG_WARN("FIFO not fully supported on Windows");
    return false;
#else
    int flags = O_WRONLY;
    if (config_.nonBlocking) {
        flags |= O_NONBLOCK;
    }

    fifoFd_ = ::open(config_.fifoPath.c_str(), flags);

    if (fifoFd_ < 0) {
        lastError_ = "Failed to open FIFO: " + std::string(strerror(errno));
        LOG_ERROR("{}", lastError_);
        return false;
    }

    LOG_INFO("Opened FIFO: {}", config_.fifoPath);

    // Start worker thread if queuing is enabled
    if (config_.queueCommands && !workerRunning_) {
        workerRunning_ = true;
        workerThread_ =
            std::make_unique<std::thread>(&FifoChannel::workerThread, this);
    }

    return true;
#endif
}

void FifoChannel::close() {
    std::lock_guard lock(mutex_);

#ifndef _WIN32
    if (fifoFd_ >= 0) {
        ::close(fifoFd_);
        fifoFd_ = -1;
        LOG_INFO("Closed FIFO");
    }
#endif
}

bool FifoChannel::isOpen() const { return fifoFd_ >= 0; }

FifoResult FifoChannel::send(const FifoCommand& command) {
    auto startTime = std::chrono::steady_clock::now();

    std::string cmdStr = command.build();
    LOG_INFO("Sending FIFO command: {}", cmdStr);

    FifoResult result = writeToFifo(cmdStr);

    // Handle restart as stop + start
    if (result.success && command.type == FifoCommandType::Restart) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        FifoCommand startCmd = FifoCommand::startDriver(command.driverBinary,
                                                        command.skeletonPath);
        result = writeToFifo(startCmd.build());
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    if (result.success) {
        totalCommandsSent_++;
    } else {
        totalErrors_++;
    }

    return result;
}

void FifoChannel::sendAsync(const FifoCommand& command,
                            FifoCommandCallback callback) {
    if (!config_.queueCommands) {
        // Execute immediately in a detached thread
        std::thread([this, command, callback]() {
            FifoResult result = send(command);
            if (callback) {
                callback(command, result);
            }
        }).detach();
        return;
    }

    std::lock_guard lock(queueMutex_);

    if (commandQueue_.size() >= static_cast<size_t>(config_.maxQueueSize)) {
        LOG_WARN("Command queue full, dropping command");
        if (callback) {
            callback(command, FifoResult::error("Queue full"));
        }
        return;
    }

    commandQueue_.push({command, callback});
}

FifoResult FifoChannel::sendRaw(const std::string& data) {
    return writeToFifo(data);
}

FifoResult FifoChannel::startDriver(const std::string& binary,
                                    const std::string& skeleton) {
    return send(FifoCommand::startDriver(binary, skeleton));
}

FifoResult FifoChannel::stopDriver(const std::string& binary) {
    return send(FifoCommand::stopDriver(binary));
}

FifoResult FifoChannel::restartDriver(const std::string& binary,
                                      const std::string& skeleton) {
    FifoCommand cmd;
    cmd.type = FifoCommandType::Restart;
    cmd.driverBinary = binary;
    cmd.skeletonPath = skeleton;
    return send(cmd);
}

size_t FifoChannel::getPendingCount() const {
    std::lock_guard lock(queueMutex_);
    return commandQueue_.size();
}

void FifoChannel::clearQueue() {
    std::lock_guard lock(queueMutex_);
    while (!commandQueue_.empty()) {
        commandQueue_.pop();
    }
}

bool FifoChannel::waitForPending(int timeoutMs) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (getPendingCount() == 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return getPendingCount() == 0;
}

uint64_t FifoChannel::getTotalCommandsSent() const {
    return totalCommandsSent_.load();
}

uint64_t FifoChannel::getTotalErrors() const { return totalErrors_.load(); }

const std::string& FifoChannel::getLastError() const { return lastError_; }

FifoResult FifoChannel::writeToFifo(const std::string& data) {
#ifdef _WIN32
    // Windows implementation using echo command
    std::string cmd = "echo \"" + data + "\" > " + config_.fifoPath;
    int result = system(cmd.c_str());
    if (result != 0) {
        lastError_ = "Failed to write to FIFO via echo";
        return FifoResult::error(lastError_);
    }
    return FifoResult::ok();
#else
    // Ensure FIFO is open
    if (fifoFd_ < 0) {
        // Try to open with non-blocking first
        int flags = O_WRONLY | O_NONBLOCK;
        int fd = ::open(config_.fifoPath.c_str(), flags);

        if (fd < 0) {
            if (errno == ENXIO) {
                // No reader - server might not be ready
                lastError_ = "No reader on FIFO (server not ready?)";
                return FifoResult::error(lastError_);
            }
            lastError_ = "Failed to open FIFO: " + std::string(strerror(errno));
            return FifoResult::error(lastError_);
        }

        // Write and close immediately for one-shot commands
        std::string dataWithNewline = data + "\n";

        for (int retry = 0; retry < config_.retryCount; retry++) {
            ssize_t written =
                write(fd, dataWithNewline.c_str(), dataWithNewline.size());

            if (written == static_cast<ssize_t>(dataWithNewline.size())) {
                ::close(fd);
                LOG_INFO("Successfully wrote to FIFO: {}", data);
                return FifoResult::ok();
            }

            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // FIFO full, wait and retry
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(config_.retryDelayMs));
                    continue;
                }

                lastError_ = "Write failed: " + std::string(strerror(errno));
                ::close(fd);
                return FifoResult::error(lastError_);
            }

            // Partial write - shouldn't happen with small commands
            LOG_WARN("Partial write to FIFO: {} of {} bytes", written,
                     dataWithNewline.size());
        }

        ::close(fd);
        lastError_ = "Failed to write after retries";
        return FifoResult::error(lastError_);
    }

    // Use existing file descriptor
    std::string dataWithNewline = data + "\n";

    for (int retry = 0; retry < config_.retryCount; retry++) {
        ssize_t written =
            write(fifoFd_, dataWithNewline.c_str(), dataWithNewline.size());

        if (written == static_cast<ssize_t>(dataWithNewline.size())) {
            LOG_INFO("Successfully wrote to FIFO: {}", data);
            return FifoResult::ok();
        }

        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.retryDelayMs));
                continue;
            }

            lastError_ = "Write failed: " + std::string(strerror(errno));
            return FifoResult::error(lastError_);
        }
    }

    lastError_ = "Failed to write after retries";
    return FifoResult::error(lastError_);
#endif
}

void FifoChannel::processQueue() {
    std::pair<FifoCommand, FifoCommandCallback> item;

    {
        std::lock_guard lock(queueMutex_);
        if (commandQueue_.empty()) {
            return;
        }
        item = std::move(commandQueue_.front());
        commandQueue_.pop();
    }

    FifoResult result = send(item.first);

    if (item.second) {
        item.second(item.first, result);
    }
}

void FifoChannel::workerThread() {
    LOG_INFO("FIFO worker thread started");

    while (workerRunning_) {
        processQueue();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Process remaining items
    while (getPendingCount() > 0) {
        processQueue();
    }

    LOG_INFO("FIFO worker thread stopped");
}

}  // namespace lithium::client::indi
