/**
 * @file test_manager.cpp
 * @brief Comprehensive unit tests for ConfigManager
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include "config/core/manager.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace lithium::config;

namespace lithium::config::test {

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_config_test";
        fs::create_directories(testDir_);
        createTestFiles();
        manager_ = std::make_unique<ConfigManager>();
    }

    void TearDown() override {
        manager_.reset();
        fs::remove_all(testDir_);
    }

    void createTestFiles() {
        // Create basic JSON config
        std::ofstream basic(testDir_ / "basic.json");
        basic << R"({
            "string_key": "test_value",
            "int_key": 42,
            "float_key": 3.14,
            "bool_key": true,
            "nested": {
                "level1": {
                    "level2": "deep_value"
                }
            },
            "array": [1, 2, 3, 4, 5]
        })";
        basic.close();

        // Create config for merge testing
        std::ofstream merge(testDir_ / "merge.json");
        merge << R"({
            "merge_key": "merge_value",
            "nested": {
                "merge_nested": "nested_merge_value"
            }
        })";
        merge.close();

        // Create schema file
        std::ofstream schema(testDir_ / "schema.json");
        schema << R"({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "age": {"type": "integer", "minimum": 0}
            },
            "required": ["name"]
        })";
        schema.close();
    }

    fs::path testDir_;
    std::unique_ptr<ConfigManager> manager_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ConfigManagerTest, DefaultConstruction) {
    ConfigManager manager;
    EXPECT_TRUE(manager.getKeys().empty());
}

TEST_F(ConfigManagerTest, ConstructionWithOptions) {
    ConfigManager::Options options;
    options.enable_caching = true;
    options.enable_validation = true;
    options.enable_auto_reload = false;

    ConfigManager manager(options);
    EXPECT_EQ(manager.getOptions().enable_caching, true);
    EXPECT_EQ(manager.getOptions().enable_validation, true);
    EXPECT_EQ(manager.getOptions().enable_auto_reload, false);
}

TEST_F(ConfigManagerTest, CreateShared) {
    auto shared = ConfigManager::createShared();
    ASSERT_NE(shared, nullptr);
    EXPECT_TRUE(shared->getKeys().empty());
}

TEST_F(ConfigManagerTest, CreateSharedWithOptions) {
    ConfigManager::Options options;
    options.enable_caching = false;

    auto shared = ConfigManager::createShared(options);
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->getOptions().enable_caching, false);
}

TEST_F(ConfigManagerTest, CreateUnique) {
    auto unique = ConfigManager::createUnique();
    ASSERT_NE(unique, nullptr);
    EXPECT_TRUE(unique->getKeys().empty());
}

TEST_F(ConfigManagerTest, CreateUniqueWithOptions) {
    ConfigManager::Options options;
    options.enable_validation = false;

    auto unique = ConfigManager::createUnique(options);
    ASSERT_NE(unique, nullptr);
    EXPECT_EQ(unique->getOptions().enable_validation, false);
}

TEST_F(ConfigManagerTest, MoveConstruction) {
    manager_->set("test/key", "value");
    ConfigManager moved(std::move(*manager_));
    EXPECT_TRUE(moved.has("test/key"));
}

TEST_F(ConfigManagerTest, MoveAssignment) {
    manager_->set("test/key", "value");
    ConfigManager other;
    other = std::move(*manager_);
    EXPECT_TRUE(other.has("test/key"));
}

// ============================================================================
// Get/Set Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, SetAndGetString) {
    EXPECT_TRUE(manager_->set("test/string", "hello"));
    auto value = manager_->get("test/string");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "hello");
}

TEST_F(ConfigManagerTest, SetAndGetInt) {
    EXPECT_TRUE(manager_->set("test/int", 42));
    auto value = manager_->get("test/int");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<int>(), 42);
}

TEST_F(ConfigManagerTest, SetAndGetDouble) {
    EXPECT_TRUE(manager_->set("test/double", 3.14159));
    auto value = manager_->get("test/double");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(value->get<double>(), 3.14159);
}

TEST_F(ConfigManagerTest, SetAndGetBool) {
    EXPECT_TRUE(manager_->set("test/bool", true));
    auto value = manager_->get("test/bool");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<bool>(), true);
}

TEST_F(ConfigManagerTest, SetAndGetArray) {
    json arr = {1, 2, 3, 4, 5};
    EXPECT_TRUE(manager_->set("test/array", arr));
    auto value = manager_->get("test/array");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->size(), 5);
}

TEST_F(ConfigManagerTest, SetAndGetObject) {
    json obj = {{"key1", "value1"}, {"key2", 42}};
    EXPECT_TRUE(manager_->set("test/object", obj));
    auto value = manager_->get("test/object");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ((*value)["key1"].get<std::string>(), "value1");
    EXPECT_EQ((*value)["key2"].get<int>(), 42);
}

TEST_F(ConfigManagerTest, SetWithMoveSemantics) {
    json obj = {{"key", "value"}};
    EXPECT_TRUE(manager_->set("test/move", std::move(obj)));
    EXPECT_TRUE(manager_->has("test/move"));
}

TEST_F(ConfigManagerTest, SetValueTemplate) {
    EXPECT_TRUE(manager_->set_value("test/template", std::string("template_value")));
    auto value = manager_->get_as<std::string>("test/template");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "template_value");
}

TEST_F(ConfigManagerTest, GetAsTyped) {
    manager_->set("test/typed", 100);
    auto value = manager_->get_as<int>("test/typed");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 100);
}

TEST_F(ConfigManagerTest, GetAsTypedWrongType) {
    manager_->set("test/typed", "not_an_int");
    auto value = manager_->get_as<int>("test/typed");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigManagerTest, GetNonExistent) {
    auto value = manager_->get("nonexistent/path");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigManagerTest, NestedSetAndGet) {
    EXPECT_TRUE(manager_->set("a/b/c/d/e", "deep_value"));
    auto value = manager_->get("a/b/c/d/e");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "deep_value");
}

// ============================================================================
// Has/Remove Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, HasExistingKey) {
    manager_->set("test/key", "value");
    EXPECT_TRUE(manager_->has("test/key"));
}

TEST_F(ConfigManagerTest, HasNonExistentKey) {
    EXPECT_FALSE(manager_->has("nonexistent/key"));
}

TEST_F(ConfigManagerTest, RemoveExistingKey) {
    manager_->set("test/key", "value");
    EXPECT_TRUE(manager_->remove("test/key"));
    EXPECT_FALSE(manager_->has("test/key"));
}

TEST_F(ConfigManagerTest, RemoveNonExistentKey) {
    EXPECT_FALSE(manager_->remove("nonexistent/key"));
}

TEST_F(ConfigManagerTest, RemoveNestedKey) {
    manager_->set("a/b/c", "value");
    EXPECT_TRUE(manager_->remove("a/b/c"));
    EXPECT_FALSE(manager_->has("a/b/c"));
    // Parent should still exist
    EXPECT_TRUE(manager_->has("a/b"));
}

// ============================================================================
// Append Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, AppendToArray) {
    manager_->set("test/array", json::array());
    EXPECT_TRUE(manager_->append("test/array", 1));
    EXPECT_TRUE(manager_->append("test/array", 2));
    EXPECT_TRUE(manager_->append("test/array", 3));

    auto value = manager_->get("test/array");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->size(), 3);
    EXPECT_EQ((*value)[0].get<int>(), 1);
    EXPECT_EQ((*value)[1].get<int>(), 2);
    EXPECT_EQ((*value)[2].get<int>(), 3);
}

TEST_F(ConfigManagerTest, AppendValueTemplate) {
    manager_->set("test/array", json::array());
    EXPECT_TRUE(manager_->append_value("test/array", std::string("item1")));
    EXPECT_TRUE(manager_->append_value("test/array", std::string("item2")));

    auto value = manager_->get("test/array");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->size(), 2);
}

TEST_F(ConfigManagerTest, AppendToNonArray) {
    manager_->set("test/string", "not_an_array");
    EXPECT_FALSE(manager_->append("test/string", 1));
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, LoadFromFile) {
    EXPECT_TRUE(manager_->loadFromFile(testDir_ / "basic.json"));
    EXPECT_TRUE(manager_->has("basic/string_key"));
    auto value = manager_->get("basic/string_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "test_value");
}

TEST_F(ConfigManagerTest, LoadFromNonExistentFile) {
    EXPECT_FALSE(manager_->loadFromFile(testDir_ / "nonexistent.json"));
}

TEST_F(ConfigManagerTest, LoadFromDirectory) {
    EXPECT_TRUE(manager_->loadFromDir(testDir_, false));
    EXPECT_TRUE(manager_->has("basic/string_key"));
    EXPECT_TRUE(manager_->has("merge/merge_key"));
}

TEST_F(ConfigManagerTest, LoadFromDirectoryRecursive) {
    fs::create_directories(testDir_ / "subdir");
    std::ofstream sub(testDir_ / "subdir" / "sub.json");
    sub << R"({"sub_key": "sub_value"})";
    sub.close();

    EXPECT_TRUE(manager_->loadFromDir(testDir_, true));
    EXPECT_TRUE(manager_->has("sub/sub_key"));
}

TEST_F(ConfigManagerTest, LoadFromFiles) {
    std::vector<fs::path> paths = {
        testDir_ / "basic.json",
        testDir_ / "merge.json"
    };
    size_t loaded = manager_->loadFromFiles(paths);
    EXPECT_EQ(loaded, 2);
}

TEST_F(ConfigManagerTest, SaveToFile) {
    manager_->set("save/key", "save_value");
    fs::path savePath = testDir_ / "saved.json";
    EXPECT_TRUE(manager_->save(savePath));
    EXPECT_TRUE(fs::exists(savePath));

    // Verify saved content
    ConfigManager verifier;
    EXPECT_TRUE(verifier.loadFromFile(savePath));
    auto value = verifier.get("save/key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "save_value");
}

TEST_F(ConfigManagerTest, SaveAll) {
    manager_->set("config1/key", "value1");
    manager_->set("config2/key", "value2");

    fs::path outputDir = testDir_ / "output";
    fs::create_directories(outputDir);
    EXPECT_TRUE(manager_->saveAll(outputDir));

    EXPECT_TRUE(fs::exists(outputDir / "config1.json"));
    EXPECT_TRUE(fs::exists(outputDir / "config2.json"));
}

// ============================================================================
// Clear/Tidy/Merge Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, Clear) {
    manager_->set("key1", "value1");
    manager_->set("key2", "value2");
    manager_->clear();
    EXPECT_TRUE(manager_->getKeys().empty());
}

TEST_F(ConfigManagerTest, Tidy) {
    manager_->set("a/b/c", "value");
    manager_->tidy();
    EXPECT_TRUE(manager_->has("a/b/c"));
}

TEST_F(ConfigManagerTest, Merge) {
    manager_->set("original", "original_value");
    json toMerge = {{"merged", "merged_value"}};
    manager_->merge(toMerge);

    EXPECT_TRUE(manager_->has("original"));
    EXPECT_TRUE(manager_->has("merged"));
}

TEST_F(ConfigManagerTest, MergeNested) {
    manager_->set("nested/key1", "value1");
    json toMerge = {{"nested", {{"key2", "value2"}}}};
    manager_->merge(toMerge);

    EXPECT_TRUE(manager_->has("nested/key1"));
    EXPECT_TRUE(manager_->has("nested/key2"));
}

// ============================================================================
// Keys/Paths Operations Tests
// ============================================================================

TEST_F(ConfigManagerTest, GetKeys) {
    manager_->set("key1", "value1");
    manager_->set("key2", "value2");
    manager_->set("nested/key3", "value3");

    auto keys = manager_->getKeys();
    EXPECT_GE(keys.size(), 2);
}

TEST_F(ConfigManagerTest, ListPaths) {
    manager_->loadFromDir(testDir_, false);
    auto paths = manager_->listPaths();
    EXPECT_GE(paths.size(), 1);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(ConfigManagerTest, OnChangedCallback) {
    bool callbackCalled = false;
    std::string changedPath;

    size_t id = manager_->onChanged([&](std::string_view path) {
        callbackCalled = true;
        changedPath = std::string(path);
    });

    manager_->set("test/callback", "value");

    // Give async callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(manager_->removeCallback(id));
}

TEST_F(ConfigManagerTest, RemoveCallback) {
    size_t id = manager_->onChanged([](std::string_view) {});
    EXPECT_TRUE(manager_->removeCallback(id));
    EXPECT_FALSE(manager_->removeCallback(id));  // Already removed
}

// ============================================================================
// Component Access Tests
// ============================================================================

TEST_F(ConfigManagerTest, GetCache) {
    const auto& cache = manager_->getCache();
    auto stats = cache.getStatistics();
    EXPECT_GE(stats.hits.load(), 0);
}

TEST_F(ConfigManagerTest, GetValidator) {
    const auto& validator = manager_->getValidator();
    EXPECT_FALSE(validator.hasSchema());
}

TEST_F(ConfigManagerTest, GetSerializer) {
    const auto& serializer = manager_->getSerializer();
    auto metrics = serializer.getMetrics();
    EXPECT_GE(metrics.totalSerializations, 0);
}

TEST_F(ConfigManagerTest, GetWatcher) {
    const auto& watcher = manager_->getWatcher();
    EXPECT_FALSE(watcher.isRunning());
}

// ============================================================================
// Options/Metrics Tests
// ============================================================================

TEST_F(ConfigManagerTest, UpdateOptions) {
    ConfigManager::Options newOptions;
    newOptions.enable_caching = false;
    manager_->updateOptions(newOptions);
    EXPECT_EQ(manager_->getOptions().enable_caching, false);
}

TEST_F(ConfigManagerTest, GetMetrics) {
    manager_->set("test/key", "value");
    manager_->get("test/key");

    auto metrics = manager_->getMetrics();
    EXPECT_GE(metrics.total_operations, 1);
}

TEST_F(ConfigManagerTest, ResetMetrics) {
    manager_->set("test/key", "value");
    manager_->resetMetrics();
    auto metrics = manager_->getMetrics();
    EXPECT_EQ(metrics.total_operations, 0);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(ConfigManagerTest, SetSchema) {
    json schema = {
        {"type", "object"},
        {"properties", {{"name", {{"type", "string"}}}}}
    };
    EXPECT_TRUE(manager_->setSchema("test", schema));
}

TEST_F(ConfigManagerTest, LoadSchema) {
    EXPECT_TRUE(manager_->loadSchema("test", testDir_ / "schema.json"));
}

TEST_F(ConfigManagerTest, Validate) {
    json schema = {
        {"type", "object"},
        {"properties", {{"name", {{"type", "string"}}}}}
    };
    manager_->setSchema("test", schema);
    manager_->set("test/name", "John");

    auto result = manager_->validate("test");
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigManagerTest, ValidateAll) {
    auto result = manager_->validateAll();
    EXPECT_TRUE(result.isValid);
}

// ============================================================================
// Auto-Reload Tests
// ============================================================================

TEST_F(ConfigManagerTest, EnableAutoReload) {
    manager_->loadFromFile(testDir_ / "basic.json");
    EXPECT_TRUE(manager_->enableAutoReload(testDir_ / "basic.json"));
}

TEST_F(ConfigManagerTest, DisableAutoReload) {
    manager_->loadFromFile(testDir_ / "basic.json");
    manager_->enableAutoReload(testDir_ / "basic.json");
    EXPECT_TRUE(manager_->disableAutoReload(testDir_ / "basic.json"));
}

TEST_F(ConfigManagerTest, IsAutoReloadEnabled) {
    manager_->loadFromFile(testDir_ / "basic.json");
    manager_->enableAutoReload(testDir_ / "basic.json");
    EXPECT_TRUE(manager_->isAutoReloadEnabled(testDir_ / "basic.json"));
}

// ============================================================================
// Hook Tests
// ============================================================================

TEST_F(ConfigManagerTest, AddHook) {
    bool hookCalled = false;
    size_t hookId = manager_->addHook(
        [&](ConfigManager::ConfigEvent event, std::string_view path,
            const std::optional<json>& value) {
            hookCalled = true;
        });

    manager_->set("test/hook", "value");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(hookCalled);
    EXPECT_TRUE(manager_->removeHook(hookId));
}

TEST_F(ConfigManagerTest, RemoveHook) {
    size_t hookId = manager_->addHook(
        [](ConfigManager::ConfigEvent, std::string_view, const std::optional<json>&) {});
    EXPECT_TRUE(manager_->removeHook(hookId));
    EXPECT_FALSE(manager_->removeHook(hookId));
}

TEST_F(ConfigManagerTest, ClearHooks) {
    manager_->addHook(
        [](ConfigManager::ConfigEvent, std::string_view, const std::optional<json>&) {});
    manager_->addHook(
        [](ConfigManager::ConfigEvent, std::string_view, const std::optional<json>&) {});
    manager_->clearHooks();
    EXPECT_EQ(manager_->getHookCount(), 0);
}

TEST_F(ConfigManagerTest, GetHookCount) {
    EXPECT_EQ(manager_->getHookCount(), 0);
    manager_->addHook(
        [](ConfigManager::ConfigEvent, std::string_view, const std::optional<json>&) {});
    EXPECT_EQ(manager_->getHookCount(), 1);
}

// ============================================================================
// Utility Methods Tests
// ============================================================================

TEST_F(ConfigManagerTest, Flatten) {
    manager_->set("a/b/c", "value1");
    manager_->set("x/y", "value2");

    auto flat = manager_->flatten();
    EXPECT_GE(flat.size(), 2);
}

TEST_F(ConfigManagerTest, Unflatten) {
    std::unordered_map<std::string, json> flatConfig = {
        {"a/b", "value1"},
        {"c/d", "value2"}
    };

    size_t imported = manager_->unflatten(flatConfig);
    EXPECT_EQ(imported, 2);
    EXPECT_TRUE(manager_->has("a/b"));
    EXPECT_TRUE(manager_->has("c/d"));
}

TEST_F(ConfigManagerTest, ExportAs) {
    manager_->set("test/key", "value");
    std::string exported = manager_->exportAs(SerializationFormat::JSON);
    EXPECT_FALSE(exported.empty());
}

TEST_F(ConfigManagerTest, ImportFrom) {
    std::string data = R"({"imported": {"key": "value"}})";
    EXPECT_TRUE(manager_->importFrom(data, SerializationFormat::JSON));
    EXPECT_TRUE(manager_->has("imported/key"));
}

TEST_F(ConfigManagerTest, Diff) {
    manager_->set("key1", "value1");
    manager_->set("key2", "value2");

    json other = {{"key1", "value1"}, {"key2", "different"}};
    json diffResult = manager_->diff(other);
    EXPECT_FALSE(diffResult.empty());
}

TEST_F(ConfigManagerTest, ApplyPatch) {
    manager_->set("key", "old_value");
    json patch = {{{"op", "replace"}, {"path", "/key"}, {"value", "new_value"}}};
    EXPECT_TRUE(manager_->applyPatch(patch));
}

// ============================================================================
// Snapshot Tests
// ============================================================================

TEST_F(ConfigManagerTest, CreateSnapshot) {
    manager_->set("test/key", "value");
    std::string snapshotId = manager_->createSnapshot();
    EXPECT_FALSE(snapshotId.empty());
}

TEST_F(ConfigManagerTest, RestoreSnapshot) {
    manager_->set("test/key", "original");
    std::string snapshotId = manager_->createSnapshot();

    manager_->set("test/key", "modified");
    EXPECT_TRUE(manager_->restoreSnapshot(snapshotId));

    auto value = manager_->get("test/key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get<std::string>(), "original");
}

TEST_F(ConfigManagerTest, ListSnapshots) {
    manager_->createSnapshot();
    manager_->createSnapshot();

    auto snapshots = manager_->listSnapshots();
    EXPECT_GE(snapshots.size(), 2);
}

TEST_F(ConfigManagerTest, DeleteSnapshot) {
    std::string snapshotId = manager_->createSnapshot();
    EXPECT_TRUE(manager_->deleteSnapshot(snapshotId));
    EXPECT_FALSE(manager_->deleteSnapshot(snapshotId));  // Already deleted
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ConfigManagerTest, ConcurrentSetGet) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                std::string key = "thread" + std::to_string(i) + "/key" + std::to_string(j);
                manager_->set(key, j);
                auto value = manager_->get(key);
                EXPECT_TRUE(value.has_value());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(ConfigManagerTest, ConcurrentReadWrite) {
    manager_->set("shared/key", 0);
    constexpr int NUM_READERS = 5;
    constexpr int NUM_WRITERS = 3;
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([this, &running, i]() {
            int counter = 0;
            while (running.load()) {
                manager_->set("shared/key", counter++);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Readers
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([this, &running]() {
            while (running.load()) {
                auto value = manager_->get("shared/key");
                EXPECT_TRUE(value.has_value());
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    for (auto& thread : threads) {
        thread.join();
    }
}

}  // namespace lithium::config::test
