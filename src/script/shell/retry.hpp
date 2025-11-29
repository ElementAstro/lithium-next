/*
 * retry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file retry.hpp
 * @brief Retry strategy and executor for shell script execution
 * @date 2024-11-29
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_RETRY_HPP
#define LITHIUM_SCRIPT_SHELL_RETRY_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>

#include "types.hpp"

namespace lithium::shell {

/**
 * @brief Enumeration of retry strategies
 */
enum class RetryStrategy {
    None,        ///< No retry attempts
    Linear,      ///< Linear backoff (constant delay)
    Exponential, ///< Exponential backoff (delay multiplies)
    Custom       ///< Custom retry logic via callback
};

/**
 * @brief Configuration for retry behavior
 *
 * Defines how operations should be retried on failure.
 * Supports multiple strategies with configurable parameters.
 */
struct RetryConfig {
    /// Retry strategy to use
    RetryStrategy strategy{RetryStrategy::None};

    /// Maximum number of retry attempts
    int maxRetries{0};

    /// Initial delay before first retry (milliseconds)
    std::chrono::milliseconds initialDelay{100};

    /// Maximum delay between retries (milliseconds)
    std::chrono::milliseconds maxDelay{30000};

    /// Multiplier for exponential backoff
    float multiplier{2.0f};

    /// Custom callback to determine if retry should occur
    /// Returns true if operation should be retried
    std::function<bool(int attemptNumber, const ScriptExecutionResult& result)>
        shouldRetry;

    /**
     * @brief Default constructor with no retries
     */
    RetryConfig() = default;

    /**
     * @brief Constructor with strategy and max retries
     * @param strategy Retry strategy to use
     * @param maxRetries Maximum number of retries
     */
    RetryConfig(RetryStrategy strategy, int maxRetries)
        : strategy(strategy), maxRetries(maxRetries) {}

    /**
     * @brief Constructor with all parameters
     * @param strategy Retry strategy
     * @param maxRetries Maximum retries
     * @param initialDelay Initial delay in milliseconds
     * @param maxDelay Maximum delay in milliseconds
     * @param multiplier Backoff multiplier
     */
    RetryConfig(RetryStrategy strategy, int maxRetries,
                std::chrono::milliseconds initialDelay,
                std::chrono::milliseconds maxDelay, float multiplier)
        : strategy(strategy),
          maxRetries(maxRetries),
          initialDelay(initialDelay),
          maxDelay(maxDelay),
          multiplier(multiplier) {}
};

/**
 * @brief Information about a retry attempt
 */
struct RetryAttemptInfo {
    /// Current attempt number (0-based)
    int attemptNumber{0};

    /// Total maximum attempts
    int totalAttempts{0};

    /// Delay before this retry attempt (milliseconds)
    std::chrono::milliseconds delayMs{0};

    /// The result that triggered the retry
    ScriptExecutionResult lastResult;
};

/**
 * @brief Executor for scripts with retry capability
 *
 * Provides configurable retry behavior for script execution with support
 * for multiple backoff strategies. Handles timing, attempt tracking, and
 * custom retry logic.
 */
class RetryExecutor {
public:
    /**
     * @brief Constructor with retry configuration
     * @param config Retry configuration
     */
    explicit RetryExecutor(const RetryConfig& config);

    /**
     * @brief Default constructor (no retries)
     */
    RetryExecutor() = default;

    /**
     * @brief Set retry configuration
     * @param config New retry configuration
     */
    void setRetryConfig(const RetryConfig& config);

    /**
     * @brief Get current retry configuration
     * @return Current retry config
     */
    [[nodiscard]] const RetryConfig& getRetryConfig() const;

    /**
     * @brief Execute operation with retry support
     *
     * Attempts to execute the given operation with retry logic based on
     * the configured strategy. On failure, may retry with appropriate delays.
     *
     * @param operation Function that performs the actual execution
     * @return Final execution result after all attempts
     */
    ScriptExecutionResult executeWithRetry(
        const std::function<ScriptExecutionResult()>& operation);

    /**
     * @brief Get information about the last retry attempt
     * @return Optional containing retry attempt info if a retry was performed
     */
    [[nodiscard]] const std::optional<RetryAttemptInfo>& getLastRetryInfo()
        const;

    /**
     * @brief Reset retry statistics
     */
    void reset();

    /**
     * @brief Calculate delay for next retry attempt
     * @param attemptNumber Current attempt number (0-based)
     * @return Delay in milliseconds
     */
    [[nodiscard]] std::chrono::milliseconds calculateDelay(
        int attemptNumber) const;

private:
    /// Current retry configuration
    RetryConfig config_;

    /// Information about the last retry attempt
    std::optional<RetryAttemptInfo> lastRetryInfo_;

    /**
     * @brief Check if operation should be retried
     * @param attemptNumber Current attempt number
     * @param result Last execution result
     * @return true if retry should occur
     */
    bool shouldRetryOperation(int attemptNumber,
                               const ScriptExecutionResult& result) const;

    /**
     * @brief Sleep for specified milliseconds
     * @param ms Milliseconds to sleep
     */
    void sleep(std::chrono::milliseconds ms) const;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_RETRY_HPP
