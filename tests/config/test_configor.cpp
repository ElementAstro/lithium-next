#include "atom/type/json.hpp"
#include "config/configor.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>


namespace lithium {
namespace test {

class ConfigManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create test directory structure
    fs::create_directories("test_config");

    // Create a test config file
    std::ofstream test_config("test_config/test.json");
    test_config << R"({
      "test_key": "test_value",
      "nested": {
        "key": "value"
      },
      "array": [1, 2, 3]
    })";
    test_config.close();

    config_manager = std::make_unique<ConfigManager>();
  }

  void TearDown() override {
    config_manager.reset();
    fs::remove_all("test_config");
  }

  std::unique_ptr<ConfigManager> config_manager;
};

TEST_F(ConfigManagerTest, Construction) {
  auto shared_manager = ConfigManager::createShared();
  ASSERT_NE(shared_manager, nullptr);

  auto unique_manager = ConfigManager::createUnique();
  ASSERT_NE(unique_manager, nullptr);
}

TEST_F(ConfigManagerTest, LoadFromFile) {
  ASSERT_TRUE(config_manager->loadFromFile("test_config/test.json"));

  auto value = config_manager->get("test/test_key");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->get<std::string>(), "test_value");
}

TEST_F(ConfigManagerTest, LoadFromInvalidFile) {
  ASSERT_FALSE(config_manager->loadFromFile("nonexistent.json"));
}

TEST_F(ConfigManagerTest, LoadFromDirectory) {
  ASSERT_TRUE(config_manager->loadFromDir("test_config", false));
  ASSERT_TRUE(config_manager->has("test/test_key"));
}

TEST_F(ConfigManagerTest, SaveConfig) {
  config_manager->loadFromFile("test_config/test.json");
  ASSERT_TRUE(config_manager->save("test_config/output.json"));

  // Verify file was created
  ASSERT_TRUE(fs::exists("test_config/output.json"));
}

TEST_F(ConfigManagerTest, GetAndSet) {
  config_manager->set("new/key", "value");
  auto value = config_manager->get("new/key");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->get<std::string>(), "value");
}

TEST_F(ConfigManagerTest, AppendToArray) {
  config_manager->set("array", json::array());
  ASSERT_TRUE(config_manager->append("array", 1));
  ASSERT_TRUE(config_manager->append("array", 2));

  auto array = config_manager->get("array");
  ASSERT_TRUE(array.has_value());
  EXPECT_EQ(array->size(), 2);
}

TEST_F(ConfigManagerTest, Remove) {
  config_manager->set("test/key", "value");
  ASSERT_TRUE(config_manager->remove("test/key"));
  ASSERT_FALSE(config_manager->has("test/key"));
}

TEST_F(ConfigManagerTest, Clear) {
  config_manager->set("test/key", "value");
  config_manager->clear();
  ASSERT_FALSE(config_manager->has("test/key"));
}

TEST_F(ConfigManagerTest, Merge) {
  json source = {{"merge_key", "merge_value"}};
  config_manager->set("original", "value");
  config_manager->merge(source);

  ASSERT_TRUE(config_manager->has("merge_key"));
  ASSERT_TRUE(config_manager->has("original"));
}

TEST_F(ConfigManagerTest, GetKeys) {
  config_manager->set("key1", "value1");
  config_manager->set("nested/key2", "value2");

  auto keys = config_manager->getKeys();
  ASSERT_GE(keys.size(), 2);
  EXPECT_TRUE(std::find(keys.begin(), keys.end(), "/key1") != keys.end());
}

TEST_F(ConfigManagerTest, Tidy) {
  config_manager->set("deeply/nested/key", "value");
  config_manager->tidy();
  ASSERT_TRUE(config_manager->has("deeply/nested/key"));
}

TEST_F(ConfigManagerTest, SaveAll) {
  config_manager->set("config1/key", "value1");
  config_manager->set("config2/key", "value2");

  fs::create_directories("test_config/output");
  ASSERT_TRUE(config_manager->saveAll("test_config/output"));

  ASSERT_TRUE(fs::exists("test_config/output/config1.json"));
  ASSERT_TRUE(fs::exists("test_config/output/config2.json"));
}

TEST_F(ConfigManagerTest, HandleInvalidPaths) {
  ASSERT_FALSE(config_manager->get("invalid/path").has_value());
  ASSERT_FALSE(config_manager->remove("invalid/path"));
  ASSERT_FALSE(config_manager->has("invalid/path"));
}

TEST_F(ConfigManagerTest, ThreadSafety) {
  constexpr int NUM_THREADS = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([this, i]() {
      std::string key = "key" + std::to_string(i);
      config_manager->set(key, i);
      auto value = config_manager->get(key);
      ASSERT_TRUE(value.has_value());
      EXPECT_EQ(value->get<int>(), i);
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }
}

} // namespace test
} // namespace lithium