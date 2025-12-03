/*
 * test_plugin_interface.cpp - Tests for Plugin Interface
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/plugin/plugin_interface.hpp"
#include "atom/type/json.hpp"

using namespace lithium::server::plugin;
using json = nlohmann::json;

class PluginInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// PluginMetadata Tests
// ============================================================================

TEST_F(PluginInterfaceTest, MetadataToJson) {
    PluginMetadata meta;
    meta.name = "test_plugin";
    meta.version = "1.0.0";
    meta.description = "A test plugin";
    meta.author = "Test Author";
    meta.license = "MIT";
    meta.dependencies = {"dep1", "dep2"};
    meta.tags = {"test", "example"};

    json j = meta.toJson();

    EXPECT_EQ(j["name"], "test_plugin");
    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["description"], "A test plugin");
    EXPECT_EQ(j["author"], "Test Author");
    EXPECT_EQ(j["license"], "MIT");
    EXPECT_EQ(j["dependencies"].size(), 2);
    EXPECT_EQ(j["tags"].size(), 2);
}

TEST_F(PluginInterfaceTest, MetadataFromJson) {
    json j = {
        {"name", "json_plugin"},
        {"version", "2.0.0"},
        {"description", "Plugin from JSON"},
        {"author", "JSON Author"},
        {"license", "GPL3"},
        {"dependencies", {"a", "b", "c"}},
        {"tags", {"json"}}
    };

    auto meta = PluginMetadata::fromJson(j);

    EXPECT_EQ(meta.name, "json_plugin");
    EXPECT_EQ(meta.version, "2.0.0");
    EXPECT_EQ(meta.description, "Plugin from JSON");
    EXPECT_EQ(meta.author, "JSON Author");
    EXPECT_EQ(meta.license, "GPL3");
    EXPECT_EQ(meta.dependencies.size(), 3);
    EXPECT_EQ(meta.tags.size(), 1);
}

TEST_F(PluginInterfaceTest, MetadataFromJsonWithDefaults) {
    json j = {{"name", "minimal"}};

    auto meta = PluginMetadata::fromJson(j);

    EXPECT_EQ(meta.name, "minimal");
    EXPECT_EQ(meta.version, "1.0.0");  // Default
    EXPECT_TRUE(meta.description.empty());
    EXPECT_TRUE(meta.dependencies.empty());
}

// ============================================================================
// PluginState Tests
// ============================================================================

TEST_F(PluginInterfaceTest, PluginStateToString) {
    EXPECT_EQ(pluginStateToString(PluginState::Unloaded), "unloaded");
    EXPECT_EQ(pluginStateToString(PluginState::Loading), "loading");
    EXPECT_EQ(pluginStateToString(PluginState::Loaded), "loaded");
    EXPECT_EQ(pluginStateToString(PluginState::Initialized), "initialized");
    EXPECT_EQ(pluginStateToString(PluginState::Running), "running");
    EXPECT_EQ(pluginStateToString(PluginState::Stopping), "stopping");
    EXPECT_EQ(pluginStateToString(PluginState::Error), "error");
}

// ============================================================================
// API Version Tests
// ============================================================================

TEST_F(PluginInterfaceTest, PluginApiVersion) {
    EXPECT_EQ(PLUGIN_API_VERSION, 1);
}
