/**
 * @file test_input_controller.cpp
 * @brief Comprehensive unit tests for InputController
 *
 * Tests for:
 * - Configuration management
 * - Input reading operations
 * - Line editing operations
 * - History management
 * - Completion handling
 * - Event handlers
 * - Terminal control
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "debug/terminal/input_controller.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// InputMode Tests
// ============================================================================

class InputModeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InputModeTest, InputModeValues) {
    EXPECT_NE(InputMode::Line, InputMode::Character);
    EXPECT_NE(InputMode::Character, InputMode::Raw);
    EXPECT_NE(InputMode::Line, InputMode::Raw);
}

// ============================================================================
// InputConfig Tests
// ============================================================================

class InputConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InputConfigTest, DefaultConstruction) {
    InputConfig config;
    EXPECT_EQ(config.mode, InputMode::Line);
    EXPECT_TRUE(config.enableHistory);
    EXPECT_TRUE(config.enableCompletion);
    EXPECT_TRUE(config.enableEditing);
    EXPECT_TRUE(config.echoInput);
    EXPECT_EQ(config.maxLineLength, 4096);
    EXPECT_EQ(config.prompt, ">");
}

TEST_F(InputConfigTest, CustomConfiguration) {
    InputConfig config;
    config.mode = InputMode::Character;
    config.enableHistory = false;
    config.enableCompletion = false;
    config.echoInput = false;
    config.maxLineLength = 1024;
    config.prompt = ">>> ";

    EXPECT_EQ(config.mode, InputMode::Character);
    EXPECT_FALSE(config.enableHistory);
    EXPECT_FALSE(config.enableCompletion);
    EXPECT_FALSE(config.echoInput);
    EXPECT_EQ(config.maxLineLength, 1024);
    EXPECT_EQ(config.prompt, ">>> ");
}

// ============================================================================
// CompletionResult Tests
// ============================================================================

class CompletionResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CompletionResultTest, DefaultConstruction) {
    CompletionResult result;
    EXPECT_TRUE(result.matches.empty());
    EXPECT_TRUE(result.commonPrefix.empty());
    EXPECT_FALSE(result.hasMultiple);
}

TEST_F(CompletionResultTest, PopulatedResult) {
    CompletionResult result;
    result.matches = {"help", "hello", "history"};
    result.commonPrefix = "he";
    result.hasMultiple = true;

    EXPECT_EQ(result.matches.size(), 3);
    EXPECT_EQ(result.commonPrefix, "he");
    EXPECT_TRUE(result.hasMultiple);
}

TEST_F(CompletionResultTest, SingleMatch) {
    CompletionResult result;
    result.matches = {"unique"};
    result.commonPrefix = "unique";
    result.hasMultiple = false;

    EXPECT_EQ(result.matches.size(), 1);
    EXPECT_FALSE(result.hasMultiple);
}

// ============================================================================
// InputController Basic Tests
// ============================================================================

class InputControllerBasicTest : public ::testing::Test {
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

TEST_F(InputControllerBasicTest, DefaultConstruction) {
    InputController defaultController;
    const InputConfig& config = defaultController.getConfig();
    EXPECT_EQ(config.mode, InputMode::Line);
}

TEST_F(InputControllerBasicTest, ConstructWithConfig) {
    const InputConfig& config = controller->getConfig();
    EXPECT_TRUE(config.enableHistory);
    EXPECT_TRUE(config.enableCompletion);
}

TEST_F(InputControllerBasicTest, SetConfig) {
    InputConfig newConfig;
    newConfig.enableHistory = false;
    newConfig.prompt = "$ ";

    controller->setConfig(newConfig);

    const InputConfig& config = controller->getConfig();
    EXPECT_FALSE(config.enableHistory);
    EXPECT_EQ(config.prompt, "$ ");
}

TEST_F(InputControllerBasicTest, GetConfig) {
    const InputConfig& config = controller->getConfig();
    EXPECT_TRUE(config.enableHistory);
}

TEST_F(InputControllerBasicTest, SetPrompt) {
    controller->setPrompt(">>> ");
    EXPECT_EQ(controller->getConfig().prompt, ">>> ");
}

TEST_F(InputControllerBasicTest, SetMode) {
    controller->setMode(InputMode::Character);
    EXPECT_EQ(controller->getConfig().mode, InputMode::Character);
}

// ============================================================================
// InputController Buffer Operations Tests
// ============================================================================

class InputControllerBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerBufferTest, GetBufferInitiallyEmpty) {
    EXPECT_TRUE(controller->getBuffer().empty());
}

TEST_F(InputControllerBufferTest, SetBuffer) {
    controller->setBuffer("test input");
    EXPECT_EQ(controller->getBuffer(), "test input");
}

TEST_F(InputControllerBufferTest, ClearBuffer) {
    controller->setBuffer("test input");
    controller->clearBuffer();
    EXPECT_TRUE(controller->getBuffer().empty());
}

TEST_F(InputControllerBufferTest, GetCursorPositionInitially) {
    EXPECT_EQ(controller->getCursorPosition(), 0);
}

TEST_F(InputControllerBufferTest, SetCursorPosition) {
    controller->setBuffer("hello world");
    controller->setCursorPosition(5);
    EXPECT_EQ(controller->getCursorPosition(), 5);
}

TEST_F(InputControllerBufferTest, SetCursorPositionBeyondEnd) {
    controller->setBuffer("hello");
    controller->setCursorPosition(100);
    // Should clamp to buffer length
    EXPECT_LE(controller->getCursorPosition(), 5);
}

TEST_F(InputControllerBufferTest, InsertTextAtEnd) {
    controller->setBuffer("hello");
    controller->setCursorPosition(5);
    controller->insertText(" world");
    EXPECT_EQ(controller->getBuffer(), "hello world");
}

TEST_F(InputControllerBufferTest, InsertTextAtBeginning) {
    controller->setBuffer("world");
    controller->setCursorPosition(0);
    controller->insertText("hello ");
    EXPECT_EQ(controller->getBuffer(), "hello world");
}

TEST_F(InputControllerBufferTest, InsertTextInMiddle) {
    controller->setBuffer("helloworld");
    controller->setCursorPosition(5);
    controller->insertText(" ");
    EXPECT_EQ(controller->getBuffer(), "hello world");
}

TEST_F(InputControllerBufferTest, DeleteChar) {
    controller->setBuffer("hello");
    controller->setCursorPosition(2);
    controller->deleteChar();
    EXPECT_EQ(controller->getBuffer(), "helo");
}

TEST_F(InputControllerBufferTest, DeleteCharAtEnd) {
    controller->setBuffer("hello");
    controller->setCursorPosition(5);
    controller->deleteChar();
    // Should have no effect at end
    EXPECT_EQ(controller->getBuffer(), "hello");
}

TEST_F(InputControllerBufferTest, Backspace) {
    controller->setBuffer("hello");
    controller->setCursorPosition(5);
    controller->backspace();
    EXPECT_EQ(controller->getBuffer(), "hell");
}

TEST_F(InputControllerBufferTest, BackspaceAtBeginning) {
    controller->setBuffer("hello");
    controller->setCursorPosition(0);
    controller->backspace();
    // Should have no effect at beginning
    EXPECT_EQ(controller->getBuffer(), "hello");
}

TEST_F(InputControllerBufferTest, BackspaceInMiddle) {
    controller->setBuffer("hello");
    controller->setCursorPosition(3);
    controller->backspace();
    EXPECT_EQ(controller->getBuffer(), "helo");
}

// ============================================================================
// InputController History Tests
// ============================================================================

class InputControllerHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        config.enableHistory = true;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerHistoryTest, AddToHistory) {
    controller->addToHistory("command1");
    auto history = controller->getHistory();
    EXPECT_EQ(history.size(), 1);
    EXPECT_EQ(history[0], "command1");
}

TEST_F(InputControllerHistoryTest, AddMultipleToHistory) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");
    controller->addToHistory("command3");

    auto history = controller->getHistory();
    EXPECT_EQ(history.size(), 3);
}

TEST_F(InputControllerHistoryTest, GetHistoryEmpty) {
    auto history = controller->getHistory();
    EXPECT_TRUE(history.empty());
}

TEST_F(InputControllerHistoryTest, ClearHistory) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");
    controller->clearHistory();

    auto history = controller->getHistory();
    EXPECT_TRUE(history.empty());
}

TEST_F(InputControllerHistoryTest, SetMaxHistorySize) {
    controller->setMaxHistorySize(2);

    controller->addToHistory("command1");
    controller->addToHistory("command2");
    controller->addToHistory("command3");

    auto history = controller->getHistory();
    EXPECT_LE(history.size(), 2);
}

TEST_F(InputControllerHistoryTest, HistoryNavigation) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");
    controller->addToHistory("command3");

    controller->historyPrevious();
    // Buffer should contain previous command
}

TEST_F(InputControllerHistoryTest, HistoryNext) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");

    controller->historyPrevious();
    controller->historyPrevious();
    controller->historyNext();
    // Buffer should contain next command
}

TEST_F(InputControllerHistoryTest, SearchHistory) {
    controller->addToHistory("git status");
    controller->addToHistory("git commit");
    controller->addToHistory("ls -la");

    auto results = controller->searchHistory("git");
    EXPECT_EQ(results.size(), 2);
}

TEST_F(InputControllerHistoryTest, SearchHistoryNoMatch) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");

    auto results = controller->searchHistory("nonexistent");
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// InputController Completion Tests
// ============================================================================

class InputControllerCompletionTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        config.enableCompletion = true;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerCompletionTest, SetCompletionHandler) {
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

TEST_F(InputControllerCompletionTest, GetCompletionsWithoutHandler) {
    controller->setBuffer("te");
    auto completions = controller->getCompletions();
    EXPECT_TRUE(completions.matches.empty());
}

TEST_F(InputControllerCompletionTest, TriggerCompletion) {
    bool handlerCalled = false;

    controller->setCompletionHandler(
        [&handlerCalled](const std::string& text, size_t pos) {
            handlerCalled = true;
            return CompletionResult{};
        });

    controller->setBuffer("te");
    controller->triggerCompletion();

    EXPECT_TRUE(handlerCalled);
}

TEST_F(InputControllerCompletionTest, CompletionWithPrefix) {
    controller->setCompletionHandler(
        [](const std::string& text, size_t pos) {
            CompletionResult result;
            if (text.find("he") == 0) {
                result.matches = {"help", "hello", "history"};
                result.commonPrefix = "he";
                result.hasMultiple = true;
            }
            return result;
        });

    controller->setBuffer("he");
    auto completions = controller->getCompletions();

    EXPECT_EQ(completions.matches.size(), 3);
    EXPECT_EQ(completions.commonPrefix, "he");
    EXPECT_TRUE(completions.hasMultiple);
}

// ============================================================================
// InputController Event Handler Tests
// ============================================================================

class InputControllerEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerEventTest, SetKeyHandler) {
    bool handlerCalled = false;

    controller->setKeyHandler([&handlerCalled](const InputEvent& event) {
        handlerCalled = true;
        return true;
    });

    // Key handler is set, but we can't easily trigger it in unit tests
}

TEST_F(InputControllerEventTest, SetValidationHandler) {
    controller->setValidationHandler([](const std::string& input) {
        return !input.empty();
    });

    // Validation handler is set
}

// ============================================================================
// InputController Terminal Control Tests
// ============================================================================

class InputControllerTerminalTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerTerminalTest, InitializeDoesNotThrow) {
    EXPECT_NO_THROW(controller->initialize());
}

TEST_F(InputControllerTerminalTest, RestoreDoesNotThrow) {
    controller->initialize();
    EXPECT_NO_THROW(controller->restore());
}

TEST_F(InputControllerTerminalTest, IsRawModeInitiallyFalse) {
    EXPECT_FALSE(controller->isRawMode());
}

TEST_F(InputControllerTerminalTest, SetRawMode) {
    controller->setRawMode(true);
    // Raw mode state depends on implementation
}

TEST_F(InputControllerTerminalTest, RefreshDoesNotThrow) {
    EXPECT_NO_THROW(controller->refresh());
}

TEST_F(InputControllerTerminalTest, BellDoesNotThrow) {
    EXPECT_NO_THROW(controller->bell());
}

TEST_F(InputControllerTerminalTest, HasInputInitiallyFalse) {
    // In non-interactive mode, hasInput should return false
    EXPECT_FALSE(controller->hasInput());
}

// ============================================================================
// InputController Move Semantics Tests
// ============================================================================

class InputControllerMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InputControllerMoveTest, MoveConstruction) {
    InputController original;
    original.setBuffer("test");
    original.addToHistory("command1");

    InputController moved(std::move(original));
    EXPECT_EQ(moved.getBuffer(), "test");
}

TEST_F(InputControllerMoveTest, MoveAssignment) {
    InputController original;
    original.setBuffer("test");

    InputController target;
    target = std::move(original);
    EXPECT_EQ(target.getBuffer(), "test");
}

// ============================================================================
// InputController Persistence Tests
// ============================================================================

class InputControllerPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        config.enableHistory = true;
        controller = std::make_unique<InputController>(config);

        testFilePath = std::filesystem::temp_directory_path() / "test_input_history.txt";
    }

    void TearDown() override {
        controller.reset();
        if (std::filesystem::exists(testFilePath)) {
            std::filesystem::remove(testFilePath);
        }
    }

    std::unique_ptr<InputController> controller;
    std::filesystem::path testFilePath;
};

TEST_F(InputControllerPersistenceTest, SaveHistory) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");

    bool result = controller->saveHistory(testFilePath.string());
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(testFilePath));
}

TEST_F(InputControllerPersistenceTest, LoadHistory) {
    controller->addToHistory("command1");
    controller->addToHistory("command2");
    controller->saveHistory(testFilePath.string());

    InputController newController;
    bool result = newController.loadHistory(testFilePath.string());
    EXPECT_TRUE(result);

    auto history = newController.getHistory();
    EXPECT_EQ(history.size(), 2);
}

TEST_F(InputControllerPersistenceTest, LoadNonexistentFile) {
    bool result = controller->loadHistory("/nonexistent/path/file.txt");
    EXPECT_FALSE(result);
}

// ============================================================================
// InputController Edge Cases Tests
// ============================================================================

class InputControllerEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {
        InputConfig config;
        controller = std::make_unique<InputController>(config);
    }

    void TearDown() override { controller.reset(); }

    std::unique_ptr<InputController> controller;
};

TEST_F(InputControllerEdgeCasesTest, EmptyBuffer) {
    EXPECT_TRUE(controller->getBuffer().empty());
    EXPECT_EQ(controller->getCursorPosition(), 0);
}

TEST_F(InputControllerEdgeCasesTest, VeryLongInput) {
    std::string longInput(10000, 'x');
    controller->setBuffer(longInput);
    // Should handle long input
}

TEST_F(InputControllerEdgeCasesTest, SpecialCharacters) {
    controller->setBuffer("hello\tworld\n");
    EXPECT_EQ(controller->getBuffer(), "hello\tworld\n");
}

TEST_F(InputControllerEdgeCasesTest, UnicodeCharacters) {
    controller->setBuffer("你好世界");
    EXPECT_EQ(controller->getBuffer(), "你好世界");
}

TEST_F(InputControllerEdgeCasesTest, EmptyHistoryNavigation) {
    controller->historyPrevious();
    controller->historyNext();
    // Should not crash with empty history
}

}  // namespace lithium::debug::terminal::test
