#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "script/sheller.hpp"

using namespace lithium;

class ScriptManagerTest : public ::testing::Test {
protected:
    void SetUp() override { manager = std::make_unique<ScriptManager>(); }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<ScriptManager> manager;
};

TEST_F(ScriptManagerTest, BasicScriptRegistration) {
    EXPECT_NO_THROW(manager->registerScript("test", "echo 'hello'"));
    auto scripts = manager->getAllScripts();
    EXPECT_EQ(scripts.size(), 1);
    EXPECT_EQ(scripts["test"], "echo 'hello'");
}

TEST_F(ScriptManagerTest, PowerShellScriptRegistration) {
    EXPECT_NO_THROW(
        manager->registerPowerShellScript("ps_test", "Write-Host 'hello'"));
    auto scripts = manager->getAllScripts();
    EXPECT_EQ(scripts.size(), 1);
    EXPECT_TRUE(scripts.contains("ps_test"));
}

TEST_F(ScriptManagerTest, ScriptDeletion) {
    manager->registerScript("test", "echo 'hello'");
    EXPECT_NO_THROW(manager->deleteScript("test"));
    auto scripts = manager->getAllScripts();
    EXPECT_TRUE(scripts.empty());
}

TEST_F(ScriptManagerTest, ScriptUpdate) {
    manager->registerScript("test", "echo 'hello'");
    EXPECT_NO_THROW(manager->updateScript("test", "echo 'updated'"));
    auto scripts = manager->getAllScripts();
    EXPECT_EQ(scripts["test"], "echo 'updated'");
}

TEST_F(ScriptManagerTest, BasicScriptExecution) {
    manager->registerScript("test", "echo 'hello'");
    auto result = manager->runScript("test", {});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 0);  // Expected success exit code
    EXPECT_TRUE(result->first.find("hello") != std::string::npos);
}

TEST_F(ScriptManagerTest, ScriptWithArguments) {
    manager->registerScript("test", "echo $1");
    auto result = manager->runScript("test", {{"arg1", "hello"}});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->first.find("hello") != std::string::npos);
}

TEST_F(ScriptManagerTest, ScriptVersioning) {
    manager->registerScript("test", "v1");
    manager->enableVersioning();
    manager->updateScript("test", "v2");
    EXPECT_TRUE(manager->rollbackScript("test", 0));
    auto scripts = manager->getAllScripts();
    EXPECT_EQ(scripts["test"], "v1");
}

TEST_F(ScriptManagerTest, MaxVersionLimit) {
    manager->setMaxScriptVersions(2);
    manager->enableVersioning();
    manager->registerScript("test", "v1");
    manager->updateScript("test", "v2");
    manager->updateScript("test", "v3");
    // Should only keep v2 and v3
    EXPECT_FALSE(manager->rollbackScript("test", 2));
}

TEST_F(ScriptManagerTest, ConcurrentExecution) {
    manager->registerScript("script1", "echo 'one'");
    manager->registerScript("script2", "echo 'two'");

    std::vector<
        std::pair<std::string, std::unordered_map<std::string, std::string>>>
        scripts = {{"script1", {}}, {"script2", {}}};

    auto results = manager->runScriptsConcurrently(scripts);
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_TRUE(results[1].has_value());
}

TEST_F(ScriptManagerTest, AsyncExecution) {
    manager->registerScript("test", "echo 'async'");
    auto future = manager->runScriptAsync("test", {});
    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->first.find("async") != std::string::npos);
}

TEST_F(ScriptManagerTest, ProgressTracking) {
    manager->registerScript("test", R"(
        echo "PROGRESS:0"
        sleep 1
        echo "PROGRESS:50"
        sleep 1
        echo "PROGRESS:100"
    )");

    auto future = manager->runScriptAsync("test", {});

    // Check progress
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    float progress = manager->getScriptProgress("test");
    EXPECT_GE(progress, 0.0f);

    future.get();
}

TEST_F(ScriptManagerTest, ScriptAbortion) {
    manager->registerScript("test", R"(
        while true; do
            echo "running"
            sleep 1
        done
    )");

    auto future = manager->runScriptAsync("test", {});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    manager->abortScript("test");

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, -999);  // Special abort status code
}

TEST_F(ScriptManagerTest, ErrorHandling) {
    EXPECT_THROW(manager->deleteScript("nonexistent"), ScriptException);
    EXPECT_THROW(manager->runScript("nonexistent", {}), ScriptException);
}

TEST_F(ScriptManagerTest, ScriptLogging) {
    manager->registerScript("test", "echo 'log test'");
    manager->runScript("test", {});
    auto logs = manager->getScriptLogs("test");
    EXPECT_FALSE(logs.empty());
}

TEST_F(ScriptManagerTest, SequentialExecution) {
    manager->registerScript("script1", "echo 'first'");
    manager->registerScript("script2", "echo 'second'");

    std::vector<
        std::pair<std::string, std::unordered_map<std::string, std::string>>>
        scripts = {{"script1", {}}, {"script2", {}}};

    auto results = manager->runScriptsSequentially(scripts);
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_TRUE(results[1].has_value());
}

TEST_F(ScriptManagerTest, ScriptOutputAndStatus) {
    manager->registerScript("test", "exit 42");
    manager->runScript("test", {});

    auto status = manager->getScriptStatus("test");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status.value(), 42);
}

TEST_F(ScriptManagerTest, PowerShellSpecificFeatures) {
    manager->registerPowerShellScript("ps_test", R"(
        $ErrorActionPreference = 'Stop'
        Write-Host "PowerShell Test"
    )");

    auto result = manager->runScript("ps_test", {});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->first.find("PowerShell Test") != std::string::npos);
}
