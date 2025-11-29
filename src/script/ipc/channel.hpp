/*
 * channel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file channel.hpp
 * @brief IPC Channel abstraction for pipe-based communication
 * @date 2024
 * @version 1.1.0
 *
 * This module provides pipe-based communication channels for IPC:
 * - PipeChannel: Unidirectional pipe communication
 * - BidirectionalChannel: Full-duplex communication using two pipes
 */

#ifndef LITHIUM_SCRIPT_IPC_CHANNEL_HPP
#define LITHIUM_SCRIPT_IPC_CHANNEL_HPP

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>

#include "message_types.hpp"

namespace lithium::ipc {

using json = nlohmann::json;

// Forward declarations
struct HandshakePayload;
struct Message;

/**
 * @brief Cross-platform pipe channel for IPC
 *
 * Provides unidirectional communication through a named or unnamed pipe.
 * Supports sending and receiving IPC messages with timeout support.
 */
class PipeChannel {
public:
    /**
     * @brief Create a pipe channel
     */
    PipeChannel();

    /**
     * @brief Destructor - closes pipes
     */
    ~PipeChannel();

    // Disable copy
    PipeChannel(const PipeChannel&) = delete;
    PipeChannel& operator=(const PipeChannel&) = delete;

    // Enable move
    PipeChannel(PipeChannel&& other) noexcept;
    PipeChannel& operator=(PipeChannel&& other) noexcept;

    /**
     * @brief Create the pipe
     *
     * Initializes the underlying pipe resource. Must be called before
     * any send/receive operations.
     *
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> create();

    /**
     * @brief Close the pipe
     *
     * Closes both read and write ends of the pipe.
     */
    void close();

    /**
     * @brief Check if pipe is open
     *
     * @return bool True if pipe is open and operational
     */
    [[nodiscard]] bool isOpen() const noexcept;

    /**
     * @brief Send a message
     *
     * Serializes and sends the message through the pipe.
     *
     * @param message The message to send
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> send(const Message& message);

    /**
     * @brief Send a message with type and JSON payload
     *
     * Convenience method to create and send a message in one call.
     *
     * @param type Message type
     * @param payload JSON payload
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> send(MessageType type, const json& payload);

    /**
     * @brief Receive a message with timeout
     *
     * Waits for and receives the next message. If no data arrives
     * within the timeout, returns a Timeout error.
     *
     * @param timeout Maximum time to wait (default 5000ms)
     * @return IPCResult<Message> The received message or error code
     */
    [[nodiscard]] IPCResult<Message> receive(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Check if data is available to read
     *
     * Non-blocking check for pending data.
     *
     * @return bool True if data is available
     */
    [[nodiscard]] bool hasData() const;

    /**
     * @brief Get read file descriptor (for subprocess)
     *
     * Returns the file descriptor for the read end of the pipe.
     * Useful for passing to subprocesses.
     *
     * @return int Read file descriptor (-1 if not open)
     */
    [[nodiscard]] int getReadFd() const noexcept;

    /**
     * @brief Get write file descriptor (for subprocess)
     *
     * Returns the file descriptor for the write end of the pipe.
     * Useful for passing to subprocesses.
     *
     * @return int Write file descriptor (-1 if not open)
     */
    [[nodiscard]] int getWriteFd() const noexcept;

    /**
     * @brief Close the read end (for parent process after fork)
     *
     * Closes the read end of the pipe. Typically called by parent
     * process after forking to clean up unused file descriptors.
     */
    void closeRead();

    /**
     * @brief Close the write end (for parent process after fork)
     *
     * Closes the write end of the pipe. Typically called by parent
     * process after forking to clean up unused file descriptors.
     */
    void closeWrite();

    /**
     * @brief Set non-blocking mode
     *
     * Configures the pipe for non-blocking I/O operations.
     *
     * @param nonBlocking True to enable non-blocking mode
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> setNonBlocking(bool nonBlocking);

    /**
     * @brief Get next sequence ID
     *
     * Returns an incrementing sequence ID for message tracking.
     *
     * @return uint32_t The next sequence ID
     */
    [[nodiscard]] uint32_t nextSequenceId();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Bidirectional channel for full-duplex communication
 *
 * Provides full-duplex communication using two unidirectional pipes:
 * one for parent->child and one for child->parent. Suitable for
 * parent-child process communication (e.g., subprocess execution).
 */
class BidirectionalChannel {
public:
    /**
     * @brief Create a bidirectional channel
     */
    BidirectionalChannel();

    /**
     * @brief Destructor - closes channel
     */
    ~BidirectionalChannel();

    // Disable copy
    BidirectionalChannel(const BidirectionalChannel&) = delete;
    BidirectionalChannel& operator=(const BidirectionalChannel&) = delete;

    /**
     * @brief Create the bidirectional channel
     *
     * Initializes both pipe pairs for bidirectional communication.
     *
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> create();

    /**
     * @brief Close the channel
     *
     * Closes both pipe pairs.
     */
    void close();

    /**
     * @brief Check if channel is open
     *
     * @return bool True if channel is open and operational
     */
    [[nodiscard]] bool isOpen() const noexcept;

    /**
     * @brief Send a message
     *
     * Sends a message through the appropriate pipe (child->parent or
     * parent->child depending on which process calls this).
     *
     * @param message The message to send
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> send(const Message& message);

    /**
     * @brief Receive a message
     *
     * Receives a message from the appropriate pipe (parent->child or
     * child->parent depending on which process calls this).
     *
     * @param timeout Maximum time to wait (default 5000ms)
     * @return IPCResult<Message> The received message or error code
     */
    [[nodiscard]] IPCResult<Message> receive(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Get file descriptors for subprocess
     *
     * Returns the pair of file descriptors that should be passed to
     * the subprocess (read from parent->child, write to child->parent).
     *
     * @return std::pair<int, int> Pair of (readFd, writeFd) for subprocess
     */
    [[nodiscard]] std::pair<int, int> getSubprocessFds() const noexcept;

    /**
     * @brief Setup for parent process (after fork/spawn)
     *
     * Configures the channel for parent process use. Should be called
     * after creating the channel and before forking/spawning the child.
     * Closes file descriptors that the parent doesn't need.
     */
    void setupParent();

    /**
     * @brief Setup for child process (after fork/spawn)
     *
     * Configures the channel for child process use. Should be called
     * in the child process after fork/spawn. Closes file descriptors
     * that the child doesn't need.
     */
    void setupChild();

    /**
     * @brief Perform handshake with subprocess
     *
     * Initiates handshake protocol with the child process. Called by
     * the parent process to establish the IPC connection and verify
     * compatibility.
     *
     * @param timeout Maximum time to wait for handshake response
     * @return IPCResult<HandshakePayload> Subprocess handshake info or error
     */
    [[nodiscard]] IPCResult<HandshakePayload> performHandshake(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Respond to handshake from parent
     *
     * Sends handshake response to the parent process. Called by the
     * child process during initialization.
     *
     * @param payload Handshake payload with subprocess information
     * @return IPCResult<void> Success or error code
     */
    [[nodiscard]] IPCResult<void> respondToHandshake(const HandshakePayload& payload);

private:
    PipeChannel parentToChild_;   ///< Parent -> Child pipe
    PipeChannel childToParent_;   ///< Child -> Parent pipe
    std::atomic<uint32_t> sequenceId_{0};
    mutable std::mutex mutex_;
};

}  // namespace lithium::ipc

#endif  // LITHIUM_SCRIPT_IPC_CHANNEL_HPP
