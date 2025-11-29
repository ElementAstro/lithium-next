/*
 * lifecycle.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "lifecycle.hpp"
#include "process_spawning.hpp"
#include "../ipc/message.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lithium::isolated {

ProcessLifecycle::ProcessLifecycle() = default;

ProcessLifecycle::~ProcessLifecycle() {
    kill();
}

ProcessLifecycle::ProcessLifecycle(ProcessLifecycle&& other) noexcept
    : channel_(std::move(other.channel_)),
      running_(other.running_.load()),
      cancelled_(other.cancelled_.load()),
      processId_(other.processId_) {
#ifdef _WIN32
    processHandle_ = other.processHandle_;
    other.processHandle_ = nullptr;
#endif
    other.processId_ = -1;
    other.running_ = false;
}

ProcessLifecycle& ProcessLifecycle::operator=(ProcessLifecycle&& other) noexcept {
    if (this != &other) {
        kill();
        channel_ = std::move(other.channel_);
        running_ = other.running_.load();
        cancelled_ = other.cancelled_.load();
        processId_ = other.processId_;
#ifdef _WIN32
        processHandle_ = other.processHandle_;
        other.processHandle_ = nullptr;
#endif
        other.processId_ = -1;
        other.running_ = false;
    }
    return *this;
}

void ProcessLifecycle::setChannel(std::shared_ptr<ipc::BidirectionalChannel> channel) {
    channel_ = std::move(channel);
}

void ProcessLifecycle::setProcessId(int processId) {
    processId_ = processId;
#ifdef _WIN32
    if (processHandle_) {
        CloseHandle(static_cast<HANDLE>(processHandle_));
    }
    processHandle_ = ProcessSpawner::getProcessHandle(processId);
#endif
}

void ProcessLifecycle::setRunning(bool running) {
    running_ = running;
}

bool ProcessLifecycle::isRunning() const {
    return running_;
}

int ProcessLifecycle::getProcessId() const {
    return processId_;
}

bool ProcessLifecycle::cancel() {
    if (!running_) {
        return false;
    }

    cancelled_ = true;

    // Try to send cancel message via IPC
    if (channel_) {
        try {
            auto msg = ipc::Message::create(ipc::MessageType::Cancel,
                                            nlohmann::json::object());
            channel_->send(msg);
        } catch (...) {
            // Ignore errors, we may kill the process anyway
        }
    }

    return true;
}

bool ProcessLifecycle::isCancelled() const {
    return cancelled_;
}

void ProcessLifecycle::resetCancellation() {
    cancelled_ = false;
}

void ProcessLifecycle::kill() {
    if (!running_) return;

    running_ = false;

#ifdef _WIN32
    if (processHandle_) {
        TerminateProcess(static_cast<HANDLE>(processHandle_), 1);
        CloseHandle(static_cast<HANDLE>(processHandle_));
        processHandle_ = nullptr;
    }
#else
    if (processId_ > 0) {
        ProcessSpawner::killProcess(processId_);
        processId_ = -1;
    }
#endif

    if (channel_) {
        channel_->close();
        channel_.reset();
    }

    spdlog::debug("Killed isolated Python process");
}

void ProcessLifecycle::waitForExit(int timeoutMs) {
    if (timeoutMs == 0) {
        timeoutMs = 5000;
    }

#ifdef _WIN32
    if (processHandle_) {
        WaitForSingleObject(static_cast<HANDLE>(processHandle_),
                           static_cast<DWORD>(timeoutMs));
        CloseHandle(static_cast<HANDLE>(processHandle_));
        processHandle_ = nullptr;
    }
#else
    if (processId_ > 0) {
        ProcessSpawner::waitForProcess(processId_, timeoutMs);
        processId_ = -1;
    }
#endif
    running_ = false;
}

void ProcessLifecycle::cleanup() {
#ifdef _WIN32
    if (processHandle_) {
        CloseHandle(static_cast<HANDLE>(processHandle_));
        processHandle_ = nullptr;
    }
#endif
    if (channel_) {
        channel_->close();
        channel_.reset();
    }
    processId_ = -1;
    running_ = false;
    cancelled_ = false;
}

}  // namespace lithium::isolated
