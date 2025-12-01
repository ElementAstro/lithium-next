// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_RATE_LIMITER_API_RATE_LIMITER_HPP
#define LITHIUM_TARGET_ONLINE_RATE_LIMITER_API_RATE_LIMITER_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace lithium::target::online {

/**
 * @brief Rate limit configuration per provider
 */
struct RateLimitRule {
    size_t maxRequestsPerSecond = 10;
    size_t maxRequestsPerMinute = 100;
    size_t maxRequestsPerHour = 1000;
    size_t burstLimit = 20;
    bool respectRetryAfter = true;
};

/**
 * @brief Rate limiter statistics
 */
struct RateLimiterStats {
    size_t totalRequests = 0;
    size_t throttledRequests = 0;
    size_t successfulRequests = 0;
    std::chrono::system_clock::time_point lastRequest;
    std::chrono::system_clock::time_point lastThrottle;
};

/**
 * @brief API rate limiter with token bucket algorithm
 *
 * Provides per-provider rate limiting with configurable limits.
 * Uses token bucket algorithm to manage request quotas across multiple
 * time windows (second, minute, hour). Respects server-sent Retry-After
 * headers and maintains sliding window counters for accurate rate limiting.
 *
 * Thread-safe for concurrent access with mutex synchronization.
 *
 * @thread_safe All public methods are thread-safe
 *
 * @example
 * ```cpp
 * ApiRateLimiter limiter;
 * RateLimitRule rule;
 * rule.maxRequestsPerSecond = 5;
 * rule.maxRequestsPerMinute = 100;
 * limiter.setProviderLimit("SIMBAD", rule);
 *
 * // Try non-blocking acquire
 * auto waitTime = limiter.tryAcquire("SIMBAD");
 * if (waitTime) {
 *     std::this_thread::sleep_for(*waitTime);
 * }
 * // Make request...
 * limiter.recordRequestComplete("SIMBAD", true);
 * ```
 */
class ApiRateLimiter {
public:
    ApiRateLimiter();
    ~ApiRateLimiter();

    // Non-copyable, movable
    ApiRateLimiter(const ApiRateLimiter&) = delete;
    ApiRateLimiter& operator=(const ApiRateLimiter&) = delete;
    ApiRateLimiter(ApiRateLimiter&&) noexcept;
    ApiRateLimiter& operator=(ApiRateLimiter&&) noexcept;

    /**
     * @brief Set rate limit rule for a provider
     *
     * Initializes or updates the rate limiting configuration for a provider.
     * If the provider already exists, its configuration is replaced.
     *
     * @param provider Provider identifier (e.g., "SIMBAD", "VizieR")
     * @param rule Rate limiting rule with limits and configuration
     * @thread_safe This method is thread-safe
     */
    void setProviderLimit(const std::string& provider, const RateLimitRule& rule);

    /**
     * @brief Try to acquire a request slot (non-blocking)
     *
     * Attempts to acquire a request slot without blocking. If rate limited,
     * returns the suggested wait time before the next request should be made.
     *
     * @param provider Provider identifier
     * @return std::nullopt if request is allowed immediately,
     *         wait time in milliseconds if rate limited
     * @thread_safe This method is thread-safe
     *
     * @note Does not consume tokens; use only for checking limits.
     *       Call recordRequestComplete() after making the request.
     */
    [[nodiscard]] auto tryAcquire(const std::string& provider)
        -> std::optional<std::chrono::milliseconds>;

    /**
     * @brief Acquire a request slot (blocking)
     *
     * Blocks the calling thread until a request slot becomes available.
     * This is useful for sequential request patterns where strict ordering
     * is required.
     *
     * @param provider Provider identifier
     * @thread_safe This method is thread-safe
     *
     * @warning May block indefinitely if rate limits are extremely restrictive.
     *          Prefer tryAcquire() with timeout handling for production code.
     */
    void acquire(const std::string& provider);

    /**
     * @brief Record a rate limit response from server
     *
     * Handles rate limit responses from API servers (HTTP 429).
     * Updates internal state to respect the Retry-After header
     * and prevents further requests until the wait period expires.
     *
     * @param provider Provider identifier
     * @param retryAfter Seconds to wait before retrying (from Retry-After header)
     * @thread_safe This method is thread-safe
     *
     * @note Should be called when receiving HTTP 429 Too Many Requests
     */
    void recordRateLimitResponse(const std::string& provider,
                                 std::chrono::seconds retryAfter);

    /**
     * @brief Mark a request as completed
     *
     * Records request completion and updates statistics.
     * Must be called after each API request to maintain accurate
     * rate limit state and statistics.
     *
     * @param provider Provider identifier
     * @param success Whether the request was successful
     * @thread_safe This method is thread-safe
     *
     * @note Always call this after an API request, whether successful or not
     */
    void recordRequestComplete(const std::string& provider, bool success);

    /**
     * @brief Get statistics for a provider
     *
     * Retrieves cumulative and time-based statistics for a provider.
     * Useful for monitoring and debugging rate limiter behavior.
     *
     * @param provider Provider identifier
     * @return Statistics including request counts and timing information
     * @thread_safe This method is thread-safe
     */
    [[nodiscard]] auto getStats(const std::string& provider) const -> RateLimiterStats;

    /**
     * @brief Reset all rate limit state for a provider
     *
     * Clears all token buckets, windows, and statistics for a specific provider.
     * Useful for resetting after configuration changes or manual intervention.
     *
     * @param provider Provider identifier
     * @thread_safe This method is thread-safe
     */
    void reset(const std::string& provider);

    /**
     * @brief Reset all rate limit state
     *
     * Clears state for all providers. Use with caution in production.
     *
     * @thread_safe This method is thread-safe
     */
    void resetAll();

    /**
     * @brief Check if provider is currently rate limited
     *
     * Non-blocking check to determine if a provider is currently
     * rate limited (either by token bucket or Retry-After).
     *
     * @param provider Provider identifier
     * @return true if provider is rate limited, false otherwise
     * @thread_safe This method is thread-safe
     */
    [[nodiscard]] auto isRateLimited(const std::string& provider) const -> bool;

    /**
     * @brief Get time until rate limit resets
     *
     * Returns the time remaining until the rate limit constraint
     * that is causing the current bottleneck expires.
     *
     * @param provider Provider identifier
     * @return Time in milliseconds until next available slot
     * @thread_safe This method is thread-safe
     */
    [[nodiscard]] auto getTimeUntilReset(const std::string& provider) const
        -> std::chrono::milliseconds;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_RATE_LIMITER_API_RATE_LIMITER_HPP
