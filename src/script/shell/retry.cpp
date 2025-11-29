/*
 * retry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file retry.cpp
 * @brief Retry strategy implementation for shell script execution
 * @date 2024-11-29
 */

#include "retry.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::shell {

RetryExecutor::RetryExecutor(const RetryConfig& config) : config_(config) {}

void RetryExecutor::setRetryConfig(const RetryConfig& config) {
    config_ = config;
    lastRetryInfo_.reset();
}

const RetryConfig& RetryExecutor::getRetryConfig() const {
    return config_;
}

ScriptExecutionResult RetryExecutor::executeWithRetry(
    const std::function<ScriptExecutionResult()>& operation) {
    if (!operation) {
        spdlog::error("RetryExecutor: operation callback is null");
        return ScriptExecutionResult{false, -1, "", "Operation is null", {}};
    }

    // If no retries configured, execute once and return
    if (config_.strategy == RetryStrategy::None || config_.maxRetries <= 0) {
        spdlog::debug("RetryExecutor: executing without retry");
        return operation();
    }

    spdlog::debug("RetryExecutor: starting execution with retry strategy={}",
                  static_cast<int>(config_.strategy));

    ScriptExecutionResult result = operation();
    int attemptNumber = 0;

    // Retry loop
    while (attemptNumber < config_.maxRetries) {
        // Check if we should retry this result
        if (!shouldRetryOperation(attemptNumber, result)) {
            spdlog::debug(
                "RetryExecutor: operation succeeded or retry not needed after "
                "attempt {}",
                attemptNumber + 1);
            break;
        }

        attemptNumber++;
        std::chrono::milliseconds delay = calculateDelay(attemptNumber);

        spdlog::info(
            "RetryExecutor: retrying after attempt {} (delay={}ms, "
            "exit_code={})",
            attemptNumber, delay.count(), result.exitCode);

        // Store retry attempt info
        lastRetryInfo_ = RetryAttemptInfo{
            attemptNumber,
            config_.maxRetries,
            delay,
            result
        };

        // Sleep before retry
        sleep(delay);

        // Execute again
        result = operation();
    }

    spdlog::debug("RetryExecutor: execution completed after {} attempts",
                  attemptNumber + 1);

    return result;
}

const std::optional<RetryAttemptInfo>& RetryExecutor::getLastRetryInfo()
    const {
    return lastRetryInfo_;
}

void RetryExecutor::reset() {
    lastRetryInfo_.reset();
}

std::chrono::milliseconds RetryExecutor::calculateDelay(
    int attemptNumber) const {
    if (attemptNumber < 0) {
        return std::chrono::milliseconds(0);
    }

    std::chrono::milliseconds delay{0};

    switch (config_.strategy) {
        case RetryStrategy::Linear: {
            // Linear backoff: delay * (attemptNumber + 1)
            delay = config_.initialDelay * (attemptNumber + 1);
            break;
        }

        case RetryStrategy::Exponential: {
            // Exponential backoff: initialDelay * (multiplier ^ attemptNumber)
            float expDelay =
                static_cast<float>(config_.initialDelay.count()) *
                std::pow(config_.multiplier, attemptNumber);
            delay = std::chrono::milliseconds(static_cast<long long>(expDelay));
            break;
        }

        case RetryStrategy::Custom:
        case RetryStrategy::None:
        default: {
            // Use initial delay for custom strategy
            delay = config_.initialDelay;
            break;
        }
    }

    // Cap delay at max delay
    if (delay > config_.maxDelay) {
        spdlog::debug(
            "RetryExecutor: calculated delay {}ms exceeds max delay {}ms, "
            "capping",
            delay.count(), config_.maxDelay.count());
        delay = config_.maxDelay;
    }

    return delay;
}

bool RetryExecutor::shouldRetryOperation(
    int attemptNumber, const ScriptExecutionResult& result) const {
    // If operation succeeded, don't retry
    if (result.success) {
        spdlog::debug("RetryExecutor: operation succeeded, no retry needed");
        return false;
    }

    // If we've exhausted retries, don't retry
    if (attemptNumber >= config_.maxRetries) {
        spdlog::debug(
            "RetryExecutor: exhausted max retries ({}/{}", attemptNumber,
            config_.maxRetries);
        return false;
    }

    // Check custom callback if provided
    if (config_.shouldRetry) {
        bool shouldRetry = config_.shouldRetry(attemptNumber, result);
        spdlog::debug(
            "RetryExecutor: custom shouldRetry callback returned {}", shouldRetry);
        return shouldRetry;
    }

    // Default: retry on failure
    spdlog::debug("RetryExecutor: operation failed with exit code {}, will retry",
                  result.exitCode);
    return true;
}

void RetryExecutor::sleep(std::chrono::milliseconds ms) const {
    if (ms.count() <= 0) {
        return;
    }

    spdlog::debug("RetryExecutor: sleeping for {}ms", ms.count());
    std::this_thread::sleep_for(ms);
}

}  // namespace lithium::shell
