#ifndef LITHIUM_DEBUG_TEST_TERMINAL_HPP
#define LITHIUM_DEBUG_TEST_TERMINAL_HPP

#include <gtest/gtest.h>
#include "debug/terminal.hpp"

namespace lithium::debug::test {

class ConsoleTerminalSuggestionsTest : public ::testing::Test {
protected:
    void SetUp() override { terminal = std::make_unique<ConsoleTerminal>(); }

    void TearDown() override { terminal.reset(); }

    std::unique_ptr<ConsoleTerminal> terminal;
};

TEST_F(ConsoleTerminalSuggestionsTest, EnableSuggestions) {
    // First disable suggestions
    terminal->enableSuggestions(false);

    // Then enable them
    terminal->enableSuggestions(true);

    // Verify suggestions are enabled by trying to execute a misspelled command
    std::vector<std::any> args;
    bool suggestionReceived = false;

    // Capture stdout to verify suggestions are shown
    testing::internal::CaptureStdout();
    terminal->callCommand("hlp", args);  // Misspelled "help"
    std::string output = testing::internal::GetCapturedStdout();

    // Check if suggestions are present in output
    EXPECT_TRUE(output.find("Did you mean") != std::string::npos);
}

TEST_F(ConsoleTerminalSuggestionsTest, DisableSuggestions) {
    // First make sure suggestions are enabled
    terminal->enableSuggestions(true);

    // Then disable them
    terminal->enableSuggestions(false);

    // Verify suggestions are disabled by trying to execute a misspelled command
    std::vector<std::any> args;

    // Capture stdout to verify no suggestions are shown
    testing::internal::CaptureStdout();
    terminal->callCommand("hlp", args);  // Misspelled "help"
    std::string output = testing::internal::GetCapturedStdout();

    // Check that no suggestions are present in output
    EXPECT_TRUE(output.find("Did you mean") == std::string::npos);
}

TEST_F(ConsoleTerminalSuggestionsTest, SuggestionsWithInvalidCommand) {
    terminal->enableSuggestions(true);
    std::vector<std::any> args;

    testing::internal::CaptureStdout();
    terminal->callCommand("invalidcommand", args);
    std::string output = testing::internal::GetCapturedStdout();

    // Should show error and suggestions
    EXPECT_TRUE(output.find("Command 'invalidcommand' not found") !=
                std::string::npos);
    EXPECT_TRUE(output.find("Did you mean") != std::string::npos);
}

TEST_F(ConsoleTerminalSuggestionsTest, NoSuggestionsWhenDisabled) {
    terminal->enableSuggestions(false);
    std::vector<std::any> args;

    testing::internal::CaptureStdout();
    terminal->callCommand("invalidcommand", args);
    std::string output = testing::internal::GetCapturedStdout();

    // Should show error but no suggestions
    EXPECT_TRUE(output.find("Command 'invalidcommand' not found") !=
                std::string::npos);
    EXPECT_TRUE(output.find("Did you mean") == std::string::npos);
}

TEST_F(ConsoleTerminalSuggestionsTest, SuggestionPersistence) {
    // Enable suggestions
    terminal->enableSuggestions(true);

    // Verify state persists across command calls
    std::vector<std::any> args;

    testing::internal::CaptureStdout();
    terminal->callCommand("hlp", args);
    std::string output1 = testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    terminal->callCommand("lst", args);
    std::string output2 = testing::internal::GetCapturedStdout();

    // Both outputs should contain suggestions
    EXPECT_TRUE(output1.find("Did you mean") != std::string::npos);
    EXPECT_TRUE(output2.find("Did you mean") != std::string::npos);
}

}  // namespace lithium::debug::test

#endif  // LITHIUM_DEBUG_TEST_TERMINAL_HPP
