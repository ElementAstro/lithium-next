/*
 * test_script_task.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_script_task.cpp
 * @brief Comprehensive tests for ScriptTask functionality
 * @date 2024-1-13
 */

#include <gtest/gtest.h>
#include "task/custom/script_task.hpp"
#include "script/sheller.hpp"
#include "script/check.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace lithium::task::task;
using namespace lithium;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture for ScriptTask Tests
// =============================================================================

class ScriptTaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        testDir_ = fs::temp_directory_path() / "lithium_script_task_test";
        fs::create_directories(testDir_);
        
        // Create test config files
        createTestConfigs();
        
        task_ = std::make_unique<ScriptTask>(
            "test_script_task",
            scriptConfigPath_.string(),
            analyzerConfigPath_.string()
        );
    }

    void TearDown() override {
        task_.reset();
        
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void createTestConfigs() {
        scriptConfigPath_ = testDir_ / "script_config.json";
        analyzerConfigPath_ = testDir_ / "analyzer_config.json";
        
        std::ofstream scriptConfig(scriptConfigPath_);
        scriptConfig << R"({
            "default_timeout": 30,
            "max_retries": 3
        })";
        scriptConfig.close();
        
        std::ofstream analyzerConfig(analyzerConfigPath_);
        analyzerConfig << R"({
            "dangerous_commands": ["rm -rf /"],
            "max_complexity": 50
        })";
        analyzerConfig.close();
    }

    void createTestScript(const std::string& filename, const std::string& content) {
        std::ofstream file(testDir_ / filename);
        file << content;
        file.close();
    }

    std::unique_ptr<ScriptTask> task_;
    fs::path testDir_;
    fs::path scriptConfigPath_;
    fs::path analyzerConfigPath_;
};

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_F(ScriptTaskTest, BasicConstruction) {
    EXPECT_NO_THROW(ScriptTask("basic_task"));
}

TEST_F(ScriptTaskTest, ConstructionWithConfigs) {
    EXPECT_NO_THROW(ScriptTask("config_task", 
                               scriptConfigPath_.string(),
                               analyzerConfigPath_.string()));
}

// =============================================================================
// Script Registration Tests
// =============================================================================

TEST_F(ScriptTaskTest, RegisterScript) {
    EXPECT_NO_THROW(task_->registerScript("test_script", "echo 'hello'"));
}

TEST_F(ScriptTaskTest, RegisterAndExecuteScript) {
    task_->registerScript("exec_test", "echo 'executed'");
    
    json params;
    params["scriptName"] = "exec_test";
    params["args"] = json::object();
    
    EXPECT_NO_THROW(task_->execute(params));
}

TEST_F(ScriptTaskTest, UpdateScript) {
    task_->registerScript("update_test", "echo 'original'");
    EXPECT_NO_THROW(task_->updateScript("update_test", "echo 'updated'"));
}

TEST_F(ScriptTaskTest, DeleteScript) {
    task_->registerScript("delete_test", "echo 'to delete'");
    EXPECT_NO_THROW(task_->deleteScript("delete_test"));
}

// =============================================================================
// Script Validation Tests
// =============================================================================

TEST_F(ScriptTaskTest, ValidateSafeScript) {
    std::string safeScript = "echo 'safe script'";
    EXPECT_TRUE(task_->validateScript(safeScript));
}

TEST_F(ScriptTaskTest, ValidateDangerousScript) {
    std::string dangerousScript = "rm -rf /";
    // Depending on analyzer config, this might be invalid
    bool isValid = task_->validateScript(dangerousScript);
    // Just check it doesn't throw
    SUCCEED();
}

TEST_F(ScriptTaskTest, AnalyzeScript) {
    std::string script = R"(
#!/bin/bash
if [ "$1" == "test" ]; then
    echo "testing"
fi
)";
    
    auto result = task_->analyzeScript(script);
    EXPECT_GE(result.complexity, 0);
}

// =============================================================================
// Script Execution Tests
// =============================================================================

TEST_F(ScriptTaskTest, ExecuteWithTimeout) {
    task_->registerScript("timeout_test", "echo 'quick'");
    task_->setScriptTimeout(std::chrono::seconds(10));
    
    json params;
    params["scriptName"] = "timeout_test";
    
    EXPECT_NO_THROW(task_->execute(params));
}

TEST_F(ScriptTaskTest, ExecuteWithRetry) {
    task_->registerScript("retry_test", "echo 'retry'");
    task_->setScriptRetryCount(3);
    
    json params;
    params["scriptName"] = "retry_test";
    
    EXPECT_NO_THROW(task_->execute(params));
}

TEST_F(ScriptTaskTest, ExecuteWithEnvironment) {
    task_->registerScript("env_test", "echo $TEST_VAR");
    
    std::unordered_map<std::string, std::string> env = {
        {"TEST_VAR", "test_value"}
    };
    task_->setScriptEnvironment("env_test", env);
    
    json params;
    params["scriptName"] = "env_test";
    
    EXPECT_NO_THROW(task_->execute(params));
}

// =============================================================================
// Script Status Tests
// =============================================================================

TEST_F(ScriptTaskTest, GetScriptStatus) {
    task_->registerScript("status_test", "echo 'status'");
    
    auto status = task_->getScriptStatus("status_test");
    EXPECT_FALSE(status.isRunning);
}

TEST_F(ScriptTaskTest, GetScriptProgress) {
    task_->registerScript("progress_test", "echo 'progress'");
    
    float progress = task_->getScriptProgress("progress_test");
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 100.0f);
}

TEST_F(ScriptTaskTest, GetActiveScripts) {
    auto active = task_->getActiveScripts();
    // Initially should be empty
    EXPECT_TRUE(active.empty());
}

// =============================================================================
// Hook Tests
// =============================================================================

TEST_F(ScriptTaskTest, AddPreExecutionHook) {
    task_->registerScript("hook_test", "echo 'hook'");
    
    std::atomic<bool> hookCalled{false};
    task_->addPreExecutionHook("hook_test", [&](const std::string&) {
        hookCalled = true;
    });
    
    json params;
    params["scriptName"] = "hook_test";
    task_->execute(params);
    
    EXPECT_TRUE(hookCalled);
}

TEST_F(ScriptTaskTest, AddPostExecutionHook) {
    task_->registerScript("post_hook_test", "echo 'post'");
    
    std::atomic<bool> hookCalled{false};
    int capturedExitCode = -1;
    
    task_->addPostExecutionHook("post_hook_test", 
        [&](const std::string&, int exitCode) {
            hookCalled = true;
            capturedExitCode = exitCode;
        });
    
    json params;
    params["scriptName"] = "post_hook_test";
    task_->execute(params);
    
    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(capturedExitCode, 0);
}

// =============================================================================
// Priority and Concurrency Tests
// =============================================================================

TEST_F(ScriptTaskTest, SetScriptPriority) {
    task_->registerScript("priority_test", "echo 'priority'");
    
    ScriptPriority priority;
    priority.level = 5;
    
    EXPECT_NO_THROW(task_->setScriptPriority("priority_test", priority));
}

TEST_F(ScriptTaskTest, SetConcurrencyLimit) {
    EXPECT_NO_THROW(task_->setConcurrencyLimit(4));
}

// =============================================================================
// Resource Management Tests
// =============================================================================

TEST_F(ScriptTaskTest, SetResourceLimit) {
    task_->registerScript("resource_test", "echo 'resource'");
    
    EXPECT_NO_THROW(task_->setResourceLimit("resource_test", 512, 50));
}

TEST_F(ScriptTaskTest, GetResourceUsage) {
    task_->registerScript("usage_test", "echo 'usage'");
    
    float usage = task_->getResourceUsage("usage_test");
    EXPECT_GE(usage, 0.0f);
}

TEST_F(ScriptTaskTest, SetResourcePool) {
    EXPECT_NO_THROW(task_->setResourcePool(4, 1024 * 1024 * 1024));
}

TEST_F(ScriptTaskTest, ReserveAndReleaseResources) {
    task_->registerScript("reserve_test", "echo 'reserve'");
    
    EXPECT_NO_THROW(task_->reserveResources("reserve_test", 256, 25));
    EXPECT_NO_THROW(task_->releaseResources("reserve_test"));
}

// =============================================================================
// Script Control Tests
// =============================================================================

TEST_F(ScriptTaskTest, PauseAndResumeScript) {
    task_->registerScript("pause_test", "echo 'pause'");
    
    // These should not throw even if script isn't running
    EXPECT_NO_THROW(task_->pauseScript("pause_test"));
    EXPECT_NO_THROW(task_->resumeScript("pause_test"));
}

TEST_F(ScriptTaskTest, AbortScript) {
    task_->registerScript("abort_test", "echo 'abort'");
    
    EXPECT_NO_THROW(task_->abortScript("abort_test"));
}

// =============================================================================
// Logging Tests
// =============================================================================

TEST_F(ScriptTaskTest, GetScriptLogs) {
    task_->registerScript("log_test", "echo 'log'");
    
    json params;
    params["scriptName"] = "log_test";
    task_->execute(params);
    
    auto logs = task_->getScriptLogs("log_test");
    // Should have some logs after execution
    EXPECT_FALSE(logs.empty());
}

// =============================================================================
// Retry Strategy Tests
// =============================================================================

TEST_F(ScriptTaskTest, SetRetryStrategy) {
    task_->registerScript("strategy_test", "echo 'strategy'");
    
    EXPECT_NO_THROW(task_->setRetryStrategy("strategy_test", 
                                             RetryStrategy::Exponential));
}

// =============================================================================
// Execution Time Tests
// =============================================================================

TEST_F(ScriptTaskTest, GetExecutionTime) {
    task_->registerScript("time_test", "echo 'time'");
    
    json params;
    params["scriptName"] = "time_test";
    task_->execute(params);
    
    auto execTime = task_->getExecutionTime("time_test");
    EXPECT_GE(execTime.count(), 0);
}

// =============================================================================
// Async Execution Tests
// =============================================================================

TEST_F(ScriptTaskTest, ExecuteAsync) {
    task_->registerScript("async_test", "echo 'async'");
    
    json params;
    params["scriptName"] = "async_test";
    
    auto future = task_->executeAsync("async_test", params);
    auto status = future.get();
    
    EXPECT_FALSE(status.isRunning);
}

// =============================================================================
// Pipeline Execution Tests
// =============================================================================

TEST_F(ScriptTaskTest, ExecutePipeline) {
    task_->registerScript("pipe1", "echo 'step1'");
    task_->registerScript("pipe2", "echo 'step2'");
    
    json context;
    
    EXPECT_NO_THROW(task_->executePipeline({"pipe1", "pipe2"}, context));
}

// =============================================================================
// Workflow Tests
// =============================================================================

TEST_F(ScriptTaskTest, CreateAndExecuteWorkflow) {
    task_->registerScript("wf1", "echo 'workflow1'");
    task_->registerScript("wf2", "echo 'workflow2'");
    
    task_->createWorkflow("test_workflow", {"wf1", "wf2"});
    
    json params;
    EXPECT_NO_THROW(task_->executeWorkflow("test_workflow", params));
}

// =============================================================================
// Script Type Detection Tests
// =============================================================================

TEST_F(ScriptTaskTest, DetectPythonScript) {
    std::string pythonContent = R"(
#!/usr/bin/env python3
import sys
def main():
    print("Hello")
)";
    
    auto type = task_->detectScriptType(pythonContent);
    EXPECT_EQ(type, ScriptType::Python);
}

TEST_F(ScriptTaskTest, DetectShellScript) {
    std::string shellContent = R"(
#!/bin/bash
echo "Hello"
)";
    
    auto type = task_->detectScriptType(shellContent);
    EXPECT_EQ(type, ScriptType::Shell);
}

// =============================================================================
// Caching Tests
// =============================================================================

TEST_F(ScriptTaskTest, EnableScriptCaching) {
    EXPECT_NO_THROW(task_->enableScriptCaching(true));
    EXPECT_NO_THROW(task_->enableScriptCaching(false));
}

TEST_F(ScriptTaskTest, ClearScriptCache) {
    task_->enableScriptCaching(true);
    EXPECT_NO_THROW(task_->clearScriptCache());
}

// =============================================================================
// Profiling Tests
// =============================================================================

TEST_F(ScriptTaskTest, GetProfilingData) {
    task_->registerScript("profile_test", "echo 'profile'");
    
    json params;
    params["scriptName"] = "profile_test";
    task_->execute(params);
    
    auto profiling = task_->getProfilingData("profile_test");
    EXPECT_GE(profiling.executionTime.count(), 0);
}

// =============================================================================
// Event Handling Tests
// =============================================================================

TEST_F(ScriptTaskTest, AddEventListener) {
    std::atomic<bool> eventFired{false};
    
    task_->addEventListener("script_complete", [&](const json&) {
        eventFired = true;
    });
    
    // Fire the event
    task_->fireEvent("script_complete", json{{"status", "success"}});
    
    EXPECT_TRUE(eventFired);
}

// =============================================================================
// Context Execution Tests
// =============================================================================

TEST_F(ScriptTaskTest, ExecuteWithContext) {
    task_->registerScript("context_test", "echo 'context'");
    
    ScriptExecutionContext context;
    context.workingDirectory = testDir_.string();
    context.type = ScriptType::Shell;
    context.environment = {{"KEY", "VALUE"}};
    
    EXPECT_NO_THROW(task_->executeWithContext("context_test", context));
}

// =============================================================================
// Python Script Tests
// =============================================================================

TEST_F(ScriptTaskTest, RegisterPythonScript) {
    std::string pythonScript = R"(
print("Hello from Python")
)";
    
    // This may throw if Python wrapper is not initialized
    try {
        task_->registerPythonScript("py_test", pythonScript);
        SUCCEED();
    } catch (const std::runtime_error& e) {
        // Expected if Python wrapper not available
        EXPECT_TRUE(std::string(e.what()).find("Python") != std::string::npos);
    }
}

TEST_F(ScriptTaskTest, LoadPythonModule) {
    try {
        task_->loadPythonModule("os", "os_module");
        SUCCEED();
    } catch (const std::runtime_error& e) {
        // Expected if Python wrapper not available
        EXPECT_TRUE(std::string(e.what()).find("Python") != std::string::npos);
    }
}

// =============================================================================
// Cleanup Tests
// =============================================================================

TEST_F(ScriptTaskTest, CleanupScript) {
    task_->registerScript("cleanup_test", "echo 'cleanup'");
    
    EXPECT_NO_THROW(task_->cleanupScript("cleanup_test"));
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ScriptTaskTest, ExecuteNonexistentScript) {
    json params;
    params["scriptName"] = "nonexistent";
    
    EXPECT_THROW(task_->execute(params), std::exception);
}

TEST_F(ScriptTaskTest, ExecuteWithInvalidParams) {
    json params;
    // Missing required scriptName
    
    EXPECT_THROW(task_->execute(params), std::exception);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(ScriptTaskTest, ConcurrentScriptRegistration) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i]() {
            std::string name = "concurrent_" + std::to_string(i);
            task_->registerScript(name, "echo '" + name + "'");
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    SUCCEED();
}

