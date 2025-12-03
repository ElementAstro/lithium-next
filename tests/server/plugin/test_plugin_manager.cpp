/*
 * test_plugin_manager.cpp - Tests for Plugin Manager
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "server/plugin/plugin_manager.hpp"
#include "atom/type/json.hpp"

using namespace lithium::server::plugin;
using json = nlohmann::json;
namespace fs = std::filesystem;

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directories
        testDir_ = fs::temp_directory_path() / "lithium_manager_test";
        pluginDir_ = testDir_ / "plugins";
        configDir_ = testDir_ / "config";

        fs::create_directories(pluginDir_);
        fs::create_directories(configDir_);
    }

    void TearDown() override {
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    fs::path testDir_;
    fs::path pluginDir_;
    fs::path configDir_;
};

// ============================================================================
// PluginManagerConfig Tests
// ============================================================================

TEST_F(PluginManagerTest, DefaultConfig) {
    PluginManagerConfig config;

    EXPECT_TRUE(config.autoRegisterOnLoad);
    EXPECT_TRUE(config.enableEventNotifications);
    EXPECT_EQ(config.configFile, "config/plugins.json");
}

TEST_F(PluginManagerTest, CustomConfig) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;
    config.configFile = configDir_ / "plugins.json";
    config.autoRegisterOnLoad = false;
    config.enableEventNotifications = false;

    auto manager = PluginManager::createShared(config);
    EXPECT_NE(manager, nullptr);
}

// ============================================================================
// PluginManager Creation Tests
// ============================================================================

TEST_F(PluginManagerTest, CreateShared) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    EXPECT_NE(manager, nullptr);
}

TEST_F(PluginManagerTest, GetLoader) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto loader = manager->getLoader();

    EXPECT_NE(loader, nullptr);
}

// ============================================================================
// Plugin State Tests
// ============================================================================

TEST_F(PluginManagerTest, IsPluginLoadedReturnsFalse) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    EXPECT_FALSE(manager->isPluginLoaded("nonexistent"));
}

TEST_F(PluginManagerTest, IsPluginEnabledReturnsFalse) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    EXPECT_FALSE(manager->isPluginEnabled("nonexistent"));
}

TEST_F(PluginManagerTest, GetPluginInfoReturnsNullopt) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto info = manager->getPluginInfo("nonexistent");

    EXPECT_FALSE(info.has_value());
}

TEST_F(PluginManagerTest, GetAllPluginsEmpty) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto plugins = manager->getAllPlugins();

    EXPECT_TRUE(plugins.empty());
}

// ============================================================================
// Plugin Loading Tests
// ============================================================================

TEST_F(PluginManagerTest, LoadPluginNotFound) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto result = manager->loadPlugin("nonexistent");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

TEST_F(PluginManagerTest, LoadPluginFromPathNotFound) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto result = manager->loadPluginFromPath("/nonexistent/plugin.so");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

TEST_F(PluginManagerTest, UnloadPluginNotLoaded) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto result = manager->unloadPlugin("nonexistent");

    EXPECT_FALSE(result.has_value());
}

TEST_F(PluginManagerTest, ReloadPluginNotLoaded) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto result = manager->reloadPlugin("nonexistent");

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Plugin Enable/Disable Tests
// ============================================================================

TEST_F(PluginManagerTest, EnablePluginNotLoaded) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    bool result = manager->enablePlugin("nonexistent");

    EXPECT_FALSE(result);
}

TEST_F(PluginManagerTest, DisablePluginNotLoaded) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    bool result = manager->disablePlugin("nonexistent");

    EXPECT_FALSE(result);
}

// ============================================================================
// Plugin Discovery Tests
// ============================================================================

TEST_F(PluginManagerTest, DiscoverAndLoadAllEmpty) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    size_t loaded = manager->discoverAndLoadAll();

    EXPECT_EQ(loaded, 0);
}

TEST_F(PluginManagerTest, GetAvailablePluginsEmpty) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto available = manager->getAvailablePlugins();

    EXPECT_TRUE(available.empty());
}

// ============================================================================
// Event Subscription Tests
// ============================================================================

TEST_F(PluginManagerTest, SubscribeToEvents) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    bool callbackCalled = false;
    int subId = manager->subscribeToEvents(
        [&callbackCalled](PluginEvent, const std::string&, const json&) {
            callbackCalled = true;
        });

    EXPECT_GE(subId, 0);
}

TEST_F(PluginManagerTest, UnsubscribeFromEvents) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    int subId = manager->subscribeToEvents(
        [](PluginEvent, const std::string&, const json&) {});

    // Should not throw
    EXPECT_NO_THROW(manager->unsubscribeFromEvents(subId));
}

// ============================================================================
// Plugin Health Tests
// ============================================================================

TEST_F(PluginManagerTest, GetPluginHealthNotFound) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto health = manager->getPluginHealth("nonexistent");

    EXPECT_TRUE(health.contains("error"));
}

TEST_F(PluginManagerTest, GetSystemStatus) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto status = manager->getSystemStatus();

    EXPECT_TRUE(status.contains("totalPlugins"));
    EXPECT_TRUE(status.contains("enabledPlugins"));
    EXPECT_TRUE(status.contains("healthyPlugins"));
    EXPECT_TRUE(status.contains("plugins"));
    EXPECT_EQ(status["totalPlugins"], 0);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(PluginManagerTest, LoadConfigurationFileNotFound) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;
    config.configFile = configDir_ / "nonexistent.json";

    auto manager = PluginManager::createShared(config);
    bool result = manager->loadConfiguration();

    EXPECT_FALSE(result);
}

TEST_F(PluginManagerTest, SaveConfiguration) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;
    config.configFile = configDir_ / "plugins.json";

    auto manager = PluginManager::createShared(config);
    bool result = manager->saveConfiguration();

    EXPECT_TRUE(result);
    EXPECT_TRUE(fs::exists(config.configFile));
}

TEST_F(PluginManagerTest, UpdatePluginConfig) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    json testConfig = {{"setting", "value"}};
    manager->updatePluginConfig("test_plugin", testConfig);

    auto retrieved = manager->getPluginConfig("test_plugin");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ((*retrieved)["setting"], "value");
}

TEST_F(PluginManagerTest, GetPluginConfigNotSet) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);
    auto result = manager->getPluginConfig("unknown");

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Plugin Type Filter Tests
// ============================================================================

TEST_F(PluginManagerTest, GetPluginsByTypeEmpty) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    auto commandPlugins = manager->getPluginsByType(LoadedPluginInfo::Type::Command);
    auto controllerPlugins = manager->getPluginsByType(LoadedPluginInfo::Type::Controller);
    auto fullPlugins = manager->getPluginsByType(LoadedPluginInfo::Type::Full);

    EXPECT_TRUE(commandPlugins.empty());
    EXPECT_TRUE(controllerPlugins.empty());
    EXPECT_TRUE(fullPlugins.empty());
}

// ============================================================================
// Shutdown Tests
// ============================================================================

TEST_F(PluginManagerTest, ShutdownEmpty) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    // Should not throw
    EXPECT_NO_THROW(manager->shutdown());
}

TEST_F(PluginManagerTest, DoubleShutdown) {
    PluginManagerConfig config;
    config.loaderConfig.pluginDirectory = pluginDir_;

    auto manager = PluginManager::createShared(config);

    manager->shutdown();
    // Second shutdown should be safe
    EXPECT_NO_THROW(manager->shutdown());
}
