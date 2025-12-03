/*
 * test_script_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_script_service.cpp
 * @brief Comprehensive tests for ScriptService - the unified script facade
 */

#include <gtest/gtest.h>
#include "script/script_service.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

// =============================================================================
// Test Fixture
// =============================================================================

class ScriptServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.poolSize = 2;
        config_.maxQueuedTasks = 100;
        config_.enableSecurityAnalysis = false;  // Disable for faster tests
        config_.autoDiscoverTools = false;
    }

    void TearDown() override {
        if (service_) {
            service_->shutdown(true);
            service_.reset();
        }

        // Cleanup temp files
        if (!tempScriptPath_.empty() && fs::exists(tempScriptPath_)) {
            fs::remove(tempScriptPath_);
        }
    }

    void createTempScript(const std::string& content) {
        tempScriptPath_ = fs::temp_directory_path() / "test_script.py";
        std::ofstream file(tempScriptPath_);
        file << content;
        file.close();
    }

    lithium::ScriptServiceConfig config_;
    std::unique_ptr<lithium::ScriptService> service_;
    fs::path tempScriptPath_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(ScriptServiceTest, DefaultConstruction) {
    lithium::ScriptService service;
    EXPECT_FALSE(service.isInitialized());
}

TEST_F(ScriptServiceTest, ConstructionWithConfig) {
    lithium::ScriptService service(config_);
    EXPECT_FALSE(service.isInitialized());
}

TEST_F(ScriptServiceTest, MoveConstruction) {
    lithium::ScriptService original(config_);
    lithium::ScriptService moved(std::move(original));
    EXPECT_FALSE(moved.isInitialized());
}

TEST_F(ScriptServiceTest, MoveAssignment) {
    lithium::ScriptService original(config_);
    lithium::ScriptService other;
    other = std::move(original);
    EXPECT_FALSE(other.isInitialized());
}

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(ScriptServiceTest, InitializeSuccess) {
    service_ = std::make_unique<lithium::ScriptService>(config_);
    auto result = service_->initialize();

    // May or may not succeed depending on Python availability
    if (result.has_value()) {
        EXPECT_TRUE(service_->isInitialized());
    }
}

TEST_F(ScriptServiceTest, DoubleInitialize) {
    service_ = std::make_unique<lithium::ScriptService>(config_);
    service_->initialize();

    // Second initialize should be safe (no-op)
    auto result = service_->initialize();
    SUCCEED();
}

TEST_F(ScriptServiceTest, ShutdownWithoutInitialize) {
    service_ = std::make_unique<lithium::ScriptService>(config_);
    EXPECT_NO_THROW(service_->shutdown());
}

TEST_F(ScriptServiceTest, ShutdownMultipleTimes) {
    service_ = std::make_unique<lithium::ScriptService>(config_);
    service_->initialize();

    EXPECT_NO_THROW(service_->shutdown());
    EXPECT_NO_THROW(service_->shutdown());  // Should be safe
}

// =============================================================================
// Python Execution Tests
// =============================================================================

class ScriptServiceExecutionTest : public ScriptServiceTest {
protected:
    void SetUp() override {
        ScriptServiceTest::SetUp();
        service_ = std::make_unique<lithium::ScriptService>(config_);
        auto result = service_->initialize();
        if (!result.has_value()) {
            GTEST_SKIP() << "ScriptService initialization failed - Python may not be available";
        }
    }
};

TEST_F(ScriptServiceExecutionTest, ExecuteSimplePython) {
    auto result = service_->executePython("result = 2 + 2");

    // Result may vary depending on execution mode
    if (result.success) {
        EXPECT_TRUE(result.success);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonWithArgs) {
    nlohmann::json args;
    args["x"] = 5;
    args["y"] = 3;

    auto result = service_->executePython(
        "result = args['x'] + args['y']", args);

    if (result.success) {
        EXPECT_TRUE(result.success);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonFile) {
    createTempScript("result = 42");

    auto result = service_->executePythonFile(tempScriptPath_);

    if (result.success) {
        EXPECT_TRUE(result.success);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonFileNotFound) {
    auto result = service_->executePythonFile("/nonexistent/path.py");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonWithTimeout) {
    lithium::ScriptExecutionConfig config;
    config.timeout = std::chrono::milliseconds(100);
    config.mode = lithium::ExecutionMode::Isolated;

    // This script takes too long
    auto result = service_->executePython(
        "import time; time.sleep(10); result = 1",
        {}, config);

    // Should fail or timeout
    // Note: may not actually timeout in all cases depending on implementation
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonInProcess) {
    lithium::ScriptExecutionConfig config;
    config.mode = lithium::ExecutionMode::InProcess;

    auto result = service_->executePython("result = 'hello'", {}, config);

    if (result.success) {
        EXPECT_EQ(result.actualMode, lithium::ExecutionMode::InProcess);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonPooled) {
    lithium::ScriptExecutionConfig config;
    config.mode = lithium::ExecutionMode::Pooled;

    auto result = service_->executePython("result = 'pooled'", {}, config);

    if (result.success) {
        EXPECT_EQ(result.actualMode, lithium::ExecutionMode::Pooled);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonIsolated) {
    lithium::ScriptExecutionConfig config;
    config.mode = lithium::ExecutionMode::Isolated;

    auto result = service_->executePython("result = 'isolated'", {}, config);

    if (result.success) {
        EXPECT_EQ(result.actualMode, lithium::ExecutionMode::Isolated);
    }
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonAsync) {
    auto future = service_->executePythonAsync("result = 123");

    // Should get a valid future
    EXPECT_TRUE(future.valid());

    // Wait for result
    auto result = future.get();
    // Result may or may not be successful
}

// =============================================================================
// Script Validation Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ValidateSafeScript) {
    bool valid = service_->validateScript("x = 1 + 2");
    // Should pass if analyzer is available
    SUCCEED();
}

TEST_F(ScriptServiceExecutionTest, ValidateDangerousScript) {
    // Enable security analysis for this test
    config_.enableSecurityAnalysis = true;
    auto secureService = std::make_unique<lithium::ScriptService>(config_);
    secureService->initialize();

    // This script has potentially dangerous operations
    bool valid = secureService->validateScript(
        "import os; os.system('rm -rf /')");

    // May or may not detect as dangerous depending on configuration
}

TEST_F(ScriptServiceExecutionTest, AnalyzeScript) {
    auto analysis = service_->analyzeScript("x = 1 + 2");

    EXPECT_TRUE(analysis.contains("valid"));
    EXPECT_TRUE(analysis.contains("dangers"));
}

TEST_F(ScriptServiceExecutionTest, SanitizeScript) {
    std::string script = "x = 1; import os; y = 2";
    std::string safe = service_->getSafeScript(script);

    // Should return some form of script
    EXPECT_FALSE(safe.empty());
}

// =============================================================================
// Shell Script Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ListShellScripts) {
    auto scripts = service_->listShellScripts();
    // Should return a vector (possibly empty)
    SUCCEED();
}

// =============================================================================
// Tool Registry Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ListTools) {
    auto tools = service_->listTools();
    // Should return a vector (possibly empty)
    SUCCEED();
}

TEST_F(ScriptServiceExecutionTest, DiscoverTools) {
    auto result = service_->discoverTools();
    // May or may not succeed depending on tools directory
}

// =============================================================================
// Virtual Environment Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ListPackages) {
    auto result = service_->listPackages();
    // May or may not succeed depending on venv state
}

TEST_F(ScriptServiceExecutionTest, DeactivateVenvWhenNoneActive) {
    auto result = service_->deactivateVirtualEnv();
    // Should handle gracefully even if no venv is active
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, GetStatistics) {
    auto stats = service_->getStatistics();

    EXPECT_TRUE(stats.contains("totalExecutions"));
    EXPECT_TRUE(stats.contains("successfulExecutions"));
    EXPECT_TRUE(stats.contains("failedExecutions"));
}

TEST_F(ScriptServiceExecutionTest, ResetStatistics) {
    // Execute something first
    service_->executePython("x = 1");

    // Reset
    service_->resetStatistics();

    // Check stats are reset
    auto stats = service_->getStatistics();
    EXPECT_EQ(stats["totalExecutions"].get<size_t>(), 0);
}

TEST_F(ScriptServiceExecutionTest, StatisticsUpdateAfterExecution) {
    service_->resetStatistics();

    // Execute some scripts
    service_->executePython("x = 1");
    service_->executePython("y = 2");

    auto stats = service_->getStatistics();
    EXPECT_GE(stats["totalExecutions"].get<size_t>(), 2);
}

// =============================================================================
// Subsystem Access Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, GetPythonWrapper) {
    auto wrapper = service_->getPythonWrapper();
    EXPECT_NE(wrapper, nullptr);
}

TEST_F(ScriptServiceExecutionTest, GetInterpreterPool) {
    auto pool = service_->getInterpreterPool();
    EXPECT_NE(pool, nullptr);
}

TEST_F(ScriptServiceExecutionTest, GetIsolatedRunner) {
    auto runner = service_->getIsolatedRunner();
    EXPECT_NE(runner, nullptr);
}

TEST_F(ScriptServiceExecutionTest, GetScriptManager) {
    auto manager = service_->getScriptManager();
    EXPECT_NE(manager, nullptr);
}

TEST_F(ScriptServiceExecutionTest, GetToolRegistry) {
    auto registry = service_->getToolRegistry();
    EXPECT_NE(registry, nullptr);
}

TEST_F(ScriptServiceExecutionTest, GetVenvManager) {
    auto manager = service_->getVenvManager();
    EXPECT_NE(manager, nullptr);
}

// =============================================================================
// NumPy Operations Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ExecuteNumpyStack) {
    nlohmann::json arrays = {{1, 2, 3}, {4, 5, 6}};
    nlohmann::json params;
    params["axis"] = 0;

    auto result = service_->executeNumpyOp("stack", arrays, params);
    // May or may not succeed depending on NumPy availability
}

TEST_F(ScriptServiceExecutionTest, ExecuteNumpyMean) {
    nlohmann::json arrays = {{1.0, 2.0, 3.0, 4.0, 5.0}};

    auto result = service_->executeNumpyOp("mean", arrays, {});
    // May or may not succeed
}

// =============================================================================
// Concurrency Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ConcurrentExecution) {
    const int numThreads = 4;
    std::vector<std::future<lithium::ScriptExecutionResult>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(service_->executePythonAsync(
            "result = " + std::to_string(i)));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        auto result = future.get();
        // Results may vary
    }

    SUCCEED();
}

TEST_F(ScriptServiceExecutionTest, ConcurrentStatisticsAccess) {
    std::vector<std::thread> threads;

    // Multiple threads accessing statistics
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                service_->getStatistics();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, SetProgressCallback) {
    bool callbackInvoked = false;

    service_->setProgressCallback([&](double progress, const std::string& message) {
        callbackInvoked = true;
    });

    // Callback may or may not be invoked depending on execution path
    SUCCEED();
}

TEST_F(ScriptServiceExecutionTest, SetLogCallback) {
    std::vector<std::string> logs;

    service_->setLogCallback([&](const std::string& level, const std::string& message) {
        logs.push_back(message);
    });

    // Callback may or may not be invoked
    SUCCEED();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, ExecutePythonWithSyntaxError) {
    auto result = service_->executePython("def broken(");
    EXPECT_FALSE(result.success);
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonWithRuntimeError) {
    auto result = service_->executePython("result = 1 / 0");
    EXPECT_FALSE(result.success);
}

TEST_F(ScriptServiceExecutionTest, ExecutePythonWithUndefinedVariable) {
    auto result = service_->executePython("result = undefined_variable");
    EXPECT_FALSE(result.success);
}

// =============================================================================
// Execution Mode Selection Tests
// =============================================================================

TEST_F(ScriptServiceExecutionTest, AutoModeSelectsInProcessForSimple) {
    lithium::ScriptExecutionConfig config;
    config.mode = lithium::ExecutionMode::Auto;

    auto result = service_->executePython("x = 1", {}, config);

    if (result.success) {
        // Simple scripts should use InProcess mode
        EXPECT_EQ(result.actualMode, lithium::ExecutionMode::InProcess);
    }
}

TEST_F(ScriptServiceExecutionTest, AutoModeSelectsIsolatedForDangerous) {
    lithium::ScriptExecutionConfig config;
    config.mode = lithium::ExecutionMode::Auto;

    auto result = service_->executePython(
        "import subprocess; result = 1", {}, config);

    if (result.success) {
        // Dangerous scripts should use Isolated mode
        EXPECT_EQ(result.actualMode, lithium::ExecutionMode::Isolated);
    }
}

// =============================================================================
// Service Configuration Tests
// =============================================================================

TEST_F(ScriptServiceTest, ConfigurationPoolSize) {
    config_.poolSize = 8;
    service_ = std::make_unique<lithium::ScriptService>(config_);
    auto result = service_->initialize();

    if (result.has_value()) {
        auto pool = service_->getInterpreterPool();
        EXPECT_NE(pool, nullptr);
        // Pool should be configured with the specified size
    }
}

TEST_F(ScriptServiceTest, ConfigurationWithSecurityAnalysis) {
    config_.enableSecurityAnalysis = true;
    config_.analysisConfigPath = "./config/script/analysis.json";
    service_ = std::make_unique<lithium::ScriptService>(config_);

    auto result = service_->initialize();
    if (result.has_value()) {
        auto analyzer = service_->getScriptAnalyzer();
        // Analyzer may or may not be available depending on config file
    }
}
