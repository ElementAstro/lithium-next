#ifndef LITHIUM_DEBUG_UNIFIED_TESTS_HPP
#define LITHIUM_DEBUG_UNIFIED_TESTS_HPP

#include "unified_manager.hpp"
#include "optimized_terminal.hpp"
#include "optimized_checker.hpp"
#include "error_handling.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include <future>

namespace lithium::debug::tests {

// Mock components for testing
class MockDebugComponent : public DebugComponentBase {
public:
    MOCK_METHOD(std::string, getName, (), (const, noexcept, override));
    MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
    MOCK_METHOD(Result<void>, initialize, (), (noexcept, override));
    MOCK_METHOD(Result<void>, shutdown, (), (noexcept, override));
    MOCK_METHOD(Result<void>, reset, (), (noexcept, override));
};

class MockAsyncDebugComponent : public AsyncDebugComponentBase {
public:
    MOCK_METHOD(std::string, getName, (), (const, noexcept, override));
    MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
    MOCK_METHOD(Result<void>, initialize, (), (noexcept, override));
    MOCK_METHOD(Result<void>, shutdown, (), (noexcept, override));
    MOCK_METHOD(Result<void>, reset, (), (noexcept, override));
    MOCK_METHOD(DebugTask<void>, initializeAsync, (), (noexcept, override));
    MOCK_METHOD(DebugTask<void>, shutdownAsync, (), (noexcept, override));
    MOCK_METHOD(DebugTask<void>, processAsync, (), (noexcept, override));
};

class MockErrorReporter : public ErrorReporterBase {
public:
    MOCK_METHOD(void, reportError, (const DebugError&), (noexcept, override));
    MOCK_METHOD(void, reportException, (const DebugException&), (noexcept, override));
    MOCK_METHOD(std::vector<DebugError>, getRecentErrors, (std::chrono::milliseconds), (const, noexcept, override));
};

// Test fixtures
class UnifiedDebugManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
        mockComponent_ = std::make_shared<MockDebugComponent>();
        mockAsyncComponent_ = std::make_shared<MockAsyncDebugComponent>();
        mockReporter_ = std::make_shared<MockErrorReporter>();
    }

    void TearDown() override {
        if (manager_) {
            [[maybe_unused]] auto result = manager_->shutdown();
        }
    }

    std::shared_ptr<UnifiedDebugManager> manager_;
    std::shared_ptr<MockDebugComponent> mockComponent_;
    std::shared_ptr<MockAsyncDebugComponent> mockAsyncComponent_;
    std::shared_ptr<MockErrorReporter> mockReporter_;
};

class OptimizedTerminalTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
        terminal_ = std::make_unique<OptimizedConsoleTerminal>(manager_);
    }

    void TearDown() override {
        if (terminal_ && terminal_->isActive()) {
            [[maybe_unused]] auto result = terminal_->shutdown();
        }
        if (manager_) {
            [[maybe_unused]] auto result = manager_->shutdown();
        }
    }

    std::shared_ptr<UnifiedDebugManager> manager_;
    std::unique_ptr<OptimizedConsoleTerminal> terminal_;
};

class OptimizedCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
        checker_ = std::make_unique<OptimizedCommandChecker>(manager_);
    }

    void TearDown() override {
        if (checker_ && checker_->isActive()) {
            [[maybe_unused]] auto result = checker_->shutdown();
        }
        if (manager_) {
            [[maybe_unused]] auto result = manager_->shutdown();
        }
    }

    std::shared_ptr<UnifiedDebugManager> manager_;
    std::unique_ptr<OptimizedCommandChecker> checker_;
};

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<ErrorRecoveryManager>();
        aggregator_ = std::make_unique<ErrorAggregator>();
    }

    std::shared_ptr<ErrorRecoveryManager> manager_;
    std::unique_ptr<ErrorAggregator> aggregator_;
};

// Test implementations
class UnifiedDebugManagerTestSuite : public UnifiedDebugManagerTest {
public:
    // Basic lifecycle tests
    void TestInitialization();
    void TestShutdown();
    void TestReset();

    // Component management tests
    void TestRegisterComponent();
    void TestUnregisterComponent();
    void TestGetComponent();
    void TestGetAllComponents();

    // Async component tests
    void TestRegisterAsyncComponent();
    void TestAsyncOperations();

    // Error reporting tests
    void TestErrorReporting();
    void TestExceptionHandling();

    // Performance tests
    void TestPerformanceMetrics();
    void TestConcurrentAccess();

    // Configuration tests
    void TestConfiguration();
    void TestConfigurationPersistence();
};

class OptimizedTerminalTestSuite : public OptimizedTerminalTest {
public:
    // Basic functionality tests
    void TestInitialization();
    void TestCommandRegistration();
    void TestCommandExecution();
    void TestAsyncCommandExecution();

    // Command management tests
    void TestCommandUnregistration();
    void TestCommandSuggestions();
    void TestCommandHistory();

    // Configuration tests
    void TestConfiguration();
    void TestTimeout();
    void TestSessionManagement();

    // Observer pattern tests
    void TestObserverRegistration();
    void TestEventNotification();

    // Error handling tests
    void TestErrorHandling();
    void TestExceptionSafety();

    // Performance tests
    void TestStatistics();
    void TestConcurrentExecution();
};

class OptimizedCheckerTestSuite : public OptimizedCheckerTest {
public:
    // Basic functionality tests
    void TestInitialization();
    void TestRuleRegistration();
    void TestCommandChecking();
    void TestAsyncChecking();

    // Rule management tests
    void TestRuleUnregistration();
    void TestRuleStatistics();

    // Security analysis tests
    void TestSecurityAnalysis();
    void TestDangerousCommands();
    void TestForbiddenPatterns();

    // Batch operations tests
    void TestBatchChecking();
    void TestParallelExecution();

    // Configuration tests
    void TestConfiguration();
    void TestConfigurationPersistence();

    // Error handling tests
    void TestErrorHandling();
    void TestRecovery();
};

class ErrorHandlingTestSuite : public ErrorHandlingTest {
public:
    // Error hierarchy tests
    void TestErrorCreation();
    void TestErrorComparison();
    void TestErrorSerialization();

    // Exception tests
    void TestDebugException();
    void TestExceptionRecovery();
    void TestNestedExceptions();

    // Recovery manager tests
    void TestRecoveryStrategies();
    void TestAutomaticRecovery();
    void TestRecoveryMetrics();

    // Error aggregation tests
    void TestErrorAggregation();
    void TestErrorStatistics();
    void TestErrorReporting();

    // Result type tests
    void TestResultType();
    void TestResultChaining();
    void TestResultMonadic();
};

// Integration tests
class IntegrationTestSuite : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
        terminal_ = std::make_unique<OptimizedConsoleTerminal>(manager_);
        checker_ = std::make_unique<OptimizedCommandChecker>(manager_);
    }

    void TearDown() override {
        if (terminal_ && terminal_->isActive()) {
            [[maybe_unused]] auto result = terminal_->shutdown();
        }
        if (checker_ && checker_->isActive()) {
            [[maybe_unused]] auto result = checker_->shutdown();
        }
        if (manager_) {
            [[maybe_unused]] auto result = manager_->shutdown();
        }
    }

    std::shared_ptr<UnifiedDebugManager> manager_;
    std::unique_ptr<OptimizedConsoleTerminal> terminal_;
    std::unique_ptr<OptimizedCommandChecker> checker_;

public:
    // End-to-end workflow tests
    void TestFullWorkflow();
    void TestComponentInteraction();
    void TestErrorPropagation();
    void TestPerformanceIntegration();
    void TestConcurrentOperations();
    void TestRecoveryScenarios();
};

// Performance benchmarks
class PerformanceBenchmarks : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
        terminal_ = std::make_unique<OptimizedConsoleTerminal>(manager_);
        checker_ = std::make_unique<OptimizedCommandChecker>(manager_);
    }

    std::shared_ptr<UnifiedDebugManager> manager_;
    std::unique_ptr<OptimizedConsoleTerminal> terminal_;
    std::unique_ptr<OptimizedCommandChecker> checker_;

public:
    // Benchmark tests
    void BenchmarkComponentRegistration();
    void BenchmarkCommandExecution();
    void BenchmarkCommandChecking();
    void BenchmarkConcurrentAccess();
    void BenchmarkMemoryUsage();
    void BenchmarkErrorHandling();
};

// Stress tests
class StressTests : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<UnifiedDebugManager>();
    }

    std::shared_ptr<UnifiedDebugManager> manager_;

public:
    // Stress test scenarios
    void StressTestComponentRegistration();
    void StressTestConcurrentOperations();
    void StressTestMemoryPressure();
    void StressTestErrorGeneration();
    void StressTestLongRunningOperations();
};

} // namespace lithium::debug::tests

#endif // LITHIUM_DEBUG_UNIFIED_TESTS_HPP
