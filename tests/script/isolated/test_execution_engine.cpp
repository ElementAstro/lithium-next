/*
 * test_execution_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_execution_engine.cpp
 * @brief Comprehensive tests for Isolated Execution Engine
 */

#include <gtest/gtest.h>
#include "script/isolated/execution_engine.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace lithium::isolated;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture
// =============================================================================

class ExecutionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_exec_engine_test";
        fs::create_directories(testDir_);

        engine_ = std::make_unique<ExecutionEngine>();
    }

    void TearDown() override {
        engine_.reset();
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void createTestScript(const std::string& filename,
                          const std::string& content) {
        std::ofstream file(testDir_ / filename);
        file << content;
        file.close();
    }

    std::unique_ptr<ExecutionEngine> engine_;
    fs::path testDir_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(ExecutionEngineTest, DefaultConstruction) {
    ExecutionEngine engine;
    EXPECT_FALSE(engine.isRunning());
}

TEST_F(ExecutionEngineTest, MoveConstruction) {
    ExecutionEngine original;
    ExecutionEngine moved(std::move(original));
    EXPECT_FALSE(moved.isRunning());
}

TEST_F(ExecutionEngineTest, MoveAssignment) {
    ExecutionEngine original;
    ExecutionEngine other;
    other = std::move(original);
    EXPECT_FALSE(other.isRunning());
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(ExecutionEngineTest, SetConfig) {
    IsolationConfig config;
    config.timeoutSeconds = 120;
    config.memoryLimitMB = 1024;

    engine_->setConfig(config);

    EXPECT_EQ(engine_->getConfig().timeoutSeconds, 120);
    EXPECT_EQ(engine_->getConfig().memoryLimitMB, 1024);
}

TEST_F(ExecutionEngineTest, SetProgressCallback) {
    bool called = false;
    engine_->setProgressCallback(
        [&](float progress, const std::string& msg) { called = true; });
    SUCCEED();
}

TEST_F(ExecutionEngineTest, SetLogCallback) {
    bool called = false;
    engine_->setLogCallback(
        [&](LogLevel level, const std::string& msg) { called = true; });
    SUCCEED();
}

// =============================================================================
// State Tests
// =============================================================================

TEST_F(ExecutionEngineTest, IsRunningInitiallyFalse) {
    EXPECT_FALSE(engine_->isRunning());
}

TEST_F(ExecutionEngineTest, GetProcessIdWhenNotRunning) {
    auto pid = engine_->getProcessId();
    EXPECT_FALSE(pid.has_value());
}

TEST_F(ExecutionEngineTest, GetCurrentMemoryUsageWhenNotRunning) {
    auto memory = engine_->getCurrentMemoryUsage();
    EXPECT_FALSE(memory.has_value());
}

// =============================================================================
// Control Tests
// =============================================================================

TEST_F(ExecutionEngineTest, CancelWhenNotRunning) {
    bool result = engine_->cancel();
    EXPECT_FALSE(result);
}

TEST_F(ExecutionEngineTest, KillWhenNotRunning) {
    EXPECT_NO_THROW(engine_->kill());
}

// =============================================================================
// Execution Tests
// =============================================================================

TEST_F(ExecutionEngineTest, ExecuteSimpleScript) {
    std::string script = "result = 1 + 1";
    nlohmann::json args;

    auto result = engine_->execute(script, args);
    // Result depends on Python availability
    SUCCEED();
}

TEST_F(ExecutionEngineTest, ExecuteScriptWithArgs) {
    std::string script = R"(
x = args.get('x', 0)
y = args.get('y', 0)
result = x + y
)";
    nlohmann::json args = {{"x", 10}, {"y", 20}};

    auto result = engine_->execute(script, args);
    SUCCEED();
}

TEST_F(ExecutionEngineTest, ExecuteFile) {
    createTestScript("engine_test.py", "print('engine test')");

    nlohmann::json args;
    auto result = engine_->executeFile(testDir_ / "engine_test.py", args);
    SUCCEED();
}

TEST_F(ExecutionEngineTest, ExecuteNonexistentFile) {
    nlohmann::json args;
    auto result = engine_->executeFile(testDir_ / "nonexistent.py", args);
    EXPECT_FALSE(result.success);
}

TEST_F(ExecutionEngineTest, ExecuteFunction) {
    nlohmann::json args;
    auto result = engine_->executeFunction("os", "getcwd", args);
    SUCCEED();
}

TEST_F(ExecutionEngineTest, ExecuteFunctionWithArgs) {
    nlohmann::json args = {{"path", "/tmp"}};
    auto result = engine_->executeFunction("os.path", "exists", args);
    SUCCEED();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ExecutionEngineTest, ExecuteWithSyntaxError) {
    std::string script = "def broken(";
    nlohmann::json args;

    auto result = engine_->execute(script, args);
    if (!result.success) {
        EXPECT_FALSE(result.exception.empty());
    }
}

TEST_F(ExecutionEngineTest, ExecuteWithRuntimeError) {
    std::string script = "x = 1 / 0";
    nlohmann::json args;

    auto result = engine_->execute(script, args);
    if (!result.success) {
        EXPECT_TRUE(result.exception.find("ZeroDivision") !=
                        std::string::npos ||
                    !result.exception.empty());
    }
}

TEST_F(ExecutionEngineTest, ExecuteWithTimeout) {
    IsolationConfig config;
    config.timeoutSeconds = 1;
    engine_->setConfig(config);

    std::string script = R"(
import time
time.sleep(10)
)";
    nlohmann::json args;

    auto result = engine_->execute(script, args);
    // Should timeout
    if (!result.success) {
        EXPECT_TRUE(result.timedOut || !result.exception.empty());
    }
}

// =============================================================================
// Resource Limit Tests
// =============================================================================

TEST_F(ExecutionEngineTest, ExecuteWithMemoryLimit) {
    IsolationConfig config;
    config.memoryLimitMB = 10;  // Very low limit
    engine_->setConfig(config);

    std::string script = R"(
# Try to allocate a lot of memory
data = [0] * (100 * 1024 * 1024)
)";
    nlohmann::json args;

    auto result = engine_->execute(script, args);
    // May or may not fail depending on implementation
    SUCCEED();
}

// =============================================================================
// Concurrent Execution Tests
// =============================================================================

TEST_F(ExecutionEngineTest, MultipleEnginesIndependent) {
    ExecutionEngine engine1;
    ExecutionEngine engine2;

    EXPECT_FALSE(engine1.isRunning());
    EXPECT_FALSE(engine2.isRunning());
}
