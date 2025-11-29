/**
 * @file test_terminal_components.cpp
 * @brief Comprehensive tests for terminal components
 *
 * Tests for:
 * - ConsoleRenderer
 * - InputController
 * - CommandExecutor
 * - HistoryManager
 * - TuiManager
 * - Types and Theme
 */

#include <gtest/gtest.h>
#include <chrono>
#include <sstream>
#include <thread>

#include "debug/terminal/command_executor.hpp"
#include "debug/terminal/history_manager.hpp"
#include "debug/terminal/input_controller.hpp"
#include "debug/terminal/renderer.hpp"
#include "debug/terminal/tui_manager.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// Theme Tests
// ============================================================================

class ThemeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThemeTest, DefaultThemeHasCorrectName) {
    Theme theme;
    EXPECT_EQ(theme.name, "default");
}

TEST_F(ThemeTest, DefaultThemeHasUnicodeEnabled) {
    Theme theme;
    EXPECT_TRUE(theme.useUnicode);
    EXPECT_TRUE(theme.useColors);
}

TEST_F(ThemeTest, AsciiThemeDisablesUnicode) {
    Theme theme = Theme::ascii();
    EXPECT_EQ(theme.name, "ascii");
    EXPECT_FALSE(theme.useUnicode);
    EXPECT_EQ(theme.promptSymbol, ">");
    EXPECT_EQ(theme.successSymbol, "[OK]");
    EXPECT_EQ(theme.errorSymbol, "[ERR]");
}

TEST_F(ThemeTest, DarkThemeHasCorrectColors) {
    Theme theme = Theme::dark();
    EXPECT_EQ(theme.name, "dark");
    EXPECT_EQ(theme.promptColor, Color::BrightBlue);
}

TEST_F(ThemeTest, LightThemeHasCorrectColors) {
    Theme theme = Theme::light();
    EXPECT_EQ(theme.name, "light");
    EXPECT_EQ(theme.promptColor, Color::Blue);
}

// ============================================================================
// ConsoleRenderer Tests
// ============================================================================

class ConsoleRendererTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererTest, ConstructorWithDefaultTheme) {
    EXPECT_EQ(renderer->getTheme().name, "default");
}

TEST_F(ConsoleRendererTest, ConstructorWithCustomTheme) {
    Theme customTheme = Theme::dark();
    ConsoleRenderer customRenderer(customTheme);
    EXPECT_EQ(customRenderer.getTheme().name, "dark");
}

TEST_F(ConsoleRendererTest, SetTheme) {
    Theme newTheme = Theme::ascii();
    renderer->setTheme(newTheme);
    EXPECT_EQ(renderer->getTheme().name, "ascii");
}

TEST_F(ConsoleRendererTest, GetTerminalSize) {
    TerminalSize size = renderer->getTerminalSize();
    EXPECT_GT(size.width, 0);
    EXPECT_GT(size.height, 0);
}

TEST_F(ConsoleRendererTest, EnableDisableColors) {
    renderer->enableColors(false);
    EXPECT_FALSE(renderer->supportsColors());

    renderer->enableColors(true);
    // Note: actual support depends on terminal
}

TEST_F(ConsoleRendererTest, EnableDisableUnicode) {
    renderer->enableUnicode(false);
    EXPECT_FALSE(renderer->supportsUnicode());

    renderer->enableUnicode(true);
    // Note: actual support depends on terminal
}

TEST_F(ConsoleRendererTest, ColorCodeGeneration) {
    std::string code =
        renderer->colorCode(Color::Red, std::nullopt, Style::Bold);
    // Should contain ANSI escape sequence if colors enabled
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
    }
}

TEST_F(ConsoleRendererTest, ResetCodeGeneration) {
    std::string code = renderer->resetCode();
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
    }
}

TEST_F(ConsoleRendererTest, StripAnsi) {
    std::string text = "\033[31mRed Text\033[0m";
    std::string stripped = ConsoleRenderer::stripAnsi(text);
    EXPECT_EQ(stripped, "Red Text");
}

TEST_F(ConsoleRendererTest, VisibleLength) {
    std::string text = "\033[31mRed\033[0m";
    size_t len = ConsoleRenderer::visibleLength(text);
    EXPECT_EQ(len, 3);  // "Red" without ANSI codes
}

// ============================================================================
// InputController Tests
// ============================================================================

class InputControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        config.enableHistory = true;
        config.enableCompletion = true;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerTest, DefaultConfiguration) {
    const InputConfig& config = controller->getConfig();
    EXPECT_TRUE(config.enableHistory);
    EXPECT_TRUE(config.enableCompletion);
}

TEST_F(InputControllerTest, SetPrompt) {
    controller->setPrompt(">>> ");
    EXPECT_EQ(controller->getConfig().prompt, ">>> ");
}

TEST_F(InputControllerTest, BufferOperations) {
    controller->setBuffer("test input");
    EXPECT_EQ(controller->getBuffer(), "test input");

    controller->clearBuffer();
    EXPECT_EQ(controller->getBuffer(), "");
}

TEST_F(InputControllerTest, CursorPosition) {
    controller->setBuffer("hello world");
    controller->setCursorPosition(5);
    EXPECT_EQ(controller->getCursorPosition(), 5);
}

TEST_F(InputControllerTest, InsertText) {
    controller->setBuffer("hello");
    controller->setCursorPosition(5);
    controller->insertText(" world");
    EXPECT_EQ(controller->getBuffer(), "hello world");
}

TEST_F(InputControllerTest, DeleteChar) {
    controller->setBuffer("hello");
    controller->setCursorPosition(2);
    controller->deleteChar();
    EXPECT_EQ(controller->getBuffer(), "helo");
}

TEST_F(InputControllerTest, Backspace) {
    controller->setBuffer("hello");
    controller->setCursorPosition(5);
    controller->backspace();
    EXPECT_EQ(controller->getBuffer(), "hell");
}

TEST_F(InputControllerTest, HistoryOperations) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");

    auto history = controller->getHistory();
    EXPECT_EQ(history.size(), 2);
    EXPECT_EQ(history[0], "command1");
    EXPECT_EQ(history[1], "command2");
}

TEST_F(InputControllerTest, ClearHistory) {
    controller->addToHistory("command1");
    controller->clearHistory();
    EXPECT_TRUE(controller->getHistory().empty());
}

TEST_F(InputControllerTest, SearchHistory) {
    controller->addToHistory("git status");
    controller->addToHistory("git commit");
    controller->addToHistory("ls -la");

    auto results = controller->searchHistory("git");
    EXPECT_EQ(results.size(), 2);
}

TEST_F(InputControllerTest, CompletionHandler) {
    bool handlerCalled = false;
    controller->setCompletionHandler(
        [&handlerCalled](const std::string& text, size_t pos) {
            handlerCalled = true;
            CompletionResult result;
            result.matches = {"test1", "test2"};
            return result;
        });

    controller->setBuffer("te");
    auto completions = controller->getCompletions();
    EXPECT_TRUE(handlerCalled);
    EXPECT_EQ(completions.matches.size(), 2);
}

// ============================================================================
// CommandExecutor Tests
// ============================================================================

class CommandExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        ExecutorConfig config;
        config.defaultTimeout = std::chrono::milliseconds(1000);
        executor = std::make_unique<CommandExecutor>(config);
    }

    void TearDown() override { executor.reset(); }

    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(CommandExecutorTest, RegisterCommand) {
    CommandDef cmd;
    cmd.name = "test";
    cmd.description = "Test command";
    cmd.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        result.output = "Test output";
        return result;
    };

    executor->registerCommand(cmd);
    EXPECT_TRUE(executor->hasCommand("test"));
}

TEST_F(CommandExecutorTest, UnregisterCommand) {
    CommandDef cmd;
    cmd.name = "temp";
    cmd.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd);
    EXPECT_TRUE(executor->hasCommand("temp"));

    executor->unregisterCommand("temp");
    EXPECT_FALSE(executor->hasCommand("temp"));
}

TEST_F(CommandExecutorTest, GetCommand) {
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

TEST_F(CommandExecutorTest, GetCommands) {
    CommandDef cmd1, cmd2;
    cmd1.name = "cmd1";
    cmd1.handler = [](const std::vector<std::any>&) { return CommandResult{}; };
    cmd2.name = "cmd2";
    cmd2.handler = [](const std::vector<std::any>&) { return CommandResult{}; };

    executor->registerCommand(cmd1);
    executor->registerCommand(cmd2);

    auto commands = executor->getCommands();
    EXPECT_GE(commands.size(), 2);
}

TEST_F(CommandExecutorTest, ParseCommand) {
    auto parsed = executor->parse("echo hello world");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 2);
    EXPECT_EQ(parsed.args[0], "hello");
    EXPECT_EQ(parsed.args[1], "world");
}

TEST_F(CommandExecutorTest, ParseCommandWithQuotes) {
    auto parsed = executor->parse("echo \"hello world\"");
    EXPECT_EQ(parsed.name, "echo");
    EXPECT_EQ(parsed.args.size(), 1);
    EXPECT_EQ(parsed.args[0], "hello world");
}

TEST_F(CommandExecutorTest, ExecuteCommand) {
    CommandDef cmd;
    cmd.name = "greet";
    cmd.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        result.output = "Hello!";
        return result;
    };

    executor->registerCommand(cmd);
    auto result = executor->execute("greet");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "Hello!");
}

TEST_F(CommandExecutorTest, ExecuteUnknownCommand) {
    auto result = executor->execute("unknowncommand");
    EXPECT_FALSE(result.success);
}

TEST_F(CommandExecutorTest, RegisterBuiltins) {
    executor->registerBuiltins();
    EXPECT_TRUE(executor->hasCommand("help"));
    EXPECT_TRUE(executor->hasCommand("exit"));
    EXPECT_TRUE(executor->hasCommand("clear"));
}

TEST_F(CommandExecutorTest, SetTimeout) {
    executor->setTimeout(std::chrono::milliseconds(2000));
    EXPECT_EQ(executor->getConfig().defaultTimeout,
              std::chrono::milliseconds(2000));
}

// ============================================================================
// HistoryManager Tests
// ============================================================================

class HistoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        config.deduplicateConsecutive = true;
        manager = std::make_unique<HistoryManager>(config);
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerTest, AddEntry) {
    manager->add("command1");
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerTest, AddMultipleEntries) {
    manager->add("command1");
    manager->add("command2");
    manager->add("command3");
    EXPECT_EQ(manager->size(), 3);
}

TEST_F(HistoryManagerTest, DeduplicateConsecutive) {
    manager->add("command1");
    manager->add("command1");  // Should not be added
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerTest, GetEntry) {
    manager->add("command1");
    auto entry = manager->get(0);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "command1");
}

TEST_F(HistoryManagerTest, GetLast) {
    manager->add("first");
    manager->add("last");
    auto entry = manager->getLast();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "last");
}

TEST_F(HistoryManagerTest, Navigation) {
    manager->add("cmd1");
    manager->add("cmd2");
    manager->add("cmd3");

    manager->resetNavigation();

    auto prev1 = manager->previous();
    ASSERT_TRUE(prev1.has_value());
    EXPECT_EQ(prev1->command, "cmd3");

    auto prev2 = manager->previous();
    ASSERT_TRUE(prev2.has_value());
    EXPECT_EQ(prev2->command, "cmd2");

    auto next = manager->next();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->command, "cmd3");
}

TEST_F(HistoryManagerTest, Search) {
    manager->add("git status");
    manager->add("git commit -m 'test'");
    manager->add("ls -la");

    HistorySearchOptions options;
    options.maxResults = 10;
    auto results = manager->search("git", options);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(HistoryManagerTest, SearchPrefix) {
    manager->add("git status");
    manager->add("grep pattern");
    manager->add("git log");

    auto results = manager->searchPrefix("git", 10);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(HistoryManagerTest, Clear) {
    manager->add("command1");
    manager->add("command2");
    manager->clear();
    EXPECT_TRUE(manager->empty());
}

TEST_F(HistoryManagerTest, Remove) {
    manager->add("command1");
    manager->add("command2");
    manager->remove(0);
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerTest, Favorites) {
    manager->add("favorite_cmd");
    manager->setFavorite(0, true);

    auto favorites = manager->getFavorites();
    EXPECT_EQ(favorites.size(), 1);
    EXPECT_EQ(favorites[0].command, "favorite_cmd");
}

TEST_F(HistoryManagerTest, Tags) {
    manager->add("tagged_cmd");
    manager->addTag(0, "important");

    auto tagged = manager->getByTag("important");
    EXPECT_EQ(tagged.size(), 1);
}

TEST_F(HistoryManagerTest, GetRecent) {
    manager->add("cmd1");
    manager->add("cmd2");
    manager->add("cmd3");

    auto recent = manager->getRecent(2);
    EXPECT_EQ(recent.size(), 2);
    EXPECT_EQ(recent[0].command, "cmd3");
    EXPECT_EQ(recent[1].command, "cmd2");
}

// ============================================================================
// TuiManager Tests
// ============================================================================

class TuiManagerTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerTest, CheckAvailability) {
    // Just check that the function doesn't crash
    bool available = TuiManager::isAvailable();
    // Result depends on system configuration
    (void)available;
}

TEST_F(TuiManagerTest, DefaultLayoutConfig) {
    const LayoutConfig& layout = tui->getLayout();
    EXPECT_TRUE(layout.showStatusBar);
    EXPECT_TRUE(layout.showSuggestions);
}

TEST_F(TuiManagerTest, SetLayout) {
    LayoutConfig newLayout;
    newLayout.showStatusBar = false;
    newLayout.showHistory = true;

    tui->setLayout(newLayout);

    const LayoutConfig& layout = tui->getLayout();
    EXPECT_FALSE(layout.showStatusBar);
    EXPECT_TRUE(layout.showHistory);
}

TEST_F(TuiManagerTest, SetTheme) {
    Theme theme = Theme::dark();
    tui->setTheme(theme);
    // Theme should be applied without error
}

TEST_F(TuiManagerTest, FallbackMode) {
    tui->setFallbackMode(true);
    EXPECT_TRUE(tui->isFallbackMode());

    tui->setFallbackMode(false);
    EXPECT_FALSE(tui->isFallbackMode());
}

TEST_F(TuiManagerTest, InputOperations) {
    tui->setInput("test input");
    EXPECT_EQ(tui->getInput(), "test input");

    tui->clearInput();
    EXPECT_EQ(tui->getInput(), "");
}

TEST_F(TuiManagerTest, PromptSetting) {
    tui->setPrompt(">>> ");
    // Prompt should be set without error
}

// ============================================================================
// Integration Tests
// ============================================================================

class TerminalIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<ConsoleRenderer>();

        InputConfig inputConfig;
        input = std::make_unique<InputController>(inputConfig);

        HistoryConfig historyConfig;
        history = std::make_unique<HistoryManager>(historyConfig);

        ExecutorConfig execConfig;
        executor = std::make_unique<CommandExecutor>(execConfig);
    }

    void TearDown() override {
        renderer.reset();
        input.reset();
        history.reset();
        executor.reset();
    }

    std::unique_ptr<ConsoleRenderer> renderer;
    std::unique_ptr<InputController> input;
    std::unique_ptr<HistoryManager> history;
    std::unique_ptr<CommandExecutor> executor;
};

TEST_F(TerminalIntegrationTest, CommandExecutionWithHistory) {
    // Register a test command
    CommandDef cmd;
    cmd.name = "test";
    cmd.handler = [](const std::vector<std::any>&) {
        CommandResult result;
        result.success = true;
        return result;
    };
    executor->registerCommand(cmd);

    // Execute command
    auto result = executor->execute("test");
    EXPECT_TRUE(result.success);

    // Add to history
    history->add("test");
    EXPECT_EQ(history->size(), 1);
}

TEST_F(TerminalIntegrationTest, InputWithCompletion) {
    // Set up completion handler
    input->setCompletionHandler([](const std::string& text, size_t) {
        CompletionResult result;
        if (text.find("he") == 0) {
            result.matches = {"help", "hello"};
        }
        return result;
    });

    input->setBuffer("he");
    auto completions = input->getCompletions();
    EXPECT_EQ(completions.matches.size(), 2);
}

TEST_F(TerminalIntegrationTest, ThemeConsistency) {
    Theme theme = Theme::dark();
    renderer->setTheme(theme);

    // Verify theme is applied
    EXPECT_EQ(renderer->getTheme().name, "dark");
    EXPECT_EQ(renderer->getTheme().promptColor, Color::BrightBlue);
}

}  // namespace lithium::debug::terminal::test
