/*
 * test_auth.cpp - Tests for Authentication Middleware
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/middleware/auth.hpp"

#include <chrono>
#include <string>
#include <thread>

using namespace lithium::server::middleware;
using namespace std::chrono_literals;

// ============================================================================
// ApiKeyAuth Tests
// ============================================================================

class ApiKeyAuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing keys and add test keys
        ApiKeyAuth::valid_keys.clear();
        ApiKeyAuth::addApiKey("test-api-key-1");
        ApiKeyAuth::addApiKey("test-api-key-2");
    }

    void TearDown() override {
        // Reset to default state
        ApiKeyAuth::valid_keys.clear();
        ApiKeyAuth::addApiKey("default-api-key-please-change-in-production");
    }
};

TEST_F(ApiKeyAuthTest, AddApiKey) {
    ApiKeyAuth::addApiKey("new-test-key");
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("new-test-key"));
}

TEST_F(ApiKeyAuthTest, RevokeApiKey) {
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("test-api-key-1"));
    ApiKeyAuth::revokeApiKey("test-api-key-1");
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey("test-api-key-1"));
}

TEST_F(ApiKeyAuthTest, IsValidApiKeyValid) {
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("test-api-key-1"));
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("test-api-key-2"));
}

TEST_F(ApiKeyAuthTest, IsValidApiKeyInvalid) {
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey("invalid-key"));
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey(""));
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey("test-api-key-3"));
}

TEST_F(ApiKeyAuthTest, MultipleKeysManagement) {
    ApiKeyAuth::addApiKey("key-a");
    ApiKeyAuth::addApiKey("key-b");
    ApiKeyAuth::addApiKey("key-c");

    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("key-a"));
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("key-b"));
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("key-c"));

    ApiKeyAuth::revokeApiKey("key-b");

    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("key-a"));
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey("key-b"));
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("key-c"));
}

TEST_F(ApiKeyAuthTest, DuplicateKeyAdd) {
    ApiKeyAuth::addApiKey("duplicate-key");
    ApiKeyAuth::addApiKey("duplicate-key");  // Should not cause issues

    EXPECT_TRUE(ApiKeyAuth::isValidApiKey("duplicate-key"));

    ApiKeyAuth::revokeApiKey("duplicate-key");
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey("duplicate-key"));
}

TEST_F(ApiKeyAuthTest, RevokeNonexistentKey) {
    // Should not throw or cause issues
    EXPECT_NO_THROW(ApiKeyAuth::revokeApiKey("nonexistent-key"));
}

TEST_F(ApiKeyAuthTest, EmptyKeyHandling) {
    ApiKeyAuth::addApiKey("");
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey(""));

    ApiKeyAuth::revokeApiKey("");
    EXPECT_FALSE(ApiKeyAuth::isValidApiKey(""));
}

TEST_F(ApiKeyAuthTest, SpecialCharactersInKey) {
    std::string specialKey = "key-with-special-chars!@#$%^&*()";
    ApiKeyAuth::addApiKey(specialKey);
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey(specialKey));
}

TEST_F(ApiKeyAuthTest, LongApiKey) {
    std::string longKey(1000, 'x');
    ApiKeyAuth::addApiKey(longKey);
    EXPECT_TRUE(ApiKeyAuth::isValidApiKey(longKey));
}

// ============================================================================
// RateLimiterMiddleware Tests
// ============================================================================

class RateLimiterMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the static limiter by waiting for refill
        std::this_thread::sleep_for(100ms);
    }

    void TearDown() override {}
};

TEST_F(RateLimiterMiddlewareTest, ContextDefaultValues) {
    RateLimiterMiddleware::context ctx;
    EXPECT_FALSE(ctx.rate_limited);
}

TEST_F(RateLimiterMiddlewareTest, LimiterExists) {
    // Verify the static limiter is accessible
    EXPECT_NO_THROW(RateLimiterMiddleware::limiter.allow_request());
}

// ============================================================================
// CORS Middleware Tests
// ============================================================================

class CORSMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CORSMiddlewareTest, ContextExists) {
    CORS::context ctx;
    // Context is empty, just verify it compiles
    SUCCEED();
}

// ============================================================================
// RequestLogger Middleware Tests
// ============================================================================

class RequestLoggerMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RequestLoggerMiddlewareTest, ContextHasStartTime) {
    RequestLogger::context ctx;
    ctx.start_time = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - ctx.start_time);

    EXPECT_GE(duration.count(), 0);
}

TEST_F(RequestLoggerMiddlewareTest, TimingMeasurement) {
    RequestLogger::context ctx;
    ctx.start_time = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(50ms);

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - ctx.start_time);

    EXPECT_GE(duration.count(), 50);
    EXPECT_LT(duration.count(), 150);  // Allow some tolerance
}

// ============================================================================
// Integration-like Tests (without actual HTTP)
// ============================================================================

class AuthMiddlewareIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ApiKeyAuth::valid_keys.clear();
        ApiKeyAuth::addApiKey("integration-test-key");
    }

    void TearDown() override {
        ApiKeyAuth::valid_keys.clear();
        ApiKeyAuth::addApiKey("default-api-key-please-change-in-production");
    }
};

TEST_F(AuthMiddlewareIntegrationTest, ValidKeyFlow) {
    // Simulate the flow of a valid API key
    std::string apiKey = "integration-test-key";

    // Check if key is valid (what before_handle would do)
    bool isValid = ApiKeyAuth::isValidApiKey(apiKey);
    EXPECT_TRUE(isValid);

    // Context should be marked as authenticated
    ApiKeyAuth::context ctx;
    if (isValid) {
        ctx.authenticated = true;
        ctx.api_key = apiKey;
    }

    EXPECT_TRUE(ctx.authenticated);
    EXPECT_EQ(ctx.api_key, apiKey);
}

TEST_F(AuthMiddlewareIntegrationTest, InvalidKeyFlow) {
    std::string apiKey = "invalid-key";

    bool isValid = ApiKeyAuth::isValidApiKey(apiKey);
    EXPECT_FALSE(isValid);

    ApiKeyAuth::context ctx;
    // Context should remain unauthenticated
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_TRUE(ctx.api_key.empty());
}

TEST_F(AuthMiddlewareIntegrationTest, MissingKeyFlow) {
    std::string apiKey = "";

    bool isValid = ApiKeyAuth::isValidApiKey(apiKey);
    EXPECT_FALSE(isValid);

    ApiKeyAuth::context ctx;
    EXPECT_FALSE(ctx.authenticated);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class AuthMiddlewareThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override { ApiKeyAuth::valid_keys.clear(); }

    void TearDown() override {
        ApiKeyAuth::valid_keys.clear();
        ApiKeyAuth::addApiKey("default-api-key-please-change-in-production");
    }
};

TEST_F(AuthMiddlewareThreadSafetyTest, ConcurrentKeyAddition) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([i, &success_count]() {
            std::string key = "concurrent-key-" + std::to_string(i);
            ApiKeyAuth::addApiKey(key);
            if (ApiKeyAuth::isValidApiKey(key)) {
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
}

TEST_F(AuthMiddlewareThreadSafetyTest, ConcurrentKeyValidation) {
    // Add keys first
    for (int i = 0; i < 10; ++i) {
        ApiKeyAuth::addApiKey("validate-key-" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    std::atomic<int> valid_count{0};

    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([i, &valid_count]() {
            std::string key = "validate-key-" + std::to_string(i % 10);
            if (ApiKeyAuth::isValidApiKey(key)) {
                ++valid_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(valid_count.load(), 100);
}

TEST_F(AuthMiddlewareThreadSafetyTest, ConcurrentAddAndRevoke) {
    std::vector<std::thread> threads;

    // Add keys
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 10; ++j) {
                std::string key =
                    "ar-key-" + std::to_string(i) + "-" + std::to_string(j);
                ApiKeyAuth::addApiKey(key);
            }
        });
    }

    // Revoke keys
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 10; ++j) {
                std::string key =
                    "ar-key-" + std::to_string(i) + "-" + std::to_string(j);
                ApiKeyAuth::revokeApiKey(key);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash or deadlock
    SUCCEED();
}
