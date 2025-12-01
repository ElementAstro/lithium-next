// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "target/online/rate_limiter/api_rate_limiter.hpp"

using namespace lithium::target::online;

class ApiRateLimiterTest : public ::testing::Test {
protected:
    ApiRateLimiter limiter;

    void SetUp() override {
        // Default setup is empty
    }
};

// ============================================================================
// Basic Configuration Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, SetProviderLimit) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 10;

    // Should not throw
    EXPECT_NO_THROW(limiter.setProviderLimit("SIMBAD", rule));
}

TEST_F(ApiRateLimiterTest, TryAcquireWithoutConfiguration) {
    auto result = limiter.tryAcquire("UNKNOWN_PROVIDER");
    // Should allow request when no limit is configured
    EXPECT_FALSE(result.has_value());
}

TEST_F(ApiRateLimiterTest, TryAcquireWithinLimits) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 10;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 20;

    limiter.setProviderLimit("TEST", rule);

    // First request should be allowed
    auto result = limiter.tryAcquire("TEST");
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Token Bucket Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, ExhaustTokens) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 2;

    limiter.setProviderLimit("TEST", rule);

    // Exhaust burst tokens
    for (int i = 0; i < 2; ++i) {
        auto result = limiter.tryAcquire("TEST");
        EXPECT_FALSE(result.has_value());
        limiter.recordRequestComplete("TEST", true);
    }

    // Next request should be rate limited
    auto result = limiter.tryAcquire("TEST");
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result.value().count(), 0);
}

TEST_F(ApiRateLimiterTest, TokenRefill) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 2;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 1;

    limiter.setProviderLimit("TEST", rule);

    // Use the single burst token
    auto result1 = limiter.tryAcquire("TEST");
    EXPECT_FALSE(result1.has_value());
    limiter.recordRequestComplete("TEST", true);

    // Next request should be rate limited
    auto result2 = limiter.tryAcquire("TEST");
    EXPECT_TRUE(result2.has_value());

    // Wait for token refill
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Should have a token now
    auto result3 = limiter.tryAcquire("TEST");
    EXPECT_FALSE(result3.has_value());
}

// ============================================================================
// Per-Minute Limit Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, MinuteLimitEnforcement) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 100;
    rule.maxRequestsPerMinute = 3;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 100;

    limiter.setProviderLimit("TEST", rule);

    // Record 3 requests
    for (int i = 0; i < 3; ++i) {
        auto result = limiter.tryAcquire("TEST");
        EXPECT_FALSE(result.has_value());
        limiter.recordRequestComplete("TEST", true);
    }

    // 4th request should be blocked
    auto result = limiter.tryAcquire("TEST");
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Server Rate Limit Response Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, RecordRateLimitResponse) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 100;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 100;
    rule.burstLimit = 50;
    rule.respectRetryAfter = true;

    limiter.setProviderLimit("TEST", rule);

    // Should initially allow requests
    auto result1 = limiter.tryAcquire("TEST");
    EXPECT_FALSE(result1.has_value());

    // Simulate server rate limit response
    limiter.recordRateLimitResponse("TEST", std::chrono::seconds(1));

    // Should now be rate limited
    auto result2 = limiter.tryAcquire("TEST");
    EXPECT_TRUE(result2.has_value());
    EXPECT_LE(result2.value().count(), 1100);  // ~1 second
    EXPECT_GT(result2.value().count(), 900);   // At least ~900ms
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, GetStats) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 10;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 20;

    limiter.setProviderLimit("TEST", rule);

    // Make some requests
    for (int i = 0; i < 5; ++i) {
        limiter.tryAcquire("TEST");
        limiter.recordRequestComplete("TEST", i % 2 == 0);
    }

    auto stats = limiter.getStats("TEST");
    EXPECT_EQ(stats.totalRequests, 5);
    EXPECT_EQ(stats.successfulRequests, 3);
    EXPECT_GE(stats.throttledRequests, 0);
}

TEST_F(ApiRateLimiterTest, GetStatsUnknownProvider) {
    auto stats = limiter.getStats("UNKNOWN");
    EXPECT_EQ(stats.totalRequests, 0);
    EXPECT_EQ(stats.successfulRequests, 0);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, ResetProvider) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.maxRequestsPerMinute = 100;
    rule.maxRequestsPerHour = 1000;
    rule.burstLimit = 2;

    limiter.setProviderLimit("TEST", rule);

    // Exhaust tokens
    for (int i = 0; i < 2; ++i) {
        limiter.tryAcquire("TEST");
        limiter.recordRequestComplete("TEST", true);
    }

    // Should be rate limited
    EXPECT_TRUE(limiter.isRateLimited("TEST"));

    // Reset
    limiter.reset("TEST");

    // Should now be allowed
    EXPECT_FALSE(limiter.isRateLimited("TEST"));
}

TEST_F(ApiRateLimiterTest, ResetAll) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.burstLimit = 2;

    limiter.setProviderLimit("PROVIDER1", rule);
    limiter.setProviderLimit("PROVIDER2", rule);

    // Exhaust tokens for both
    for (int i = 0; i < 2; ++i) {
        limiter.tryAcquire("PROVIDER1");
        limiter.recordRequestComplete("PROVIDER1", true);
        limiter.tryAcquire("PROVIDER2");
        limiter.recordRequestComplete("PROVIDER2", true);
    }

    EXPECT_TRUE(limiter.isRateLimited("PROVIDER1"));
    EXPECT_TRUE(limiter.isRateLimited("PROVIDER2"));

    // Reset all
    limiter.resetAll();

    EXPECT_FALSE(limiter.isRateLimited("PROVIDER1"));
    EXPECT_FALSE(limiter.isRateLimited("PROVIDER2"));
}

// ============================================================================
// Status Check Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, IsRateLimited) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.burstLimit = 1;

    limiter.setProviderLimit("TEST", rule);

    EXPECT_FALSE(limiter.isRateLimited("TEST"));

    // Exhaust burst
    limiter.tryAcquire("TEST");
    limiter.recordRequestComplete("TEST", true);

    EXPECT_TRUE(limiter.isRateLimited("TEST"));
}

TEST_F(ApiRateLimiterTest, GetTimeUntilReset) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 5;
    rule.burstLimit = 1;

    limiter.setProviderLimit("TEST", rule);

    // No rate limit initially
    auto time1 = limiter.getTimeUntilReset("TEST");
    EXPECT_EQ(time1.count(), 0);

    // Exhaust burst
    limiter.tryAcquire("TEST");
    limiter.recordRequestComplete("TEST", true);

    // Should have wait time
    auto time2 = limiter.getTimeUntilReset("TEST");
    EXPECT_GT(time2.count(), 0);
    EXPECT_LE(time2.count(), 1000);  // Less than 1 second for 5 req/s
}

// ============================================================================
// Multiple Provider Tests
// ============================================================================

TEST_F(ApiRateLimiterTest, MultipleProvidersIndependent) {
    RateLimitRule rule1;
    rule1.maxRequestsPerSecond = 10;
    rule1.burstLimit = 2;

    RateLimitRule rule2;
    rule2.maxRequestsPerSecond = 5;
    rule2.burstLimit = 1;

    limiter.setProviderLimit("SIMBAD", rule1);
    limiter.setProviderLimit("VIZIER", rule2);

    // SIMBAD should allow 2 requests
    EXPECT_FALSE(limiter.tryAcquire("SIMBAD").has_value());
    limiter.recordRequestComplete("SIMBAD", true);

    EXPECT_FALSE(limiter.tryAcquire("SIMBAD").has_value());
    limiter.recordRequestComplete("SIMBAD", true);

    EXPECT_TRUE(limiter.tryAcquire("SIMBAD").has_value());

    // VIZIER should allow 1 request
    EXPECT_FALSE(limiter.tryAcquire("VIZIER").has_value());
    limiter.recordRequestComplete("VIZIER", true);

    EXPECT_TRUE(limiter.tryAcquire("VIZIER").has_value());
}

// ============================================================================
// Concurrency Tests (Basic)
// ============================================================================

TEST_F(ApiRateLimiterTest, ConcurrentAcquire) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 100;
    rule.maxRequestsPerMinute = 1000;
    rule.maxRequestsPerHour = 10000;
    rule.burstLimit = 100;

    limiter.setProviderLimit("TEST", rule);

    // Launch multiple threads
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            auto result = limiter.tryAcquire("TEST");
            if (!result.has_value()) {
                successCount++;
                limiter.recordRequestComplete("TEST", true);
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // At least some should succeed
    EXPECT_GT(successCount, 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ApiRateLimiterTest, ZeroRequestsPerSecond) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 0;
    rule.burstLimit = 1;

    limiter.setProviderLimit("TEST", rule);

    auto result = limiter.tryAcquire("TEST");
    EXPECT_FALSE(result.has_value());
    limiter.recordRequestComplete("TEST", true);

    // Should always be rate limited with 0 requests/sec
    result = limiter.tryAcquire("TEST");
    EXPECT_TRUE(result.has_value());
}

TEST_F(ApiRateLimiterTest, VeryLargeRequestLimits) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 1000000;
    rule.maxRequestsPerMinute = 100000000;
    rule.maxRequestsPerHour = 10000000000;
    rule.burstLimit = 10000;

    limiter.setProviderLimit("TEST", rule);

    // Should allow many requests
    for (int i = 0; i < 100; ++i) {
        auto result = limiter.tryAcquire("TEST");
        EXPECT_FALSE(result.has_value());
        limiter.recordRequestComplete("TEST", true);
    }
}

TEST_F(ApiRateLimiterTest, FailedRequestsNotCounted) {
    RateLimitRule rule;
    rule.maxRequestsPerSecond = 100;
    rule.burstLimit = 50;

    limiter.setProviderLimit("TEST", rule);

    auto stats1 = limiter.getStats("TEST");
    EXPECT_EQ(stats1.totalRequests, 0);
    EXPECT_EQ(stats1.successfulRequests, 0);

    limiter.recordRequestComplete("TEST", false);

    auto stats2 = limiter.getStats("TEST");
    EXPECT_EQ(stats2.totalRequests, 1);
    EXPECT_EQ(stats2.successfulRequests, 0);  // Failed request
}
