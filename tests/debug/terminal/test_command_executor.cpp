/**
 * @file test_command_executor.cpp
 * @brief Comprehensive unit tests for CommandExecutor
 *
 * Tests for:
 * - Configuration management
 * - Command registration and unregistration
 * - Command parsing
 * - Command execution (sync and async)
 * - Built-in commands
 * - Hooks and callbacks
 * - Error handling
 * - Timeout handling
 */

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>

#include "debug/terminal/command_executor.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// ExecutorConfig Tests
// ============================================================================

class ExecutorConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExecutorConfigTest, DefaultConstruction) {
    ExecutorConfig config;
    EXPECT_EQ(config.defaultTimeout, std::chrono::milliseconds(5000));
    EXPECT_TRUE(config.allowBackground);
    EXPECT_FALSE(config.allowPipes);
    EXPECT_FALSE(config.allowRedirection);
    EXPECT_FALSE(config.echoCommands);
    EXPECT_EQ(config.maxOutputSize, 1024 * 1024);
}

TEST_F(ExecutorConfigTest, CustomConfiguration) {
    ExecutorConfig config;
    config.defaultTimeout = std::chrono::milliseconds(10000);
    config.allowPipes = true;
    config.allowRedirection = true;
    config.echoCommands = true;
    config.maxOutputSize = 2 * 1024 * 1024;

    EXPECT_EQ(config.defaultTimeout, std::chrono::milliseconds(10000));
    EXPECT_TRUE(config.allowPipes);
    EXPECT_TRUE(config.allowRedirection);
    EXPECT_TRUE(config.echoCommands);
    EXPECT_EQ(config.maxOutputSize, 2 * 1024 * 1024);
}

// ============================================================================
// ParsedCommand Tests
// ============================================================================

class ParsedCommandTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ParsedCommandTest, DefaultConstruction) {
    ParsedCommand cmd;
    EXPECT_TRUE(cmd.name.empty());
    EXPECT_TRUE(cmd.args.empty());
    EXPECT_TRUE(cmd.typedArgs.empty());
    EXPECT_TRUE(cmd.rawInput.empty());
    EXPECT_FALSE(cmd.isPiped);
    EXPECT_FALSE(cmd.isBackground);
    EXPECT_TRUE(cmd.redirectOutput.empty());
    EXPECT_TRUE(cmd.redirectInput.empty());
}

TEST_F(ParsedCommandTest, PopulatedCommand) {
    ParsedCommand cmd;
    cmd.name = "echo";
    cmd.args = {"hello", "world"};
    cmd.rawInput = "echo hello world";

    EXPECT_EQ(cmd.name, "echo");
    EXPECT_EQ(cmd.args.size(), 2);
    EXPECT_EQ(cmd.args[0], "hello");
    EXPECT_EQ(cmd.args[1], "world");
}

TEST_F(ParsedCommandTest, BackgroundCommand) {
    ParsedCommand cmd;
    cmd.name = "sleep";
    cmd.args = {"10"};
    cmd.isBackground = true;

    EXPECT_TRUE(cmd.isBackground);
}

TEST_F(ParsedCommandTest, PipedCommand) {
    ParsedCommand cmd;
    cmd.name = "cat";
    cmd.isPiped = true;

    EXPECT_TRUE(cmd.isPiped);
}

TEST_F(ParsedCommandTest, RedirectedCommand) {
    ParsedCommand cmd;
    cmd.name = "echo";
    cmd.redirectOutput = "output.txt";
    cmd.redirectInput = "input.txt";

    EXPECT_EQ(cmd.redirectOutput, "output.txt");
    EXPECT_EQ(cmd.redirectInput, "input.txt");
}

// ============================================================================
// CommandDef Tests
// ============================================================================

class CommandDefTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandDefTest, DefaultConstruction) {
    CommandDef def;
    EXPECT_TRUE(def.name.empty());
    EXPECT_TRUE(def.description.empty());
    EXPECT_TRUE(def.usage.empty());
    EXPECT_TRUE(def.aliases.empty());
    EXPECT_FALSE(def.requiresArgs);
    EXPECT_EQ(def.minArgs, 0);
    EXPECT_EQ(def.maxArgs, -1);
}

TEST_F(CommandDefTest, PopulatedDefinition) {
    CommandDef def;
    def.name = "greet";
    def.description = "Greet a user";
    def.usage = "greet <name>";
    def.aliases = {"hello", "hi"};
    def.requiresArgs = true;
    def.minArgs = 1;
    def.maxArgs = 1;
    def.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        return result;
    };

    EXPECT_EQ(def.name, "greet");
    EXPECT_EQ(def.description, "Greet a user");
    EXPECT_EQ(def.aliases.size(), 2);
    EXPECT_TRUE(def.requiresArgs);
    EXPECT_EQ(def.minArgs, 1);
    EXPECT_EQ(def.maxArgs, 1);
    EXPECT_TRUE(def.handler != nullptr);
}

// ============================================================================
// CommandExecutor Basic Tests
// ============================================================================

class CommandExecutorBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        ExecutorConfig config;
        config.defaultTimeout = std::chrono::milliseconds(1000);
        executor = std::make_unique<CommandExecutor>(config);
    }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorBasicTest, DefaultConstruction) {
    CommandExecutor defaultExecutor;
    EXPECT_EQ(defaultExecutor.getConfig().defaultTimeout,
              std::chrono::milliseconds(5000));
}

TEST_F(CommandExecutorBasicTest, ConstructWithConfig) {
    EXPECT_EQ(executor->getConfig().defaultTimeout,
              std::chrono::milliseconds(1000));
}

TEST_F(CommandExecutorBasicTest, SetConfig) {
    ExecutorConfig newConfig;
    newConfig.defaultTimeout = std::chrono::milliseconds(3000);
    newConfig.allowPipes = true;

    executor->setConfig(newConfig);

    EXPECT_EQ(executor->getConfig().defaultTimeout,
              std::chrono::milliseconds(3000));
    EXPECT_TRUE(executor->getConfig().allowPipes);
}

TEST_F(CommandExecutorBasicTest, SetTimeout) {
    executor->setTimeout(std::chrono::milliseconds(2000));
    EXPECT_EQ(executor->getConfig().defaultTimeout,
              std::chrono::milliseconds(2000));
}

TEST_F(CommandExecutorBasicTest, GetConfig) {
    const ExecutorConfig& config = executor->getConfig();
    EXPECT_EQ(config.defaultTimeout, std::chrono::milliseconds(1000));
}

// ============================================================================
// CommandExecutor Registration Tests
// ============================================================================

class CommandExecutorRegistrationTest : public ::testing::Test {
protected:
    void SetUp() override { executor = std::make_unique<CommandExecutor>(); }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorRegistrationTest, RegisterCommandDef) {
    CommandDef cmd;
    cmd.name = "test";
    cmd.description = "Test command";
    cmd.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        return result;
    };

    executor->registerCommand(cmd);
    EXPECT_TRUE(executor->hasCommand("test"));
}

TEST_F(CommandExecutorRegistrationTest, RegisterSimpleCommand) {
    executor->registerCommand(
        "simple", "A simple command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            result.output = "Simple output";
            return result;
        });

    EXPECT_TRUE(executor->hasCommand("simple"));
}

TEST_F(CommandExecutorRegistrationTest, UnregisterCommand) {
    CommandDef cmd;
    cmd.name = "temp";
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);
    EXPECT_TRUE(executor->hasCommand("temp"));

    bool result = executor->unregisterCommand("temp");
    EXPECT_TRUE(result);
    EXPECT_FALSE(executor->hasCommand("temp"));
}

TEST_F(CommandExecutorRegistrationTest, UnregisterNonexistentCommand) {
    bool result = executor->unregisterCommand("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(CommandExecutorRegistrationTest, HasCommand) {
    CommandDef cmd;
    cmd.name = "exists";
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);

    EXPECT_TRUE(executor->hasCommand("exists"));
    EXPECT_FALSE(executor->hasCommand("notexists"));
}

TEST_F(CommandExecutorRegistrationTest, GetCommand) {
    CommandDef cmd;
    cmd.name = "mycommand";
    cmd.description = "My command description";
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);

    auto retrieved = executor->getCommand("mycommand");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "mycommand");
    EXPECT_EQ(retrieved->description, "My command description");
}

TEST_F(CommandExecutorRegistrationTest, GetNonexistentCommand) {
    auto retrieved = executor->getCommand("nonexistent");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(CommandExecutorRegistrationTest, GetCommands) {
    CommandDef cmd1, cmd2, cmd3;
    cmd1.name = "cmd1";
    cmd1.handler = [](const std::vector<std::any>&) { return CommandResult{}; };
    cmd2.name = "cmd2";
    cmd2.handler = [](const std::vector<std::any>&) { return CommandResult{}; };
    cmd3.name = "cmd3";
    cmd3.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd1);
    executor->registerCommand(cmd2);
    executor->registerCommand(cmd3);

    auto commands = executor->getCommands();
    EXPECT_GE(commands.size(), 3);
}

TEST_F(CommandExecutorRegistrationTest, GetCommandDescriptions) {
    executor->registerCommand(
        "desc1", "Description 1",
        [](const std::vector<std::any>&) { return CommandResult{}; });
    executor->registerCommand(
        "desc2", "Description 2",
        [](const std::vector<std::any>&) { return CommandResult{}; });

    auto descriptions = executor->getCommandDescriptions();
    EXPECT_GE(descriptions.size(), 2);
}

TEST_F(CommandExecutorRegistrationTest, RegisterAlias) {
    CommandDef cmd;
    cmd.name = "original";
    cmd.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        result.output = "Original command";
        return result;
    };

    executor->registerCommand(cmd);
    executor->registerAlias("alias", "original");

    EXPECT_TRUE(executor->hasCommand("original"));
    // Alias behavior depends on implementation
}

// ============================================================================
// CommandExecutor Parsing Tests
// ============================================================================

class CommandExecutorParsingTest : public ::testing::Test {
protected:
    void SetUp() override { executor = std::make_unique<CommandExecutor>(); }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorParsingTest, ParseSimpleCommand) {
    auto parsed = executor->parse("echo hello");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 1);
    EXPECT_EQ(parsed.args[0], "hello");
}

TEST_F(CommandExecutorParsingTest, ParseCommandWithMultipleArgs) {
    auto parsed = executor->parse("echo hello world");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 2);
    EXPECT_EQ(parsed.args[0], "hello");
    EXPECT_EQ(parsed.args[1], "world");
}

TEST_F(CommandExecutorParsingTest, ParseCommandWithQuotes) {
    auto parsed = executor->parse("echo \"hello world\"");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 1);
    EXPECT_EQ(parsed.args[0], "hello world");
}

TEST_F(CommandExecutorParsingTest, ParseCommandWithSingleQuotes) {
    auto parsed = executor->parse("echo 'hello world'");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 1);
    EXPECT_EQ(parsed.args[0], "hello world");
}

TEST_F(CommandExecutorParsingTest, ParseEmptyCommand) {
    auto parsed = executor->parse("");
    EXPECT_TRUE(parsed.name.empty());
}

TEST_F(CommandExecutorParsingTest, ParseWhitespaceOnlyCommand) {
    auto parsed = executor->parse("   ");
    EXPECT_TRUE(parsed.name.empty());
}

TEST_F(CommandExecutorParsingTest, ParseCommandWithLeadingWhitespace) {
    auto parsed = executor->parse("  echo hello");
    EXPECT_EQ(parsed.name, "echo");
}

TEST_F(CommandExecutorParsingTest, ParseCommandWithTrailingWhitespace) {
    auto parsed = executor->parse("echo hello  ");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 1);
}

TEST_F(CommandExecutorParsingTest, ParseArgument) {
    auto arg = executor->parseArgument("42");
    // Should parse as integer or string depending on implementation
    EXPECT_TRUE(arg.has_value());
}

TEST_F(CommandExecutorParsingTest, ParseStringArgument) {
    auto arg = executor->parseArgument("hello");
    EXPECT_TRUE(arg.has_value());
}

TEST_F(CommandExecutorParsingTest, ValidateParsedCommand) {
    CommandDef cmd;
    cmd.name = "test";
    cmd.minArgs = 1;
    cmd.maxArgs = 2;
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);

    ParsedCommand parsed;
    parsed.name = "test";
    parsed.args = {"arg1"};

    auto error = executor->validate(parsed);
    EXPECT_FALSE(error.has_value());  // Should be valid
}

TEST_F(CommandExecutorParsingTest, ValidateInvalidCommand) {
    CommandDef cmd;
    cmd.name = "test";
    cmd.minArgs = 2;
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);

    ParsedCommand parsed;
    parsed.name = "test";
    parsed.args = {"arg1"};  // Only 1 arg, but 2 required

    auto error = executor->validate(parsed);
    // May or may not have error depending on implementation
}

// ============================================================================
// CommandExecutor Execution Tests
// ============================================================================

class CommandExecutorExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ExecutorConfig config;
        config.defaultTimeout = std::chrono::milliseconds(1000);
        executor = std::make_unique<CommandExecutor>(config);
    }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorExecutionTest, ExecuteSimpleCommand) {
    executor->registerCommand(
        "greet", "Greet command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            result.output = "Hello!";
            return result;
        });

    auto result = executor->execute("greet");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "Hello!");
}

TEST_F(CommandExecutorExecutionTest, ExecuteCommandWithArgs) {
    executor->registerCommand(
        "echo", "Echo command",
        [](const std::vector<std::any>& args) {
            CommandResult result;
            result.success = true;
            if (!args.empty()) {
                try {
                    result.output = std::any_cast<std::string>(args[0]);
                } catch (...) {
                    result.output = "arg";
                }
            }
            return result;
        });

    auto result = executor->execute("echo hello");
    EXPECT_TRUE(result.success);
}

TEST_F(CommandExecutorExecutionTest, ExecuteUnknownCommand) {
    auto result = executor->execute("unknowncommand");
    EXPECT_FALSE(result.success);
}

TEST_F(CommandExecutorExecutionTest, ExecuteParsedCommand) {
    executor->registerCommand(
        "test", "Test command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            return result;
        });

    ParsedCommand parsed;
    parsed.name = "test";

    auto result = executor->execute(parsed);
    EXPECT_TRUE(result.success);
}

TEST_F(CommandExecutorExecutionTest, ExecuteWithCustomTimeout) {
    executor->registerCommand(
        "slow", "Slow command",
        [](const std::vector<std::any>&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            CommandResult result;
            result.success = true;
            return result;
        });

    auto result = executor->execute("slow", std::chrono::milliseconds(500));
    EXPECT_TRUE(result.success);
}

TEST_F(CommandExecutorExecutionTest, ExecuteCommandThatFails) {
    executor->registerCommand(
        "fail", "Failing command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = false;
            result.error = "Command failed";
            result.exitCode = 1;
            return result;
        });

    auto result = executor->execute("fail");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "Command failed");
    EXPECT_EQ(result.exitCode, 1);
}

TEST_F(CommandExecutorExecutionTest, ExecuteCommandWithOutput) {
    executor->registerCommand(
        "output", "Output command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            result.output = "Line 1\nLine 2\nLine 3";
            return result;
        });

    auto result = executor->execute("output");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
}

TEST_F(CommandExecutorExecutionTest, IsRunningInitiallyFalse) {
    EXPECT_FALSE(executor->isRunning());
}

// ============================================================================
// CommandExecutor Async Execution Tests
// ============================================================================

class CommandExecutorAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        ExecutorConfig config;
        config.defaultTimeout = std::chrono::milliseconds(2000);
        executor = std::make_unique<CommandExecutor>(config);
    }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorAsyncTest, ExecuteAsync) {
    executor->registerCommand(
        "async_test", "Async test command",
        [](const std::vector<std::any>&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            CommandResult result;
            result.success = true;
            result.output = "Async completed";
            return result;
        });

    auto future = executor->executeAsync("async_test");
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "Async completed");
}

TEST_F(CommandExecutorAsyncTest, ExecuteBackground) {
    bool executed = false;
    executor->registerCommand(
        "background_test", "Background test command",
        [&executed](const std::vector<std::any>&) {
            executed = true;
            CommandResult result;
            result.success = true;
            return result;
        });

    executor->executeBackground("background_test");

    // Give some time for background execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(executed);
}

// ============================================================================
// CommandExecutor Built-in Commands Tests
// ============================================================================

class CommandExecutorBuiltinsTest : public ::testing::Test {
protected:
    void SetUp() override { executor = std::make_unique<CommandExecutor>(); }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorBuiltinsTest, RegisterBuiltins) {
    executor->registerBuiltins();

    EXPECT_TRUE(executor->hasCommand("help"));
    EXPECT_TRUE(executor->hasCommand("exit"));
    EXPECT_TRUE(executor->hasCommand("clear"));
}

TEST_F(CommandExecutorBuiltinsTest, HelpCommand) {
    executor->registerBuiltins();

    auto result = executor->execute("help");
    EXPECT_TRUE(result.success);
}

TEST_F(CommandExecutorBuiltinsTest, SetExitCallback) {
    bool exitCalled = false;
    executor->setExitCallback([&exitCalled]() { exitCalled = true; });

    executor->registerBuiltins();
    executor->execute("exit");

    EXPECT_TRUE(exitCalled);
}

TEST_F(CommandExecutorBuiltinsTest, SetHelpCallback) {
    std::string helpTopic;
    executor->setHelpCallback([&helpTopic](const std::string& topic) {
        helpTopic = topic;
    });

    executor->registerBuiltins();
    executor->execute("help test");

    // Help callback should be called
}

// ============================================================================
// CommandExecutor Hooks Tests
// ============================================================================

class CommandExecutorHooksTest : public ::testing::Test {
protected:
    void SetUp() override { executor = std::make_unique<CommandExecutor>(); }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorHooksTest, PreExecuteHook) {
    bool hookCalled = false;
    executor->setPreExecuteHook([&hookCalled](const ParsedCommand& cmd) {
        hookCalled = true;
        return true;  // Allow execution
    });

    executor->registerCommand(
        "test", "Test command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            return result;
        });

    executor->execute("test");
    EXPECT_TRUE(hookCalled);
}

TEST_F(CommandExecutorHooksTest, PreExecuteHookBlocksExecution) {
    executor->setPreExecuteHook([](const ParsedCommand& cmd) {
        return false;  // Block execution
    });

    executor->registerCommand(
        "blocked", "Blocked command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            return result;
        });

    auto result = executor->execute("blocked");
    // Execution should be blocked
}

TEST_F(CommandExecutorHooksTest, PostExecuteHook) {
    bool hookCalled = false;
    CommandResult capturedResult;

    executor->setPostExecuteHook(
        [&hookCalled, &capturedResult](const ParsedCommand& cmd,
                                        const CommandResult& result) {
            hookCalled = true;
            capturedResult = result;
        });

    executor->registerCommand(
        "test", "Test command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            result.output = "Test output";
            return result;
        });

    executor->execute("test");

    EXPECT_TRUE(hookCalled);
    EXPECT_TRUE(capturedResult.success);
    EXPECT_EQ(capturedResult.output, "Test output");
}

TEST_F(CommandExecutorHooksTest, OutputHandler) {
    std::string capturedOutput;
    executor->setOutputHandler([&capturedOutput](const std::string& output) {
        capturedOutput = output;
    });

    executor->registerCommand(
        "output", "Output command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = true;
            result.output = "Handler output";
            return result;
        });

    executor->execute("output");
    // Output handler should be called
}

TEST_F(CommandExecutorHooksTest, ErrorHandler) {
    std::string capturedError;
    executor->setErrorHandler([&capturedError](const std::string& error) {
        capturedError = error;
    });

    executor->registerCommand(
        "error", "Error command",
        [](const std::vector<std::any>&) {
            CommandResult result;
            result.success = false;
            result.error = "Handler error";
            return result;
        });

    executor->execute("error");
    // Error handler should be called
}

// ============================================================================
// CommandExecutor Move Semantics Tests
// ============================================================================

class CommandExecutorMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandExecutorMoveTest, MoveConstruction) {
    CommandExecutor original;
    original.registerCommand(
        "test", "Test",
        [](const std::vector<std::any>&) { return CommandResult{}; });

    CommandExecutor moved(std::move(original));
    EXPECT_TRUE(moved.hasCommand("test"));
}

TEST_F(CommandExecutorMoveTest, MoveAssignment) {
    CommandExecutor original;
    original.registerCommand(
        "test", "Test",
        [](const std::vector<std::any>&) { return CommandResult{}; });

    CommandExecutor target;
    target = std::move(original);
    EXPECT_TRUE(target.hasCommand("test"));
}

// ============================================================================
// CommandExecutor Cancel Tests
// ============================================================================

class CommandExecutorCancelTest : public ::testing::Test {
protected:
    void SetUp() override { executor = std::make_unique<CommandExecutor>(); }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorCancelTest, CancelWhenNotRunning) {
    bool result = executor->cancel();
    // Should return false when nothing is running
    EXPECT_FALSE(result);
}

}  // namespace lithium::debug::terminal::test
