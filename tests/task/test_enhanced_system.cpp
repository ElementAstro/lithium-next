// test_enhanced_system.cpp - Comprehensive integration tests for enhanced sequence system
// This project is licensed under the terms of the GPL3 license.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "task/custom/enhanced_sequencer.hpp"
#include "task/custom/task_manager.hpp"
#include "task/custom/task_templates.hpp"
#include "task/custom/factory.hpp"
#include "task/custom/script_task.hpp"
#include "task/custom/device_task.hpp"
#include "task/custom/config_task.hpp"

using namespace lithium::sequencer;
using namespace std::chrono_literals;

class EnhancedSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<TaskManager>();
        sequencer_ = std::make_unique<EnhancedSequencer>(manager_.get());
        templates_ = std::make_unique<TaskTemplateManager>();
    }

    void TearDown() override {
        sequencer_.reset();
        manager_.reset();
        templates_.reset();
    }

    std::unique_ptr<TaskManager> manager_;
    std::unique_ptr<EnhancedSequencer> sequencer_;
    std::unique_ptr<TaskTemplateManager> templates_;
};

// Test Task Factory Registration
TEST_F(EnhancedSystemTest, TaskFactoryRegistration) {
    auto& factory = TaskFactory::getInstance();

    // Test script task registration
    ASSERT_TRUE(factory.isRegistered("script_task"));
    auto scriptTask = factory.createTask("script_task", "test_script", json{});
    ASSERT_NE(scriptTask, nullptr);

    // Test device task registration
    ASSERT_TRUE(factory.isRegistered("device_task"));
    auto deviceTask = factory.createTask("device_task", "test_device", json{});
    ASSERT_NE(deviceTask, nullptr);

    // Test config task registration
    ASSERT_TRUE(factory.isRegistered("config_task"));
    auto configTask = factory.createTask("config_task", "test_config", json{});
    ASSERT_NE(configTask, nullptr);
}

// Test Task Template System
TEST_F(EnhancedSystemTest, TaskTemplateSystem) {
    // Test template existence
    ASSERT_TRUE(templates_->hasTemplate("imaging"));
    ASSERT_TRUE(templates_->hasTemplate("calibration"));
    ASSERT_TRUE(templates_->hasTemplate("focus"));
    ASSERT_TRUE(templates_->hasTemplate("platesolve"));

    // Test template creation
    json params = {
        {"target", "M31"},
        {"exposure_time", 300},
        {"filter", "Ha"},
        {"count", 10}
    };

    auto imagingTask = templates_->createTask("imaging", "test_imaging", params);
    ASSERT_NE(imagingTask, nullptr);

    // Test parameter substitution
    auto templateData = templates_->getTemplate("imaging");
    auto substituted = templates_->substituteParameters(templateData, params);
    ASSERT_TRUE(substituted.contains("target"));
    ASSERT_EQ(substituted["target"], "M31");
}

// Test Enhanced Sequencer Execution Strategies
TEST_F(EnhancedSystemTest, SequencerExecutionStrategies) {
    // Test sequential execution
    sequencer_->setExecutionStrategy(ExecutionStrategy::Sequential);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExecutionStrategy::Sequential);

    // Test parallel execution
    sequencer_->setExecutionStrategy(ExecutionStrategy::Parallel);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExecutionStrategy::Parallel);

    // Test adaptive execution
    sequencer_->setExecutionStrategy(ExecutionStrategy::Adaptive);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExecutionStrategy::Adaptive);

    // Test priority execution
    sequencer_->setExecutionStrategy(ExecutionStrategy::Priority);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExecutionStrategy::Priority);
}

// Test Task Dependencies
TEST_F(EnhancedSystemTest, TaskDependencies) {
    auto& factory = TaskFactory::getInstance();

    // Create tasks
    auto task1 = factory.createTask("script_task", "init_task", json{
        {"script_path", "/tmp/init.py"},
        {"script_type", "python"}
    });

    auto task2 = factory.createTask("device_task", "connect_task", json{
        {"operation", "connect"},
        {"deviceName", "camera1"}
    });

    auto task3 = factory.createTask("script_task", "capture_task", json{
        {"script_path", "/tmp/capture.py"},
        {"script_type", "python"}
    });

    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
    ASSERT_NE(task3, nullptr);

    // Add tasks to manager
    auto id1 = manager_->addTask(std::move(task1));
    auto id2 = manager_->addTask(std::move(task2));
    auto id3 = manager_->addTask(std::move(task3));

    // Set up dependencies: task3 depends on task1 and task2
    manager_->addDependency(id3, id1);
    manager_->addDependency(id3, id2);

    // Test dependency resolution
    auto readyTasks = manager_->getReadyTasks();
    ASSERT_EQ(readyTasks.size(), 2); // task1 and task2 should be ready

    // Check that task3 is not ready until dependencies complete
    auto task3Status = manager_->getTaskStatus(id3);
    ASSERT_EQ(task3Status, TaskStatus::Pending);
}

// Test Parallel Execution
TEST_F(EnhancedSystemTest, ParallelExecution) {
    auto& factory = TaskFactory::getInstance();

    // Create multiple independent tasks
    std::vector<std::string> taskIds;
    for (int i = 0; i < 5; ++i) {
        auto task = factory.createTask("script_task", "parallel_task_" + std::to_string(i), json{
            {"script_path", "/tmp/task_" + std::to_string(i) + ".py"},
            {"script_type", "python"}
        });
        ASSERT_NE(task, nullptr);
        taskIds.push_back(manager_->addTask(std::move(task)));
    }

    // Set parallel execution
    sequencer_->setExecutionStrategy(ExecutionStrategy::Parallel);

    // Start execution in background
    std::thread executionThread([this, &taskIds]() {
        auto sequence = json::array();
        for (const auto& id : taskIds) {
            sequence.push_back(json{{"task_id", id}});
        }
        sequencer_->executeSequence(sequence);
    });

    // Wait a bit for tasks to start
    std::this_thread::sleep_for(100ms);

    // Check that multiple tasks are running concurrently
    int runningCount = 0;
    for (const auto& id : taskIds) {
        if (manager_->getTaskStatus(id) == TaskStatus::Running) {
            runningCount++;
        }
    }

    // Should have multiple tasks running in parallel
    ASSERT_GT(runningCount, 1);

    // Cancel all tasks and wait for completion
    for (const auto& id : taskIds) {
        manager_->cancelTask(id);
    }

    if (executionThread.joinable()) {
        executionThread.join();
    }
}

// Test Task Monitoring and Metrics
TEST_F(EnhancedSystemTest, TaskMonitoring) {
    auto& factory = TaskFactory::getInstance();

    auto task = factory.createTask("script_task", "monitored_task", json{
        {"script_path", "/tmp/monitor_test.py"},
        {"script_type", "python"}
    });
    ASSERT_NE(task, nullptr);

    auto taskId = manager_->addTask(std::move(task));

    // Enable monitoring
    sequencer_->enableMonitoring(true);

    // Execute task
    auto sequence = json::array();
    sequence.push_back(json{{"task_id", taskId}});

    std::thread executionThread([this, &sequence]() {
        sequencer_->executeSequence(sequence);
    });

    // Wait for execution to start
    std::this_thread::sleep_for(50ms);

    // Check metrics
    auto metrics = sequencer_->getMetrics();
    ASSERT_TRUE(metrics.contains("total_tasks"));
    ASSERT_TRUE(metrics.contains("completed_tasks"));
    ASSERT_TRUE(metrics.contains("failed_tasks"));
    ASSERT_TRUE(metrics.contains("average_execution_time"));

    // Cancel and wait
    manager_->cancelTask(taskId);
    if (executionThread.joinable()) {
        executionThread.join();
    }
}

// Test Template Parameter Generation
TEST_F(EnhancedSystemTest, TemplateParameterGeneration) {
    using namespace TaskUtils;

    // Test imaging parameters
    auto imagingParams = CommonTasks::generateImagingParameters(
        "M31", "Ha", 300, 10, 1, 1.0, true, -10.0
    );

    ASSERT_EQ(imagingParams["target"], "M31");
    ASSERT_EQ(imagingParams["filter"], "Ha");
    ASSERT_EQ(imagingParams["exposure_time"], 300);
    ASSERT_EQ(imagingParams["count"], 10);

    // Test calibration parameters
    auto calibrationParams = CommonTasks::generateCalibrationParameters(
        "dark", 300, 10, 1, -10.0
    );

    ASSERT_EQ(calibrationParams["frame_type"], "dark");
    ASSERT_EQ(calibrationParams["exposure_time"], 300);
    ASSERT_EQ(calibrationParams["count"], 10);

    // Test focus parameters
    auto focusParams = CommonTasks::generateFocusParameters(
        "star", 5.0, 50, 5, 2.0
    );

    ASSERT_EQ(focusParams["focus_method"], "star");
    ASSERT_EQ(focusParams["step_size"], 5.0);
    ASSERT_EQ(focusParams["max_steps"], 50);
}

// Test Script Integration
TEST_F(EnhancedSystemTest, ScriptIntegration) {
    auto& factory = TaskFactory::getInstance();

    // Test Python script task
    auto pythonTask = factory.createTask("script_task", "python_test", json{
        {"script_path", "/tmp/test.py"},
        {"script_type", "python"},
        {"timeout", 5000},
        {"capture_output", true}
    });
    ASSERT_NE(pythonTask, nullptr);

    // Test JavaScript script task
    auto jsTask = factory.createTask("script_task", "js_test", json{
        {"script_path", "/tmp/test.js"},
        {"script_type", "javascript"},
        {"timeout", 3000}
    });
    ASSERT_NE(jsTask, nullptr);

    // Test shell script task
    auto shellTask = factory.createTask("script_task", "shell_test", json{
        {"script_path", "/tmp/test.sh"},
        {"script_type", "shell"},
        {"capture_output", false}
    });
    ASSERT_NE(shellTask, nullptr);
}

// Test Error Handling and Recovery
TEST_F(EnhancedSystemTest, ErrorHandlingAndRecovery) {
    auto& factory = TaskFactory::getInstance();

    // Create a task that will fail
    auto failingTask = factory.createTask("script_task", "failing_task", json{
        {"script_path", "/nonexistent/script.py"},
        {"script_type", "python"},
        {"retry_count", 2}
    });
    ASSERT_NE(failingTask, nullptr);

    auto taskId = manager_->addTask(std::move(failingTask));

    // Execute and expect failure
    auto sequence = json::array();
    sequence.push_back(json{{"task_id", taskId}});

    // This should complete with failure
    sequencer_->executeSequence(sequence);

    // Check that task failed
    auto status = manager_->getTaskStatus(taskId);
    ASSERT_EQ(status, TaskStatus::Failed);

    // Check error information
    auto errorInfo = manager_->getTaskResult(taskId);
    ASSERT_TRUE(errorInfo.contains("error"));
}

// Test Sequence Optimization
TEST_F(EnhancedSystemTest, SequenceOptimization) {
    using namespace SequencePatterns;

    // Create sample tasks
    json tasks = json::array();
    for (int i = 0; i < 10; ++i) {
        tasks.push_back(json{
            {"name", "task_" + std::to_string(i)},
            {"priority", i % 3 + 1},
            {"estimated_duration", (i + 1) * 60},
            {"dependencies", json::array()}
        });
    }

    // Test optimization
    auto optimized = optimizeSequence(tasks, OptimizationCriteria{
        .minimizeTime = true,
        .balanceLoad = true,
        .respectPriority = true,
        .maxParallelTasks = 3
    });

    ASSERT_FALSE(optimized.empty());
    ASSERT_LE(optimized.size(), tasks.size());

    // Test pattern application
    auto pattern = createOptimalPattern(tasks, "imaging");
    ASSERT_TRUE(pattern.contains("execution_order"));
    ASSERT_TRUE(pattern.contains("parallel_groups"));
}

// Performance benchmark test
TEST_F(EnhancedSystemTest, PerformanceBenchmark) {
    auto& factory = TaskFactory::getInstance();
    const int numTasks = 100;

    // Create many lightweight tasks
    std::vector<std::string> taskIds;
    for (int i = 0; i < numTasks; ++i) {
        auto task = factory.createTask("script_task", "benchmark_task_" + std::to_string(i), json{
            {"script_path", "/bin/true"}, // Quick-running command
            {"script_type", "shell"}
        });
        taskIds.push_back(manager_->addTask(std::move(task)));
    }

    // Measure parallel execution time
    auto startTime = std::chrono::high_resolution_clock::now();

    sequencer_->setExecutionStrategy(ExecutionStrategy::Parallel);
    sequencer_->setConcurrencyLimit(10);

    auto sequence = json::array();
    for (const auto& id : taskIds) {
        sequence.push_back(json{{"task_id", id}});
    }

    sequencer_->executeSequence(sequence);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Should complete reasonably quickly with parallel execution
    ASSERT_LT(duration.count(), 30000); // Less than 30 seconds

    // Check all tasks completed
    for (const auto& id : taskIds) {
        auto status = manager_->getTaskStatus(id);
        // Tasks should be completed or failed (depending on system)
        ASSERT_TRUE(status == TaskStatus::Completed || status == TaskStatus::Failed);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
