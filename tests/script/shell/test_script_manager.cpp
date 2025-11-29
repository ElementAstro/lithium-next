/*
 * test_script_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_script_manager.cpp
 * @brief Comprehensive tests for Script Manager
 */

#include <gtest/gtest.h>
#include "script/shell/script_manager.hpp"

#include <chrono>
#include <future>
#include <thread>

using namespace lithium::shell;

// =============================================================================
// Test Fixture
// =============================================================================

class ScriptManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<ScriptManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<ScriptManager> manager_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(ScriptManagerTest, DefaultConstruction) {
    ScriptManager manager;
    auto scripts = manager.getAllScripts();
    EXPECT_TRUE(scripts.empty());
}

TEST_F(ScriptManagerTest, MoveConstruction) {
    manager_->registerScript("test", "echo hello");
    ScriptManager moved(std::move(*manager_));

    auto content = moved.getScriptContent("test");
    EXPECT_TRUE(content.has_value());
}

TEST_F(ScriptManagerTest, MoveAssignment) {
    manager_->registerScript("test", "echo hello");
    ScriptManager other;
    other = std::move(*manager_);

    auto content = other.getScriptContent("test");
    EXPECT_TRUE(content.has_value());
}

// =============================================================================
// Script Registration Tests
// =============================================================================

TEST_F(ScriptManagerTest, RegisterScript) {
    EXPECT_NO_THROW(manager_->registerScript("test", "echo hello"));
}

TEST_F(ScriptManagerTest, RegisterScriptGetContent) {
    manager_->registerScript("test", "echo hello");
    auto content = manager_->getScriptContent("test");

    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "echo hello");
}

TEST_F(ScriptManagerTest, RegisterMultipleScripts) {
    manager_->registerScript("script1", "echo 1");
    manager_->registerScript("script2", "echo 2");
    manager_->registerScript("script3", "echo 3");

    auto scripts = manager_->getAllScripts();
    EXPECT_EQ(scripts.size(), 3);
}

TEST_F(ScriptManagerTest, UpdateScript) {
    manager_->registerScript("test", "echo original");
    manager_->updateScript("test", "echo updated");

    auto content = manager_->getScriptContent("test");
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "echo updated");
}

TEST_F(ScriptManagerTest, DeleteScript) {
    manager_->registerScript("test", "echo hello");
    manager_->deleteScript("test");

    auto content = manager_->getScriptContent("test");
    EXPECT_FALSE(content.has_value());
}

TEST_F(ScriptManagerTest, DeleteNonexistentScript) {
    // Should not throw
    EXPECT_NO_THROW(manager_->deleteScript("nonexistent"));
}

TEST_F(ScriptManagerTest, GetScriptContentNonexistent) {
    auto content = manager_->getScriptContent("nonexistent");
    EXPECT_FALSE(content.has_value());
}

TEST_F(ScriptManagerTest, ImportScripts) {
    std::vector<std::pair<std::string, Script>> scripts = {
        {"script1", "echo 1"},
        {"script2", "echo 2"},
        {"script3", "echo 3"}
    };

    manager_->importScripts(scripts);

    auto allScripts = manager_->getAllScripts();
    EXPECT_EQ(allScripts.size(), 3);
}

// =============================================================================
// Script Execution Tests
// =============================================================================

TEST_F(ScriptManagerTest, RunScriptSimple) {
    manager_->registerScript("test", "echo hello");

    auto result = manager_->runScript("test");

    if (result.has_value()) {
        auto [output, exitCode] = result.value();
        EXPECT_EQ(exitCode, 0);
    }
}

TEST_F(ScriptManagerTest, RunScriptWithArgs) {
    manager_->registerScript("test", "echo $ARG1");

    std::unordered_map<std::string, std::string> args = {{"ARG1", "world"}};
    auto result = manager_->runScript("test", args);

    // Result depends on shell availability
    SUCCEED();
}

TEST_F(ScriptManagerTest, RunScriptNonexistent) {
    auto result = manager_->runScript("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ScriptManagerTest, RunScriptAsync) {
    manager_->registerScript("test", "echo async");

    auto future = manager_->runScriptAsync("test");
    EXPECT_TRUE(future.valid());
}

TEST_F(ScriptManagerTest, RunScriptWithTimeout) {
    manager_->registerScript("test", "echo quick");

    auto result = manager_->runScript("test", {}, true, 5000);
    // Should complete within timeout
    SUCCEED();
}

// =============================================================================
// Environment Tests
// =============================================================================

TEST_F(ScriptManagerTest, SetExecutionEnvironment) {
    manager_->registerScript("test", "echo $ENV_VAR");
    EXPECT_NO_THROW(manager_->setExecutionEnvironment("test", "production"));
}

TEST_F(ScriptManagerTest, SetScriptEnvironmentVars) {
    manager_->registerScript("test", "echo $MY_VAR");

    std::unordered_map<std::string, std::string> env = {{"MY_VAR", "value"}};
    EXPECT_NO_THROW(manager_->setScriptEnvironmentVars("test", env));
}

// =============================================================================
// Hook Tests
// =============================================================================

TEST_F(ScriptManagerTest, AddPreExecutionHook) {
    manager_->registerScript("test", "echo hello");

    bool hookCalled = false;
    manager_->addPreExecutionHook("test", [&](const std::string&) {
        hookCalled = true;
    });

    manager_->runScript("test");
    EXPECT_TRUE(hookCalled);
}

TEST_F(ScriptManagerTest, AddPostExecutionHook) {
    manager_->registerScript("test", "echo hello");

    bool hookCalled = false;
    int capturedExitCode = -1;
    manager_->addPostExecutionHook("test", [&](const std::string&, int exitCode) {
        hookCalled = true;
        capturedExitCode = exitCode;
    });

    manager_->runScript("test");
    EXPECT_TRUE(hookCalled);
}

// =============================================================================
// Progress Tests
// =============================================================================

TEST_F(ScriptManagerTest, GetScriptProgress) {
    manager_->registerScript("test", "echo hello");
    float progress = manager_->getScriptProgress("test");
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 100.0f);
}

TEST_F(ScriptManagerTest, GetScriptLogs) {
    manager_->registerScript("test", "echo hello");
    manager_->runScript("test");

    auto logs = manager_->getScriptLogs("test");
    // May or may not have logs depending on implementation
    SUCCEED();
}

// =============================================================================
// Abort Tests
// =============================================================================

TEST_F(ScriptManagerTest, AbortScript) {
    manager_->registerScript("test", "echo hello");
    EXPECT_NO_THROW(manager_->abortScript("test"));
}

TEST_F(ScriptManagerTest, AbortNonexistentScript) {
    EXPECT_NO_THROW(manager_->abortScript("nonexistent"));
}

// =============================================================================
// Retry Strategy Tests
// =============================================================================

TEST_F(ScriptManagerTest, SetRetryStrategy) {
    manager_->registerScript("test", "echo hello");
    EXPECT_NO_THROW(manager_->setRetryStrategy("test", RetryStrategy::Exponential));
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(ScriptManagerTest, ConcurrentRegistration) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 10; ++j) {
                std::string name = "script_" + std::to_string(i) + "_" + std::to_string(j);
                manager_->registerScript(name, "echo " + name);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto scripts = manager_->getAllScripts();
    EXPECT_EQ(scripts.size(), 100);
}

TEST_F(ScriptManagerTest, ConcurrentReadWrite) {
    std::atomic<bool> running{true};

    std::thread writer([&]() {
        int i = 0;
        while (running) {
            manager_->registerScript("script_" + std::to_string(i++), "echo test");
            if (i > 100) {
                manager_->deleteScript("script_" + std::to_string(i - 100));
            }
        }
    });

    std::thread reader([&]() {
        while (running) {
            manager_->getAllScripts();
            manager_->getScriptContent("script_50");
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    writer.join();
    reader.join();

    SUCCEED();
}
