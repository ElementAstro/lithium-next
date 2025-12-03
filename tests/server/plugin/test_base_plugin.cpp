/*
 * test_base_plugin.cpp - Tests for Base Plugin Implementation
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/plugin/base_plugin.hpp"
#include "atom/type/json.hpp"

using namespace lithium::server::plugin;
using json = nlohmann::json;

// ============================================================================
// Mock Plugin Implementations for Testing
// ============================================================================

class MockBasePlugin : public BasePlugin {
public:
    MockBasePlugin()
        : BasePlugin(PluginMetadata{
              .name = "mock_plugin",
              .version = "1.0.0",
              .description = "Mock plugin for testing",
              .author = "Test",
              .license = "MIT",
              .dependencies = {},
              .tags = {"test"}
          }) {}

    bool initializeCalled = false;
    bool shutdownCalled = false;
    bool shouldFailInit = false;

protected:
    auto onInitialize(const json& config) -> bool override {
        initializeCalled = true;
        if (shouldFailInit) {
            setError("Forced initialization failure");
            return false;
        }
        return true;
    }

    void onShutdown() override {
        shutdownCalled = true;
    }
};

class MockCommandPlugin : public BaseCommandPlugin {
public:
    MockCommandPlugin()
        : BaseCommandPlugin(PluginMetadata{
              .name = "mock_command_plugin",
              .version = "1.0.0",
              .description = "Mock command plugin",
              .author = "Test",
              .license = "MIT",
              .dependencies = {},
              .tags = {"command", "test"}
          }) {}

    bool registerCalled = false;
    bool unregisterCalled = false;

protected:
    void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        registerCalled = true;
        addCommandId("mock.command1");
        addCommandId("mock.command2");
    }

    void onUnregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        unregisterCalled = true;
        BaseCommandPlugin::onUnregisterCommands(dispatcher);
    }
};

class MockControllerPlugin : public BaseControllerPlugin {
public:
    MockControllerPlugin()
        : BaseControllerPlugin(
              PluginMetadata{
                  .name = "mock_controller_plugin",
                  .version = "1.0.0",
                  .description = "Mock controller plugin",
                  .author = "Test",
                  .license = "MIT",
                  .dependencies = {},
                  .tags = {"controller", "test"}
              },
              "/api/v1/mock") {}

    bool registerCalled = false;

protected:
    void onRegisterRoutes(ServerApp& app) override {
        registerCalled = true;
        addRoutePath("/api/v1/mock/test");
        addRoutePath("/api/v1/mock/info");
    }
};

class MockFullPlugin : public BaseFullPlugin {
public:
    MockFullPlugin()
        : BaseFullPlugin(
              PluginMetadata{
                  .name = "mock_full_plugin",
                  .version = "1.0.0",
                  .description = "Mock full plugin",
                  .author = "Test",
                  .license = "MIT",
                  .dependencies = {},
                  .tags = {"full", "test"}
              },
              "/api/v1/full") {}

    bool commandsRegistered = false;
    bool routesRegistered = false;

protected:
    void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        commandsRegistered = true;
        addCommandId("full.command");
    }

    void onRegisterRoutes(ServerApp& app) override {
        routesRegistered = true;
        addRoutePath("/api/v1/full/endpoint");
    }
};

// ============================================================================
// BasePlugin Tests
// ============================================================================

class BasePluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_ = std::make_unique<MockBasePlugin>();
    }

    std::unique_ptr<MockBasePlugin> plugin_;
};

TEST_F(BasePluginTest, GetMetadata) {
    const auto& meta = plugin_->getMetadata();

    EXPECT_EQ(meta.name, "mock_plugin");
    EXPECT_EQ(meta.version, "1.0.0");
    EXPECT_EQ(meta.description, "Mock plugin for testing");
}

TEST_F(BasePluginTest, InitialState) {
    EXPECT_EQ(plugin_->getState(), PluginState::Unloaded);
    EXPECT_FALSE(plugin_->isHealthy());
}

TEST_F(BasePluginTest, Initialize) {
    json config = {{"key", "value"}};
    bool result = plugin_->initialize(config);

    EXPECT_TRUE(result);
    EXPECT_TRUE(plugin_->initializeCalled);
    EXPECT_EQ(plugin_->getState(), PluginState::Initialized);
    EXPECT_TRUE(plugin_->isHealthy());
}

TEST_F(BasePluginTest, InitializeFails) {
    plugin_->shouldFailInit = true;

    json config = {};
    bool result = plugin_->initialize(config);

    EXPECT_FALSE(result);
    EXPECT_EQ(plugin_->getState(), PluginState::Error);
    EXPECT_FALSE(plugin_->isHealthy());
    EXPECT_FALSE(plugin_->getLastError().empty());
}

TEST_F(BasePluginTest, DoubleInitialize) {
    plugin_->initialize({});

    // Second initialize should fail
    bool result = plugin_->initialize({});

    EXPECT_FALSE(result);
}

TEST_F(BasePluginTest, Shutdown) {
    plugin_->initialize({});
    plugin_->shutdown();

    EXPECT_TRUE(plugin_->shutdownCalled);
    EXPECT_EQ(plugin_->getState(), PluginState::Unloaded);
}

TEST_F(BasePluginTest, ShutdownWithoutInitialize) {
    plugin_->shutdown();

    EXPECT_FALSE(plugin_->shutdownCalled);
    EXPECT_EQ(plugin_->getState(), PluginState::Unloaded);
}

// ============================================================================
// BaseCommandPlugin Tests
// ============================================================================

class BaseCommandPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_ = std::make_unique<MockCommandPlugin>();
    }

    std::unique_ptr<MockCommandPlugin> plugin_;
};

TEST_F(BaseCommandPluginTest, GetMetadata) {
    const auto& meta = plugin_->getMetadata();

    EXPECT_EQ(meta.name, "mock_command_plugin");
}

TEST_F(BaseCommandPluginTest, GetCommandIdsEmpty) {
    auto ids = plugin_->getCommandIds();

    EXPECT_TRUE(ids.empty());
}

TEST_F(BaseCommandPluginTest, RegisterCommands) {
    plugin_->registerCommands(nullptr);

    EXPECT_TRUE(plugin_->registerCalled);

    auto ids = plugin_->getCommandIds();
    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], "mock.command1");
    EXPECT_EQ(ids[1], "mock.command2");
}

TEST_F(BaseCommandPluginTest, UnregisterCommands) {
    plugin_->registerCommands(nullptr);
    plugin_->unregisterCommands(nullptr);

    EXPECT_TRUE(plugin_->unregisterCalled);
}

// ============================================================================
// BaseControllerPlugin Tests
// ============================================================================

class BaseControllerPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_ = std::make_unique<MockControllerPlugin>();
    }

    std::unique_ptr<MockControllerPlugin> plugin_;
};

TEST_F(BaseControllerPluginTest, GetMetadata) {
    const auto& meta = plugin_->getMetadata();

    EXPECT_EQ(meta.name, "mock_controller_plugin");
}

TEST_F(BaseControllerPluginTest, GetRoutePrefix) {
    EXPECT_EQ(plugin_->getRoutePrefix(), "/api/v1/mock");
}

TEST_F(BaseControllerPluginTest, GetRoutePathsEmpty) {
    auto paths = plugin_->getRoutePaths();

    EXPECT_TRUE(paths.empty());
}

// ============================================================================
// BaseFullPlugin Tests
// ============================================================================

class BaseFullPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_ = std::make_unique<MockFullPlugin>();
    }

    std::unique_ptr<MockFullPlugin> plugin_;
};

TEST_F(BaseFullPluginTest, GetMetadata) {
    const auto& meta = plugin_->getMetadata();

    EXPECT_EQ(meta.name, "mock_full_plugin");
}

TEST_F(BaseFullPluginTest, GetRoutePrefix) {
    EXPECT_EQ(plugin_->getRoutePrefix(), "/api/v1/full");
}

TEST_F(BaseFullPluginTest, RegisterCommands) {
    plugin_->registerCommands(nullptr);

    EXPECT_TRUE(plugin_->commandsRegistered);

    auto ids = plugin_->getCommandIds();
    EXPECT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], "full.command");
}

TEST_F(BaseFullPluginTest, InitializeAndShutdown) {
    EXPECT_TRUE(plugin_->initialize({}));
    EXPECT_EQ(plugin_->getState(), PluginState::Initialized);

    plugin_->shutdown();
    EXPECT_EQ(plugin_->getState(), PluginState::Unloaded);
}
