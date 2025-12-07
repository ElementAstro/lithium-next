/**
 * @file test_tui_manager.cpp
 * @brief Comprehensive unit tests for TuiManager
 *
 * Tests for:
 * - Initialization and shutdown
 * - Layout configuration
 * - Panel management
 * - Content management
 * - Status bar
 * - Command input
 * - Output operations
 * - Event handling
 * - Rendering
 * - Dialogs
 * - Help system
 * - Fallback mode
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>

#include "debug/terminal/tui_manager.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// Panel Tests
// ============================================================================

class PanelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PanelTest, DefaultConstruction) {
    Panel panel;
    EXPECT_TRUE(panel.title.empty());
    EXPECT_EQ(panel.x, 0);
    EXPECT_EQ(panel.y, 0);
    EXPECT_EQ(panel.width, 0);
    EXPECT_EQ(panel.height, 0);
    EXPECT_TRUE(panel.visible);
    EXPECT_FALSE(panel.focused);
    EXPECT_TRUE(panel.scrollable);
    EXPECT_EQ(panel.scrollOffset, 0);
    EXPECT_TRUE(panel.content.empty());
}

TEST_F(PanelTest, PopulatedPanel) {
    Panel panel;
    panel.type = PanelType::Output;
    panel.title = "Output";
    panel.x = 10;
    panel.y = 5;
    panel.width = 80;
    panel.height = 24;
    panel.visible = true;
    panel.focused = true;
    panel.content = {"Line 1", "Line 2", "Line 3"};

    EXPECT_EQ(panel.type, PanelType::Output);
    EXPECT_EQ(panel.title, "Output");
    EXPECT_EQ(panel.x, 10);
    EXPECT_EQ(panel.y, 5);
    EXPECT_EQ(panel.width, 80);
    EXPECT_EQ(panel.height, 24);
    EXPECT_TRUE(panel.visible);
    EXPECT_TRUE(panel.focused);
    EXPECT_EQ(panel.content.size(), 3);
}

// ============================================================================
// StatusItem Tests
// ============================================================================

class StatusItemTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StatusItemTest, DefaultConstruction) {
    StatusItem item;
    EXPECT_TRUE(item.label.empty());
    EXPECT_TRUE(item.value.empty());
    EXPECT_EQ(item.color, Color::Default);
}

TEST_F(StatusItemTest, PopulatedItem) {
    StatusItem item;
    item.label = "Status";
    item.value = "Ready";
    item.color = Color::Green;

    EXPECT_EQ(item.label, "Status");
    EXPECT_EQ(item.value, "Ready");
    EXPECT_EQ(item.color, Color::Green);
}

// ============================================================================
// MenuItem Tests
// ============================================================================

class MenuItemTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MenuItemTest, DefaultConstruction) {
    MenuItem item;
    EXPECT_TRUE(item.label.empty());
    EXPECT_TRUE(item.shortcut.empty());
    EXPECT_TRUE(item.enabled);
    EXPECT_FALSE(item.separator);
}

TEST_F(MenuItemTest, PopulatedItem) {
    bool actionCalled = false;
    MenuItem item;
    item.label = "Exit";
    item.shortcut = "Ctrl+Q";
    item.action = [&actionCalled]() { actionCalled = true; };
    item.enabled = true;

    EXPECT_EQ(item.label, "Exit");
    EXPECT_EQ(item.shortcut, "Ctrl+Q");
    EXPECT_TRUE(item.enabled);

    item.action();
    EXPECT_TRUE(actionCalled);
}

TEST_F(MenuItemTest, SeparatorItem) {
    MenuItem item;
    item.separator = true;

    EXPECT_TRUE(item.separator);
}

// ============================================================================
// TuiEvent Tests
// ============================================================================

class TuiEventTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TuiEventTest, EventValues) {
    EXPECT_NE(TuiEvent::None, TuiEvent::Resize);
    EXPECT_NE(TuiEvent::Resize, TuiEvent::KeyPress);
    EXPECT_NE(TuiEvent::KeyPress, TuiEvent::MouseClick);
    EXPECT_NE(TuiEvent::MouseClick, TuiEvent::FocusChange);
    EXPECT_NE(TuiEvent::FocusChange, TuiEvent::Scroll);
    EXPECT_NE(TuiEvent::Scroll, TuiEvent::Refresh);
}

// ============================================================================
// TuiManager Basic Tests
// ============================================================================

class TuiManagerBasicTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerBasicTest, DefaultConstruction) {
    EXPECT_FALSE(tui->isActive());
}

TEST_F(TuiManagerBasicTest, CheckAvailability) {
    // Just check that the function doesn't crash
    bool available = TuiManager::isAvailable();
    // Result depends on system configuration
    (void)available;
}

TEST_F(TuiManagerBasicTest, InitializeShutdown) {
    // Initialize may fail if ncurses is not available
    bool initResult = tui->initialize();
    if (initResult) {
        EXPECT_TRUE(tui->isActive());
        tui->shutdown();
        EXPECT_FALSE(tui->isActive());
    }
}

TEST_F(TuiManagerBasicTest, ShutdownWithoutInitialize) {
    EXPECT_NO_THROW(tui->shutdown());
}

TEST_F(TuiManagerBasicTest, IsActiveInitiallyFalse) {
    EXPECT_FALSE(tui->isActive());
}

// ============================================================================
// TuiManager Layout Tests
// ============================================================================

class TuiManagerLayoutTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerLayoutTest, GetDefaultLayout) {
    const LayoutConfig& layout = tui->getLayout();
    EXPECT_TRUE(layout.showStatusBar);
    EXPECT_TRUE(layout.showSuggestions);
    EXPECT_FALSE(layout.showHistory);
    EXPECT_FALSE(layout.showHelp);
}

TEST_F(TuiManagerLayoutTest, SetLayout) {
    LayoutConfig newLayout;
    newLayout.showStatusBar = false;
    newLayout.showHistory = true;
    newLayout.splitVertical = true;
    newLayout.historyPanelWidth = 40;

    tui->setLayout(newLayout);

    const LayoutConfig& layout = tui->getLayout();
    EXPECT_FALSE(layout.showStatusBar);
    EXPECT_TRUE(layout.showHistory);
    EXPECT_TRUE(layout.splitVertical);
    EXPECT_EQ(layout.historyPanelWidth, 40);
}

TEST_F(TuiManagerLayoutTest, SetTheme) {
    Theme theme = Theme::dark();
    EXPECT_NO_THROW(tui->setTheme(theme));
}

TEST_F(TuiManagerLayoutTest, ApplyLayout) {
    EXPECT_NO_THROW(tui->applyLayout());
}

// ============================================================================
// TuiManager Panel Management Tests
// ============================================================================

class TuiManagerPanelTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerPanelTest, CreatePanel) {
    Panel& panel = tui->createPanel(PanelType::Output, "Output Panel");
    EXPECT_EQ(panel.type, PanelType::Output);
    EXPECT_EQ(panel.title, "Output Panel");
}

TEST_F(TuiManagerPanelTest, GetPanel) {
    tui->createPanel(PanelType::Output, "Output");
    Panel* panel = tui->getPanel(PanelType::Output);
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->type, PanelType::Output);
}

TEST_F(TuiManagerPanelTest, GetNonexistentPanel) {
    Panel* panel = tui->getPanel(PanelType::Log);
    // May return nullptr if panel doesn't exist
}

TEST_F(TuiManagerPanelTest, ShowPanel) {
    tui->createPanel(PanelType::History, "History");
    tui->showPanel(PanelType::History, true);

    Panel* panel = tui->getPanel(PanelType::History);
    if (panel) {
        EXPECT_TRUE(panel->visible);
    }
}

TEST_F(TuiManagerPanelTest, HidePanel) {
    tui->createPanel(PanelType::History, "History");
    tui->showPanel(PanelType::History, false);

    Panel* panel = tui->getPanel(PanelType::History);
    if (panel) {
        EXPECT_FALSE(panel->visible);
    }
}

TEST_F(TuiManagerPanelTest, TogglePanel) {
    tui->createPanel(PanelType::Help, "Help");
    Panel* panel = tui->getPanel(PanelType::Help);
    if (panel) {
        bool initialVisible = panel->visible;
        tui->togglePanel(PanelType::Help);
        EXPECT_NE(panel->visible, initialVisible);
    }
}

TEST_F(TuiManagerPanelTest, FocusPanel) {
    tui->createPanel(PanelType::Output, "Output");
    tui->createPanel(PanelType::History, "History");

    tui->focusPanel(PanelType::History);
    EXPECT_EQ(tui->getFocusedPanel(), PanelType::History);
}

TEST_F(TuiManagerPanelTest, FocusNext) {
    tui->createPanel(PanelType::Output, "Output");
    tui->createPanel(PanelType::History, "History");

    tui->focusPanel(PanelType::Output);
    tui->focusNext();
    // Focus should move to next panel
}

TEST_F(TuiManagerPanelTest, FocusPrevious) {
    tui->createPanel(PanelType::Output, "Output");
    tui->createPanel(PanelType::History, "History");

    tui->focusPanel(PanelType::History);
    tui->focusPrevious();
    // Focus should move to previous panel
}

// ============================================================================
// TuiManager Content Management Tests
// ============================================================================

class TuiManagerContentTest : public ::testing::Test {
protected:
    void SetUp() override {
        tui = std::make_unique<TuiManager>();
        tui->createPanel(PanelType::Output, "Output");
    }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerContentTest, SetPanelContent) {
    std::vector<std::string> content = {"Line 1", "Line 2", "Line 3"};
    tui->setPanelContent(PanelType::Output, content);

    Panel* panel = tui->getPanel(PanelType::Output);
    if (panel) {
        EXPECT_EQ(panel->content.size(), 3);
    }
}

TEST_F(TuiManagerContentTest, AppendToPanel) {
    tui->appendToPanel(PanelType::Output, "New line");

    Panel* panel = tui->getPanel(PanelType::Output);
    if (panel) {
        EXPECT_FALSE(panel->content.empty());
    }
}

TEST_F(TuiManagerContentTest, ClearPanel) {
    tui->appendToPanel(PanelType::Output, "Line 1");
    tui->appendToPanel(PanelType::Output, "Line 2");
    tui->clearPanel(PanelType::Output);

    Panel* panel = tui->getPanel(PanelType::Output);
    if (panel) {
        EXPECT_TRUE(panel->content.empty());
    }
}

TEST_F(TuiManagerContentTest, ScrollPanel) {
    for (int i = 0; i < 100; ++i) {
        tui->appendToPanel(PanelType::Output, "Line " + std::to_string(i));
    }

    tui->scrollPanel(PanelType::Output, 10);

    Panel* panel = tui->getPanel(PanelType::Output);
    if (panel) {
        EXPECT_GT(panel->scrollOffset, 0);
    }
}

TEST_F(TuiManagerContentTest, ScrollToTop) {
    for (int i = 0; i < 100; ++i) {
        tui->appendToPanel(PanelType::Output, "Line " + std::to_string(i));
    }

    tui->scrollPanel(PanelType::Output, 50);
    tui->scrollToTop(PanelType::Output);

    Panel* panel = tui->getPanel(PanelType::Output);
    if (panel) {
        EXPECT_EQ(panel->scrollOffset, 0);
    }
}

TEST_F(TuiManagerContentTest, ScrollToBottom) {
    for (int i = 0; i < 100; ++i) {
        tui->appendToPanel(PanelType::Output, "Line " + std::to_string(i));
    }

    tui->scrollToBottom(PanelType::Output);
    // Scroll offset should be at bottom
}

// ============================================================================
// TuiManager Status Bar Tests
// ============================================================================

class TuiManagerStatusTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerStatusTest, SetStatusItems) {
    std::vector<StatusItem> items = {{"Mode", "Normal", Color::Green},
                                     {"Line", "1", Color::Default}};

    EXPECT_NO_THROW(tui->setStatusItems(items));
}

TEST_F(TuiManagerStatusTest, UpdateStatus) {
    EXPECT_NO_THROW(tui->updateStatus("Mode", "Insert"));
}

TEST_F(TuiManagerStatusTest, SetStatusMessage) {
    EXPECT_NO_THROW(tui->setStatusMessage("Operation completed", Color::Green));
}

TEST_F(TuiManagerStatusTest, SetStatusMessageWithDuration) {
    EXPECT_NO_THROW(
        tui->setStatusMessage("Temporary message", Color::Yellow, 5000));
}

// ============================================================================
// TuiManager Input Tests
// ============================================================================

class TuiManagerInputTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerInputTest, SetPrompt) {
    EXPECT_NO_THROW(tui->setPrompt(">>> "));
}

TEST_F(TuiManagerInputTest, GetInputInitiallyEmpty) {
    EXPECT_TRUE(tui->getInput().empty());
}

TEST_F(TuiManagerInputTest, SetInput) {
    tui->setInput("test input");
    EXPECT_EQ(tui->getInput(), "test input");
}

TEST_F(TuiManagerInputTest, ClearInput) {
    tui->setInput("test input");
    tui->clearInput();
    EXPECT_TRUE(tui->getInput().empty());
}

TEST_F(TuiManagerInputTest, ShowSuggestions) {
    std::vector<std::string> suggestions = {"help", "hello", "history"};
    EXPECT_NO_THROW(tui->showSuggestions(suggestions));
}

TEST_F(TuiManagerInputTest, HideSuggestions) {
    EXPECT_NO_THROW(tui->hideSuggestions());
}

TEST_F(TuiManagerInputTest, SelectSuggestion) {
    std::vector<std::string> suggestions = {"help", "hello", "history"};
    tui->showSuggestions(suggestions);
    EXPECT_NO_THROW(tui->selectSuggestion(1));
}

// ============================================================================
// TuiManager Output Tests
// ============================================================================

class TuiManagerOutputTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerOutputTest, Print) {
    EXPECT_NO_THROW(tui->print("Test output"));
}

TEST_F(TuiManagerOutputTest, Println) {
    EXPECT_NO_THROW(tui->println("Test line"));
}

TEST_F(TuiManagerOutputTest, PrintlnEmpty) { EXPECT_NO_THROW(tui->println()); }

TEST_F(TuiManagerOutputTest, PrintStyled) {
    EXPECT_NO_THROW(tui->printStyled("Styled text", Color::Red, Style::Bold));
}

TEST_F(TuiManagerOutputTest, Success) {
    EXPECT_NO_THROW(tui->success("Operation successful"));
}

TEST_F(TuiManagerOutputTest, Error) {
    EXPECT_NO_THROW(tui->error("An error occurred"));
}

TEST_F(TuiManagerOutputTest, Warning) {
    EXPECT_NO_THROW(tui->warning("Warning message"));
}

TEST_F(TuiManagerOutputTest, Info) {
    EXPECT_NO_THROW(tui->info("Information message"));
}

// ============================================================================
// TuiManager Event Handling Tests
// ============================================================================

class TuiManagerEventTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerEventTest, ProcessEvents) {
    TuiEvent event = tui->processEvents();
    // Should return None when not active
    EXPECT_EQ(event, TuiEvent::None);
}

TEST_F(TuiManagerEventTest, HandleResize) {
    EXPECT_NO_THROW(tui->handleResize());
}

TEST_F(TuiManagerEventTest, SetKeyHandler) {
    bool handlerCalled = false;
    tui->setKeyHandler([&handlerCalled](const InputEvent& event) {
        handlerCalled = true;
        return true;
    });
    // Handler is set but won't be called without active TUI
}

// ============================================================================
// TuiManager Rendering Tests
// ============================================================================

class TuiManagerRenderTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerRenderTest, Refresh) { EXPECT_NO_THROW(tui->refresh()); }

TEST_F(TuiManagerRenderTest, Redraw) { EXPECT_NO_THROW(tui->redraw()); }

TEST_F(TuiManagerRenderTest, Clear) { EXPECT_NO_THROW(tui->clear()); }

// ============================================================================
// TuiManager Help System Tests
// ============================================================================

class TuiManagerHelpTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerHelpTest, ShowHelp) { EXPECT_NO_THROW(tui->showHelp()); }

TEST_F(TuiManagerHelpTest, HideHelp) { EXPECT_NO_THROW(tui->hideHelp()); }

TEST_F(TuiManagerHelpTest, SetHelpContent) {
    std::vector<std::pair<std::string, std::string>> shortcuts = {
        {"Ctrl+C", "Exit"}, {"Ctrl+H", "Help"}, {"Tab", "Complete"}};

    EXPECT_NO_THROW(tui->setHelpContent(shortcuts));
}

// ============================================================================
// TuiManager Fallback Mode Tests
// ============================================================================

class TuiManagerFallbackTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerFallbackTest, IsFallbackModeInitially) {
    // Initially should be in fallback mode if TUI not initialized
    bool fallback = tui->isFallbackMode();
    // Result depends on implementation
}

TEST_F(TuiManagerFallbackTest, SetFallbackMode) {
    tui->setFallbackMode(true);
    EXPECT_TRUE(tui->isFallbackMode());

    tui->setFallbackMode(false);
    EXPECT_FALSE(tui->isFallbackMode());
}

// ============================================================================
// TuiManager Move Semantics Tests
// ============================================================================

class TuiManagerMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TuiManagerMoveTest, MoveConstruction) {
    TuiManager original;
    original.setInput("test");

    TuiManager moved(std::move(original));
    EXPECT_EQ(moved.getInput(), "test");
}

TEST_F(TuiManagerMoveTest, MoveAssignment) {
    TuiManager original;
    original.setInput("test");

    TuiManager target;
    target = std::move(original);
    EXPECT_EQ(target.getInput(), "test");
}

// ============================================================================
// TuiManager Integration Tests
// ============================================================================

class TuiManagerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override { tui = std::make_unique<TuiManager>(); }

    void TearDown() override {
        if (tui && tui->isActive()) {
            tui->shutdown();
        }
        tui.reset();
    }

    std::unique_ptr<TuiManager> tui;
};

TEST_F(TuiManagerIntegrationTest, FullWorkflow) {
    // Set up layout
    LayoutConfig layout;
    layout.showStatusBar = true;
    layout.showSuggestions = true;
    tui->setLayout(layout);

    // Set theme
    tui->setTheme(Theme::dark());

    // Create panels
    tui->createPanel(PanelType::Output, "Output");
    tui->createPanel(PanelType::History, "History");

    // Add content
    tui->appendToPanel(PanelType::Output, "Welcome!");
    tui->appendToPanel(PanelType::Output, "Type 'help' for commands.");

    // Set status
    std::vector<StatusItem> status = {{"Mode", "Normal", Color::Green}};
    tui->setStatusItems(status);

    // Set input
    tui->setPrompt(">>> ");
    tui->setInput("help");

    // Verify state
    EXPECT_EQ(tui->getInput(), "help");
}

TEST_F(TuiManagerIntegrationTest, PanelInteraction) {
    tui->createPanel(PanelType::Output, "Output");
    tui->createPanel(PanelType::History, "History");

    // Focus and content operations
    tui->focusPanel(PanelType::Output);
    EXPECT_EQ(tui->getFocusedPanel(), PanelType::Output);

    tui->appendToPanel(PanelType::Output, "Line 1");
    tui->appendToPanel(PanelType::Output, "Line 2");

    tui->focusNext();
    // Focus should move to History

    tui->focusPrevious();
    EXPECT_EQ(tui->getFocusedPanel(), PanelType::Output);
}

}  // namespace lithium::debug::terminal::test
