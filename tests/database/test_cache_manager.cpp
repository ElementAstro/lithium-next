// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * test_cache_manager.cpp
 *
 * Tests for the CacheManager class
 * - Singleton instance retrieval
 * - Put and get operations
 * - TTL expiration
 * - Remove single entry
 * - Clear all entries
 * - PurgeExpired cleanup
 * - Size tracking
 * - Default TTL configuration
 * - Thread-safe operations
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "database/cache/cache_manager.hpp"

using namespace lithium::database::cache;

// ==================== CacheManager Tests ====================

class CacheManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear the cache before each test
        auto& cache = CacheManager::getInstance();
        cache.clear();
    }

    void TearDown() override {
        // Clean up after tests
        auto& cache = CacheManager::getInstance();
        cache.clear();
    }
};

TEST_F(CacheManagerTest, GetInstance) {
    // Test that getInstance returns a valid reference
    auto& cache1 = CacheManager::getInstance();
    auto& cache2 = CacheManager::getInstance();

    // Both should be the same instance (singleton)
    EXPECT_EQ(&cache1, &cache2);
}

TEST_F(CacheManagerTest, PutAndGet) {
    auto& cache = CacheManager::getInstance();

    // Put a value
    cache.put("key1", "value1");

    // Get the value
    auto result = cache.get("key1");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
}

TEST_F(CacheManagerTest, GetNonexistentKey) {
    auto& cache = CacheManager::getInstance();

    // Get a non-existent key
    auto result = cache.get("nonexistent");

    EXPECT_FALSE(result.has_value());
}

TEST_F(CacheManagerTest, PutMultipleValues) {
    auto& cache = CacheManager::getInstance();

    // Put multiple values
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    // Retrieve all
    EXPECT_EQ(cache.get("key1").value(), "value1");
    EXPECT_EQ(cache.get("key2").value(), "value2");
    EXPECT_EQ(cache.get("key3").value(), "value3");
}

TEST_F(CacheManagerTest, PutWithCustomTTL) {
    auto& cache = CacheManager::getInstance();

    // Put with custom TTL (1 second)
    cache.put("ttl_key", "ttl_value", 1);

    // Should be retrievable immediately
    EXPECT_EQ(cache.get("ttl_key").value(), "ttl_value");
}

TEST_F(CacheManagerTest, TTLExpiration) {
    auto& cache = CacheManager::getInstance();

    // Put with short TTL (1 second)
    cache.put("short_ttl", "value", 1);

    // Should be available immediately
    EXPECT_TRUE(cache.get("short_ttl").has_value());

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Should be expired
    auto result = cache.get("short_ttl");
    EXPECT_FALSE(result.has_value());
}

TEST_F(CacheManagerTest, RemoveEntry) {
    auto& cache = CacheManager::getInstance();

    // Put and then remove
    cache.put("key_to_remove", "value");
    EXPECT_TRUE(cache.get("key_to_remove").has_value());

    // Remove the entry
    bool removed = cache.remove("key_to_remove");
    EXPECT_TRUE(removed);

    // Should no longer be available
    EXPECT_FALSE(cache.get("key_to_remove").has_value());
}

TEST_F(CacheManagerTest, RemoveNonexistentEntry) {
    auto& cache = CacheManager::getInstance();

    // Try to remove a non-existent entry
    bool removed = cache.remove("nonexistent");

    EXPECT_FALSE(removed);
}

TEST_F(CacheManagerTest, Clear) {
    auto& cache = CacheManager::getInstance();

    // Add multiple entries
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    // Verify they exist
    EXPECT_EQ(cache.size(), 3);

    // Clear all
    cache.clear();

    // All should be gone
    EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.get("key1").has_value());
    EXPECT_FALSE(cache.get("key2").has_value());
    EXPECT_FALSE(cache.get("key3").has_value());
}

TEST_F(CacheManagerTest, Size) {
    auto& cache = CacheManager::getInstance();

    EXPECT_EQ(cache.size(), 0);

    cache.put("key1", "value1");
    EXPECT_EQ(cache.size(), 1);

    cache.put("key2", "value2");
    EXPECT_EQ(cache.size(), 2);

    cache.put("key3", "value3");
    EXPECT_EQ(cache.size(), 3);

    cache.remove("key2");
    EXPECT_EQ(cache.size(), 2);

    cache.clear();
    EXPECT_EQ(cache.size(), 0);
}

TEST_F(CacheManagerTest, PurgeExpired) {
    auto& cache = CacheManager::getInstance();

    // Add entries with different TTLs
    cache.put("long_ttl", "value1", 100);  // Long TTL
    cache.put("short_ttl", "value2", 1);   // Short TTL

    EXPECT_EQ(cache.size(), 2);

    // Wait for short TTL to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Manually purge expired entries
    size_t purged = cache.purgeExpired();

    // At least one should be purged (the short_ttl entry)
    EXPECT_GE(purged, 1);

    // Long TTL entry should still be there
    EXPECT_TRUE(cache.get("long_ttl").has_value());

    // Short TTL entry should be gone
    EXPECT_FALSE(cache.get("short_ttl").has_value());
}

TEST_F(CacheManagerTest, PurgeExpiredWhenNothingExpired) {
    auto& cache = CacheManager::getInstance();

    cache.put("key1", "value1", 100);
    cache.put("key2", "value2", 100);

    // Nothing should be expired
    size_t purged = cache.purgeExpired();
    EXPECT_EQ(purged, 0);

    EXPECT_EQ(cache.size(), 2);
}

TEST_F(CacheManagerTest, SetDefaultTTL) {
    auto& cache = CacheManager::getInstance();

    // Set default TTL to 2 seconds
    cache.setDefaultTTL(2);

    // Put without specifying TTL (should use default)
    cache.put("key1", "value1");

    // Should be available immediately
    EXPECT_TRUE(cache.get("key1").has_value());

    // After 1 second, still available
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(cache.get("key1").has_value());

    // After 3 seconds total, should be expired
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_FALSE(cache.get("key1").has_value());
}

TEST_F(CacheManagerTest, OverrideDefaultTTL) {
    auto& cache = CacheManager::getInstance();

    // Set default TTL to 10 seconds
    cache.setDefaultTTL(10);

    // Put with explicit TTL (should override default)
    cache.put("key1", "value1", 1);

    // Should be available immediately
    EXPECT_TRUE(cache.get("key1").has_value());

    // After 2 seconds, should expire (using explicit TTL, not default)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_FALSE(cache.get("key1").has_value());
}

TEST_F(CacheManagerTest, UpdateExistingKey) {
    auto& cache = CacheManager::getInstance();

    // Put initial value
    cache.put("key", "value1");
    EXPECT_EQ(cache.get("key").value(), "value1");

    // Update with new value
    cache.put("key", "value2");
    EXPECT_EQ(cache.get("key").value(), "value2");

    // Size should still be 1
    EXPECT_EQ(cache.size(), 1);
}

TEST_F(CacheManagerTest, LargeValues) {
    auto& cache = CacheManager::getInstance();

    // Create a large value
    std::string largeValue(10000, 'x');
    cache.put("large", largeValue);

    // Should be retrievable
    auto result = cache.get("large");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().length(), 10000);
}

TEST_F(CacheManagerTest, SpecialCharactersInKey) {
    auto& cache = CacheManager::getInstance();

    // Keys with special characters
    std::string specialKey = "key:with:colons:and-dashes_and_underscores";
    cache.put(specialKey, "value");

    EXPECT_EQ(cache.get(specialKey).value(), "value");
}

TEST_F(CacheManagerTest, SpecialCharactersInValue) {
    auto& cache = CacheManager::getInstance();

    // Values with special characters
    std::string specialValue = "!@#$%^&*()_+-=[]{}|;':\",./<>?";
    cache.put("key", specialValue);

    EXPECT_EQ(cache.get("key").value(), specialValue);
}

TEST_F(CacheManagerTest, EmptyKey) {
    auto& cache = CacheManager::getInstance();

    // Empty key is still valid
    cache.put("", "empty_key_value");
    EXPECT_EQ(cache.get("").value(), "empty_key_value");
}

TEST_F(CacheManagerTest, EmptyValue) {
    auto& cache = CacheManager::getInstance();

    // Empty value is still valid
    cache.put("empty_value_key", "");
    auto result = cache.get("empty_value_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(CacheManagerTest, CaseSensitiveKeys) {
    auto& cache = CacheManager::getInstance();

    // Keys are case-sensitive
    cache.put("Key", "value1");
    cache.put("key", "value2");
    cache.put("KEY", "value3");

    EXPECT_EQ(cache.get("Key").value(), "value1");
    EXPECT_EQ(cache.get("key").value(), "value2");
    EXPECT_EQ(cache.get("KEY").value(), "value3");
    EXPECT_EQ(cache.size(), 3);
}

TEST_F(CacheManagerTest, IntensivePutGet) {
    auto& cache = CacheManager::getInstance();

    // Perform many put/get operations
    for (int i = 0; i < 100; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);

        cache.put(key, value);
        auto result = cache.get(key);

        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    }

    EXPECT_EQ(cache.size(), 100);
}

TEST_F(CacheManagerTest, MixedOperations) {
    auto& cache = CacheManager::getInstance();

    // Mix of operations
    cache.put("key1", "value1", 100);
    cache.put("key2", "value2", 100);
    cache.put("key3", "value3", 1);

    EXPECT_EQ(cache.size(), 3);

    cache.remove("key2");
    EXPECT_EQ(cache.size(), 2);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // key3 should be expired
    auto result = cache.get("key3");
    EXPECT_FALSE(result.has_value());

    // key1 should still be there
    EXPECT_TRUE(cache.get("key1").has_value());

    cache.put("key4", "value4");
    EXPECT_EQ(cache.size(), 2);  // key1 and key4

    cache.clear();
    EXPECT_EQ(cache.size(), 0);
}

TEST_F(CacheManagerTest, RepeatedClear) {
    auto& cache = CacheManager::getInstance();

    cache.put("key", "value");
    EXPECT_EQ(cache.size(), 1);

    cache.clear();
    EXPECT_EQ(cache.size(), 0);

    // Clear again when empty
    cache.clear();
    EXPECT_EQ(cache.size(), 0);

    // Should still work
    cache.put("new_key", "new_value");
    EXPECT_EQ(cache.size(), 1);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
