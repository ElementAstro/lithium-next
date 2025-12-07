/*
 * test_rate_limiter.cpp - Tests for Rate Limiter
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/rate_limiter.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(RateLimiterTest, BasicConstruction) {
    EXPECT_NO_THROW(RateLimiter(10, 100ms));
    EXPECT_NO_THROW(RateLimiter(100, 1000ms));
    EXPECT_NO_THROW(RateLimiter(1, 1ms));
}

TEST_F(RateLimiterTest, ConstructionWithDifferentParameters) {
    RateLimiter limiter1(5, 50ms);
    RateLimiter limiter2(1000, 10000ms);

    EXPECT_EQ(limiter1.get_remaining_tokens(), 5);
    EXPECT_EQ(limiter2.get_remaining_tokens(), 1000);
}

// ============================================================================
// Basic Token Bucket Tests
// ============================================================================

TEST_F(RateLimiterTest, AllowRequestConsumesToken) {
    RateLimiter limiter(10, 1000ms);

    int initial_tokens = limiter.get_remaining_tokens();
    EXPECT_TRUE(limiter.allow_request());
    int after_tokens = limiter.get_remaining_tokens();

    EXPECT_EQ(after_tokens, initial_tokens - 1);
}

TEST_F(RateLimiterTest, AllowRequestUntilEmpty) {
    RateLimiter limiter(5, 10000ms);  // Long refill to prevent auto-refill

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.allow_request());
    }

    // Should be denied now
    EXPECT_FALSE(limiter.allow_request());
}

TEST_F(RateLimiterTest, TokenRefillAfterInterval) {
    RateLimiter limiter(5, 50ms);

    // Consume all tokens
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request();
    }

    EXPECT_FALSE(limiter.allow_request());

    // Wait for refill
    std::this_thread::sleep_for(100ms);

    // Should have tokens again
    EXPECT_TRUE(limiter.allow_request());
}

TEST_F(RateLimiterTest, GetRemainingTokens) {
    RateLimiter limiter(10, 1000ms);

    EXPECT_EQ(limiter.get_remaining_tokens(), 10);

    limiter.allow_request();
    limiter.allow_request();
    limiter.allow_request();

    EXPECT_EQ(limiter.get_remaining_tokens(), 7);
}

// ============================================================================
// Refill Rate Tests
// ============================================================================

TEST_F(RateLimiterTest, SetRefillRate) {
    RateLimiter limiter(5, 1000ms);

    // Consume all tokens
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request();
    }

    // Change refill rate to faster
    limiter.set_refill_rate(50ms);

    // Wait for new refill interval
    std::this_thread::sleep_for(100ms);

    // Should have tokens with new rate
    EXPECT_TRUE(limiter.allow_request());
}

TEST_F(RateLimiterTest, RefillDoesNotExceedMax) {
    RateLimiter limiter(5, 10ms);

    // Consume some tokens
    limiter.allow_request();
    limiter.allow_request();

    // Wait for multiple refill intervals
    std::this_thread::sleep_for(100ms);

    // Should not exceed max tokens
    EXPECT_LE(limiter.get_remaining_tokens(), 5);
}

// ============================================================================
// Request Count Tests
// ============================================================================

TEST_F(RateLimiterTest, GetRequestCountWithinWindow) {
    RateLimiter limiter(100, 1000ms);

    // Make some requests
    for (int i = 0; i < 10; ++i) {
        limiter.allow_request();
    }

    // Count should be 10 within 1 second window
    EXPECT_EQ(limiter.get_request_count(1s), 10);
}

TEST_F(RateLimiterTest, GetRequestCountExpiredRequests) {
    RateLimiter limiter(100, 100ms);

    // Make some requests
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request();
    }

    // Wait for requests to expire
    std::this_thread::sleep_for(1100ms);

    // Count should be 0 as requests expired
    EXPECT_EQ(limiter.get_request_count(1s), 0);
}

// ============================================================================
// IP-Based Rate Limiting Tests
// ============================================================================

TEST_F(RateLimiterTest, AllowRequestForIpBasic) {
    RateLimiter limiter(100, 1000ms);

    EXPECT_TRUE(limiter.allow_request_for_ip("192.168.1.1"));
    EXPECT_TRUE(limiter.allow_request_for_ip("192.168.1.2"));
}

TEST_F(RateLimiterTest, IpRateLimitExceeded) {
    RateLimiter limiter(100, 1000ms);

    // IpRateLimiter has MAX_REQUESTS_PER_SECOND = 5
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.allow_request_for_ip("192.168.1.100"));
    }

    // 6th request should be denied
    EXPECT_FALSE(limiter.allow_request_for_ip("192.168.1.100"));
}

TEST_F(RateLimiterTest, DifferentIpsHaveSeparateLimits) {
    RateLimiter limiter(100, 1000ms);

    // Exhaust limit for IP1
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request_for_ip("10.0.0.1");
    }
    EXPECT_FALSE(limiter.allow_request_for_ip("10.0.0.1"));

    // IP2 should still have quota
    EXPECT_TRUE(limiter.allow_request_for_ip("10.0.0.2"));
}

TEST_F(RateLimiterTest, IpRateLimitResetsAfterSecond) {
    RateLimiter limiter(100, 1000ms);

    // Exhaust limit
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request_for_ip("172.16.0.1");
    }
    EXPECT_FALSE(limiter.allow_request_for_ip("172.16.0.1"));

    // Wait for reset
    std::this_thread::sleep_for(1100ms);

    // Should be allowed again
    EXPECT_TRUE(limiter.allow_request_for_ip("172.16.0.1"));
}

// ============================================================================
// User-Based Rate Limiting Tests
// ============================================================================

TEST_F(RateLimiterTest, AllowRequestWithLimitBasic) {
    RateLimiter limiter(100, 1000ms);

    EXPECT_TRUE(limiter.allow_request_with_limit("user1", 10));
    EXPECT_TRUE(limiter.allow_request_with_limit("user2", 10));
}

TEST_F(RateLimiterTest, UserRateLimitExceeded) {
    RateLimiter limiter(100, 1000ms);

    // Make 10 requests (limit is 10 per minute)
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter.allow_request_with_limit("test_user", 10));
    }

    // 11th request should be denied
    EXPECT_FALSE(limiter.allow_request_with_limit("test_user", 10));
}

TEST_F(RateLimiterTest, DifferentUsersHaveSeparateLimits) {
    RateLimiter limiter(100, 1000ms);

    // Exhaust limit for user1
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request_with_limit("user_a", 5);
    }
    EXPECT_FALSE(limiter.allow_request_with_limit("user_a", 5));

    // user_b should still have quota
    EXPECT_TRUE(limiter.allow_request_with_limit("user_b", 5));
}

TEST_F(RateLimiterTest, DifferentLimitsPerUser) {
    RateLimiter limiter(100, 1000ms);

    // User with higher limit
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(limiter.allow_request_with_limit("premium_user", 100));
    }

    // User with lower limit
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.allow_request_with_limit("basic_user", 5));
    }
    EXPECT_FALSE(limiter.allow_request_with_limit("basic_user", 5));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(RateLimiterTest, ConcurrentAllowRequest) {
    RateLimiter limiter(100, 1000ms);
    std::atomic<int> allowed_count{0};
    std::atomic<int> denied_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&limiter, &allowed_count, &denied_count]() {
            for (int j = 0; j < 20; ++j) {
                if (limiter.allow_request()) {
                    ++allowed_count;
                } else {
                    ++denied_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Total requests = 10 threads * 20 requests = 200
    // Max allowed = 100 (initial tokens)
    EXPECT_EQ(allowed_count + denied_count, 200);
    EXPECT_LE(allowed_count.load(), 100);
}

TEST_F(RateLimiterTest, ConcurrentIpRequests) {
    RateLimiter limiter(1000, 1000ms);
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&limiter, &success_count, i]() {
            std::string ip = "192.168.1." + std::to_string(i);
            for (int j = 0; j < 10; ++j) {
                if (limiter.allow_request_for_ip(ip)) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each IP can make 5 requests per second
    // 5 IPs * 5 allowed = 25 max allowed
    EXPECT_LE(success_count.load(), 25);
}

TEST_F(RateLimiterTest, ConcurrentUserRequests) {
    RateLimiter limiter(1000, 1000ms);
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&limiter, &success_count, i]() {
            std::string user = "user_" + std::to_string(i);
            for (int j = 0; j < 20; ++j) {
                if (limiter.allow_request_with_limit(user, 10)) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each user can make 10 requests per minute
    // 5 users * 10 allowed = 50 max allowed
    EXPECT_LE(success_count.load(), 50);
}

TEST_F(RateLimiterTest, ConcurrentRefillRateChange) {
    RateLimiter limiter(10, 100ms);

    std::vector<std::thread> threads;

    // Thread changing refill rate
    threads.emplace_back([&limiter]() {
        for (int i = 0; i < 10; ++i) {
            limiter.set_refill_rate(std::chrono::milliseconds(50 + i * 10));
            std::this_thread::sleep_for(10ms);
        }
    });

    // Threads making requests
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&limiter]() {
            for (int j = 0; j < 20; ++j) {
                limiter.allow_request();
                std::this_thread::sleep_for(5ms);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash or deadlock
    SUCCEED();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(RateLimiterTest, SingleTokenBucket) {
    RateLimiter limiter(1, 100ms);

    EXPECT_TRUE(limiter.allow_request());
    EXPECT_FALSE(limiter.allow_request());

    std::this_thread::sleep_for(150ms);
    EXPECT_TRUE(limiter.allow_request());
}

TEST_F(RateLimiterTest, VeryFastRefill) {
    RateLimiter limiter(5, 1ms);

    // Consume all tokens
    for (int i = 0; i < 5; ++i) {
        limiter.allow_request();
    }

    // Wait a bit for refill
    std::this_thread::sleep_for(10ms);

    // Should have tokens again
    EXPECT_TRUE(limiter.allow_request());
}

TEST_F(RateLimiterTest, EmptyIpString) {
    RateLimiter limiter(100, 1000ms);

    // Empty IP should still work
    EXPECT_TRUE(limiter.allow_request_for_ip(""));
}

TEST_F(RateLimiterTest, EmptyUserIdString) {
    RateLimiter limiter(100, 1000ms);

    // Empty user ID should still work
    EXPECT_TRUE(limiter.allow_request_with_limit("", 10));
}

TEST_F(RateLimiterTest, ZeroLimitPerMinute) {
    RateLimiter limiter(100, 1000ms);

    // With 0 limit, first request should be denied
    EXPECT_FALSE(limiter.allow_request_with_limit("zero_limit_user", 0));
}

TEST_F(RateLimiterTest, HighLimitPerMinute) {
    RateLimiter limiter(100, 1000ms);

    // With very high limit, many requests should be allowed
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(limiter.allow_request_with_limit("high_limit_user", 1000));
    }
}
