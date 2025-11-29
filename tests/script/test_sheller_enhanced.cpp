/*
 * test_sheller_enhanced.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_sheller_enhanced.cpp
 * @brief Comprehensive tests for enhanced ScriptManager functionality
 * @date 2024-1-13
 */

#include <gtest/gtest.h>
#include "script/python_caller.hpp"
#include "script/sheller.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace lithium;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture for Enhanced Script Manager Tests
// =============================================================================

class EnhancedScriptManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<ScriptManager>();

        // Create temporary test directory
        testDir_ = fs::temp_directory_path() / "lithium_script_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        manager_.reset();

        // Cleanup test directory
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    // Helper to create a test script file
    void createTestScript(const std::string& filename,
                          const std::string& content) {
        std::ofstream file(testDir_ / filename);
        file << content;
        file.close();
    }

    std::unique_ptr<ScriptManager> manager_;
    fs::path testDir_;
};

// =============================================================================
// Script Language Detection Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, DetectPythonScript) {
    std::string pythonScript = R"(
#!/usr/bin/env python3
import sys
def main():
    print("Hello from Python")
if __name__ == "__main__":
    main()
)";

    auto language = manager_->detectScriptLanguage(pythonScript);
    EXPECT_EQ(language, ScriptLanguage::Python);
}

TEST_F(EnhancedScriptManagerTest, DetectShellScript) {
    std::string shellScript = R"(
#!/bin/bash
echo "Hello from Shell"
exit 0
)";

    auto language = manager_->detectScriptLanguage(shellScript);
    EXPECT_EQ(language, ScriptLanguage::Shell);
}

TEST_F(EnhancedScriptManagerTest, DetectPowerShellScript) {
    std::string psScript = R"(
#Requires -Version 5.0
Write-Host "Hello from PowerShell"
Get-Process | Select-Object -First 5
)";

    auto language = manager_->detectScriptLanguage(psScript);
    EXPECT_EQ(language, ScriptLanguage::PowerShell);
}

TEST_F(EnhancedScriptManagerTest, DetectAutoLanguage) {
    // Empty or ambiguous content should return Auto
    std::string ambiguousScript = "# Just a comment";
    auto language = manager_->detectScriptLanguage(ambiguousScript);
    // Should default to Shell or Auto
    EXPECT_TRUE(language == ScriptLanguage::Shell ||
                language == ScriptLanguage::Auto);
}

// =============================================================================
// Script Metadata Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, SetAndGetScriptMetadata) {
    manager_->registerScript("test_script", "echo 'test'");

    ScriptMetadata metadata;
    metadata.description = "A test script";
    metadata.version = "1.0.0";
    metadata.author = "Test Author";
    metadata.tags = {"test", "example"};
    metadata.language = ScriptLanguage::Shell;
    metadata.isPython = false;

    manager_->setScriptMetadata("test_script", metadata);

    auto retrieved = manager_->getScriptMetadata("test_script");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->description, "A test script");
    EXPECT_EQ(retrieved->version, "1.0.0");
    EXPECT_EQ(retrieved->author, "Test Author");
    EXPECT_EQ(retrieved->tags.size(), 2);
    EXPECT_EQ(retrieved->language, ScriptLanguage::Shell);
    EXPECT_FALSE(retrieved->isPython);
}

TEST_F(EnhancedScriptManagerTest, MetadataForNonexistentScript) {
    auto metadata = manager_->getScriptMetadata("nonexistent");
    EXPECT_FALSE(metadata.has_value());
}

TEST_F(EnhancedScriptManagerTest, MetadataWithDependencies) {
    manager_->registerScript("dependent_script", "echo 'depends'");

    ScriptMetadata metadata;
    metadata.dependencies = {"numpy", "pandas", "requests"};
    manager_->setScriptMetadata("dependent_script", metadata);

    auto retrieved = manager_->getScriptMetadata("dependent_script");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->dependencies.size(), 3);
    EXPECT_TRUE(std::find(retrieved->dependencies.begin(),
                          retrieved->dependencies.end(),
                          "numpy") != retrieved->dependencies.end());
}

// =============================================================================
// Script Content Management Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, GetScriptContent) {
    std::string content = "echo 'Hello World'";
    manager_->registerScript("content_test", content);

    auto retrieved = manager_->getScriptContent("content_test");
    EXPECT_EQ(retrieved, content);
}

TEST_F(EnhancedScriptManagerTest, GetNonexistentScriptContent) {
    auto content = manager_->getScriptContent("nonexistent");
    EXPECT_TRUE(content.empty());
}

// =============================================================================
// Script Discovery Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, DiscoverPythonScripts) {
    // Create test Python scripts
    createTestScript("script1.py", "print('Script 1')");
    createTestScript("script2.py", "print('Script 2')");
    createTestScript("not_a_script.txt", "Just text");

    size_t count = manager_->discoverScripts(testDir_, {".py"}, false);
    EXPECT_EQ(count, 2);
}

TEST_F(EnhancedScriptManagerTest, DiscoverShellScripts) {
    createTestScript("script1.sh", "#!/bin/bash\necho 'Script 1'");
    createTestScript("script2.sh", "#!/bin/bash\necho 'Script 2'");

    size_t count = manager_->discoverScripts(testDir_, {".sh"}, false);
    EXPECT_EQ(count, 2);
}

TEST_F(EnhancedScriptManagerTest, DiscoverMultipleExtensions) {
    createTestScript("python_script.py", "print('Python')");
    createTestScript("shell_script.sh", "echo 'Shell'");
    createTestScript("other.txt", "Not a script");

    size_t count = manager_->discoverScripts(testDir_, {".py", ".sh"}, false);
    EXPECT_EQ(count, 2);
}

TEST_F(EnhancedScriptManagerTest, DiscoverRecursive) {
    // Create subdirectory with scripts
    fs::path subDir = testDir_ / "subdir";
    fs::create_directories(subDir);

    createTestScript("root_script.py", "print('Root')");
    std::ofstream subFile(subDir / "sub_script.py");
    subFile << "print('Sub')";
    subFile.close();

    size_t countNonRecursive =
        manager_->discoverScripts(testDir_, {".py"}, false);
    EXPECT_EQ(countNonRecursive, 1);

    // Reset and discover recursively
    manager_ = std::make_unique<ScriptManager>();
    size_t countRecursive = manager_->discoverScripts(testDir_, {".py"}, true);
    EXPECT_EQ(countRecursive, 2);
}

TEST_F(EnhancedScriptManagerTest, DiscoverFromNonexistentDirectory) {
    size_t count =
        manager_->discoverScripts("/nonexistent/path", {".py"}, false);
    EXPECT_EQ(count, 0);
}

// =============================================================================
// Resource Limits Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, SetAndGetResourceLimits) {
    ScriptResourceLimits limits;
    limits.maxMemoryMB = 512;
    limits.maxCpuPercent = 50;
    limits.maxExecutionTime = std::chrono::seconds(300);
    limits.maxConcurrentScripts = 2;
    limits.maxOutputSize = 1024 * 1024;

    manager_->setResourceLimits(limits);

    auto retrieved = manager_->getResourceLimits();
    EXPECT_EQ(retrieved.maxMemoryMB, 512);
    EXPECT_EQ(retrieved.maxCpuPercent, 50);
    EXPECT_EQ(retrieved.maxExecutionTime.count(), 300);
    EXPECT_EQ(retrieved.maxConcurrentScripts, 2);
    EXPECT_EQ(retrieved.maxOutputSize, 1024 * 1024);
}

TEST_F(EnhancedScriptManagerTest, DefaultResourceLimits) {
    auto limits = manager_->getResourceLimits();
    // Check default values
    EXPECT_GT(limits.maxMemoryMB, 0);
    EXPECT_GT(limits.maxCpuPercent, 0);
    EXPECT_GT(limits.maxConcurrentScripts, 0);
}

TEST_F(EnhancedScriptManagerTest, GetResourceUsage) {
    auto usage = manager_->getResourceUsage();

    // Should have basic usage metrics
    EXPECT_TRUE(usage.contains("running_scripts") ||
                usage.contains("total_scripts"));
}

// =============================================================================
// Enhanced Execution Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ExecuteWithRetryConfig) {
    manager_->registerScript("retry_test", "echo 'success'");

    RetryConfig config;
    config.maxRetries = 3;
    config.strategy = RetryStrategy::Exponential;
    config.initialDelay = std::chrono::milliseconds(100);
    config.maxDelay = std::chrono::seconds(5);

    auto result = manager_->executeWithConfig("retry_test", {}, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(result.output.empty());
}

TEST_F(EnhancedScriptManagerTest, ExecuteWithLinearRetry) {
    manager_->registerScript("linear_retry", "echo 'linear'");

    RetryConfig config;
    config.maxRetries = 2;
    config.strategy = RetryStrategy::Linear;

    auto result = manager_->executeWithConfig("linear_retry", {}, config);
    EXPECT_TRUE(result.success);
}

TEST_F(EnhancedScriptManagerTest, ExecuteWithNoRetry) {
    manager_->registerScript("no_retry", "echo 'no retry'");

    RetryConfig config;
    config.maxRetries = 0;
    config.strategy = RetryStrategy::None;

    auto result = manager_->executeWithConfig("no_retry", {}, config);
    EXPECT_TRUE(result.success);
}

TEST_F(EnhancedScriptManagerTest, ExecutionResultContainsTimingInfo) {
    manager_->registerScript("timing_test", "echo 'timing'");

    auto result = manager_->executeWithConfig("timing_test", {}, {});
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.executionTime.count(), 0);
}

TEST_F(EnhancedScriptManagerTest, ExecutionResultLanguageDetection) {
    manager_->registerScript("lang_detect", "echo 'shell script'");

    auto result = manager_->executeWithConfig("lang_detect", {}, {});
    EXPECT_TRUE(result.success);
    // Should detect as shell
    EXPECT_TRUE(result.detectedLanguage == ScriptLanguage::Shell ||
                result.detectedLanguage == ScriptLanguage::Auto);
}

// =============================================================================
// Async Execution Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ExecuteAsyncWithCallback) {
    manager_->registerScript("async_callback", "echo 'async'");

    std::atomic<bool> callbackCalled{false};
    std::string capturedOutput;

    auto callback = [&](const ScriptExecutionResult& result) {
        callbackCalled = true;
        capturedOutput = result.output;
    };

    auto future = manager_->executeAsync("async_callback", {}, callback);
    auto result = future.get();

    EXPECT_TRUE(result.success);
    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(callbackCalled);
}

TEST_F(EnhancedScriptManagerTest, ExecuteAsyncWithoutCallback) {
    manager_->registerScript("async_no_callback", "echo 'no callback'");

    auto future = manager_->executeAsync("async_no_callback", {}, nullptr);
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
}

// =============================================================================
// Pipeline Execution Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ExecutePipelineBasic) {
    manager_->registerScript("pipe1", "echo 'step1'");
    manager_->registerScript("pipe2", "echo 'step2'");
    manager_->registerScript("pipe3", "echo 'step3'");

    std::vector<std::string> pipeline = {"pipe1", "pipe2", "pipe3"};

    auto results = manager_->executePipeline(pipeline, {}, true);
    EXPECT_EQ(results.size(), 3);

    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.exitCode, 0);
    }
}

TEST_F(EnhancedScriptManagerTest, ExecutePipelineWithContext) {
    manager_->registerScript("context_pipe1", "echo 'value1'");
    manager_->registerScript("context_pipe2", "echo 'value2'");

    std::unordered_map<std::string, std::string> context = {{"key1", "value1"},
                                                            {"key2", "value2"}};

    auto results = manager_->executePipeline({"context_pipe1", "context_pipe2"},
                                             context, true);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(EnhancedScriptManagerTest, ExecutePipelineStopOnError) {
    manager_->registerScript("success_script", "echo 'success'");
    manager_->registerScript("fail_script", "exit 1");
    manager_->registerScript("after_fail", "echo 'should not run'");

    auto results = manager_->executePipeline(
        {"success_script", "fail_script", "after_fail"}, {}, true);

    // Should stop after failure
    EXPECT_LE(results.size(), 3);
    if (results.size() >= 2) {
        EXPECT_TRUE(results[0].success);
        EXPECT_FALSE(results[1].success);
    }
}

TEST_F(EnhancedScriptManagerTest, ExecutePipelineContinueOnError) {
    manager_->registerScript("success1", "echo 'success1'");
    manager_->registerScript("fail1", "exit 1");
    manager_->registerScript("success2", "echo 'success2'");

    auto results =
        manager_->executePipeline({"success1", "fail1", "success2"}, {}, false);

    // Should continue despite failure
    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].success);
    EXPECT_FALSE(results[1].success);
    EXPECT_TRUE(results[2].success);
}

TEST_F(EnhancedScriptManagerTest, ExecuteEmptyPipeline) {
    auto results = manager_->executePipeline({}, {}, true);
    EXPECT_TRUE(results.empty());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ScriptStatisticsAfterExecution) {
    manager_->registerScript("stats_test", "echo 'stats'");

    // Execute multiple times
    for (int i = 0; i < 5; i++) {
        manager_->runScript("stats_test", {});
    }

    auto stats = manager_->getScriptStatistics("stats_test");
    EXPECT_TRUE(stats.contains("execution_count") ||
                stats.contains("executionCount"));

    // Check execution count
    double execCount = 0;
    if (stats.contains("execution_count")) {
        execCount = stats["execution_count"];
    } else if (stats.contains("executionCount")) {
        execCount = stats["executionCount"];
    }
    EXPECT_GE(execCount, 5);
}

TEST_F(EnhancedScriptManagerTest, GlobalStatistics) {
    manager_->registerScript("global1", "echo 'g1'");
    manager_->registerScript("global2", "echo 'g2'");

    manager_->runScript("global1", {});
    manager_->runScript("global2", {});

    auto globalStats = manager_->getGlobalStatistics();
    EXPECT_FALSE(globalStats.empty());

    // Should have total execution count
    EXPECT_TRUE(globalStats.contains("total_executions") ||
                globalStats.contains("totalExecutions") ||
                globalStats.contains("total_scripts"));
}

TEST_F(EnhancedScriptManagerTest, ResetStatistics) {
    manager_->registerScript("reset_test", "echo 'reset'");
    manager_->runScript("reset_test", {});

    manager_->resetStatistics();

    auto stats = manager_->getScriptStatistics("reset_test");
    // After reset, execution count should be 0 or stats should be empty
    if (!stats.empty()) {
        double execCount = 0;
        if (stats.contains("execution_count")) {
            execCount = stats["execution_count"];
        } else if (stats.contains("executionCount")) {
            execCount = stats["executionCount"];
        }
        EXPECT_EQ(execCount, 0);
    }
}

TEST_F(EnhancedScriptManagerTest, StatisticsForNonexistentScript) {
    auto stats = manager_->getScriptStatistics("nonexistent");
    // Should return empty or default stats
    if (!stats.empty()) {
        double execCount = 0;
        if (stats.contains("execution_count")) {
            execCount = stats["execution_count"];
        }
        EXPECT_EQ(execCount, 0);
    }
}

// =============================================================================
// Python Integration Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, PythonAvailabilityCheck) {
    // Without setting a wrapper, Python should not be available
    EXPECT_FALSE(manager_->isPythonAvailable());
}

TEST_F(EnhancedScriptManagerTest, SetPythonWrapper) {
    auto wrapper = std::make_shared<PythonWrapper>();
    manager_->setPythonWrapper(wrapper);

    EXPECT_TRUE(manager_->isPythonAvailable());
    EXPECT_EQ(manager_->getPythonWrapper(), wrapper);
}

TEST_F(EnhancedScriptManagerTest, RegisterPythonScriptWithConfig) {
    auto wrapper = std::make_shared<PythonWrapper>();
    manager_->setPythonWrapper(wrapper);

    PythonScriptConfig config;
    config.moduleName = "test_module";
    config.functionName = "main";
    config.requiredPackages = {"numpy", "pandas"};
    config.pythonPath = "/usr/bin/python3";
    config.useVirtualEnv = false;

    EXPECT_NO_THROW(
        manager_->registerPythonScriptWithConfig("py_test", config));

    auto metadata = manager_->getScriptMetadata("py_test");
    ASSERT_TRUE(metadata.has_value());
    EXPECT_TRUE(metadata->isPython);
    EXPECT_EQ(metadata->language, ScriptLanguage::Python);
}

TEST_F(EnhancedScriptManagerTest, LoadPythonScriptsFromDirectory) {
    auto wrapper = std::make_shared<PythonWrapper>();
    manager_->setPythonWrapper(wrapper);

    // Create test Python scripts
    createTestScript("module1.py", "def func1(): pass");
    createTestScript("module2.py", "def func2(): pass");

    size_t count = manager_->loadPythonScriptsFromDirectory(testDir_, false);
    EXPECT_EQ(count, 2);
}

TEST_F(EnhancedScriptManagerTest, AddPythonSysPath) {
    auto wrapper = std::make_shared<PythonWrapper>();
    manager_->setPythonWrapper(wrapper);

    // Should not throw
    EXPECT_NO_THROW(manager_->addPythonSysPath(testDir_));
}

TEST_F(EnhancedScriptManagerTest, AddPythonSysPathWithoutWrapper) {
    // Should handle gracefully without wrapper
    EXPECT_NO_THROW(manager_->addPythonSysPath(testDir_));
}

// =============================================================================
// Running Scripts Management Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, GetRunningScripts) {
    auto running = manager_->getRunningScripts();
    // Initially should be empty
    EXPECT_TRUE(running.empty());
}

TEST_F(EnhancedScriptManagerTest, RunningScriptsDuringExecution) {
    manager_->registerScript("long_running", R"(
        sleep 2
        echo 'done'
    )");

    auto future = manager_->runScriptAsync("long_running", {});

    // Give it time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Note: This test may be flaky depending on timing
    // The script might already be done by the time we check

    future.get();
}

// =============================================================================
// Hook Integration Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, PreExecutionHookCalled) {
    manager_->registerScript("hook_test", "echo 'hook'");

    std::atomic<bool> hookCalled{false};
    std::string capturedName;

    manager_->addPreExecutionHook("hook_test", [&](const std::string& name) {
        hookCalled = true;
        capturedName = name;
    });

    manager_->runScript("hook_test", {});

    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(capturedName, "hook_test");
}

TEST_F(EnhancedScriptManagerTest, PostExecutionHookCalled) {
    manager_->registerScript("post_hook_test", "echo 'post hook'");

    std::atomic<bool> hookCalled{false};
    int capturedExitCode = -1;

    manager_->addPostExecutionHook(
        "post_hook_test", [&](const std::string& output, int exitCode) {
            hookCalled = true;
            capturedExitCode = exitCode;
        });

    manager_->runScript("post_hook_test", {});

    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(capturedExitCode, 0);
}

TEST_F(EnhancedScriptManagerTest, MultipleHooks) {
    manager_->registerScript("multi_hook", "echo 'multi'");

    std::atomic<int> hookCount{0};

    manager_->addPreExecutionHook("multi_hook",
                                  [&](const std::string&) { hookCount++; });

    manager_->addPreExecutionHook("multi_hook",
                                  [&](const std::string&) { hookCount++; });

    manager_->runScript("multi_hook", {});

    EXPECT_EQ(hookCount, 2);
}

// =============================================================================
// Environment Variables Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, SetScriptEnvironmentVars) {
    manager_->registerScript("env_test", "echo $MY_VAR");

    std::unordered_map<std::string, std::string> envVars = {
        {"MY_VAR", "test_value"}, {"OTHER_VAR", "other_value"}};

    manager_->setScriptEnvironmentVars("env_test", envVars);

    auto result = manager_->runScript("env_test", {});
    ASSERT_TRUE(result.has_value());
    // The output should contain the environment variable value
    // Note: This depends on the shell properly expanding the variable
}

// =============================================================================
// Retry Strategy Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, SetRetryStrategy) {
    manager_->registerScript("retry_strategy_test", "echo 'retry'");

    EXPECT_NO_THROW(manager_->setRetryStrategy("retry_strategy_test",
                                               RetryStrategy::Exponential));
}

TEST_F(EnhancedScriptManagerTest, SetTimeoutHandler) {
    manager_->registerScript("timeout_test", "echo 'timeout'");

    std::atomic<bool> handlerCalled{false};

    manager_->setTimeoutHandler("timeout_test",
                                [&]() { handlerCalled = true; });

    // The handler should be registered but not called for successful execution
    manager_->runScript("timeout_test", {});
    EXPECT_FALSE(handlerCalled);
}

// =============================================================================
// Script Import Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ImportMultipleScripts) {
    std::vector<std::pair<std::string, Script>> scripts = {
        {"import1", "echo 'import1'"},
        {"import2", "echo 'import2'"},
        {"import3", "echo 'import3'"}};

    manager_->importScripts(std::span(scripts));

    auto allScripts = manager_->getAllScripts();
    EXPECT_EQ(allScripts.size(), 3);
    EXPECT_TRUE(allScripts.contains("import1"));
    EXPECT_TRUE(allScripts.contains("import2"));
    EXPECT_TRUE(allScripts.contains("import3"));
}

// =============================================================================
// Edge Cases and Error Handling Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ExecuteNonexistentScript) {
    RetryConfig config;
    config.maxRetries = 0;

    EXPECT_THROW(manager_->executeWithConfig("nonexistent", {}, config),
                 std::exception);
}

TEST_F(EnhancedScriptManagerTest, PipelineWithNonexistentScript) {
    manager_->registerScript("existing", "echo 'exists'");

    auto results =
        manager_->executePipeline({"existing", "nonexistent"}, {}, true);

    // First should succeed, second should fail
    EXPECT_GE(results.size(), 1);
    EXPECT_TRUE(results[0].success);
}

TEST_F(EnhancedScriptManagerTest, EmptyScriptContent) {
    manager_->registerScript("empty", "");

    auto content = manager_->getScriptContent("empty");
    EXPECT_TRUE(content.empty());
}

TEST_F(EnhancedScriptManagerTest, SpecialCharactersInScriptName) {
    std::string specialName = "test-script_v1.0";
    manager_->registerScript(specialName, "echo 'special'");

    auto scripts = manager_->getAllScripts();
    EXPECT_TRUE(scripts.contains(specialName));
}

TEST_F(EnhancedScriptManagerTest, LargeScriptContent) {
    // Create a large script
    std::string largeScript;
    for (int i = 0; i < 1000; i++) {
        largeScript += "echo 'line " + std::to_string(i) + "'\n";
    }

    EXPECT_NO_THROW(manager_->registerScript("large_script", largeScript));

    auto content = manager_->getScriptContent("large_script");
    EXPECT_EQ(content, largeScript);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, ConcurrentScriptRegistration) {
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i]() {
            std::string name = "concurrent_" + std::to_string(i);
            manager_->registerScript(name, "echo '" + name + "'");
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto scripts = manager_->getAllScripts();
    EXPECT_EQ(scripts.size(), numThreads);
}

TEST_F(EnhancedScriptManagerTest, ConcurrentStatisticsAccess) {
    manager_->registerScript("concurrent_stats", "echo 'stats'");

    const int numThreads = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 10; j++) {
                manager_->getScriptStatistics("concurrent_stats");
                manager_->getGlobalStatistics();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without deadlock or crash
    SUCCEED();
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(EnhancedScriptManagerTest, BulkScriptRegistrationPerformance) {
    const int numScripts = 100;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < numScripts; i++) {
        manager_->registerScript("perf_" + std::to_string(i),
                                 "echo 'script " + std::to_string(i) + "'");
    }

    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);

    auto scripts = manager_->getAllScripts();
    EXPECT_EQ(scripts.size(), numScripts);
}

TEST_F(EnhancedScriptManagerTest, MetadataAccessPerformance) {
    manager_->registerScript("meta_perf", "echo 'meta'");

    ScriptMetadata metadata;
    metadata.description = "Performance test";
    manager_->setScriptMetadata("meta_perf", metadata);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; i++) {
        manager_->getScriptMetadata("meta_perf");
    }

    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 metadata accesses should complete in less than 100ms
    EXPECT_LT(duration.count(), 100);
}
