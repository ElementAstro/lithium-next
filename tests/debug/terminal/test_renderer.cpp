/**
 * @file test_renderer.cpp
 * @brief Comprehensive unit tests for ConsoleRenderer
 *
 * Tests for:
 * - Theme management
 * - Basic output operations
 * - Styled messages
 * - UI elements (headers, boxes, lists)
 * - Tables
 * - Progress indicators
 * - Terminal prompt
 * - Syntax highlighting
 * - Utility functions
 */

#include <gtest/gtest.h>
#include <chrono>
#include <sstream>
#include <thread>

#include "debug/terminal/renderer.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// ConsoleRenderer Basic Tests
// ============================================================================

class ConsoleRendererBasicTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererBasicTest, DefaultConstruction) {
    EXPECT_EQ(renderer->getTheme().name, "default");
}

TEST_F(ConsoleRendererBasicTest, ConstructWithCustomTheme) {
    Theme darkTheme = Theme::dark();
    ConsoleRenderer darkRenderer(darkTheme);
    EXPECT_EQ(darkRenderer.getTheme().name, "dark");
}

TEST_F(ConsoleRendererBasicTest, ConstructWithAsciiTheme) {
    Theme asciiTheme = Theme::ascii();
    ConsoleRenderer asciiRenderer(asciiTheme);
    EXPECT_EQ(asciiRenderer.getTheme().name, "ascii");
    EXPECT_FALSE(asciiRenderer.getTheme().useUnicode);
}

TEST_F(ConsoleRendererBasicTest, ConstructWithLightTheme) {
    Theme lightTheme = Theme::light();
    ConsoleRenderer lightRenderer(lightTheme);
    EXPECT_EQ(lightRenderer.getTheme().name, "light");
}

// ============================================================================
// Theme Management Tests
// ============================================================================

class ConsoleRendererThemeTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererThemeTest, SetTheme) {
    Theme newTheme = Theme::dark();
    renderer->setTheme(newTheme);
    EXPECT_EQ(renderer->getTheme().name, "dark");
    EXPECT_EQ(renderer->getTheme().promptColor, Color::BrightBlue);
}

TEST_F(ConsoleRendererThemeTest, GetTheme) {
    const Theme& theme = renderer->getTheme();
    EXPECT_EQ(theme.name, "default");
    EXPECT_TRUE(theme.useUnicode);
    EXPECT_TRUE(theme.useColors);
}

TEST_F(ConsoleRendererThemeTest, SwitchBetweenThemes) {
    renderer->setTheme(Theme::dark());
    EXPECT_EQ(renderer->getTheme().name, "dark");

    renderer->setTheme(Theme::light());
    EXPECT_EQ(renderer->getTheme().name, "light");

    renderer->setTheme(Theme::ascii());
    EXPECT_EQ(renderer->getTheme().name, "ascii");
}

TEST_F(ConsoleRendererThemeTest, ThemePreservesCustomSettings) {
    Theme customTheme;
    customTheme.name = "custom";
    customTheme.promptColor = Color::Magenta;
    customTheme.successColor = Color::Cyan;

    renderer->setTheme(customTheme);

    EXPECT_EQ(renderer->getTheme().name, "custom");
    EXPECT_EQ(renderer->getTheme().promptColor, Color::Magenta);
    EXPECT_EQ(renderer->getTheme().successColor, Color::Cyan);
}

// ============================================================================
// Terminal Size Tests
// ============================================================================

class ConsoleRendererSizeTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererSizeTest, GetTerminalSize) {
    TerminalSize size = renderer->getTerminalSize();
    // Terminal size should be positive
    EXPECT_GT(size.width, 0);
    EXPECT_GT(size.height, 0);
}

TEST_F(ConsoleRendererSizeTest, TerminalSizeReasonableBounds) {
    TerminalSize size = renderer->getTerminalSize();
    // Reasonable bounds for terminal size
    EXPECT_GE(size.width, 10);
    EXPECT_LE(size.width, 1000);
    EXPECT_GE(size.height, 5);
    EXPECT_LE(size.height, 500);
}

// ============================================================================
// Color Support Tests
// ============================================================================

class ConsoleRendererColorTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererColorTest, EnableDisableColors) {
    renderer->enableColors(false);
    // After disabling, supportsColors should return false
    EXPECT_FALSE(renderer->supportsColors());

    renderer->enableColors(true);
    // Note: actual support depends on terminal capabilities
}

TEST_F(ConsoleRendererColorTest, EnableDisableUnicode) {
    renderer->enableUnicode(false);
    EXPECT_FALSE(renderer->supportsUnicode());

    renderer->enableUnicode(true);
    // Note: actual support depends on terminal capabilities
}

TEST_F(ConsoleRendererColorTest, ColorCodeGeneration) {
    std::string code = renderer->colorCode(Color::Red);
    // If colors are supported, code should not be empty
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
        // Should contain escape sequence
        EXPECT_NE(code.find("\033["), std::string::npos);
    }
}

TEST_F(ConsoleRendererColorTest, ColorCodeWithBackground) {
    std::string code = renderer->colorCode(Color::White, Color::Blue);
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
    }
}

TEST_F(ConsoleRendererColorTest, ColorCodeWithStyle) {
    std::string code =
        renderer->colorCode(Color::Green, std::nullopt, Style::Bold);
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
    }
}

TEST_F(ConsoleRendererColorTest, ColorCodeWithAllOptions) {
    std::string code = renderer->colorCode(Color::Yellow, Color::Black, Style::Underline);
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
    }
}

TEST_F(ConsoleRendererColorTest, ResetCode) {
    std::string code = renderer->resetCode();
    if (renderer->supportsColors()) {
        EXPECT_FALSE(code.empty());
        // Should contain reset sequence
        EXPECT_NE(code.find("\033["), std::string::npos);
    }
}

// ============================================================================
// ANSI Utility Tests
// ============================================================================

class ConsoleRendererAnsiTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConsoleRendererAnsiTest, StripAnsiFromColoredText) {
    std::string colored = "\033[31mRed Text\033[0m";
    std::string stripped = ConsoleRenderer::stripAnsi(colored);
    EXPECT_EQ(stripped, "Red Text");
}

TEST_F(ConsoleRendererAnsiTest, StripAnsiFromPlainText) {
    std::string plain = "Plain Text";
    std::string stripped = ConsoleRenderer::stripAnsi(plain);
    EXPECT_EQ(stripped, "Plain Text");
}

TEST_F(ConsoleRendererAnsiTest, StripAnsiMultipleCodes) {
    std::string text = "\033[1m\033[31mBold Red\033[0m Normal \033[32mGreen\033[0m";
    std::string stripped = ConsoleRenderer::stripAnsi(text);
    EXPECT_EQ(stripped, "Bold Red Normal Green");
}

TEST_F(ConsoleRendererAnsiTest, StripAnsiEmptyString) {
    std::string empty = "";
    std::string stripped = ConsoleRenderer::stripAnsi(empty);
    EXPECT_TRUE(stripped.empty());
}

TEST_F(ConsoleRendererAnsiTest, VisibleLengthColoredText) {
    std::string colored = "\033[31mRed\033[0m";
    size_t len = ConsoleRenderer::visibleLength(colored);
    EXPECT_EQ(len, 3);  // "Red" without ANSI codes
}

TEST_F(ConsoleRendererAnsiTest, VisibleLengthPlainText) {
    std::string plain = "Hello World";
    size_t len = ConsoleRenderer::visibleLength(plain);
    EXPECT_EQ(len, 11);
}

TEST_F(ConsoleRendererAnsiTest, VisibleLengthEmptyString) {
    std::string empty = "";
    size_t len = ConsoleRenderer::visibleLength(empty);
    EXPECT_EQ(len, 0);
}

TEST_F(ConsoleRendererAnsiTest, VisibleLengthOnlyAnsi) {
    std::string onlyAnsi = "\033[31m\033[0m";
    size_t len = ConsoleRenderer::visibleLength(onlyAnsi);
    EXPECT_EQ(len, 0);
}

TEST_F(ConsoleRendererAnsiTest, VisibleLengthComplexFormatting) {
    std::string complex = "\033[1;31;44mStyled\033[0m Text \033[4mUnderlined\033[0m";
    size_t len = ConsoleRenderer::visibleLength(complex);
    EXPECT_EQ(len, 18);  // "Styled Text Underlined"
}

// ============================================================================
// Table Column Tests
// ============================================================================

class TableColumnTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TableColumnTest, DefaultConstruction) {
    TableColumn column;
    EXPECT_TRUE(column.header.empty());
    EXPECT_EQ(column.width, 0);
    EXPECT_EQ(column.alignment, Alignment::Left);
}

TEST_F(TableColumnTest, CustomColumn) {
    TableColumn column;
    column.header = "Name";
    column.width = 20;
    column.alignment = Alignment::Center;

    EXPECT_EQ(column.header, "Name");
    EXPECT_EQ(column.width, 20);
    EXPECT_EQ(column.alignment, Alignment::Center);
}

TEST_F(TableColumnTest, AlignmentValues) {
    EXPECT_NE(Alignment::Left, Alignment::Center);
    EXPECT_NE(Alignment::Center, Alignment::Right);
    EXPECT_NE(Alignment::Left, Alignment::Right);
}

// ============================================================================
// Progress Style Tests
// ============================================================================

class ProgressStyleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ProgressStyleTest, DefaultConstruction) {
    ProgressStyle style;
    EXPECT_EQ(style.fillChar, "█");
    EXPECT_EQ(style.emptyChar, "░");
    EXPECT_EQ(style.leftBracket, "[");
    EXPECT_EQ(style.rightBracket, "]");
    EXPECT_EQ(style.fillColor, Color::BrightGreen);
    EXPECT_EQ(style.emptyColor, Color::BrightBlack);
    EXPECT_TRUE(style.showPercentage);
    EXPECT_FALSE(style.showEta);
    EXPECT_EQ(style.width, 40);
}

TEST_F(ProgressStyleTest, CustomStyle) {
    ProgressStyle style;
    style.fillChar = "#";
    style.emptyChar = "-";
    style.width = 50;
    style.showEta = true;

    EXPECT_EQ(style.fillChar, "#");
    EXPECT_EQ(style.emptyChar, "-");
    EXPECT_EQ(style.width, 50);
    EXPECT_TRUE(style.showEta);
}

// ============================================================================
// Spinner Style Tests
// ============================================================================

class SpinnerStyleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SpinnerStyleTest, DefaultConstruction) {
    SpinnerStyle style;
    EXPECT_FALSE(style.frames.empty());
    EXPECT_EQ(style.color, Color::BrightCyan);
    EXPECT_EQ(style.intervalMs, 80);
}

TEST_F(SpinnerStyleTest, DefaultFrames) {
    SpinnerStyle style;
    EXPECT_EQ(style.frames.size(), 10);
    EXPECT_EQ(style.frames[0], "⠋");
}

TEST_F(SpinnerStyleTest, CustomStyle) {
    SpinnerStyle style;
    style.frames = {"|", "/", "-", "\\"};
    style.color = Color::Yellow;
    style.intervalMs = 100;

    EXPECT_EQ(style.frames.size(), 4);
    EXPECT_EQ(style.color, Color::Yellow);
    EXPECT_EQ(style.intervalMs, 100);
}

// ============================================================================
// ConsoleRenderer Move Semantics Tests
// ============================================================================

class ConsoleRendererMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConsoleRendererMoveTest, MoveConstruction) {
    ConsoleRenderer original(Theme::dark());
    EXPECT_EQ(original.getTheme().name, "dark");

    ConsoleRenderer moved(std::move(original));
    EXPECT_EQ(moved.getTheme().name, "dark");
}

TEST_F(ConsoleRendererMoveTest, MoveAssignment) {
    ConsoleRenderer original(Theme::dark());
    ConsoleRenderer target;

    target = std::move(original);
    EXPECT_EQ(target.getTheme().name, "dark");
}

// ============================================================================
// ConsoleRenderer Output Tests (Non-destructive)
// ============================================================================

class ConsoleRendererOutputTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererOutputTest, FlushDoesNotThrow) {
    EXPECT_NO_THROW(renderer->flush());
}

TEST_F(ConsoleRendererOutputTest, PrintDoesNotThrow) {
    EXPECT_NO_THROW(renderer->print("Test message"));
}

TEST_F(ConsoleRendererOutputTest, PrintlnDoesNotThrow) {
    EXPECT_NO_THROW(renderer->println("Test message"));
}

TEST_F(ConsoleRendererOutputTest, PrintWithColorDoesNotThrow) {
    EXPECT_NO_THROW(renderer->print("Colored", Color::Red));
}

TEST_F(ConsoleRendererOutputTest, PrintWithStyleDoesNotThrow) {
    EXPECT_NO_THROW(
        renderer->print("Styled", Color::Blue, std::nullopt, Style::Bold));
}

TEST_F(ConsoleRendererOutputTest, SuccessMessageDoesNotThrow) {
    EXPECT_NO_THROW(renderer->success("Success message"));
}

TEST_F(ConsoleRendererOutputTest, ErrorMessageDoesNotThrow) {
    EXPECT_NO_THROW(renderer->error("Error message"));
}

TEST_F(ConsoleRendererOutputTest, WarningMessageDoesNotThrow) {
    EXPECT_NO_THROW(renderer->warning("Warning message"));
}

TEST_F(ConsoleRendererOutputTest, InfoMessageDoesNotThrow) {
    EXPECT_NO_THROW(renderer->info("Info message"));
}

TEST_F(ConsoleRendererOutputTest, DebugMessageDoesNotThrow) {
    EXPECT_NO_THROW(renderer->debug("Debug message"));
}

// ============================================================================
// ConsoleRenderer UI Elements Tests
// ============================================================================

class ConsoleRendererUITest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererUITest, HeaderDoesNotThrow) {
    EXPECT_NO_THROW(renderer->header("Test Header"));
}

TEST_F(ConsoleRendererUITest, HeaderWithCustomFillDoesNotThrow) {
    EXPECT_NO_THROW(renderer->header("Test Header", '*'));
}

TEST_F(ConsoleRendererUITest, SubheaderDoesNotThrow) {
    EXPECT_NO_THROW(renderer->subheader("Test Subheader"));
}

TEST_F(ConsoleRendererUITest, HorizontalRuleDoesNotThrow) {
    EXPECT_NO_THROW(renderer->horizontalRule());
}

TEST_F(ConsoleRendererUITest, HorizontalRuleWithCustomCharDoesNotThrow) {
    EXPECT_NO_THROW(renderer->horizontalRule('=', 40));
}

TEST_F(ConsoleRendererUITest, BoxDoesNotThrow) {
    EXPECT_NO_THROW(renderer->box("Box content"));
}

TEST_F(ConsoleRendererUITest, BoxWithTitleDoesNotThrow) {
    EXPECT_NO_THROW(renderer->box("Box content", "Title"));
}

TEST_F(ConsoleRendererUITest, MultilineBoxDoesNotThrow) {
    std::vector<std::string> lines = {"Line 1", "Line 2", "Line 3"};
    EXPECT_NO_THROW(renderer->box(lines, "Multiline Box"));
}

TEST_F(ConsoleRendererUITest, BulletListDoesNotThrow) {
    std::vector<std::string> items = {"Item 1", "Item 2", "Item 3"};
    EXPECT_NO_THROW(renderer->bulletList(items));
}

TEST_F(ConsoleRendererUITest, BulletListWithIndentDoesNotThrow) {
    std::vector<std::string> items = {"Item 1", "Item 2"};
    EXPECT_NO_THROW(renderer->bulletList(items, 2));
}

TEST_F(ConsoleRendererUITest, NumberedListDoesNotThrow) {
    std::vector<std::string> items = {"First", "Second", "Third"};
    EXPECT_NO_THROW(renderer->numberedList(items));
}

TEST_F(ConsoleRendererUITest, NumberedListWithStartNumDoesNotThrow) {
    std::vector<std::string> items = {"First", "Second"};
    EXPECT_NO_THROW(renderer->numberedList(items, 5));
}

TEST_F(ConsoleRendererUITest, KeyValueDoesNotThrow) {
    EXPECT_NO_THROW(renderer->keyValue("Key", "Value"));
}

TEST_F(ConsoleRendererUITest, KeyValueWithWidthDoesNotThrow) {
    EXPECT_NO_THROW(renderer->keyValue("Key", "Value", 30));
}

TEST_F(ConsoleRendererUITest, KeyValueListDoesNotThrow) {
    std::vector<std::pair<std::string, std::string>> pairs = {
        {"Key1", "Value1"}, {"Key2", "Value2"}};
    EXPECT_NO_THROW(renderer->keyValueList(pairs));
}

// ============================================================================
// ConsoleRenderer Table Tests
// ============================================================================

class ConsoleRendererTableTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererTableTest, SimpleTableDoesNotThrow) {
    std::vector<std::string> headers = {"Name", "Age", "City"};
    std::vector<std::vector<std::string>> rows = {
        {"Alice", "30", "New York"},
        {"Bob", "25", "Los Angeles"}};
    EXPECT_NO_THROW(renderer->simpleTable(headers, rows));
}

TEST_F(ConsoleRendererTableTest, TableWithColumnsDoesNotThrow) {
    std::vector<TableColumn> columns = {
        {"Name", 20, Alignment::Left},
        {"Age", 10, Alignment::Right},
        {"City", 15, Alignment::Center}};
    std::vector<std::vector<std::string>> rows = {
        {"Alice", "30", "New York"}};
    EXPECT_NO_THROW(renderer->table(columns, rows));
}

TEST_F(ConsoleRendererTableTest, EmptyTableDoesNotThrow) {
    std::vector<std::string> headers = {"Col1", "Col2"};
    std::vector<std::vector<std::string>> rows;
    EXPECT_NO_THROW(renderer->simpleTable(headers, rows));
}

// ============================================================================
// ConsoleRenderer Progress Tests
// ============================================================================

class ConsoleRendererProgressTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererProgressTest, ProgressBarDoesNotThrow) {
    EXPECT_NO_THROW(renderer->progressBar(0.5f));
}

TEST_F(ConsoleRendererProgressTest, ProgressBarWithLabelDoesNotThrow) {
    EXPECT_NO_THROW(renderer->progressBar(0.75f, "Loading..."));
}

TEST_F(ConsoleRendererProgressTest, ProgressBarWithStyleDoesNotThrow) {
    ProgressStyle style;
    style.width = 30;
    EXPECT_NO_THROW(renderer->progressBar(0.5f, "Progress", style));
}

TEST_F(ConsoleRendererProgressTest, ProgressBarZeroPercent) {
    EXPECT_NO_THROW(renderer->progressBar(0.0f));
}

TEST_F(ConsoleRendererProgressTest, ProgressBarFullPercent) {
    EXPECT_NO_THROW(renderer->progressBar(1.0f));
}

TEST_F(ConsoleRendererProgressTest, UpdateProgressDoesNotThrow) {
    EXPECT_NO_THROW(renderer->updateProgress(0.5f));
}

TEST_F(ConsoleRendererProgressTest, UpdateProgressWithLabelDoesNotThrow) {
    EXPECT_NO_THROW(renderer->updateProgress(0.8f, "Almost done"));
}

// ============================================================================
// ConsoleRenderer Prompt Tests
// ============================================================================

class ConsoleRendererPromptTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererPromptTest, PromptDoesNotThrow) {
    EXPECT_NO_THROW(renderer->prompt());
}

TEST_F(ConsoleRendererPromptTest, PromptWithPrefixDoesNotThrow) {
    EXPECT_NO_THROW(renderer->prompt("user@host"));
}

TEST_F(ConsoleRendererPromptTest, WelcomeHeaderDoesNotThrow) {
    EXPECT_NO_THROW(renderer->welcomeHeader("App", "1.0.0"));
}

TEST_F(ConsoleRendererPromptTest, WelcomeHeaderWithDescriptionDoesNotThrow) {
    EXPECT_NO_THROW(
        renderer->welcomeHeader("App", "1.0.0", "A sample application"));
}

TEST_F(ConsoleRendererPromptTest, SuggestionsDoesNotThrow) {
    std::vector<std::string> suggestions = {"help", "hello", "history"};
    EXPECT_NO_THROW(renderer->suggestions(suggestions));
}

TEST_F(ConsoleRendererPromptTest, SuggestionsWithPrefixDoesNotThrow) {
    std::vector<std::string> suggestions = {"cmd1", "cmd2"};
    EXPECT_NO_THROW(renderer->suggestions(suggestions, "Available commands:"));
}

TEST_F(ConsoleRendererPromptTest, CommandHelpDoesNotThrow) {
    EXPECT_NO_THROW(renderer->commandHelp("test", "A test command"));
}

TEST_F(ConsoleRendererPromptTest, CommandHelpWithOptionsDoesNotThrow) {
    std::vector<std::pair<std::string, std::string>> options = {
        {"-v", "Verbose output"}, {"-h", "Show help"}};
    EXPECT_NO_THROW(renderer->commandHelp("test", "A test command", options));
}

// ============================================================================
// ConsoleRenderer Syntax Highlighting Tests
// ============================================================================

class ConsoleRendererHighlightTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererHighlightTest, HighlightedCommandDoesNotThrow) {
    EXPECT_NO_THROW(renderer->highlightedCommand("echo hello"));
}

TEST_F(ConsoleRendererHighlightTest, HighlightedCommandWithKeywordsDoesNotThrow) {
    std::vector<std::string> keywords = {"if", "then", "else", "fi"};
    EXPECT_NO_THROW(renderer->highlightedCommand("if true; then echo yes; fi", keywords));
}

TEST_F(ConsoleRendererHighlightTest, ErrorWithPositionDoesNotThrow) {
    EXPECT_NO_THROW(renderer->errorWithPosition("echo \"hello", 5, "Unclosed quote"));
}

// ============================================================================
// ConsoleRenderer Cursor Tests
// ============================================================================

class ConsoleRendererCursorTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererCursorTest, MoveCursorDoesNotThrow) {
    EXPECT_NO_THROW(renderer->moveCursor(10, 5));
}

TEST_F(ConsoleRendererCursorTest, MoveCursorUpDoesNotThrow) {
    EXPECT_NO_THROW(renderer->moveCursorUp(2));
}

TEST_F(ConsoleRendererCursorTest, MoveCursorDownDoesNotThrow) {
    EXPECT_NO_THROW(renderer->moveCursorDown(2));
}

TEST_F(ConsoleRendererCursorTest, SaveRestoreCursorDoesNotThrow) {
    EXPECT_NO_THROW(renderer->saveCursor());
    EXPECT_NO_THROW(renderer->restoreCursor());
}

TEST_F(ConsoleRendererCursorTest, HideShowCursorDoesNotThrow) {
    EXPECT_NO_THROW(renderer->hideCursor());
    EXPECT_NO_THROW(renderer->showCursor());
}

// ============================================================================
// ConsoleRenderer Clear Tests
// ============================================================================

class ConsoleRendererClearTest : public ::testing::Test {
protected:
    void SetUp() override { renderer = std::make_unique<ConsoleRenderer>(); }

    void TearDown() override { renderer.reset(); }

    std::unique_ptr<ConsoleRenderer> renderer;
};

TEST_F(ConsoleRendererClearTest, ClearDoesNotThrow) {
    EXPECT_NO_THROW(renderer->clear());
}

TEST_F(ConsoleRendererClearTest, ClearLineDoesNotThrow) {
    EXPECT_NO_THROW(renderer->clearLine());
}

}  // namespace lithium::debug::terminal::test
