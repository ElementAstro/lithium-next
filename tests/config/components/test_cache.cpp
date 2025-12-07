/**
 * @file test_cache.cpp
 * @brief Comprehensive unit tests for ConfigCache component
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "atom/type/json.hpp"
#include "config/components/cache.hpp"

using json = nlohmann::json;
using namespace lithium::config;
using namespace std::chrono_literals;

namespace lithium::config::test {

class ConfigCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConfigCache::Config config;
        config.maxSize = 100;
        config.defaultTtl = 5000ms;
        config.enableStats = true;
        cache_ = std::make_unique<ConfigCache>(config);
    }

    void TearDown() override { cache_.reset(); }

    std::unique_ptr<ConfigCache> cache_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ConfigCacheTest, DefaultConstruction) {
    ConfigCache cache;
    EXPECT_TRUE(cache.empty());
    EXPECT_EQ(cache.size(), 0);
}

TEST_F(ConfigCacheTest, ConstructionWithConfig) {
    ConfigCache::Config config;
    config.maxSize = 50;
    config.defaultTtl = 10000ms;
    config.enableStats = false;

    ConfigCache cache(config);
    EXPECT_EQ(cache.getConfig().maxSize, 50);
    EXPECT_EQ(cache.getConfig().defaultTtl, 10000ms);
    EXPECT_EQ(cache.getConfig().enableStats, false);
}

TEST_F(ConfigCacheTest, MoveConstruction) {
    cache_->put("key", json("value"));
    ConfigCache moved(std::move(*cache_));
    EXPECT_TRUE(moved.contains("key"));
}

TEST_F(ConfigCacheTest, MoveAssignment) {
    cache_->put("key", json("value"));
    ConfigCache other;
    other = std::move(*cache_);
    EXPECT_TRUE(other.contains("key"));
}

// ============================================================================
// Basic Put/Get Operations Tests
// ============================================================================

TEST_F(ConfigCacheTest, PutAndGetString) {
    cache_->put("string_key", json("test_value"));
    auto value = cache_->get("string_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "test_value");
}

TEST_F(ConfigCacheTest, PutAndGetInt) {
    cache_->put("int_key", json(42));
    auto value = cache_->get("int_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<int>(), 42);
}

TEST_F(ConfigCacheTest, PutAndGetDouble) {
    cache_->put("double_key", json(3.14159));
    auto value = cache_->get("double_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(value->get<double>(), 3.14159);
}

TEST_F(ConfigCacheTest, PutAndGetBool) {
    cache_->put("bool_key", json(true));
    auto value = cache_->get("bool_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<bool>(), true);
}

TEST_F(ConfigCacheTest, PutAndGetArray) {
    json arr = {1, 2, 3, 4, 5};
    cache_->put("array_key", arr);
    auto value = cache_->get("array_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->size(), 5);
}

TEST_F(ConfigCacheTest, PutAndGetObject) {
    json obj = {{"name", "test"}, {"value", 42}};
    cache_->put("object_key", obj);
    auto value = cache_->get("object_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ((*value)["name"].get<std::string>(), "test");
    EXPECT_EQ((*value)["value"].get<int>(), 42);
}

TEST_F(ConfigCacheTest, GetNonExistent) {
    auto value = cache_->get("nonexistent");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigCacheTest, OverwriteExisting) {
    cache_->put("key", json("original"));
    cache_->put("key", json("updated"));
    auto value = cache_->get("key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "updated");
}

// ============================================================================
// TTL Tests
// ============================================================================

TEST_F(ConfigCacheTest, PutWithCustomTtl) {
    cache_->put("ttl_key", json("value"), 100ms);
    EXPECT_TRUE(cache_->contains("ttl_key"));

    std::this_thread::sleep_for(150ms);
    auto value = cache_->get("ttl_key");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigCacheTest, DefaultTtlApplied) {
    ConfigCache::Config config;
    config.defaultTtl = 50ms;
    ConfigCache cache(config);

    cache.put("key", json("value"));
    EXPECT_TRUE(cache.contains("key"));

    std::this_thread::sleep_for(100ms);
    auto value = cache.get("key");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigCacheTest, ZeroTtlMeansNoExpiry) {
    cache_->put("no_expiry", json("value"), 0ms);
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(cache_->contains("no_expiry"));
}

// ============================================================================
// Remove/Clear Operations Tests
// ============================================================================

TEST_F(ConfigCacheTest, RemoveExisting) {
    cache_->put("key", json("value"));
    EXPECT_TRUE(cache_->remove("key"));
    EXPECT_FALSE(cache_->contains("key"));
}

TEST_F(ConfigCacheTest, RemoveNonExistent) {
    EXPECT_FALSE(cache_->remove("nonexistent"));
}

TEST_F(ConfigCacheTest, Clear) {
    cache_->put("key1", json("value1"));
    cache_->put("key2", json("value2"));
    cache_->put("key3", json("value3"));

    cache_->clear();
    EXPECT_TRUE(cache_->empty());
    EXPECT_EQ(cache_->size(), 0);
}

// ============================================================================
// Contains/Size/Empty Tests
// ============================================================================

TEST_F(ConfigCacheTest, ContainsExisting) {
    cache_->put("key", json("value"));
    EXPECT_TRUE(cache_->contains("key"));
}

TEST_F(ConfigCacheTest, ContainsNonExistent) {
    EXPECT_FALSE(cache_->contains("nonexistent"));
}

TEST_F(ConfigCacheTest, ContainsExpired) {
    cache_->put("key", json("value"), 50ms);
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(cache_->contains("key"));
}

TEST_F(ConfigCacheTest, Size) {
    EXPECT_EQ(cache_->size(), 0);
    cache_->put("key1", json("value1"));
    EXPECT_EQ(cache_->size(), 1);
    cache_->put("key2", json("value2"));
    EXPECT_EQ(cache_->size(), 2);
    cache_->remove("key1");
    EXPECT_EQ(cache_->size(), 1);
}

TEST_F(ConfigCacheTest, Empty) {
    EXPECT_TRUE(cache_->empty());
    cache_->put("key", json("value"));
    EXPECT_FALSE(cache_->empty());
    cache_->clear();
    EXPECT_TRUE(cache_->empty());
}

// ============================================================================
// LRU Eviction Tests
// ============================================================================

TEST_F(ConfigCacheTest, LruEviction) {
    ConfigCache::Config config;
    config.maxSize = 3;
    ConfigCache cache(config);

    cache.put("key1", json("value1"));
    cache.put("key2", json("value2"));
    cache.put("key3", json("value3"));

    // Access key1 to make it recently used
    cache.get("key1");

    // Add new key, should evict key2 (least recently used)
    cache.put("key4", json("value4"));

    EXPECT_TRUE(cache.contains("key1"));
    EXPECT_FALSE(cache.contains("key2"));
    EXPECT_TRUE(cache.contains("key3"));
    EXPECT_TRUE(cache.contains("key4"));
}

TEST_F(ConfigCacheTest, SetMaxSize) {
    cache_->put("key1", json("value1"));
    cache_->put("key2", json("value2"));
    cache_->put("key3", json("value3"));

    cache_->setMaxSize(2);
    EXPECT_LE(cache_->size(), 2);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ConfigCacheTest, StatisticsHits) {
    cache_->put("key", json("value"));
    cache_->get("key");
    cache_->get("key");

    auto stats = cache_->getStatistics();
    EXPECT_EQ(stats.hits.load(), 2);
}

TEST_F(ConfigCacheTest, StatisticsMisses) {
    cache_->get("nonexistent1");
    cache_->get("nonexistent2");

    auto stats = cache_->getStatistics();
    EXPECT_EQ(stats.misses.load(), 2);
}

TEST_F(ConfigCacheTest, StatisticsHitRatio) {
    cache_->put("key", json("value"));
    cache_->get("key");          // hit
    cache_->get("key");          // hit
    cache_->get("nonexistent");  // miss

    auto stats = cache_->getStatistics();
    double ratio = stats.getHitRatio();
    EXPECT_GT(ratio, 60.0);  // Should be ~66.7%
    EXPECT_LT(ratio, 70.0);
}

TEST_F(ConfigCacheTest, StatisticsCurrentSize) {
    cache_->put("key1", json("value1"));
    cache_->put("key2", json("value2"));

    auto stats = cache_->getStatistics();
    EXPECT_EQ(stats.currentSize.load(), 2);
}

TEST_F(ConfigCacheTest, ResetStatistics) {
    cache_->put("key", json("value"));
    cache_->get("key");
    cache_->get("nonexistent");

    cache_->resetStatistics();
    auto stats = cache_->getStatistics();
    EXPECT_EQ(stats.hits.load(), 0);
    EXPECT_EQ(stats.misses.load(), 0);
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(ConfigCacheTest, ManualCleanup) {
    cache_->put("key1", json("value1"), 50ms);
    cache_->put("key2", json("value2"), 50ms);
    cache_->put("key3", json("value3"), 5000ms);

    std::this_thread::sleep_for(100ms);
    size_t cleaned = cache_->cleanup();

    EXPECT_GE(cleaned, 2);
    EXPECT_FALSE(cache_->contains("key1"));
    EXPECT_FALSE(cache_->contains("key2"));
    EXPECT_TRUE(cache_->contains("key3"));
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ConfigCacheTest, GetConfig) {
    auto config = cache_->getConfig();
    EXPECT_EQ(config.maxSize, 100);
    EXPECT_EQ(config.defaultTtl, 5000ms);
    EXPECT_EQ(config.enableStats, true);
}

TEST_F(ConfigCacheTest, SetConfig) {
    ConfigCache::Config newConfig;
    newConfig.maxSize = 50;
    newConfig.defaultTtl = 10000ms;

    cache_->setConfig(newConfig);
    auto config = cache_->getConfig();
    EXPECT_EQ(config.maxSize, 50);
    EXPECT_EQ(config.defaultTtl, 10000ms);
}

TEST_F(ConfigCacheTest, SetDefaultTtl) {
    cache_->setDefaultTtl(2000ms);
    EXPECT_EQ(cache_->getConfig().defaultTtl, 2000ms);
}

// ============================================================================
// GetKeys Tests
// ============================================================================

TEST_F(ConfigCacheTest, GetKeys) {
    cache_->put("key1", json("value1"));
    cache_->put("key2", json("value2"));
    cache_->put("key3", json("value3"));

    auto keys = cache_->getKeys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key1") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key2") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "key3") != keys.end());
}

TEST_F(ConfigCacheTest, GetKeysEmpty) {
    auto keys = cache_->getKeys();
    EXPECT_TRUE(keys.empty());
}

// ============================================================================
// GetOrCompute Tests
// ============================================================================

TEST_F(ConfigCacheTest, GetOrComputeExisting) {
    cache_->put("key", json("cached_value"));

    bool factoryCalled = false;
    auto value = cache_->getOrCompute("key", [&]() {
        factoryCalled = true;
        return json("computed_value");
    });

    EXPECT_FALSE(factoryCalled);
    EXPECT_EQ(value.get<std::string>(), "cached_value");
}

TEST_F(ConfigCacheTest, GetOrComputeNonExistent) {
    bool factoryCalled = false;
    auto value = cache_->getOrCompute("key", [&]() {
        factoryCalled = true;
        return json("computed_value");
    });

    EXPECT_TRUE(factoryCalled);
    EXPECT_EQ(value.get<std::string>(), "computed_value");
    EXPECT_TRUE(cache_->contains("key"));
}

TEST_F(ConfigCacheTest, GetOrComputeWithTtl) {
    auto value =
        cache_->getOrCompute("key", []() { return json("value"); }, 50ms);

    EXPECT_TRUE(cache_->contains("key"));
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(cache_->contains("key"));
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

TEST_F(ConfigCacheTest, GetBatch) {
    cache_->put("key1", json("value1"));
    cache_->put("key2", json("value2"));

    std::vector<std::string> keys = {"key1", "key2", "key3"};
    auto results = cache_->getBatch(keys);

    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results["key1"].has_value());
    EXPECT_TRUE(results["key2"].has_value());
    EXPECT_FALSE(results["key3"].has_value());
}

TEST_F(ConfigCacheTest, PutBatch) {
    std::unordered_map<std::string, json> entries = {{"key1", json("value1")},
                                                     {"key2", json("value2")},
                                                     {"key3", json("value3")}};

    cache_->putBatch(entries);

    EXPECT_TRUE(cache_->contains("key1"));
    EXPECT_TRUE(cache_->contains("key2"));
    EXPECT_TRUE(cache_->contains("key3"));
}

TEST_F(ConfigCacheTest, PutBatchWithTtl) {
    std::unordered_map<std::string, json> entries = {{"key1", json("value1")},
                                                     {"key2", json("value2")}};

    cache_->putBatch(entries, 50ms);

    EXPECT_TRUE(cache_->contains("key1"));
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(cache_->contains("key1"));
}

// ============================================================================
// Hook Tests
// ============================================================================

TEST_F(ConfigCacheTest, AddHook) {
    bool hookCalled = false;
    ConfigCache::CacheEvent receivedEvent;

    size_t hookId =
        cache_->addHook([&](ConfigCache::CacheEvent event, std::string_view key,
                            const std::optional<json>& value) {
            hookCalled = true;
            receivedEvent = event;
        });

    cache_->put("key", json("value"));

    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(receivedEvent, ConfigCache::CacheEvent::PUT);
    EXPECT_TRUE(cache_->removeHook(hookId));
}

TEST_F(ConfigCacheTest, HookOnGet) {
    ConfigCache::CacheEvent receivedEvent;
    cache_->put("key", json("value"));

    size_t hookId = cache_->addHook(
        [&](ConfigCache::CacheEvent event, std::string_view key,
            const std::optional<json>& value) { receivedEvent = event; });

    cache_->get("key");
    EXPECT_EQ(receivedEvent, ConfigCache::CacheEvent::GET);
    cache_->removeHook(hookId);
}

TEST_F(ConfigCacheTest, HookOnRemove) {
    ConfigCache::CacheEvent receivedEvent;
    cache_->put("key", json("value"));

    size_t hookId = cache_->addHook(
        [&](ConfigCache::CacheEvent event, std::string_view key,
            const std::optional<json>& value) { receivedEvent = event; });

    cache_->remove("key");
    EXPECT_EQ(receivedEvent, ConfigCache::CacheEvent::REMOVE);
    cache_->removeHook(hookId);
}

TEST_F(ConfigCacheTest, HookOnClear) {
    ConfigCache::CacheEvent receivedEvent;
    cache_->put("key", json("value"));

    size_t hookId = cache_->addHook(
        [&](ConfigCache::CacheEvent event, std::string_view key,
            const std::optional<json>& value) { receivedEvent = event; });

    cache_->clear();
    EXPECT_EQ(receivedEvent, ConfigCache::CacheEvent::CLEAR);
    cache_->removeHook(hookId);
}

TEST_F(ConfigCacheTest, RemoveHook) {
    size_t hookId =
        cache_->addHook([](ConfigCache::CacheEvent, std::string_view,
                           const std::optional<json>&) {});
    EXPECT_TRUE(cache_->removeHook(hookId));
    EXPECT_FALSE(cache_->removeHook(hookId));
}

TEST_F(ConfigCacheTest, ClearHooks) {
    cache_->addHook([](ConfigCache::CacheEvent, std::string_view,
                       const std::optional<json>&) {});
    cache_->addHook([](ConfigCache::CacheEvent, std::string_view,
                       const std::optional<json>&) {});

    cache_->clearHooks();

    // After clearing, hooks should not be called
    bool hookCalled = false;
    cache_->addHook([&](ConfigCache::CacheEvent, std::string_view,
                        const std::optional<json>&) { hookCalled = true; });
    cache_->clearHooks();
    cache_->put("key", json("value"));
    EXPECT_FALSE(hookCalled);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ConfigCacheTest, ConcurrentPutGet) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                std::string key =
                    "thread" + std::to_string(i) + "_key" + std::to_string(j);
                cache_->put(key, json(j));
                auto value = cache_->get(key);
                EXPECT_TRUE(value.has_value());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(ConfigCacheTest, ConcurrentReadWrite) {
    cache_->put("shared_key", json(0));
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &running]() {
            int counter = 0;
            while (running.load()) {
                cache_->put("shared_key", json(counter++));
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    // Readers
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &running]() {
            while (running.load()) {
                auto value = cache_->get("shared_key");
                EXPECT_TRUE(value.has_value());
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    std::this_thread::sleep_for(100ms);
    running.store(false);

    for (auto& thread : threads) {
        thread.join();
    }
}

// ============================================================================
// CacheEntry Tests
// ============================================================================

TEST_F(ConfigCacheTest, CacheEntryConstruction) {
    ConfigCache::CacheEntry entry(json("test_value"), 1000ms);
    EXPECT_EQ(entry.value.get<std::string>(), "test_value");
    EXPECT_EQ(entry.accessCount.load(), 0);
}

TEST_F(ConfigCacheTest, CacheEntryDefaultTtl) {
    ConfigCache::CacheEntry entry(json("value"));
    // With zero TTL, expiry should be max
    EXPECT_EQ(entry.expiry, std::chrono::steady_clock::time_point::max());
}

}  // namespace lithium::config::test
