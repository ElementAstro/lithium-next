// test_enhanced_system.cpp - Integration tests for task sequence system
// This project is licensed under the terms of the GPL3 license.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "task/sequencer.hpp"
#include "task/target.hpp"
#include "task/custom/factory.hpp"
#include "task/registration.hpp"

using namespace lithium::task;
using namespace std::chrono_literals;
using json = nlohmann::json;

class EnhancedSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        registerBuiltInTasks();
        sequencer_ = std::make_unique<ExposureSequence>();
    }

    void TearDown() override {
        if (sequencer_ && sequencer_->isRunning()) {
            sequencer_->stop();
        }
        sequencer_.reset();
    }

    std::unique_ptr<ExposureSequence> sequencer_;
};

// Test Task Factory Registration
TEST_F(EnhancedSystemTest, TaskFactoryRegistration) {
    auto& factory = TaskFactory::getInstance();
    
    // Test camera task registration
    ASSERT_TRUE(factory.isTaskRegistered("TakeExposure"));
    auto exposureTask = factory.createTask("TakeExposure", "test_exposure", json{});
    ASSERT_NE(exposureTask, nullptr);
    
    // Test device task registration
    ASSERT_TRUE(factory.isTaskRegistered("DeviceConnect"));
    auto deviceTask = factory.createTask("DeviceConnect", "test_device", json{});
    ASSERT_NE(deviceTask, nullptr);
    
    // Test config task registration
    ASSERT_TRUE(factory.isTaskRegistered("LoadConfig"));
    auto configTask = factory.createTask("LoadConfig", "test_config", json{});
    ASSERT_NE(configTask, nullptr);
}

// Test Task Categories
TEST_F(EnhancedSystemTest, TaskCategories) {
    auto& factory = TaskFactory::getInstance();
    
    auto tasksByCategory = factory.getTasksByCategory();
    
    // Test that we have tasks in various categories
    ASSERT_TRUE(tasksByCategory.count("Camera") > 0);
    ASSERT_TRUE(tasksByCategory.count("Focus") > 0);
    ASSERT_TRUE(tasksByCategory.count("Device") > 0);
    
    // Test task info retrieval
    auto taskInfo = factory.getTaskInfo("TakeExposure");
    ASSERT_TRUE(taskInfo.has_value());
    ASSERT_EQ(taskInfo->name, "TakeExposure");
    ASSERT_EQ(taskInfo->category, "Camera");
}

// Test Sequencer Execution Strategies
TEST_F(EnhancedSystemTest, SequencerExecutionStrategies) {
    // Test sequential execution
    sequencer_->setExecutionStrategy(ExposureSequence::ExecutionStrategy::Sequential);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExposureSequence::ExecutionStrategy::Sequential);
    
    // Test parallel execution
    sequencer_->setExecutionStrategy(ExposureSequence::ExecutionStrategy::Parallel);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExposureSequence::ExecutionStrategy::Parallel);
    
    // Test adaptive execution
    sequencer_->setExecutionStrategy(ExposureSequence::ExecutionStrategy::Adaptive);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExposureSequence::ExecutionStrategy::Adaptive);
    
    // Test priority execution
    sequencer_->setExecutionStrategy(ExposureSequence::ExecutionStrategy::Priority);
    ASSERT_EQ(sequencer_->getExecutionStrategy(), ExposureSequence::ExecutionStrategy::Priority);
}

// Test Target Dependencies
TEST_F(EnhancedSystemTest, TargetDependencies) {
    // Create targets
    auto target1 = std::make_unique<Target>("Target1");
    auto target2 = std::make_unique<Target>("Target2");
    auto target3 = std::make_unique<Target>("Target3");
    
    // Add targets to sequencer
    sequencer_->addTarget(std::move(target1));
    sequencer_->addTarget(std::move(target2));
    sequencer_->addTarget(std::move(target3));
    
    // Set up dependencies: target3 depends on target1 and target2
    sequencer_->addTargetDependency("Target3", "Target1");
    sequencer_->addTargetDependency("Target3", "Target2");
    
    // Check dependencies
    auto deps = sequencer_->getTargetDependencies("Target3");
    ASSERT_EQ(deps.size(), 2);
    
    // Target3 should not be ready initially
    ASSERT_FALSE(sequencer_->isTargetReady("Target3"));
}

// Test Scheduling Strategies
TEST_F(EnhancedSystemTest, SchedulingStrategies) {
    // Create targets with different priorities
    auto target1 = std::make_unique<Target>("LowPriority");
    auto target2 = std::make_unique<Target>("HighPriority");
    
    sequencer_->addTarget(std::move(target1));
    sequencer_->addTarget(std::move(target2));
    
    // Test FIFO scheduling
    sequencer_->setSchedulingStrategy(ExposureSequence::SchedulingStrategy::FIFO);
    
    // Test Priority scheduling
    sequencer_->setSchedulingStrategy(ExposureSequence::SchedulingStrategy::Priority);
    
    // Test Dependencies scheduling
    sequencer_->setSchedulingStrategy(ExposureSequence::SchedulingStrategy::Dependencies);
    
    // Verify targets are present
    auto names = sequencer_->getTargetNames();
    ASSERT_EQ(names.size(), 2);
}

// Test Monitoring and Metrics
TEST_F(EnhancedSystemTest, MonitoringAndMetrics) {
    // Enable monitoring
    sequencer_->enableMonitoring(true);
    ASSERT_TRUE(sequencer_->isMonitoringEnabled());
    
    // Add a target
    auto target = std::make_unique<Target>("TestTarget");
    sequencer_->addTarget(std::move(target));
    
    // Get execution stats
    auto stats = sequencer_->getExecutionStats();
    ASSERT_TRUE(stats.is_object());
    ASSERT_TRUE(stats.contains("totalExecutions"));
    
    // Get resource usage
    auto resources = sequencer_->getResourceUsage();
    ASSERT_TRUE(resources.is_object());
    ASSERT_TRUE(resources.contains("memoryUsage"));
    
    // Get metrics
    auto metrics = sequencer_->getMetrics();
    ASSERT_TRUE(metrics.is_object());
}

// Test Task Parameter Validation
TEST_F(EnhancedSystemTest, TaskParameterValidation) {
    auto& factory = TaskFactory::getInstance();
    
    // Test valid parameters
    json validParams = {
        {"exposure", 30.0},
        {"type", "light"},
        {"binning", 1},
        {"gain", 100},
        {"offset", 10}
    };
    ASSERT_TRUE(factory.validateTaskParameters("TakeExposure", validParams));
    
    // Test parameter schema retrieval
    auto taskInfo = factory.getTaskInfo("TakeExposure");
    ASSERT_TRUE(taskInfo.has_value());
    ASSERT_FALSE(taskInfo->parameterSchema.empty());
}

// Test Script Task Creation
TEST_F(EnhancedSystemTest, ScriptTaskCreation) {
    auto& factory = TaskFactory::getInstance();
    
    // Test RunScript task creation
    ASSERT_TRUE(factory.isTaskRegistered("RunScript"));
    
    auto scriptTask = factory.createTask("RunScript", "test_script", json{
        {"script_path", "/tmp/test.py"},
        {"script_type", "python"},
        {"timeout", 5000}
    });
    ASSERT_NE(scriptTask, nullptr);
    ASSERT_EQ(scriptTask->getName(), "test_script");
}

// Test Error Handling and Recovery Strategies
TEST_F(EnhancedSystemTest, ErrorHandlingAndRecovery) {
    // Test recovery strategy setting
    sequencer_->setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Stop);
    
    sequencer_->setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Skip);
    
    sequencer_->setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Retry);
    
    sequencer_->setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Alternative);
    
    // Add alternative target
    auto mainTarget = std::make_unique<Target>("MainTarget");
    auto altTarget = std::make_unique<Target>("AlternativeTarget");
    
    sequencer_->addTarget(std::move(mainTarget));
    sequencer_->addAlternativeTarget("MainTarget", std::move(altTarget));
    
    // Verify target names
    auto names = sequencer_->getTargetNames();
    ASSERT_EQ(names.size(), 1);
    ASSERT_EQ(names[0], "MainTarget");
}

// Test Progress Callback
TEST_F(EnhancedSystemTest, ProgressCallback) {
    bool callbackCalled = false;
    json lastProgress;
    
    // Set progress callback
    sequencer_->setProgressCallback([&callbackCalled, &lastProgress](const json& progress) {
        callbackCalled = true;
        lastProgress = progress;
    });
    
    // Add a target
    auto target = std::make_unique<Target>("ProgressTest");
    sequencer_->addTarget(std::move(target));
    
    // Get progress
    auto progress = sequencer_->getProgress();
    ASSERT_GE(progress, 0.0);
    ASSERT_LE(progress, 100.0);
}

// Test Concurrency Settings
TEST_F(EnhancedSystemTest, ConcurrencySettings) {
    // Set concurrency limit
    sequencer_->setConcurrencyLimit(4);
    ASSERT_EQ(sequencer_->getConcurrencyLimit(), 4);
    
    // Set max concurrent targets
    sequencer_->setMaxConcurrentTargets(2);
    
    // Set resource limits
    sequencer_->setResourceLimits(80.0, 1024 * 1024 * 1024);
    
    // Enable performance optimization
    sequencer_->enablePerformanceOptimization(true);
    
    // Get optimization suggestions
    auto suggestions = sequencer_->getOptimizationSuggestions();
    ASSERT_TRUE(suggestions.is_object());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
