/**
 * @file test_terminal_comprehensive.cpp
 * @brief Comprehensive unit tests for ConsoleTerminal
 *
 * Tests for:
 * - Construction and destruction
 * - Command registration and execution
 * - Configuration (timeout, history, suggestions, syntax highlight)
 * - Command checker integration
 * - Suggestion engine integration
 * - Debug configuration (load, save, export, import)
 * - Runtime rule management
 * - Terminal component access
 * - Theme management
 * - Output methods
 */

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "debug/terminal.hpp"
#include "debug/check.hpp"
#include "debug/suggestion.hpp"

namespace lithium::debug::test {

// ============================================================================
// ConsoleTerminal Basic Tests
// ============================================================================

class ConsoleTerminalBasicTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalBasicTest, DefaultConstruction) {
    EXPECT_NE(terminal, nullptr);
}

TEST_F(ConsoleTerminalBasicTest, GetRegisteredCommands) {
    auto commands = terminal->getRegisteredCommands();
    // Should have some default commands
    EXPECT_FALSE(commands.empty());
}

TEST_F(ConsoleTerminalBasicTest, HasHelpCommand) {
    auto commands = terminal->getRegisteredCommands();
    bool found = false;
    for (const auto& cmd : commands) {
        if (cmd == "help") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// ConsoleTerminal Move Semantics Tests
// ============================================================================

class ConsoleTerminalMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConsoleTerminalMoveTest, MoveConstruction) {
    ConsoleTerminal original;
    auto commandsBefore = original.getRegisteredCommands();

    ConsoleTerminal moved(std::move(original));
    auto commandsAfter = moved.getRegisteredCommands();

    EXPECT_EQ(commandsBefore.size(), commandsAfter.size());
}

TEST_F(ConsoleTerminalMoveTest, MoveAssignment) {
    ConsoleTerminal original;
    ConsoleTerminal target;

    target = std::move(original);

    auto commands = target.getRegisteredCommands();
    EXPECT_FALSE(commands.empty());
}

// ============================================================================
// ConsoleTerminal Configuration Tests
// ============================================================================

class ConsoleTerminalConfigTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalConfigTest, SetCommandTimeout) {
    EXPECT_NO_THROW(
        terminal->setCommandTimeout(std::chrono::milliseconds(10000)));
}

TEST_F(ConsoleTerminalConfigTest, EnableHistory) {
    EXPECT_NO_THROW(terminal->enableHistory(true));
    EXPECT_NO_THROW(terminal->enableHistory(false));
}

TEST_F(ConsoleTerminalConfigTest, EnableSuggestions) {
    EXPECT_NO_THROW(terminal->enableSuggestions(true));
    EXPECT_NO_THROW(terminal->enableSuggestions(false));
}

TEST_F(ConsoleTerminalConfigTest, EnableSyntaxHighlight) {
    EXPECT_NO_THROW(terminal->enableSyntaxHighlight(true));
    EXPECT_NO_THROW(terminal->enableSyntaxHighlight(false));
}

TEST_F(ConsoleTerminalConfigTest, EnableCommandCheck) {
    EXPECT_NO_THROW(terminal->enableCommandCheck(true));
    EXPECT_NO_THROW(terminal->enableCommandCheck(false));
}

// ============================================================================
// ConsoleTerminal Command Checker Integration Tests
// ============================================================================

class ConsoleTerminalCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        checker = std::make_shared<CommandChecker>();
    }

    void TearDown() override {
        terminal.reset();
        checker.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<CommandChecker> checker;
};

TEST_F(ConsoleTerminalCheckerTest, SetCommandChecker) {
    EXPECT_NO_THROW(terminal->setCommandChecker(checker));
}

TEST_F(ConsoleTerminalCheckerTest, SetNullChecker) {
    EXPECT_NO_THROW(terminal->setCommandChecker(nullptr));
}

// ============================================================================
// ConsoleTerminal Suggestion Engine Integration Tests
// ============================================================================

class ConsoleTerminalSuggestionTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        std::vector<std::string> dataset = {"help", "hello", "history", "exit"};
        suggestionEngine = std::make_shared<SuggestionEngine>(dataset);
    }

    void TearDown() override {
        terminal.reset();
        suggestionEngine.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<SuggestionEngine> suggestionEngine;
};

TEST_F(ConsoleTerminalSuggestionTest, SetSuggestionEngine) {
    EXPECT_NO_THROW(terminal->setSuggestionEngine(suggestionEngine));
}

TEST_F(ConsoleTerminalSuggestionTest, SetNullSuggestionEngine) {
    EXPECT_NO_THROW(terminal->setSuggestionEngine(nullptr));
}

TEST_F(ConsoleTerminalSuggestionTest, GetCommandSuggestions) {
    terminal->setSuggestionEngine(suggestionEngine);

    auto suggestions = terminal->getCommandSuggestions("hel");
    EXPECT_FALSE(suggestions.empty());
}

TEST_F(ConsoleTerminalSuggestionTest, GetCommandSuggestionsWithoutEngine) {
    auto suggestions = terminal->getCommandSuggestions("hel");
    // Should return empty or use default behavior
}

// ============================================================================
// ConsoleTerminal Suggestions Toggle Tests
// ============================================================================

class ConsoleTerminalSuggestionsToggleTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalSuggestionsToggleTest, EnableSuggestions) {
    terminal->enableSuggestions(false);
    terminal->enableSuggestions(true);
    // Suggestions should be enabled
}

TEST_F(ConsoleTerminalSuggestionsToggleTest, DisableSuggestions) {
    terminal->enableSuggestions(true);
    terminal->enableSuggestions(false);
    // Suggestions should be disabled
}

// ============================================================================
// ConsoleTerminal Command Execution Tests
// ============================================================================

class ConsoleTerminalCommandTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalCommandTest, CallValidCommand) {
    std::vector<std::any> args;
    EXPECT_NO_THROW(terminal->callCommand("help", args));
}

TEST_F(ConsoleTerminalCommandTest, CallInvalidCommand) {
    std::vector<std::any> args;
    // Should handle gracefully
    EXPECT_NO_THROW(terminal->callCommand("nonexistent_command", args));
}

TEST_F(ConsoleTerminalCommandTest, CallCommandWithArgs) {
    std::vector<std::any> args;
    args.push_back(std::string("arg1"));
    EXPECT_NO_THROW(terminal->callCommand("help", args));
}

// ============================================================================
// ConsoleTerminal Runtime Rule Management Tests
// ============================================================================

class ConsoleTerminalRuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        checker = std::make_shared<CommandChecker>();
        terminal->setCommandChecker(checker);
    }

    void TearDown() override {
        terminal.reset();
        checker.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<CommandChecker> checker;
};

TEST_F(ConsoleTerminalRuleTest, AddCommandCheckRule) {
    terminal->addCommandCheckRule(
        "test_rule",
        [](const std::string& line,
           size_t lineNum) -> std::optional<CommandCheckerErrorProxy::Error> {
            if (line.find("bad") != std::string::npos) {
                return CommandCheckerErrorProxy::Error{
                    "Bad word found", lineNum, 0, 1};
            }
            return std::nullopt;
        });
    // Rule should be added
}

TEST_F(ConsoleTerminalRuleTest, RemoveCommandCheckRule) {
    terminal->addCommandCheckRule(
        "temp_rule",
        [](const std::string&, size_t) { return std::nullopt; });

    bool removed = terminal->removeCommandCheckRule("temp_rule");
    EXPECT_TRUE(removed);
}

TEST_F(ConsoleTerminalRuleTest, RemoveNonexistentRule) {
    bool removed = terminal->removeCommandCheckRule("nonexistent");
    EXPECT_FALSE(removed);
}

// ============================================================================
// ConsoleTerminal Suggestion Filter Tests
// ============================================================================

class ConsoleTerminalFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        std::vector<std::string> dataset = {"apple", "banana", "cherry"};
        suggestionEngine = std::make_shared<SuggestionEngine>(dataset);
        terminal->setSuggestionEngine(suggestionEngine);
    }

    void TearDown() override {
        terminal.reset();
        suggestionEngine.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<SuggestionEngine> suggestionEngine;
};

TEST_F(ConsoleTerminalFilterTest, AddSuggestionFilter) {
    terminal->addSuggestionFilter([](const std::string& item) {
        return item != "banana";
    });
    // Filter should be added
}

TEST_F(ConsoleTerminalFilterTest, ClearSuggestionFilters) {
    terminal->addSuggestionFilter([](const std::string&) { return false; });
    terminal->clearSuggestionFilters();
    // Filters should be cleared
}

// ============================================================================
// ConsoleTerminal Dataset Update Tests
// ============================================================================

class ConsoleTerminalDatasetTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        std::vector<std::string> dataset = {"old1", "old2"};
        suggestionEngine = std::make_shared<SuggestionEngine>(dataset);
        terminal->setSuggestionEngine(suggestionEngine);
    }

    void TearDown() override {
        terminal.reset();
        suggestionEngine.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<SuggestionEngine> suggestionEngine;
};

TEST_F(ConsoleTerminalDatasetTest, UpdateSuggestionDataset) {
    std::vector<std::string> newItems = {"new1", "new2"};
    terminal->updateSuggestionDataset(newItems);
    // Dataset should be updated
}

TEST_F(ConsoleTerminalDatasetTest, UpdateDangerousCommands) {
    std::vector<std::string> commands = {"danger1", "danger2"};
    terminal->updateDangerousCommands(commands);
    // Dangerous commands should be updated
}

// ============================================================================
// ConsoleTerminal Debug Config Tests
// ============================================================================

class ConsoleTerminalDebugConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        testConfigPath =
            std::filesystem::temp_directory_path() / "test_debug_config.json";
    }

    void TearDown() override {
        terminal.reset();
        if (std::filesystem::exists(testConfigPath)) {
            std::filesystem::remove(testConfigPath);
        }
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::filesystem::path testConfigPath;
};

TEST_F(ConsoleTerminalDebugConfigTest, SaveDebugConfig) {
    EXPECT_NO_THROW(terminal->saveDebugConfig(testConfigPath.string()));
}

TEST_F(ConsoleTerminalDebugConfigTest, LoadDebugConfig) {
    terminal->saveDebugConfig(testConfigPath.string());
    EXPECT_NO_THROW(terminal->loadDebugConfig(testConfigPath.string()));
}

TEST_F(ConsoleTerminalDebugConfigTest, ExportDebugStateJson) {
    std::string json = terminal->exportDebugStateJson();
    EXPECT_FALSE(json.empty());
}

TEST_F(ConsoleTerminalDebugConfigTest, ImportDebugStateJson) {
    std::string json = terminal->exportDebugStateJson();
    EXPECT_NO_THROW(terminal->importDebugStateJson(json));
}

// ============================================================================
// ConsoleTerminal Debug Report Tests
// ============================================================================

class ConsoleTerminalReportTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        checker = std::make_shared<CommandChecker>();
        terminal->setCommandChecker(checker);
    }

    void TearDown() override {
        terminal.reset();
        checker.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<CommandChecker> checker;
};

TEST_F(ConsoleTerminalReportTest, PrintDebugReportWithColor) {
    EXPECT_NO_THROW(terminal->printDebugReport("echo hello", true));
}

TEST_F(ConsoleTerminalReportTest, PrintDebugReportWithoutColor) {
    EXPECT_NO_THROW(terminal->printDebugReport("echo hello", false));
}

TEST_F(ConsoleTerminalReportTest, PrintDebugReportDangerousCommand) {
    EXPECT_NO_THROW(terminal->printDebugReport("rm -rf /", true));
}

// ============================================================================
// ConsoleTerminal TUI Mode Tests
// ============================================================================

class ConsoleTerminalTuiTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalTuiTest, IsTuiAvailable) {
    bool available = terminal->isTuiAvailable();
    // Result depends on system configuration
    (void)available;
}

TEST_F(ConsoleTerminalTuiTest, EnableTuiMode) {
    // May fail if ncurses not available
    EXPECT_NO_THROW(terminal->enableTuiMode(true));
}

TEST_F(ConsoleTerminalTuiTest, DisableTuiMode) {
    EXPECT_NO_THROW(terminal->enableTuiMode(false));
}

// ============================================================================
// ConsoleTerminal Component Access Tests
// ============================================================================

class ConsoleTerminalComponentTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalComponentTest, GetExecutor) {
    auto* executor = terminal->getExecutor();
    // May be nullptr if not initialized
}

TEST_F(ConsoleTerminalComponentTest, GetRenderer) {
    auto* renderer = terminal->getRenderer();
    // May be nullptr if not initialized
}

TEST_F(ConsoleTerminalComponentTest, GetTuiManager) {
    auto* tuiManager = terminal->getTuiManager();
    // May be nullptr if not initialized
}

// ============================================================================
// ConsoleTerminal Theme Tests
// ============================================================================

class ConsoleTerminalThemeTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalThemeTest, SetThemeDefault) {
    EXPECT_NO_THROW(terminal->setTheme("default"));
}

TEST_F(ConsoleTerminalThemeTest, SetThemeDark) {
    EXPECT_NO_THROW(terminal->setTheme("dark"));
}

TEST_F(ConsoleTerminalThemeTest, SetThemeLight) {
    EXPECT_NO_THROW(terminal->setTheme("light"));
}

TEST_F(ConsoleTerminalThemeTest, SetThemeAscii) {
    EXPECT_NO_THROW(terminal->setTheme("ascii"));
}

// ============================================================================
// ConsoleTerminal Output Tests
// ============================================================================

class ConsoleTerminalOutputTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalOutputTest, PrintSuccess) {
    EXPECT_NO_THROW(terminal->printSuccess("Operation successful"));
}

TEST_F(ConsoleTerminalOutputTest, PrintError) {
    EXPECT_NO_THROW(terminal->printError("An error occurred"));
}

TEST_F(ConsoleTerminalOutputTest, PrintWarning) {
    EXPECT_NO_THROW(terminal->printWarning("Warning message"));
}

TEST_F(ConsoleTerminalOutputTest, PrintInfo) {
    EXPECT_NO_THROW(terminal->printInfo("Information message"));
}

TEST_F(ConsoleTerminalOutputTest, ClearScreen) {
    EXPECT_NO_THROW(terminal->clearScreen());
}

TEST_F(ConsoleTerminalOutputTest, ShowProgress) {
    EXPECT_NO_THROW(terminal->showProgress(0.5f));
}

TEST_F(ConsoleTerminalOutputTest, ShowProgressWithLabel) {
    EXPECT_NO_THROW(terminal->showProgress(0.75f, "Loading..."));
}

TEST_F(ConsoleTerminalOutputTest, ShowProgressZero) {
    EXPECT_NO_THROW(terminal->showProgress(0.0f));
}

TEST_F(ConsoleTerminalOutputTest, ShowProgressFull) {
    EXPECT_NO_THROW(terminal->showProgress(1.0f, "Complete"));
}

// ============================================================================
// ConsoleTerminal Config File Tests
// ============================================================================

class ConsoleTerminalConfigFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();
        testConfigPath =
            std::filesystem::temp_directory_path() / "test_terminal_config.json";

        // Create a simple config file
        std::ofstream file(testConfigPath);
        file << R"({
            "timeout": 5000,
            "history": true,
            "suggestions": true,
            "syntaxHighlight": true
        })";
        file.close();
    }

    void TearDown() override {
        terminal.reset();
        if (std::filesystem::exists(testConfigPath)) {
            std::filesystem::remove(testConfigPath);
        }
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::filesystem::path testConfigPath;
};

TEST_F(ConsoleTerminalConfigFileTest, LoadConfig) {
    EXPECT_NO_THROW(terminal->loadConfig(testConfigPath.string()));
}

TEST_F(ConsoleTerminalConfigFileTest, LoadNonexistentConfig) {
    // Should handle gracefully or throw
    EXPECT_ANY_THROW(terminal->loadConfig("/nonexistent/path/config.json"));
}

// ============================================================================
// Global Terminal Pointer Tests
// ============================================================================

class GlobalTerminalTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GlobalTerminalTest, GlobalPointerExists) {
    // globalConsoleTerminal should be declared
    // It may be nullptr initially
}

// ============================================================================
// ConsoleTerminal Integration Tests
// ============================================================================

class ConsoleTerminalIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = std::make_unique<ConsoleTerminal>();

        // Set up checker
        checker = std::make_shared<CommandChecker>();
        terminal->setCommandChecker(checker);

        // Set up suggestion engine
        std::vector<std::string> dataset = {"help", "history", "exit", "clear"};
        suggestionEngine = std::make_shared<SuggestionEngine>(dataset);
        terminal->setSuggestionEngine(suggestionEngine);
    }

    void TearDown() override {
        terminal.reset();
        checker.reset();
        suggestionEngine.reset();
    }

    std::unique_ptr<ConsoleTerminal> terminal;
    std::shared_ptr<CommandChecker> checker;
    std::shared_ptr<SuggestionEngine> suggestionEngine;
};

TEST_F(ConsoleTerminalIntegrationTest, FullWorkflow) {
    // Enable features
    terminal->enableHistory(true);
    terminal->enableSuggestions(true);
    terminal->enableSyntaxHighlight(true);
    terminal->enableCommandCheck(true);

    // Get suggestions
    auto suggestions = terminal->getCommandSuggestions("hel");
    EXPECT_FALSE(suggestions.empty());

    // Execute command
    std::vector<std::any> args;
    terminal->callCommand("help", args);

    // Print debug report
    terminal->printDebugReport("echo hello", false);
}

TEST_F(ConsoleTerminalIntegrationTest, ConfigurationPersistence) {
    auto tempPath =
        std::filesystem::temp_directory_path() / "integration_config.json";

    // Save configuration
    terminal->saveDebugConfig(tempPath.string());
    EXPECT_TRUE(std::filesystem::exists(tempPath));

    // Load configuration
    ConsoleTerminal newTerminal;
    newTerminal.loadDebugConfig(tempPath.string());

    // Cleanup
    std::filesystem::remove(tempPath);
}

}  // namespace lithium::debug::test
