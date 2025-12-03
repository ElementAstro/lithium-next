/*
 * test_plugin_loader.cpp - Tests for Plugin Loader
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "server/plugin/plugin_loader.hpp"
#include "server/plugin/base_plugin.hpp"
#include "atom/type/json.hpp"

using namespace lithium::server::plugin;
using json = nlohmann::json;
namespace fs = std::filesystem;

class PluginLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary plugin directory
        testDir_ = fs::temp_directory_path() / "lithium_plugin_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        // Clean up
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    fs::path testDir_;
};

// ============================================================================
// PluginLoadError Tests
// ============================================================================

TEST_F(PluginLoaderTest, PluginLoadErrorToString) {
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::FileNotFound),
              "Plugin file not found");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::InvalidPlugin),
              "Invalid plugin format");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::ApiVersionMismatch),
              "Plugin API version mismatch");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::DependencyMissing),
              "Plugin dependency missing");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::InitializationFailed),
              "Plugin initialization failed");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::AlreadyLoaded),
              "Plugin already loaded");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::LoadFailed),
              "Plugin load failed");
    EXPECT_EQ(pluginLoadErrorToString(PluginLoadError::SymbolNotFound),
              "Required symbol not found in plugin");
}

// ============================================================================
// PluginLoaderConfig Tests
// ============================================================================

TEST_F(PluginLoaderTest, DefaultConfig) {
    PluginLoaderConfig config;

    EXPECT_EQ(config.pluginDirectory, "plugins/server");
    EXPECT_TRUE(config.searchPaths.empty());
    EXPECT_TRUE(config.autoLoadOnStartup);
    EXPECT_TRUE(config.enableHotReload);
    EXPECT_EQ(config.apiVersion, PLUGIN_API_VERSION);
    EXPECT_EQ(config.threadPoolSize, 4);
}

TEST_F(PluginLoaderTest, CustomConfig) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;
    config.searchPaths = {"/path1", "/path2"};
    config.autoLoadOnStartup = false;
    config.enableHotReload = false;

    auto loader = PluginLoader::createShared(config);
    EXPECT_NE(loader, nullptr);
}

// ============================================================================
// PluginLoader Basic Tests
// ============================================================================

TEST_F(PluginLoaderTest, CreateShared) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    EXPECT_NE(loader, nullptr);
}

TEST_F(PluginLoaderTest, IsPluginLoadedReturnsFalseForUnknown) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    EXPECT_FALSE(loader->isPluginLoaded("nonexistent"));
}

TEST_F(PluginLoaderTest, GetPluginReturnsNulloptForUnknown) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto plugin = loader->getPlugin("nonexistent");
    EXPECT_FALSE(plugin.has_value());
}

TEST_F(PluginLoaderTest, GetAllPluginsEmptyInitially) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto plugins = loader->getAllPlugins();
    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginLoaderTest, LoadPluginFileNotFound) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto result = loader->loadPlugin("/nonexistent/path/plugin.so");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

TEST_F(PluginLoaderTest, LoadPluginByNameNotFound) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto result = loader->loadPluginByName("nonexistent");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

TEST_F(PluginLoaderTest, UnloadPluginNotLoaded) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto result = loader->unloadPlugin("nonexistent");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

TEST_F(PluginLoaderTest, ReloadPluginNotLoaded) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto result = loader->reloadPlugin("nonexistent");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PluginLoadError::FileNotFound);
}

// ============================================================================
// Plugin Discovery Tests
// ============================================================================

TEST_F(PluginLoaderTest, DiscoverPluginsEmptyDirectory) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto discovered = loader->discoverPlugins();

    EXPECT_TRUE(discovered.empty());
}

TEST_F(PluginLoaderTest, LoadAllDiscoveredEmpty) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    size_t loaded = loader->loadAllDiscovered();

    EXPECT_EQ(loaded, 0);
}

// ============================================================================
// Plugin Configuration Tests
// ============================================================================

TEST_F(PluginLoaderTest, SetAndGetPluginConfig) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);

    json testConfig = {{"key", "value"}, {"number", 42}};
    loader->setPluginConfig("test_plugin", testConfig);

    auto retrieved = loader->getPluginConfig("test_plugin");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ((*retrieved)["key"], "value");
    EXPECT_EQ((*retrieved)["number"], 42);
}

TEST_F(PluginLoaderTest, GetPluginConfigNotSet) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto config_result = loader->getPluginConfig("unknown");

    EXPECT_FALSE(config_result.has_value());
}

// ============================================================================
// Dependency Validation Tests
// ============================================================================

TEST_F(PluginLoaderTest, ValidateDependenciesNotLoaded) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    bool valid = loader->validateDependencies("nonexistent");

    EXPECT_FALSE(valid);
}

// ============================================================================
// Load Order Tests
// ============================================================================

TEST_F(PluginLoaderTest, GetLoadOrderEmpty) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto order = loader->getLoadOrder();

    EXPECT_TRUE(order.empty());
}

// ============================================================================
// UnloadAll Tests
// ============================================================================

TEST_F(PluginLoaderTest, UnloadAllEmpty) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);

    // Should not throw
    EXPECT_NO_THROW(loader->unloadAll());
}

// ============================================================================
// Command/Controller Plugin Getters Tests
// ============================================================================

TEST_F(PluginLoaderTest, GetCommandPluginsEmpty) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto plugins = loader->getCommandPlugins();

    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginLoaderTest, GetControllerPluginsEmpty) {
    PluginLoaderConfig config;
    config.pluginDirectory = testDir_;

    auto loader = PluginLoader::createShared(config);
    auto plugins = loader->getControllerPlugins();

    EXPECT_TRUE(plugins.empty());
}
