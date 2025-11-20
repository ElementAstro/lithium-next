#include "unified_tests.hpp"
#include <gtest/gtest.h>

namespace lithium::debug::tests {

// Test instantiations for UnifiedDebugManager
TEST_F(UnifiedDebugManagerTest, Initialization) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestInitialization();
}

TEST_F(UnifiedDebugManagerTest, Shutdown) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestShutdown();
}

TEST_F(UnifiedDebugManagerTest, Reset) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestReset();
}

TEST_F(UnifiedDebugManagerTest, RegisterComponent) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestRegisterComponent();
}

TEST_F(UnifiedDebugManagerTest, UnregisterComponent) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestUnregisterComponent();
}

TEST_F(UnifiedDebugManagerTest, ErrorReporting) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestErrorReporting();
}

TEST_F(UnifiedDebugManagerTest, ConcurrentAccess) {
    UnifiedDebugManagerTestSuite suite;
    suite.manager_ = manager_;
    suite.mockComponent_ = mockComponent_;
    suite.mockAsyncComponent_ = mockAsyncComponent_;
    suite.mockReporter_ = mockReporter_;
    suite.TestConcurrentAccess();
}

// Test instantiations for OptimizedTerminal
TEST_F(OptimizedTerminalTest, Initialization) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestInitialization();
    terminal_ = std::move(suite.terminal_);
}

TEST_F(OptimizedTerminalTest, CommandRegistration) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestCommandRegistration();
    terminal_ = std::move(suite.terminal_);
}

TEST_F(OptimizedTerminalTest, CommandExecution) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestCommandExecution();
    terminal_ = std::move(suite.terminal_);
}

TEST_F(OptimizedTerminalTest, AsyncCommandExecution) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestAsyncCommandExecution();
    terminal_ = std::move(suite.terminal_);
}

TEST_F(OptimizedTerminalTest, CommandHistory) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestCommandHistory();
    terminal_ = std::move(suite.terminal_);
}

TEST_F(OptimizedTerminalTest, Statistics) {
    OptimizedTerminalTestSuite suite;
    suite.manager_ = manager_;
    suite.terminal_ = std::move(terminal_);
    suite.TestStatistics();
    terminal_ = std::move(suite.terminal_);
}

// Test instantiations for OptimizedChecker
TEST_F(OptimizedCheckerTest, Initialization) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestInitialization();
    checker_ = std::move(suite.checker_);
}

TEST_F(OptimizedCheckerTest, RuleRegistration) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestRuleRegistration();
    checker_ = std::move(suite.checker_);
}

TEST_F(OptimizedCheckerTest, CommandChecking) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestCommandChecking();
    checker_ = std::move(suite.checker_);
}

TEST_F(OptimizedCheckerTest, SecurityAnalysis) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestSecurityAnalysis();
    checker_ = std::move(suite.checker_);
}

TEST_F(OptimizedCheckerTest, BatchChecking) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestBatchChecking();
    checker_ = std::move(suite.checker_);
}

TEST_F(OptimizedCheckerTest, DangerousCommands) {
    OptimizedCheckerTestSuite suite;
    suite.manager_ = manager_;
    suite.checker_ = std::move(checker_);
    suite.TestDangerousCommands();
    checker_ = std::move(suite.checker_);
}

// Test instantiations for ErrorHandling
TEST_F(ErrorHandlingTest, ErrorCreation) {
    ErrorHandlingTestSuite suite;
    suite.manager_ = manager_;
    suite.aggregator_ = std::move(aggregator_);
    suite.TestErrorCreation();
    aggregator_ = std::move(suite.aggregator_);
}

TEST_F(ErrorHandlingTest, DebugException) {
    ErrorHandlingTestSuite suite;
    suite.manager_ = manager_;
    suite.aggregator_ = std::move(aggregator_);
    suite.TestDebugException();
    aggregator_ = std::move(suite.aggregator_);
}

TEST_F(ErrorHandlingTest, RecoveryStrategies) {
    ErrorHandlingTestSuite suite;
    suite.manager_ = manager_;
    suite.aggregator_ = std::move(aggregator_);
    suite.TestRecoveryStrategies();
    aggregator_ = std::move(suite.aggregator_);
}

TEST_F(ErrorHandlingTest, ErrorAggregation) {
    ErrorHandlingTestSuite suite;
    suite.manager_ = manager_;
    suite.aggregator_ = std::move(aggregator_);
    suite.TestErrorAggregation();
    aggregator_ = std::move(suite.aggregator_);
}

TEST_F(ErrorHandlingTest, ResultType) {
    ErrorHandlingTestSuite suite;
    suite.manager_ = manager_;
    suite.aggregator_ = std::move(aggregator_);
    suite.TestResultType();
    aggregator_ = std::move(suite.aggregator_);
}

// Integration tests
TEST_F(IntegrationTestSuite, FullWorkflow) {
    TestFullWorkflow();
}

TEST_F(IntegrationTestSuite, ComponentInteraction) {
    TestComponentInteraction();
}

TEST_F(IntegrationTestSuite, ErrorPropagation) {
    TestErrorPropagation();
}

TEST_F(IntegrationTestSuite, ConcurrentOperations) {
    TestConcurrentOperations();
}

// Performance benchmarks (marked as disabled by default)
TEST_F(PerformanceBenchmarks, DISABLED_ComponentRegistration) {
    BenchmarkComponentRegistration();
}

TEST_F(PerformanceBenchmarks, DISABLED_CommandExecution) {
    BenchmarkCommandExecution();
}

TEST_F(PerformanceBenchmarks, DISABLED_CommandChecking) {
    BenchmarkCommandChecking();
}

TEST_F(PerformanceBenchmarks, DISABLED_MemoryUsage) {
    BenchmarkMemoryUsage();
}

// Stress tests (marked as disabled by default)
class StressTestSuite : public StressTests {
public:
    void TestMassiveComponentRegistration() {
        ASSERT_TRUE(manager_->initialize().has_value());
        
        const int numComponents = 10000;
        std::vector<std::shared_ptr<MockDebugComponent>> components;
        
        for (int i = 0; i < numComponents; ++i) {
            auto component = std::make_shared<MockDebugComponent>();
            EXPECT_CALL(*component, getName())
                .WillRepeatedly(::testing::Return(std::format("StressComponent{}", i)));
            EXPECT_CALL(*component, initialize())
                .WillOnce(::testing::Return(Result<void>{}));
            EXPECT_CALL(*component, shutdown())
                .WillOnce(::testing::Return(Result<void>{}));
                
            components.push_back(component);
            auto result = manager_->registerComponent(component);
            ASSERT_TRUE(result.has_value()) << std::format("Failed to register component {}", i);
        }
        
        // Verify all components are registered
        auto allComponents = manager_->getAllComponents();
        EXPECT_EQ(allComponents.size(), numComponents) << "All components should be registered";
        
        std::cout << std::format("Successfully registered {} components\n", numComponents);
    }
    
    void TestHighConcurrency() {
        ASSERT_TRUE(manager_->initialize().has_value());
        
        const int numThreads = 50;
        const int operationsPerThread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> totalOperations{0};
        std::atomic<int> successfulOperations{0};
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < operationsPerThread; ++j) {
                    totalOperations.fetch_add(1, std::memory_order_relaxed);
                    
                    auto component = std::make_shared<MockDebugComponent>();
                    EXPECT_CALL(*component, getName())
                        .WillRepeatedly(::testing::Return(std::format("ConcurrentComponent{}_{}", i, j)));
                    EXPECT_CALL(*component, initialize())
                        .WillOnce(::testing::Return(Result<void>{}));
                    EXPECT_CALL(*component, shutdown())
                        .WillOnce(::testing::Return(Result<void>{}));
                    
                    auto regResult = manager_->registerComponent(component);
                    if (regResult.has_value()) {
                        successfulOperations.fetch_add(1, std::memory_order_relaxed);
                        
                        // Small delay to simulate work
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                        
                        [[maybe_unused]] auto unregResult = manager_->unregisterComponent(component);
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto total = totalOperations.load();
        auto successful = successfulOperations.load();
        double successRate = static_cast<double>(successful) / total * 100.0;
        
        std::cout << std::format("Concurrent operations: {}/{} successful ({:.2f}%)\n", 
                                successful, total, successRate);
        
        EXPECT_GT(successRate, 95.0) << "Success rate should be high under concurrent load";
    }
};

TEST_F(StressTestSuite, DISABLED_MassiveComponentRegistration) {
    TestMassiveComponentRegistration();
}

TEST_F(StressTestSuite, DISABLED_HighConcurrency) {
    TestHighConcurrency();
}

// Custom test for async operations
class AsyncOperationTest : public ::testing::Test {
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
};

TEST_F(AsyncOperationTest, AsyncCommandExecution) {
    ASSERT_TRUE(terminal_->initialize().has_value());
    
    // Register an async command with a delay
    auto regResult = terminal_->registerAsyncCommand("slow_command",
        [](std::span<const std::any> args) -> DebugTask<std::string> {
            co_await std::suspend_for(std::chrono::milliseconds(100));
            co_return "Slow command completed";
        });
    ASSERT_TRUE(regResult.has_value());
    
    // Execute async command
    auto start = std::chrono::steady_clock::now();
    auto task = terminal_->executeCommandAsync("slow_command");
    auto result = task.get();
    auto end = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(result.has_value()) << "Async command should succeed";
    EXPECT_EQ(result.value(), "Slow command completed");
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 100) << "Should take at least 100ms due to delay";
}

TEST_F(AsyncOperationTest, AsyncCommandChecking) {
    ASSERT_TRUE(checker_->initialize().has_value());
    
    // Create an async rule with delay
    struct AsyncTestRule {
        using result_type = OptimizedCommandChecker::CheckError;
        
        DebugTask<result_type> checkAsync(std::string_view command, size_t line, size_t column) const {
            co_await std::suspend_for(std::chrono::milliseconds(50));
            
            if (command.find("async_test") != std::string_view::npos) {
                co_return OptimizedCommandChecker::CheckError{
                    .message = "Async test rule triggered",
                    .severity = ErrorSeverity::WARNING,
                    .ruleName = "async_test_rule"
                };
            }
            co_return OptimizedCommandChecker::CheckError{};
        }
        
        result_type check(std::string_view command, size_t line, size_t column) const {
            return OptimizedCommandChecker::CheckError{};
        }
        
        std::string_view getName() const { return "async_test_rule"; }
        ErrorSeverity getSeverity() const { return ErrorSeverity::WARNING; }
        bool isEnabled() const { return true; }
    };
    
    auto regResult = checker_->registerAsyncRule("async_test_rule", AsyncTestRule{});
    ASSERT_TRUE(regResult.has_value());
    
    // Execute async check
    auto start = std::chrono::steady_clock::now();
    auto task = checker_->checkCommandAsync("async_test command");
    auto result = task.get();
    auto end = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(result.has_value()) << "Async check should succeed";
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 50) << "Should take at least 50ms due to async rule delay";
}

} // namespace lithium::debug::tests

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Print information about the test suite
    std::cout << "Lithium Debug System Unified Test Suite\n";
    std::cout << "========================================\n";
    std::cout << "Testing modern C++ optimized debug components with:\n";
    std::cout << "- Unified debug management\n";
    std::cout << "- Optimized terminal with C++20/23 features\n";
    std::cout << "- Enhanced command checker with security analysis\n";
    std::cout << "- Comprehensive error handling and recovery\n";
    std::cout << "- Performance benchmarks and stress tests\n";
    std::cout << "\nTo run performance benchmarks: --gtest_also_run_disabled_tests\n";
    std::cout << "To run specific tests: --gtest_filter=TestName\n\n";
    
    return RUN_ALL_TESTS();
}
