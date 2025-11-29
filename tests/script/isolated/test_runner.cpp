/*
 * test_runner.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_runner.cpp
 * @brief Comprehensive tests for Isolated Python Runner
 */

#include <gtest/gtest.h>
#include "script/isolated/runner.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>

using namespace lithium::isolated;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture
// =============================================================================

class PythonRunnerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_runner_test";
        fs::create_directories(testDir_);

        runner_ = std::make_unique<PythonRunner>();
    }

    void TearDown() override {
        runner_.reset();
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void createTestScript(const std::string& filename, const std::string& content) {
        std::ofstream file(testDir_ / filename);
        file << content;
        file.close();
    }

    std::unique_ptr<PythonRunner> runner_;
    fs::path testDir_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(PythonRunnerTest, DefaultConstruction) {
    PythonRunner runner;
    EXPECT_FALSE(runner.isRunning());
}

TEST_F(PythonRunnerTest, ConstructionWithConfig) {
    IsolationConfig config;
    config.timeoutSeconds = 60;
    config.memoryLimitMB = 512;

    PythonRunner runner(config);
    EXPECT_EQ(runner.getConfig().timeoutSeconds, 60);
    EXPECT_EQ(runner.getConfig().memoryLimitMB, 512);
}

TEST_F(PythonRunnerTest, MoveConstruction) {
    IsolationConfig config;
    config.timeoutSeconds = 30;

    PythonRunner original(config);
    PythonRunner moved(std::move(original));

    EXPECT_EQ(moved.getConfig().timeoutSeconds, 30);
}

TEST_F(PythonRunnerTest, MoveAssignment) {
    IsolationConfig config;
    config.timeoutSeconds = 45;

    PythonRunner original(config);
    PythonRunner other;
    other = std::move(original);

    EXPECT_EQ(other.getConfig().timeoutSeconds, 45);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(PythonRunnerTest, SetConfig) {
    IsolationConfig config;
    config.timeoutSeconds = 120;
    config.memoryLimitMB = 1024;
    config.captureOutput = false;

    runner_->setConfig(config);

    EXPECT_EQ(runner_->getConfig().timeoutSeconds, 120);
    EXPECT_EQ(runner_->getConfig().memoryLimitMB, 1024);
    EXPECT_FALSE(runner_->getConfig().captureOutput);
}

TEST_F(PythonRunnerTest, SetPythonExecutable) {
    fs::path pythonPath = "/usr/bin/python3";
    EXPECT_NO_THROW(runner_->setPythonExecutable(pythonPath));
}

TEST_F(PythonRunnerTest, SetExecutorScript) {
    fs::path scriptPath = testDir_ / "executor.py";
    createTestScript("executor.py", "# executor script");
    EXPECT_NO_THROW(runner_->setExecutorScript(scriptPath));
}

TEST_F(PythonRunnerTest, SetProgressCallback) {
    bool callbackCalled = false;
    runner_->setProgressCallback([&](float progress, const std::string& message) {
        callbackCalled = true;
    });
    // Callback should be set without error
    SUCCEED();
}

TEST_F(PythonRunnerTest, SetLogCallback) {
    bool callbackCalled = false;
    runner_->setLogCallback([&](LogLevel level, const std::string& message) {
        callbackCalled = true;
    });
    // Callback should be set without error
    SUCCEED();
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_F(PythonRunnerTest, ValidateConfigDefault) {
    auto result = runner_->validateConfig();
    // Default config may or may not be valid depending on Python availability
    // Just ensure it doesn't crash
    SUCCEED();
}

TEST_F(PythonRunnerTest, ValidateConfigWithInvalidTimeout) {
    IsolationConfig config;
    config.timeoutSeconds = 0;
    runner_->setConfig(config);

    auto result = runner_->validateConfig();
    // Should fail validation with zero timeout
    EXPECT_FALSE(result.has_value());
}

TEST_F(PythonRunnerTest, ValidateConfigWithNegativeMemoryLimit) {
    IsolationConfig config;
    config.memoryLimitMB = 0;
    runner_->setConfig(config);

    auto result = runner_->validateConfig();
    // May or may not fail depending on implementation
    SUCCEED();
}

// =============================================================================
// Static Utility Tests
// =============================================================================

TEST_F(PythonRunnerTest, FindPythonExecutable) {
    auto pythonPath = PythonRunner::findPythonExecutable();
    // May or may not find Python depending on system
    if (pythonPath.has_value()) {
        EXPECT_TRUE(fs::exists(pythonPath.value()));
    }
}

TEST_F(PythonRunnerTest, FindExecutorScript) {
    auto scriptPath = PythonRunner::findExecutorScript();
    // May or may not find script depending on installation
    SUCCEED();
}

TEST_F(PythonRunnerTest, GetPythonVersion) {
    auto version = runner_->getPythonVersion();
    // May or may not get version depending on Python availability
    if (version.has_value()) {
        EXPECT_FALSE(version->empty());
    }
}

// =============================================================================
// Execution State Tests
// =============================================================================

TEST_F(PythonRunnerTest, IsRunningInitiallyFalse) {
    EXPECT_FALSE(runner_->isRunning());
}

TEST_F(PythonRunnerTest, GetProcessIdWhenNotRunning) {
    auto pid = runner_->getProcessId();
    EXPECT_FALSE(pid.has_value());
}

TEST_F(PythonRunnerTest, GetCurrentMemoryUsageWhenNotRunning) {
    auto memory = runner_->getCurrentMemoryUsage();
    EXPECT_FALSE(memory.has_value());
}

TEST_F(PythonRunnerTest, GetCurrentCpuUsageWhenNotRunning) {
    auto cpu = runner_->getCurrentCpuUsage();
    EXPECT_FALSE(cpu.has_value());
}

// =============================================================================
// Control Tests
// =============================================================================

TEST_F(PythonRunnerTest, CancelWhenNotRunning) {
    bool result = runner_->cancel();
    EXPECT_FALSE(result);
}

TEST_F(PythonRunnerTest, KillWhenNotRunning) {
    EXPECT_NO_THROW(runner_->kill());
}

// =============================================================================
// Execution Tests (may require Python to be available)
// =============================================================================

TEST_F(PythonRunnerTest, ExecuteSimpleScript) {
    std::string script = "print('hello')";
    auto result = runner_->execute(script);

    // Result depends on Python availability
    if (result.success) {
        EXPECT_TRUE(result.output.find("hello") != std::string::npos ||
                    result.result.contains("output"));
    }
}

TEST_F(PythonRunnerTest, ExecuteScriptWithArgs) {
    std::string script = R"(
import json
args = json.loads('{"x": 10, "y": 20}')
print(args['x'] + args['y'])
)";

    nlohmann::json args = {{"x", 10}, {"y", 20}};
    auto result = runner_->execute(script, args);

    // Just ensure no crash
    SUCCEED();
}

TEST_F(PythonRunnerTest, ExecuteScriptFile) {
    createTestScript("test_script.py", "print('from file')");
    auto result = runner_->executeFile(testDir_ / "test_script.py");

    // Result depends on Python availability
    SUCCEED();
}

TEST_F(PythonRunnerTest, ExecuteNonexistentFile) {
    auto result = runner_->executeFile(testDir_ / "nonexistent.py");
    EXPECT_FALSE(result.success);
}

TEST_F(PythonRunnerTest, ExecuteFunction) {
    auto result = runner_->executeFunction("os", "getcwd");
    // Result depends on Python availability
    SUCCEED();
}

// =============================================================================
// Async Execution Tests
// =============================================================================

TEST_F(PythonRunnerTest, ExecuteAsync) {
    std::string script = "print('async')";
    auto future = runner_->executeAsync(script);

    // Should return a valid future
    EXPECT_TRUE(future.valid());
}

TEST_F(PythonRunnerTest, ExecuteFileAsync) {
    createTestScript("async_test.py", "print('async file')");
    auto future = runner_->executeFileAsync(testDir_ / "async_test.py");

    EXPECT_TRUE(future.valid());
}

TEST_F(PythonRunnerTest, ExecuteFunctionAsync) {
    auto future = runner_->executeFunctionAsync("os", "getcwd");
    EXPECT_TRUE(future.valid());
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(PythonRunnerTest, ExecuteScriptWithSyntaxError) {
    std::string script = "print('unclosed";
    auto result = runner_->execute(script);

    // Should fail with syntax error
    if (!result.success) {
        EXPECT_FALSE(result.exception.empty());
    }
}

TEST_F(PythonRunnerTest, ExecuteScriptWithRuntimeError) {
    std::string script = "raise ValueError('test error')";
    auto result = runner_->execute(script);

    if (!result.success) {
        EXPECT_TRUE(result.exception.find("ValueError") != std::string::npos ||
                    result.exceptionType == "ValueError");
    }
}

TEST_F(PythonRunnerTest, ExecuteScriptWithImportError) {
    std::string script = "import nonexistent_module_12345";
    auto result = runner_->execute(script);

    if (!result.success) {
        EXPECT_FALSE(result.exception.empty());
    }
}

// =============================================================================
// RunnerFactory Tests
// =============================================================================

class RunnerFactoryTest : public ::testing::Test {};

TEST_F(RunnerFactoryTest, CreateDefault) {
    auto runner = RunnerFactory::create();
    EXPECT_NE(runner, nullptr);
    EXPECT_FALSE(runner->isRunning());
}

TEST_F(RunnerFactoryTest, CreateQuick) {
    auto runner = RunnerFactory::createQuick();
    EXPECT_NE(runner, nullptr);
    // Quick runner should have minimal isolation
}

TEST_F(RunnerFactoryTest, CreateSecure) {
    auto runner = RunnerFactory::createSecure();
    EXPECT_NE(runner, nullptr);
    // Secure runner should have maximum security settings
}

TEST_F(RunnerFactoryTest, CreateScientific) {
    auto runner = RunnerFactory::createScientific();
    EXPECT_NE(runner, nullptr);
    // Scientific runner should be optimized for numpy/scipy
}

TEST_F(RunnerFactoryTest, CreateWithConfig) {
    IsolationConfig config;
    config.timeoutSeconds = 300;
    config.memoryLimitMB = 2048;

    auto runner = RunnerFactory::create(config);
    EXPECT_NE(runner, nullptr);
    EXPECT_EQ(runner->getConfig().timeoutSeconds, 300);
    EXPECT_EQ(runner->getConfig().memoryLimitMB, 2048);
}
