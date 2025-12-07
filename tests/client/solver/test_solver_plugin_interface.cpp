/*
 * test_solver_plugin_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "client/solver/plugin/solver_plugin_interface.hpp"

namespace lithium::solver {
namespace {

// Concrete test implementation of SolverPluginBase
class TestSolverPlugin : public SolverPluginBase {
public:
    TestSolverPlugin() : SolverPluginBase("TestPlugin", "1.0.0") {}

    auto getSolverTypes() const -> std::vector<SolverTypeInfo> override {
        SolverTypeInfo info;
        info.typeName = "TestSolver";
        info.displayName = "Test Solver";
        info.pluginName = "TestPlugin";
        info.version = "1.0.0";
        info.enabled = true;
        return {info};
    }

    auto registerSolverTypes(SolverTypeRegistry&) -> size_t override {
        return 1;
    }

    auto unregisterSolverTypes(SolverTypeRegistry&) -> size_t override {
        return 1;
    }

    void registerSolverCreators(SolverFactory&) override {}
    void unregisterSolverCreators(SolverFactory&) override {}

    auto createSolver(const std::string& solverId, const nlohmann::json&)
        -> std::shared_ptr<client::SolverClient> override {
        return nullptr;  // Mock implementation
    }

    auto hasExternalBinary() const -> bool override { return true; }

    auto findBinary() -> std::optional<std::filesystem::path> override {
        return testBinaryPath_;
    }

    auto validateBinary(const std::filesystem::path& path) -> bool override {
        return std::filesystem::exists(path);
    }

    auto getBinaryVersion() const -> std::string override {
        return binaryVersion_;
    }

    auto setBinaryPath(const std::filesystem::path& path) -> bool override {
        if (std::filesystem::exists(path)) {
            binaryPath_ = path;
            return true;
        }
        return false;
    }

    auto getBinaryPath() const
        -> std::optional<std::filesystem::path> override {
        return binaryPath_;
    }

    auto getDefaultOptions() const -> nlohmann::json override {
        return {{"timeout", 60}, {"downsample", 2}};
    }

    auto validateOptions(const nlohmann::json& options)
        -> SolverResult<bool> override {
        if (options.contains("timeout")) {
            int timeout = options["timeout"].get<int>();
            if (timeout < 0) {
                return SolverResult<bool>::failure("Timeout must be positive");
            }
        }
        return SolverResult<bool>::success(true);
    }

    // Test helpers
    void setTestBinaryPath(const std::filesystem::path& path) {
        testBinaryPath_ = path;
    }

    void setBinaryVersion(const std::string& version) {
        binaryVersion_ = version;
    }

private:
    std::optional<std::filesystem::path> testBinaryPath_;
    std::string binaryVersion_{"1.0.0"};
};

class SolverPluginInterfaceTest : public ::testing::Test {
protected:
    std::unique_ptr<TestSolverPlugin> plugin;

    void SetUp() override {
        plugin = std::make_unique<TestSolverPlugin>();
    }
};

TEST_F(SolverPluginInterfaceTest, PluginNameAndVersion) {
    EXPECT_EQ(plugin->getName(), "TestPlugin");
    EXPECT_EQ(plugin->getVersion(), "1.0.0");
}

TEST_F(SolverPluginInterfaceTest, InitialState) {
    EXPECT_EQ(plugin->getState(), SolverPluginState::Unloaded);
}

TEST_F(SolverPluginInterfaceTest, InitializePlugin) {
    nlohmann::json config = {{"setting1", "value1"}};
    EXPECT_TRUE(plugin->initialize(config));
    EXPECT_EQ(plugin->getState(), SolverPluginState::Running);
}

TEST_F(SolverPluginInterfaceTest, ShutdownPlugin) {
    plugin->initialize({});
    EXPECT_EQ(plugin->getState(), SolverPluginState::Running);

    plugin->shutdown();
    EXPECT_EQ(plugin->getState(), SolverPluginState::Unloaded);
}

TEST_F(SolverPluginInterfaceTest, GetSolverTypes) {
    auto types = plugin->getSolverTypes();
    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0].typeName, "TestSolver");
    EXPECT_EQ(types[0].displayName, "Test Solver");
}

TEST_F(SolverPluginInterfaceTest, HasExternalBinary) {
    EXPECT_TRUE(plugin->hasExternalBinary());
}

TEST_F(SolverPluginInterfaceTest, GetDefaultOptions) {
    auto options = plugin->getDefaultOptions();
    EXPECT_EQ(options["timeout"], 60);
    EXPECT_EQ(options["downsample"], 2);
}

TEST_F(SolverPluginInterfaceTest, ValidateValidOptions) {
    nlohmann::json options = {{"timeout", 120}};
    auto result = plugin->validateOptions(options);
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value());
}

TEST_F(SolverPluginInterfaceTest, ValidateInvalidOptions) {
    nlohmann::json options = {{"timeout", -10}};
    auto result = plugin->validateOptions(options);
    EXPECT_FALSE(result.hasValue());
}

TEST_F(SolverPluginInterfaceTest, BinaryVersionAccess) {
    plugin->setBinaryVersion("2.5.3");
    EXPECT_EQ(plugin->getBinaryVersion(), "2.5.3");
}

TEST_F(SolverPluginInterfaceTest, EventSubscription) {
    bool eventReceived = false;
    SolverPluginEvent receivedEvent;

    auto subId = plugin->subscribe([&](const SolverPluginEvent& event) {
        eventReceived = true;
        receivedEvent = event;
    });

    plugin->initialize({});

    // Events should be emitted on state change
    EXPECT_TRUE(eventReceived);
    EXPECT_EQ(receivedEvent.pluginName, "TestPlugin");

    plugin->unsubscribe(subId);
}

TEST_F(SolverPluginInterfaceTest, SolverResultSuccess) {
    auto result = SolverResult<int>::success(42);
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(SolverPluginInterfaceTest, SolverResultFailure) {
    auto result = SolverResult<int>::failure("Test error");
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), "Test error");
}

TEST_F(SolverPluginInterfaceTest, SolverPluginMetadataSerialization) {
    SolverPluginMetadata meta;
    meta.name = "TestMeta";
    meta.version = "1.2.3";
    meta.description = "Test description";
    meta.author = "Test Author";
    meta.solverType = "test";
    meta.supportsBlindSolve = true;
    meta.requiresExternalBinary = false;
    meta.supportedFormats = {"FITS", "JPEG"};

    nlohmann::json j = meta;

    EXPECT_EQ(j["name"], "TestMeta");
    EXPECT_EQ(j["version"], "1.2.3");
    EXPECT_EQ(j["solverType"], "test");
    EXPECT_TRUE(j["supportsBlindSolve"]);
    EXPECT_FALSE(j["requiresExternalBinary"]);
    EXPECT_EQ(j["supportedFormats"].size(), 2);
}

TEST_F(SolverPluginInterfaceTest, SolverCapabilitiesDefaults) {
    SolverCapabilities caps;

    EXPECT_TRUE(caps.supportedFormats.empty());
    EXPECT_FALSE(caps.supportsBlindSolve);
    EXPECT_FALSE(caps.supportsHintedSolve);
    EXPECT_FALSE(caps.supportsAbort);
    EXPECT_FALSE(caps.supportsAsync);
    EXPECT_EQ(caps.maxConcurrentSolves, 1);
}

TEST_F(SolverPluginInterfaceTest, SolverTypeInfoComplete) {
    SolverTypeInfo info;
    info.typeName = "CompleteTest";
    info.displayName = "Complete Test Solver";
    info.pluginName = "TestPlugin";
    info.version = "1.0.0";
    info.description = "A complete test solver";
    info.priority = 75;
    info.enabled = true;
    info.capabilities.supportsBlindSolve = true;
    info.capabilities.supportsAbort = true;
    info.optionSchema = {{"type", "object"}};

    EXPECT_EQ(info.typeName, "CompleteTest");
    EXPECT_EQ(info.priority, 75);
    EXPECT_TRUE(info.enabled);
    EXPECT_TRUE(info.capabilities.supportsBlindSolve);
}

}  // namespace
}  // namespace lithium::solver
