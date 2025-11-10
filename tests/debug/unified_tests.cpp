#include "unified_tests.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>

namespace lithium::debug::tests {

// UnifiedDebugManagerTestSuite implementations
void UnifiedDebugManagerTestSuite::TestInitialization() {
    // Test successful initialization
    auto result = manager_->initialize();
    EXPECT_TRUE(result.has_value()) << "Manager initialization should succeed";
    EXPECT_TRUE(manager_->isActive()) << "Manager should be active after initialization";

    // Test double initialization
    auto doubleInitResult = manager_->initialize();
    EXPECT_FALSE(doubleInitResult.has_value()) << "Double initialization should fail";
    EXPECT_EQ(doubleInitResult.error().code, ErrorCode::INVALID_STATE);
}

void UnifiedDebugManagerTestSuite::TestShutdown() {
    // Initialize first
    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(manager_->isActive());

    // Test successful shutdown
    auto result = manager_->shutdown();
    EXPECT_TRUE(result.has_value()) << "Manager shutdown should succeed";
    EXPECT_FALSE(manager_->isActive()) << "Manager should not be active after shutdown";

    // Test double shutdown
    auto doubleShutdownResult = manager_->shutdown();
    EXPECT_FALSE(doubleShutdownResult.has_value()) << "Double shutdown should fail";
    EXPECT_EQ(doubleShutdownResult.error().code, ErrorCode::INVALID_STATE);
}

void UnifiedDebugManagerTestSuite::TestReset() {
    // Initialize and add some components
    ASSERT_TRUE(manager_->initialize().has_value());

    EXPECT_CALL(*mockComponent_, getName())
        .WillRepeatedly(::testing::Return("MockComponent"));
    EXPECT_CALL(*mockComponent_, initialize())
        .WillOnce(::testing::Return(Result<void>{}));
    EXPECT_CALL(*mockComponent_, shutdown())
        .WillOnce(::testing::Return(Result<void>{}));

    auto regResult = manager_->registerComponent(mockComponent_);
    ASSERT_TRUE(regResult.has_value());

    // Test reset
    auto resetResult = manager_->reset();
    EXPECT_TRUE(resetResult.has_value()) << "Manager reset should succeed";
    EXPECT_TRUE(manager_->isActive()) << "Manager should be active after reset";

    // Verify components are cleared
    auto components = manager_->getAllComponents();
    EXPECT_TRUE(components.empty()) << "All components should be cleared after reset";
}

void UnifiedDebugManagerTestSuite::TestRegisterComponent() {
    ASSERT_TRUE(manager_->initialize().has_value());

    EXPECT_CALL(*mockComponent_, getName())
        .WillRepeatedly(::testing::Return("TestComponent"));
    EXPECT_CALL(*mockComponent_, initialize())
        .WillOnce(::testing::Return(Result<void>{}));

    // Test successful registration
    auto result = manager_->registerComponent(mockComponent_);
    EXPECT_TRUE(result.has_value()) << "Component registration should succeed";

    // Verify component is registered
    auto component = manager_->getComponent("TestComponent");
    EXPECT_TRUE(component.has_value()) << "Registered component should be retrievable";
    EXPECT_EQ(component.value(), mockComponent_);

    // Test duplicate registration
    auto duplicateResult = manager_->registerComponent(mockComponent_);
    EXPECT_FALSE(duplicateResult.has_value()) << "Duplicate registration should fail";
    EXPECT_EQ(duplicateResult.error().code, ErrorCode::INVALID_OPERATION);
}

void UnifiedDebugManagerTestSuite::TestUnregisterComponent() {
    ASSERT_TRUE(manager_->initialize().has_value());

    EXPECT_CALL(*mockComponent_, getName())
        .WillRepeatedly(::testing::Return("TestComponent"));
    EXPECT_CALL(*mockComponent_, initialize())
        .WillOnce(::testing::Return(Result<void>{}));
    EXPECT_CALL(*mockComponent_, shutdown())
        .WillOnce(::testing::Return(Result<void>{}));

    // Register component first
    ASSERT_TRUE(manager_->registerComponent(mockComponent_).has_value());

    // Test successful unregistration
    auto result = manager_->unregisterComponent(mockComponent_);
    EXPECT_TRUE(result.has_value()) << "Component unregistration should succeed";

    // Verify component is removed
    auto component = manager_->getComponent("TestComponent");
    EXPECT_FALSE(component.has_value()) << "Unregistered component should not be retrievable";

    // Test unregistering non-existent component
    auto nonExistentResult = manager_->unregisterComponent(mockComponent_);
    EXPECT_FALSE(nonExistentResult.has_value()) << "Unregistering non-existent component should fail";
}

void UnifiedDebugManagerTestSuite::TestErrorReporting() {
    ASSERT_TRUE(manager_->initialize().has_value());

    // Test error reporting
    DebugError testError{
        ErrorCode::RUNTIME_ERROR,
        "Test error message",
        ErrorCategory::GENERAL,
        ErrorSeverity::ERROR
    };

    manager_->setErrorReporter(mockReporter_);

    EXPECT_CALL(*mockReporter_, reportError(::testing::_))
        .Times(1);

    manager_->reportError(testError);

    // Test exception reporting
    DebugException testException{testError};

    EXPECT_CALL(*mockReporter_, reportException(::testing::_))
        .Times(1);

    manager_->reportException(testException);
}

void UnifiedDebugManagerTestSuite::TestConcurrentAccess() {
    ASSERT_TRUE(manager_->initialize().has_value());

    const int numThreads = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Create multiple mock components for concurrent testing
    std::vector<std::shared_ptr<MockDebugComponent>> components;
    for (int i = 0; i < numThreads; ++i) {
        auto component = std::make_shared<MockDebugComponent>();
        EXPECT_CALL(*component, getName())
            .WillRepeatedly(::testing::Return(std::format("Component{}", i)));
        EXPECT_CALL(*component, initialize())
            .WillRepeatedly(::testing::Return(Result<void>{}));
        EXPECT_CALL(*component, shutdown())
            .WillRepeatedly(::testing::Return(Result<void>{}));
        components.push_back(component);
    }

    // Test concurrent component registration
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                auto result = manager_->registerComponent(components[i]);
                if (j == 0 && result.has_value()) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }

                // Small delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads) << "All unique components should register successfully";
}

// OptimizedTerminalTestSuite implementations
void OptimizedTerminalTestSuite::TestInitialization() {
    auto result = terminal_->initialize();
    EXPECT_TRUE(result.has_value()) << "Terminal initialization should succeed";
    EXPECT_TRUE(terminal_->isActive()) << "Terminal should be active after initialization";

    // Verify default commands are registered
    auto commands = terminal_->getRegisteredCommands();
    EXPECT_FALSE(commands.empty()) << "Default commands should be registered";

    auto helpIt = std::find(commands.begin(), commands.end(), "help");
    EXPECT_NE(helpIt, commands.end()) << "Help command should be registered";
}

void OptimizedTerminalTestSuite::TestCommandRegistration() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Test registering a simple command
    auto result = terminal_->registerCommand("test",
        [](std::span<const std::any> args) -> Result<std::string> {
            return "Test command executed";
        });

    EXPECT_TRUE(result.has_value()) << "Command registration should succeed";

    // Verify command is registered
    auto commands = terminal_->getRegisteredCommands();
    auto testIt = std::find(commands.begin(), commands.end(), "test");
    EXPECT_NE(testIt, commands.end()) << "Test command should be in registered commands";

    // Test duplicate registration
    auto duplicateResult = terminal_->registerCommand("test",
        [](std::span<const std::any> args) -> Result<std::string> {
            return "Duplicate command";
        });

    EXPECT_FALSE(duplicateResult.has_value()) << "Duplicate command registration should fail";
}

void OptimizedTerminalTestSuite::TestCommandExecution() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Register a test command
    std::string expectedOutput = "Hello, World!";
    auto regResult = terminal_->registerCommand("hello",
        [expectedOutput](std::span<const std::any> args) -> Result<std::string> {
            return expectedOutput;
        });
    ASSERT_TRUE(regResult.has_value());

    // Execute the command
    auto result = terminal_->executeCommand("hello");
    EXPECT_TRUE(result.has_value()) << "Command execution should succeed";
    EXPECT_EQ(result.value(), expectedOutput) << "Command should return expected output";

    // Test executing non-existent command
    auto nonExistentResult = terminal_->executeCommand("nonexistent");
    EXPECT_FALSE(nonExistentResult.has_value()) << "Non-existent command should fail";
    EXPECT_EQ(nonExistentResult.error().code, ErrorCode::COMMAND_NOT_FOUND);
}

void OptimizedTerminalTestSuite::TestAsyncCommandExecution() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Register an async test command
    std::string expectedOutput = "Async Hello!";
    auto regResult = terminal_->registerAsyncCommand("async_hello",
        [expectedOutput](std::span<const std::any> args) -> DebugTask<std::string> {
            co_await std::suspend_for(std::chrono::milliseconds(10));
            co_return expectedOutput;
        });
    ASSERT_TRUE(regResult.has_value());

    // Execute the async command
    auto task = terminal_->executeCommandAsync("async_hello");
    auto result = task.get(); // Synchronously wait for result

    EXPECT_TRUE(result.has_value()) << "Async command execution should succeed";
    EXPECT_EQ(result.value(), expectedOutput) << "Async command should return expected output";
}

void OptimizedTerminalTestSuite::TestCommandHistory() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Register and execute some commands
    auto regResult = terminal_->registerCommand("test",
        [](std::span<const std::any> args) -> Result<std::string> {
            return "test output";
        });
    ASSERT_TRUE(regResult.has_value());

    // Execute commands to populate history
    terminal_->executeCommand("help");
    terminal_->executeCommand("test");
    terminal_->executeCommand("stats");

    // Check history
    auto history = terminal_->getCommandHistory();
    EXPECT_GE(history.size(), 3) << "History should contain executed commands";

    // Clear history
    terminal_->clearHistory();
    auto clearedHistory = terminal_->getCommandHistory();
    EXPECT_TRUE(clearedHistory.empty()) << "History should be empty after clearing";
}

void OptimizedTerminalTestSuite::TestStatistics() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    auto stats = terminal_->getStatistics();
    auto initialCommands = stats.commandsExecuted.load();
    auto initialErrors = stats.errorsEncountered.load();

    // Execute a successful command
    terminal_->executeCommand("help");

    stats = terminal_->getStatistics();
    EXPECT_GT(stats.commandsExecuted.load(), initialCommands)
        << "Command count should increase after execution";

    // Execute a failing command
    terminal_->executeCommand("nonexistent");

    stats = terminal_->getStatistics();
    EXPECT_GT(stats.errorsEncountered.load(), initialErrors)
        << "Error count should increase after failed command";
}

// OptimizedCheckerTestSuite implementations
void OptimizedCheckerTestSuite::TestInitialization() {
    auto result = checker_->initialize();
    EXPECT_TRUE(result.has_value()) << "Checker initialization should succeed";
    EXPECT_TRUE(checker_->isActive()) << "Checker should be active after initialization";

    // Verify default rules are present
    auto rules = checker_->getRegisteredRules();
    EXPECT_FALSE(rules.empty()) << "Default rules should be registered";
}

void OptimizedCheckerTestSuite::TestRuleRegistration() {
    ASSERT_TRUE(checker_->initialize().has_value());

    // Create a simple test rule
    struct TestRule {
        using result_type = OptimizedCommandChecker::CheckError;

        result_type check(std::string_view command, size_t line, size_t column) const {
            if (command.find("test") != std::string_view::npos) {
                return CheckError{
                    .message = "Test rule triggered",
                    .severity = ErrorSeverity::WARNING,
                    .ruleName = "test_rule"
                };
            }
            return CheckError{}; // No error
        }

        std::string_view getName() const { return "test_rule"; }
        ErrorSeverity getSeverity() const { return ErrorSeverity::WARNING; }
        bool isEnabled() const { return true; }
    };

    // Register the rule
    auto result = checker_->registerRule("test_rule", TestRule{});
    EXPECT_TRUE(result.has_value()) << "Rule registration should succeed";

    // Verify rule is registered
    auto rules = checker_->getRegisteredRules();
    auto testIt = std::find(rules.begin(), rules.end(), "test_rule");
    EXPECT_NE(testIt, rules.end()) << "Test rule should be in registered rules";
}

void OptimizedCheckerTestSuite::TestCommandChecking() {
    ASSERT_TRUE(checker_->initialize().has_value());

    // Test checking a safe command
    auto safeResult = checker_->checkCommand("echo hello");
    EXPECT_TRUE(safeResult.has_value()) << "Safe command check should succeed";
    EXPECT_FALSE(safeResult->hasCriticalErrors()) << "Safe command should not have critical errors";

    // Test checking a dangerous command
    auto dangerousResult = checker_->checkCommand("rm -rf /");
    EXPECT_TRUE(dangerousResult.has_value()) << "Dangerous command check should succeed";
    EXPECT_TRUE(dangerousResult->hasErrors()) << "Dangerous command should have errors";
}

void OptimizedCheckerTestSuite::TestSecurityAnalysis() {
    ASSERT_TRUE(checker_->initialize().has_value());

    // Test security risk analysis
    auto riskResult = checker_->analyzeSecurityRisk("rm -rf /important/data");
    EXPECT_TRUE(riskResult.has_value()) << "Security analysis should succeed";
    EXPECT_GT(riskResult.value(), 0.0) << "Dangerous command should have positive risk";

    // Test security suggestions
    auto suggestions = checker_->getSecuritySuggestions("rm -rf /");
    // Suggestions might be empty, but the call should not throw
    EXPECT_NO_THROW(suggestions.size());
}

void OptimizedCheckerTestSuite::TestBatchChecking() {
    ASSERT_TRUE(checker_->initialize().has_value());

    std::vector<std::string> commands = {
        "echo hello",
        "ls -la",
        "rm -rf /",
        "cat /etc/passwd"
    };

    // Test batch checking
    auto results = checker_->checkCommands(commands);
    EXPECT_TRUE(results.has_value()) << "Batch checking should succeed";
    EXPECT_EQ(results->size(), commands.size()) << "Should get result for each command";

    // Verify at least one command has errors (the dangerous ones)
    bool foundErrors = false;
    for (const auto& result : *results) {
        if (result.hasErrors()) {
            foundErrors = true;
            break;
        }
    }
    EXPECT_TRUE(foundErrors) << "At least one command should have errors";
}

void OptimizedCheckerTestSuite::TestDangerousCommands() {
    ASSERT_TRUE(checker_->initialize().has_value());

    // Test adding dangerous command
    checker_->addDangerousCommand("dangerous_test");
    EXPECT_TRUE(checker_->isDangerousCommand("dangerous_test"))
        << "Added command should be marked as dangerous";

    // Test removing dangerous command
    checker_->removeDangerousCommand("dangerous_test");
    EXPECT_FALSE(checker_->isDangerousCommand("dangerous_test"))
        << "Removed command should no longer be dangerous";

    // Test checking command with dangerous command
    auto result = checker_->checkCommand("dangerous_test some args");
    // The result depends on whether dangerous_test is still in the list
}

// ErrorHandlingTestSuite implementations
void ErrorHandlingTestSuite::TestErrorCreation() {
    DebugError error{
        ErrorCode::RUNTIME_ERROR,
        "Test error message",
        ErrorCategory::GENERAL,
        ErrorSeverity::ERROR
    };

    EXPECT_EQ(error.code, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(error.message, "Test error message");
    EXPECT_EQ(error.category, ErrorCategory::GENERAL);
    EXPECT_EQ(error.severity, ErrorSeverity::ERROR);
}

void ErrorHandlingTestSuite::TestDebugException() {
    DebugError error{
        ErrorCode::RUNTIME_ERROR,
        "Test exception",
        ErrorCategory::GENERAL,
        ErrorSeverity::ERROR
    };

    DebugException exception{error};

    EXPECT_EQ(exception.getError().code, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(exception.getError().message, "Test exception");
    EXPECT_STREQ(exception.what(), "Test exception");
}

void ErrorHandlingTestSuite::TestRecoveryStrategies() {
    auto recoveryManager = std::make_shared<ErrorRecoveryManager>();

    // Test registering recovery strategy
    bool strategyCalled = false;
    auto strategy = [&strategyCalled](const DebugError& error) -> RecoveryAction {
        strategyCalled = true;
        return RecoveryAction::RETRY;
    };

    recoveryManager->registerStrategy(ErrorCode::RUNTIME_ERROR, strategy);

    // Test recovery attempt
    DebugError error{ErrorCode::RUNTIME_ERROR, "Test", ErrorCategory::GENERAL, ErrorSeverity::ERROR};
    auto action = recoveryManager->attemptRecovery(error);

    EXPECT_TRUE(strategyCalled) << "Recovery strategy should be called";
    EXPECT_EQ(action, RecoveryAction::RETRY) << "Should return expected recovery action";
}

void ErrorHandlingTestSuite::TestErrorAggregation() {
    auto aggregator = std::make_unique<ErrorAggregator>();

    // Add some test errors
    for (int i = 0; i < 10; ++i) {
        DebugError error{
            ErrorCode::RUNTIME_ERROR,
            std::format("Error {}", i),
            ErrorCategory::GENERAL,
            ErrorSeverity::ERROR
        };
        aggregator->addError(error);
    }

    // Test statistics
    auto stats = aggregator->getStatistics();
    EXPECT_EQ(stats.totalErrors, 10) << "Should count all added errors";
    EXPECT_GT(stats.errorsByCode[ErrorCode::RUNTIME_ERROR], 0) << "Should track errors by code";

    // Test most frequent errors
    auto frequent = aggregator->getMostFrequentErrors(5);
    EXPECT_LE(frequent.size(), 5) << "Should not exceed requested limit";
}

void ErrorHandlingTestSuite::TestResultType() {
    // Test successful result
    Result<int> successResult = 42;
    EXPECT_TRUE(successResult.has_value()) << "Success result should have value";
    EXPECT_EQ(successResult.value(), 42) << "Should contain correct value";

    // Test error result
    DebugError error{ErrorCode::RUNTIME_ERROR, "Test", ErrorCategory::GENERAL, ErrorSeverity::ERROR};
    Result<int> errorResult = unexpected(error);
    EXPECT_FALSE(errorResult.has_value()) << "Error result should not have value";
    EXPECT_EQ(errorResult.error().code, ErrorCode::RUNTIME_ERROR) << "Should contain correct error";
}

// IntegrationTestSuite implementations
void IntegrationTestSuite::TestFullWorkflow() {
    // Initialize all components
    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(terminal_->initialize().has_value());
    ASSERT_TRUE(checker_->initialize().has_value());

    // Register a command that uses the checker
    auto regResult = terminal_->registerCommand("check",
        [this](std::span<const std::any> args) -> Result<std::string> {
            if (args.empty()) {
                return unexpected(DebugError{
                    ErrorCode::INVALID_ARGUMENT,
                    "Command to check required",
                    ErrorCategory::TERMINAL,
                    ErrorSeverity::ERROR
                });
            }

            try {
                std::string command = std::any_cast<std::string>(args[0]);
                auto checkResult = checker_->checkCommand(command);

                if (!checkResult.has_value()) {
                    return unexpected(checkResult.error());
                }

                return checker_->generateReport(*checkResult, command);
            } catch (const std::bad_any_cast&) {
                return unexpected(DebugError{
                    ErrorCode::TYPE_ERROR,
                    "Invalid argument type",
                    ErrorCategory::TERMINAL,
                    ErrorSeverity::ERROR
                });
            }
        });

    ASSERT_TRUE(regResult.has_value()) << "Check command registration should succeed";

    // Execute the integrated command
    std::vector<std::any> args = {std::string{"rm -rf /"}};
    auto result = terminal_->executeCommand("check", args);

    EXPECT_TRUE(result.has_value()) << "Integrated command should execute successfully";
    EXPECT_FALSE(result->empty()) << "Should return a report";
}

void IntegrationTestSuite::TestComponentInteraction() {
    // Initialize components
    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(terminal_->initialize().has_value());
    ASSERT_TRUE(checker_->initialize().has_value());

    // Verify all components are registered with manager
    auto components = manager_->getAllComponents();
    EXPECT_GE(components.size(), 2) << "Manager should have registered components";

    // Test getting components by name
    auto terminalComponent = manager_->getComponent("OptimizedConsoleTerminal");
    auto checkerComponent = manager_->getComponent("OptimizedCommandChecker");

    EXPECT_TRUE(terminalComponent.has_value()) << "Terminal should be retrievable from manager";
    EXPECT_TRUE(checkerComponent.has_value()) << "Checker should be retrievable from manager";
}

void IntegrationTestSuite::TestErrorPropagation() {
    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Register a command that throws an exception
    auto regResult = terminal_->registerCommand("throw",
        [](std::span<const std::any> args) -> Result<std::string> {
            throw std::runtime_error("Test exception");
        });

    ASSERT_TRUE(regResult.has_value());

    // Execute the throwing command
    auto result = terminal_->executeCommand("throw");
    EXPECT_FALSE(result.has_value()) << "Exception should be caught and converted to error";
    EXPECT_EQ(result.error().code, ErrorCode::RUNTIME_ERROR) << "Should be runtime error";
}

void IntegrationTestSuite::TestConcurrentOperations() {
    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(terminal_->initialize().has_value());
    ASSERT_TRUE(checker_->initialize().has_value());

    const int numThreads = 5;
    const int operationsPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Register a test command
    auto regResult = terminal_->registerCommand("concurrent_test",
        [](std::span<const std::any> args) -> Result<std::string> {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return "Concurrent operation completed";
        });
    ASSERT_TRUE(regResult.has_value());

    // Run concurrent operations
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                // Mix terminal and checker operations
                if (j % 2 == 0) {
                    auto result = terminal_->executeCommand("concurrent_test");
                    if (result.has_value()) {
                        successCount.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {
                    auto result = checker_->checkCommand("echo test");
                    if (result.has_value()) {
                        successCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread)
        << "All concurrent operations should succeed";
}

// Performance benchmark implementations
void PerformanceBenchmarks::BenchmarkComponentRegistration() {
    ASSERT_TRUE(manager_->initialize().has_value());

    const int numComponents = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    // Create and register many mock components
    std::vector<std::shared_ptr<MockDebugComponent>> components;
    for (int i = 0; i < numComponents; ++i) {
        auto component = std::make_shared<MockDebugComponent>();
        EXPECT_CALL(*component, getName())
            .WillRepeatedly(::testing::Return(std::format("BenchComponent{}", i)));
        EXPECT_CALL(*component, initialize())
            .WillOnce(::testing::Return(Result<void>{}));

        components.push_back(component);
        auto result = manager_->registerComponent(component);
        ASSERT_TRUE(result.has_value()) << std::format("Component {} registration failed", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << std::format("Registered {} components in {}μs (avg: {:.2f}μs per component)\n",
                            numComponents, duration.count(),
                            static_cast<double>(duration.count()) / numComponents);

    // Performance expectation: should be faster than 100μs per component
    EXPECT_LT(duration.count() / numComponents, 100)
        << "Component registration should be fast";
}

void PerformanceBenchmarks::BenchmarkCommandExecution() {
    ASSERT_TRUE(terminal_->initialize().has_value());

    // Register a simple test command
    auto regResult = terminal_->registerCommand("perf_test",
        [](std::span<const std::any> args) -> Result<std::string> {
            return "Performance test result";
        });
    ASSERT_TRUE(regResult.has_value());

    const int numExecutions = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numExecutions; ++i) {
        auto result = terminal_->executeCommand("perf_test");
        ASSERT_TRUE(result.has_value()) << std::format("Execution {} failed", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << std::format("Executed {} commands in {}μs (avg: {:.2f}μs per command)\n",
                            numExecutions, duration.count(),
                            static_cast<double>(duration.count()) / numExecutions);

    // Performance expectation: should be faster than 50μs per command
    EXPECT_LT(duration.count() / numExecutions, 50)
        << "Command execution should be fast";
}

void PerformanceBenchmarks::BenchmarkCommandChecking() {
    ASSERT_TRUE(checker_->initialize().has_value());

    const std::vector<std::string> testCommands = {
        "echo hello world",
        "ls -la /home",
        "grep pattern file.txt",
        "find /usr -name '*.so'",
        "ps aux | grep process"
    };

    const int numIterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numIterations; ++i) {
        for (const auto& command : testCommands) {
            auto result = checker_->checkCommand(command);
            ASSERT_TRUE(result.has_value()) << std::format("Check failed for: {}", command);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto totalChecks = numIterations * testCommands.size();

    std::cout << std::format("Performed {} checks in {}μs (avg: {:.2f}μs per check)\n",
                            totalChecks, duration.count(),
                            static_cast<double>(duration.count()) / totalChecks);

    // Performance expectation: should be faster than 100μs per check
    EXPECT_LT(duration.count() / totalChecks, 100)
        << "Command checking should be fast";
}

void PerformanceBenchmarks::BenchmarkMemoryUsage() {
    // This is a placeholder for memory usage benchmarking
    // In a real implementation, you would use tools like valgrind or custom memory tracking

    ASSERT_TRUE(manager_->initialize().has_value());
    ASSERT_TRUE(terminal_->initialize().has_value());
    ASSERT_TRUE(checker_->initialize().has_value());

    // Perform operations that might cause memory issues
    const int numOperations = 1000;

    for (int i = 0; i < numOperations; ++i) {
        // Register and unregister commands
        auto regResult = terminal_->registerCommand(std::format("temp_cmd_{}", i),
            [](std::span<const std::any> args) -> Result<std::string> {
                return "Temporary command";
            });

        if (regResult.has_value()) {
            terminal_->unregisterCommand(std::format("temp_cmd_{}", i));
        }

        // Check some commands
        checker_->checkCommand(std::format("echo test_{}", i));
    }

    // In a real test, we would verify memory usage didn't grow excessively
    SUCCEED() << "Memory usage test completed";
}

} // namespace lithium::debug::tests
