// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "api_rate_limiter.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "atom/log/spdlog_logger.hpp"

namespace lithium::target::online {

/**
 * @brief Per-provider token bucket state
 *
 * Maintains sliding window counters for three time scales:
 * - Per-second: strict token bucket for burst control
 * - Per-minute: medium-term quota enforcement
 * - Per-hour: long-term quota enforcement
 */
struct TokenBucket {
    // Configuration
    RateLimitRule rule;

    // Token bucket state (per second)
    double tokensPerSecond = 0.0;
    std::chrono::system_clock::time_point lastRefillSecond =
        std::chrono::system_clock::now();

    // Sliding window counters for minute and hour
    struct WindowCounter {
        std::deque<std::chrono::system_clock::time_point> timestamps;
        size_t count = 0;
    };

    WindowCounter minuteWindow;
    WindowCounter hourWindow;

    // Server-imposed retry-after state
    std::chrono::system_clock::time_point retryAfterTime =
        std::chrono::system_clock::now();

    // Statistics
    RateLimiterStats stats;

    explicit TokenBucket(const RateLimitRule& r) : rule(r) {
        // Initialize tokens to burst limit
        tokensPerSecond = static_cast<double>(r.burstLimit);
        stats.lastRequest = std::chrono::system_clock::now();
        stats.lastThrottle = std::chrono::system_clock::now();
    }
};

/**
 * @brief Implementation class using PIMPL pattern
 */
class ApiRateLimiter::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    // Delete copy operations
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void setProviderLimit(const std::string& provider, const RateLimitRule& rule) {
        std::unique_lock lock(mutex_);
        auto it = buckets_.find(provider);
        if (it != buckets_.end()) {
            it->second->rule = rule;
            LOG_INFO("Updated rate limit for provider '{}': {} req/s, {} req/min, "
                     "{} req/h, burst={}",
                     provider, rule.maxRequestsPerSecond,
                     rule.maxRequestsPerMinute, rule.maxRequestsPerHour,
                     rule.burstLimit);
        } else {
            buckets_[provider] = std::make_unique<TokenBucket>(rule);
            LOG_INFO("Initialized rate limit for provider '{}': {} req/s, {} req/min, "
                     "{} req/h, burst={}",
                     provider, rule.maxRequestsPerSecond,
                     rule.maxRequestsPerMinute, rule.maxRequestsPerHour,
                     rule.burstLimit);
        }
    }

    [[nodiscard]] auto tryAcquire(const std::string& provider)
        -> std::optional<std::chrono::milliseconds> {
        std::unique_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            // No limit configured for this provider
            return std::nullopt;
        }

        auto& bucket = *it->second;
        const auto now = std::chrono::system_clock::now();

        // Check server-imposed retry-after
        if (now < bucket.retryAfterTime) {
            const auto waitTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    bucket.retryAfterTime - now);
            DLOG_DEBUG("Provider '{}' rate limited by Retry-After: {} ms",
                       provider, waitTime.count());
            return waitTime;
        }

        // Refill tokens for per-second window
        refillTokens(bucket, now);

        // Check per-second limit
        if (bucket.tokensPerSecond < 1.0) {
            const auto waitTime = calculateWaitTime(bucket);
            LOG_DEBUG("Provider '{}' per-second limit reached, wait: {} ms",
                      provider, waitTime.count());
            bucket.stats.throttledRequests++;
            bucket.stats.lastThrottle = now;
            return waitTime;
        }

        // Clean up and check per-minute window
        cleanupWindow(bucket.minuteWindow, now,
                      std::chrono::minutes(1));
        if (bucket.minuteWindow.count >=
            bucket.rule.maxRequestsPerMinute) {
            const auto waitTime =
                calculateWindowWaitTime(bucket.minuteWindow, now,
                                       std::chrono::minutes(1));
            LOG_DEBUG("Provider '{}' per-minute limit reached, wait: {} ms",
                      provider, waitTime.count());
            bucket.stats.throttledRequests++;
            bucket.stats.lastThrottle = now;
            return waitTime;
        }

        // Clean up and check per-hour window
        cleanupWindow(bucket.hourWindow, now,
                      std::chrono::hours(1));
        if (bucket.hourWindow.count >= bucket.rule.maxRequestsPerHour) {
            const auto waitTime =
                calculateWindowWaitTime(bucket.hourWindow, now,
                                       std::chrono::hours(1));
            LOG_DEBUG("Provider '{}' per-hour limit reached, wait: {} ms",
                      provider, waitTime.count());
            bucket.stats.throttledRequests++;
            bucket.stats.lastThrottle = now;
            return waitTime;
        }

        // All checks passed - tentatively allow
        // Note: tokens and window counts are NOT consumed here
        // They will be consumed in recordRequestComplete()
        DLOG_DEBUG("Provider '{}' request allowed. Tokens: {:.1f}, "
                   "minute: {}/{}, hour: {}/{}",
                   provider, bucket.tokensPerSecond,
                   bucket.minuteWindow.count,
                   bucket.rule.maxRequestsPerMinute,
                   bucket.hourWindow.count,
                   bucket.rule.maxRequestsPerHour);

        return std::nullopt;
    }

    void acquire(const std::string& provider) {
        while (true) {
            auto waitTime = tryAcquire(provider);
            if (!waitTime) {
                return;  // Acquired immediately
            }

            // Sleep and retry
            std::this_thread::sleep_for(*waitTime);
        }
    }

    void recordRateLimitResponse(const std::string& provider,
                                 std::chrono::seconds retryAfter) {
        std::unique_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            LOG_WARN("Rate limit response for unknown provider '{}'", provider);
            return;
        }

        auto& bucket = *it->second;
        const auto now = std::chrono::system_clock::now();
        bucket.retryAfterTime = now + retryAfter;

        LOG_WARN("Provider '{}' returned rate limit (Retry-After: {} seconds)",
                 provider, retryAfter.count());
    }

    void recordRequestComplete(const std::string& provider, bool success) {
        std::unique_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            LOG_WARN("Request complete for unknown provider '{}'", provider);
            return;
        }

        auto& bucket = *it->second;
        const auto now = std::chrono::system_clock::now();

        // Consume one token
        bucket.tokensPerSecond -= 1.0;

        // Record in sliding windows
        bucket.minuteWindow.timestamps.push_back(now);
        bucket.minuteWindow.count++;

        bucket.hourWindow.timestamps.push_back(now);
        bucket.hourWindow.count++;

        // Update statistics
        bucket.stats.totalRequests++;
        bucket.stats.lastRequest = now;
        if (success) {
            bucket.stats.successfulRequests++;
        }

        DLOG_DEBUG("Provider '{}' request recorded. Tokens: {:.1f}, "
                   "minute: {}/{}, hour: {}/{}",
                   provider, bucket.tokensPerSecond,
                   bucket.minuteWindow.count,
                   bucket.rule.maxRequestsPerMinute,
                   bucket.hourWindow.count,
                   bucket.rule.maxRequestsPerHour);
    }

    [[nodiscard]] auto getStats(const std::string& provider) const
        -> RateLimiterStats {
        std::shared_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            return RateLimiterStats{};
        }

        return it->second->stats;
    }

    void reset(const std::string& provider) {
        std::unique_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            LOG_WARN("Attempted to reset unknown provider '{}'", provider);
            return;
        }

        const auto& rule = it->second->rule;
        it->second = std::make_unique<TokenBucket>(rule);

        LOG_INFO("Reset rate limiter state for provider '{}'", provider);
    }

    void resetAll() {
        std::unique_lock lock(mutex_);

        for (auto& [provider, bucket] : buckets_) {
            const auto& rule = bucket->rule;
            bucket = std::make_unique<TokenBucket>(rule);
        }

        LOG_INFO("Reset rate limiter state for all providers");
    }

    [[nodiscard]] auto isRateLimited(const std::string& provider) const -> bool {
        std::shared_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            return false;
        }

        const auto& bucket = *it->second;
        const auto now = std::chrono::system_clock::now();

        // Check Retry-After
        if (now < bucket.retryAfterTime) {
            return true;
        }

        // Check token bucket
        if (bucket.tokensPerSecond < 1.0) {
            return true;
        }

        return false;
    }

    [[nodiscard]] auto getTimeUntilReset(const std::string& provider) const
        -> std::chrono::milliseconds {
        std::shared_lock lock(mutex_);

        auto it = buckets_.find(provider);
        if (it == buckets_.end()) {
            return std::chrono::milliseconds(0);
        }

        const auto& bucket = *it->second;
        const auto now = std::chrono::system_clock::now();

        // Check Retry-After first
        if (now < bucket.retryAfterTime) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                bucket.retryAfterTime - now);
        }

        // Return time until next token is available
        return calculateWaitTime(bucket);
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<TokenBucket>> buckets_;

    /**
     * @brief Refill tokens based on elapsed time
     */
    void refillTokens(TokenBucket& bucket,
                      std::chrono::system_clock::time_point now) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - bucket.lastRefillSecond);
        const double tokensToAdd =
            (static_cast<double>(elapsed.count()) / 1000.0) *
            static_cast<double>(bucket.rule.maxRequestsPerSecond);

        bucket.tokensPerSecond =
            std::min(static_cast<double>(bucket.rule.burstLimit),
                     bucket.tokensPerSecond + tokensToAdd);

        if (tokensToAdd > 0.1) {  // Only update if meaningful change
            bucket.lastRefillSecond = now;
        }
    }

    /**
     * @brief Remove old entries from sliding window
     */
    void cleanupWindow(TokenBucket::WindowCounter& window,
                       std::chrono::system_clock::time_point now,
                       std::chrono::nanoseconds windowSize) {
        while (!window.timestamps.empty() &&
               (now - window.timestamps.front()) > windowSize) {
            window.timestamps.pop_front();
            window.count--;
        }
    }

    /**
     * @brief Calculate wait time for token bucket
     */
    [[nodiscard]] static auto calculateWaitTime(const TokenBucket& bucket)
        -> std::chrono::milliseconds {
        const auto tokensNeeded = 1.0 - bucket.tokensPerSecond;
        const auto timeNeeded = static_cast<double>(bucket.rule.maxRequestsPerSecond) == 0.0
            ? 1000.0
            : (tokensNeeded / static_cast<double>(bucket.rule.maxRequestsPerSecond)) *
              1000.0;

        return std::chrono::milliseconds(
            static_cast<long long>(std::ceil(timeNeeded)));
    }

    /**
     * @brief Calculate wait time for sliding window
     */
    [[nodiscard]] static auto calculateWindowWaitTime(
        const TokenBucket::WindowCounter& window,
        std::chrono::system_clock::time_point now,
        std::chrono::nanoseconds windowSize) -> std::chrono::milliseconds {
        if (window.timestamps.empty()) {
            return std::chrono::milliseconds(0);
        }

        const auto oldestTimestamp = window.timestamps.front();
        const auto resetTime = oldestTimestamp + windowSize;

        if (now >= resetTime) {
            return std::chrono::milliseconds(0);
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(
            resetTime - now);
    }
};

// ============================================================================
// ApiRateLimiter Implementation
// ============================================================================

ApiRateLimiter::ApiRateLimiter() : pImpl_(std::make_unique<Impl>()) {}

ApiRateLimiter::~ApiRateLimiter() = default;

ApiRateLimiter::ApiRateLimiter(ApiRateLimiter&&) noexcept = default;

ApiRateLimiter& ApiRateLimiter::operator=(ApiRateLimiter&&) noexcept = default;

void ApiRateLimiter::setProviderLimit(const std::string& provider,
                                       const RateLimitRule& rule) {
    pImpl_->setProviderLimit(provider, rule);
}

auto ApiRateLimiter::tryAcquire(const std::string& provider)
    -> std::optional<std::chrono::milliseconds> {
    return pImpl_->tryAcquire(provider);
}

void ApiRateLimiter::acquire(const std::string& provider) {
    pImpl_->acquire(provider);
}

void ApiRateLimiter::recordRateLimitResponse(const std::string& provider,
                                             std::chrono::seconds retryAfter) {
    pImpl_->recordRateLimitResponse(provider, retryAfter);
}

void ApiRateLimiter::recordRequestComplete(const std::string& provider,
                                           bool success) {
    pImpl_->recordRequestComplete(provider, success);
}

auto ApiRateLimiter::getStats(const std::string& provider) const
    -> RateLimiterStats {
    return pImpl_->getStats(provider);
}

void ApiRateLimiter::reset(const std::string& provider) {
    pImpl_->reset(provider);
}

void ApiRateLimiter::resetAll() { pImpl_->resetAll(); }

auto ApiRateLimiter::isRateLimited(const std::string& provider) const -> bool {
    return pImpl_->isRateLimited(provider);
}

auto ApiRateLimiter::getTimeUntilReset(const std::string& provider) const
    -> std::chrono::milliseconds {
    return pImpl_->getTimeUntilReset(provider);
}

}  // namespace lithium::target::online
