/**
 * @file tui_manager.cpp
 * @brief Implementation of TUI manager
 */

#include "tui_manager.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

#include "history_manager.hpp"
#include "input_controller.hpp"
#include "renderer.hpp"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

#if __has_include(<ncurses.h>)
#include <ncurses.h>
#define HAS_NCURSES 1
#elif __has_include(<ncurses/ncurses.h>)
#include <ncurses/ncurses.h>
#define HAS_NCURSES 1
#elif __has_include(<curses.h>)
#include <curses.h>
#define HAS_NCURSES 1
#else
#define HAS_NCURSES 0
#endif

namespace lithium::debug::terminal {

class TuiManager::Impl {
public:
    std::shared_ptr<ConsoleRenderer> renderer_;
    std::shared_ptr<InputController> input_;
    std::shared_ptr<HistoryManager> history_;

    LayoutConfig layout_;
    Theme theme_;

    std::map<PanelType, Panel> panels_;
    PanelType focusedPanel_{PanelType::Command};

    std::vector<StatusItem> statusItems_;
    std::string statusMessage_;
    std::chrono::steady_clock::time_point statusMessageExpiry_;

    std::string prompt_{">"};
    std::string currentInput_;
    size_t cursorPos_{0};

    std::vector<std::string> suggestions_;
    int selectedSuggestion_{-1};
    bool suggestionsVisible_{false};

    std::vector<std::pair<std::string, std::string>> helpContent_;

    std::function<bool(const InputEvent&)> keyHandler_;

    bool active_{false};
    bool fallbackMode_{false};

    std::mutex mutex_;

#if HAS_NCURSES
    std::map<PanelType, WINDOW*> windows_;
    WINDOW* statusWin_{nullptr};
    WINDOW* inputWin_{nullptr};
#endif

    Impl() {
        renderer_ = std::make_shared<ConsoleRenderer>();
        input_ = std::make_shared<InputController>();
        history_ = std::make_shared<HistoryManager>();
        initializeDefaultPanels();
    }

    Impl(std::shared_ptr<ConsoleRenderer> renderer,
         std::shared_ptr<InputController> input,
         std::shared_ptr<HistoryManager> history)
        : renderer_(renderer), input_(input), history_(history) {
        if (!renderer_)
            renderer_ = std::make_shared<ConsoleRenderer>();
        if (!input_)
            input_ = std::make_shared<InputController>();
        if (!history_)
            history_ = std::make_shared<HistoryManager>();
        initializeDefaultPanels();
    }

    ~Impl() { shutdown(); }

    void initializeDefaultPanels() {
        // Create default panels
        panels_[PanelType::Command] =
            Panel{PanelType::Command, "Command", 0, 0, 0, 1, true, true};
        panels_[PanelType::Output] =
            Panel{PanelType::Output, "Output", 0, 0, 0, 0, true, false};
        panels_[PanelType::History] =
            Panel{PanelType::History, "History", 0, 0, 0, 0, false, false};
        panels_[PanelType::Suggestions] = Panel{
            PanelType::Suggestions, "Suggestions", 0, 0, 0, 0, false, false};
        panels_[PanelType::Status] =
            Panel{PanelType::Status, "Status", 0, 0, 0, 1, true, false};
        panels_[PanelType::Help] =
            Panel{PanelType::Help, "Help", 0, 0, 0, 0, false, false};
        panels_[PanelType::Log] =
            Panel{PanelType::Log, "Log", 0, 0, 0, 0, false, false};

        // Default help content
        helpContent_ = {
            {"Ctrl+C", "Exit / Cancel"},       {"Ctrl+L", "Clear screen"},
            {"Ctrl+R", "Reverse search"},      {"Tab", "Auto-complete"},
            {"Up/Down", "History navigation"}, {"F1", "Toggle help"},
            {"F2", "Toggle history panel"},    {"F3", "Toggle log panel"}};
    }

    bool initialize() {
#if HAS_NCURSES
        if (!fallbackMode_) {
            // Initialize ncurses
            initscr();

            if (has_colors()) {
                start_color();
                use_default_colors();
                initColorPairs();
            }

            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            nodelay(stdscr, TRUE);
            curs_set(1);

            // Create windows
            createWindows();

            active_ = true;
            return true;
        }
#endif

        // Fallback mode
        fallbackMode_ = true;
        active_ = true;
        return true;
    }

    void shutdown() {
        if (!active_)
            return;

#if HAS_NCURSES
        if (!fallbackMode_) {
            // Destroy windows
            for (auto& [type, win] : windows_) {
                if (win) {
                    delwin(win);
                    win = nullptr;
                }
            }

            if (statusWin_) {
                delwin(statusWin_);
                statusWin_ = nullptr;
            }

            if (inputWin_) {
                delwin(inputWin_);
                inputWin_ = nullptr;
            }

            endwin();
        }
#endif

        active_ = false;
    }

#if HAS_NCURSES
    void initColorPairs() {
        // Initialize color pairs for theming
        init_pair(1, COLOR_RED, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_BLUE, -1);
        init_pair(5, COLOR_MAGENTA, -1);
        init_pair(6, COLOR_CYAN, -1);
        init_pair(7, COLOR_WHITE, -1);
    }

    void createWindows() {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        // Calculate panel sizes based on layout
        int statusHeight = layout_.showStatusBar ? layout_.statusBarHeight : 0;
        int historyWidth = layout_.showHistory ? layout_.historyPanelWidth : 0;
        int suggestHeight =
            layout_.showSuggestions ? layout_.suggestionPanelHeight : 0;

        int outputHeight =
            maxY - 1 - statusHeight - suggestHeight;  // -1 for input line
        int outputWidth = maxX - historyWidth;

        // Create output window
        if (windows_[PanelType::Output]) {
            delwin(windows_[PanelType::Output]);
        }
        windows_[PanelType::Output] = newwin(outputHeight, outputWidth, 0, 0);
        scrollok(windows_[PanelType::Output], TRUE);

        auto& outputPanel = panels_[PanelType::Output];
        outputPanel.x = 0;
        outputPanel.y = 0;
        outputPanel.width = outputWidth;
        outputPanel.height = outputHeight;

        // Create history window if enabled
        if (layout_.showHistory) {
            if (windows_[PanelType::History]) {
                delwin(windows_[PanelType::History]);
            }
            windows_[PanelType::History] =
                newwin(outputHeight, historyWidth, 0, outputWidth);
            scrollok(windows_[PanelType::History], TRUE);

            auto& histPanel = panels_[PanelType::History];
            histPanel.x = outputWidth;
            histPanel.y = 0;
            histPanel.width = historyWidth;
            histPanel.height = outputHeight;
            histPanel.visible = true;
        }

        // Create suggestions window if enabled
        if (layout_.showSuggestions) {
            if (windows_[PanelType::Suggestions]) {
                delwin(windows_[PanelType::Suggestions]);
            }
            windows_[PanelType::Suggestions] =
                newwin(suggestHeight, maxX, outputHeight, 0);

            auto& suggestPanel = panels_[PanelType::Suggestions];
            suggestPanel.x = 0;
            suggestPanel.y = outputHeight;
            suggestPanel.width = maxX;
            suggestPanel.height = suggestHeight;
        }

        // Create input window
        if (inputWin_) {
            delwin(inputWin_);
        }
        inputWin_ = newwin(1, maxX, maxY - 1 - statusHeight, 0);
        keypad(inputWin_, TRUE);

        // Create status bar window
        if (layout_.showStatusBar) {
            if (statusWin_) {
                delwin(statusWin_);
            }
            statusWin_ = newwin(statusHeight, maxX, maxY - statusHeight, 0);
        }
    }

    int colorToNcurses(Color color) {
        switch (color) {
            case Color::Red:
            case Color::BrightRed:
                return COLOR_PAIR(1);
            case Color::Green:
            case Color::BrightGreen:
                return COLOR_PAIR(2);
            case Color::Yellow:
            case Color::BrightYellow:
                return COLOR_PAIR(3);
            case Color::Blue:
            case Color::BrightBlue:
                return COLOR_PAIR(4);
            case Color::Magenta:
            case Color::BrightMagenta:
                return COLOR_PAIR(5);
            case Color::Cyan:
            case Color::BrightCyan:
                return COLOR_PAIR(6);
            case Color::White:
            case Color::BrightWhite:
                return COLOR_PAIR(7);
            default:
                return 0;
        }
    }
#endif

    void applyLayout() {
#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            createWindows();
            redraw();
        }
#endif
    }

    void refresh() {
        std::lock_guard<std::mutex> lock(mutex_);

#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            // Refresh all visible windows
            for (auto& [type, panel] : panels_) {
                if (panel.visible && windows_.count(type) && windows_[type]) {
                    wrefresh(windows_[type]);
                }
            }

            if (inputWin_) {
                wrefresh(inputWin_);
            }

            if (statusWin_ && layout_.showStatusBar) {
                wrefresh(statusWin_);
            }

            doupdate();
        }
#endif
    }

    void redraw() {
        std::lock_guard<std::mutex> lock(mutex_);

#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            ::clear();

            // Redraw all panels
            for (auto& [type, panel] : panels_) {
                if (panel.visible) {
                    drawPanel(type);
                }
            }

            drawInput();
            drawStatusBar();

            refresh();
        }
#endif
    }

#if HAS_NCURSES
    void drawPanel(PanelType type) {
        if (!windows_.count(type) || !windows_[type])
            return;

        auto& panel = panels_[type];
        WINDOW* win = windows_[type];

        werase(win);

        // Draw border if focused
        if (panel.focused) {
            wattron(win, A_BOLD);
            box(win, 0, 0);
            wattroff(win, A_BOLD);
        } else {
            box(win, 0, 0);
        }

        // Draw title
        if (!panel.title.empty()) {
            mvwprintw(win, 0, 2, " %s ", panel.title.c_str());
        }

        // Draw content
        int startLine = panel.scrollOffset;
        int maxLines = panel.height - 2;  // Account for border

        for (int i = 0;
             i < maxLines &&
             (startLine + i) < static_cast<int>(panel.content.size());
             ++i) {
            mvwprintw(win, i + 1, 1, "%.*s", panel.width - 2,
                      panel.content[startLine + i].c_str());
        }

        wnoutrefresh(win);
    }

    void drawInput() {
        if (!inputWin_)
            return;

        werase(inputWin_);

        // Draw prompt and input
        wattron(inputWin_, colorToNcurses(theme_.promptColor) | A_BOLD);
        wprintw(inputWin_, "%s ", prompt_.c_str());
        wattroff(inputWin_, colorToNcurses(theme_.promptColor) | A_BOLD);

        wprintw(inputWin_, "%s", currentInput_.c_str());

        // Position cursor
        wmove(inputWin_, 0, prompt_.size() + 1 + cursorPos_);

        wnoutrefresh(inputWin_);
    }

    void drawStatusBar() {
        if (!statusWin_ || !layout_.showStatusBar)
            return;

        werase(statusWin_);

        // Check for temporary message
        auto now = std::chrono::steady_clock::now();
        if (!statusMessage_.empty() && now < statusMessageExpiry_) {
            wattron(statusWin_, A_REVERSE);
            mvwprintw(statusWin_, 0, 0, " %s ", statusMessage_.c_str());
            wattroff(statusWin_, A_REVERSE);
        } else {
            // Draw status items
            int x = 0;
            for (const auto& item : statusItems_) {
                wattron(statusWin_, A_REVERSE);
                mvwprintw(statusWin_, 0, x, " %s: %s ", item.label.c_str(),
                          item.value.c_str());
                wattroff(statusWin_, A_REVERSE);
                x += item.label.size() + item.value.size() + 5;
            }
        }

        wnoutrefresh(statusWin_);
    }
#endif

    TuiEvent processEvents() {
#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            int ch = getch();

            if (ch == ERR) {
                return TuiEvent::None;
            }

            if (ch == KEY_RESIZE) {
                handleResize();
                return TuiEvent::Resize;
            }

            // Handle key
            InputEvent event;
            event.character = static_cast<char>(ch);

            switch (ch) {
                case KEY_UP:
                    event.key = Key::Up;
                    event.isSpecialKey = true;
                    break;
                case KEY_DOWN:
                    event.key = Key::Down;
                    event.isSpecialKey = true;
                    break;
                case KEY_LEFT:
                    event.key = Key::Left;
                    event.isSpecialKey = true;
                    break;
                case KEY_RIGHT:
                    event.key = Key::Right;
                    event.isSpecialKey = true;
                    break;
                case KEY_BACKSPACE:
                case 127:
                    event.key = Key::Backspace;
                    event.isSpecialKey = true;
                    break;
                case '\n':
                case '\r':
                    event.key = Key::Enter;
                    event.isSpecialKey = true;
                    break;
                case '\t':
                    event.key = Key::Tab;
                    event.isSpecialKey = true;
                    break;
                case 27:
                    event.key = Key::Escape;
                    event.isSpecialKey = true;
                    break;
                case KEY_F(1):
                    event.key = Key::F1;
                    event.isSpecialKey = true;
                    break;
                case KEY_F(2):
                    event.key = Key::F2;
                    event.isSpecialKey = true;
                    break;
                case KEY_F(3):
                    event.key = Key::F3;
                    event.isSpecialKey = true;
                    break;
                default:
                    if (ch >= 1 && ch <= 26) {
                        event.ctrl = true;
                        event.hasModifier = true;
                        event.key = static_cast<Key>(ch);
                    }
                    break;
            }

            // Call key handler if set
            if (keyHandler_ && keyHandler_(event)) {
                return TuiEvent::KeyPress;
            }

            // Default key handling
            handleDefaultKey(event, ch);

            return TuiEvent::KeyPress;
        }
#endif

        return TuiEvent::None;
    }

    void handleDefaultKey(const InputEvent& event, int ch) {
        (void)event;

        switch (ch) {
#if HAS_NCURSES
            case KEY_UP:
                // History previous
                if (auto entry = history_->previous()) {
                    currentInput_ = entry->command;
                    cursorPos_ = currentInput_.size();
                    drawInput();
                    refresh();
                }
                break;

            case KEY_DOWN:
                // History next
                if (auto entry = history_->next()) {
                    currentInput_ = entry->command;
                    cursorPos_ = currentInput_.size();
                } else {
                    currentInput_.clear();
                    cursorPos_ = 0;
                }
                drawInput();
                refresh();
                break;

            case KEY_LEFT:
                if (cursorPos_ > 0) {
                    cursorPos_--;
                    drawInput();
                    refresh();
                }
                break;

            case KEY_RIGHT:
                if (cursorPos_ < currentInput_.size()) {
                    cursorPos_++;
                    drawInput();
                    refresh();
                }
                break;

            case KEY_BACKSPACE:
            case 127:
                if (cursorPos_ > 0) {
                    currentInput_.erase(cursorPos_ - 1, 1);
                    cursorPos_--;
                    drawInput();
                    refresh();
                }
                break;

            case KEY_DC:  // Delete
                if (cursorPos_ < currentInput_.size()) {
                    currentInput_.erase(cursorPos_, 1);
                    drawInput();
                    refresh();
                }
                break;

            case KEY_HOME:
                cursorPos_ = 0;
                drawInput();
                refresh();
                break;

            case KEY_END:
                cursorPos_ = currentInput_.size();
                drawInput();
                refresh();
                break;

            case KEY_F(1):
                togglePanel(PanelType::Help);
                break;

            case KEY_F(2):
                togglePanel(PanelType::History);
                break;

            case KEY_F(3):
                togglePanel(PanelType::Log);
                break;
#endif

            default:
                // Regular character input
                if (ch >= 32 && ch < 127) {
                    currentInput_.insert(cursorPos_, 1, static_cast<char>(ch));
                    cursorPos_++;
#if HAS_NCURSES
                    drawInput();
                    refresh();
#endif
                }
                break;
        }
    }

    void handleResize() {
#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            endwin();
            refresh();
            createWindows();
            redraw();
        }
#endif
    }

    void togglePanel(PanelType type) {
        auto& panel = panels_[type];
        panel.visible = !panel.visible;

        // Update layout config
        switch (type) {
            case PanelType::History:
                layout_.showHistory = panel.visible;
                break;
            case PanelType::Help:
                layout_.showHelp = panel.visible;
                break;
            default:
                break;
        }

        applyLayout();
    }

    void appendToOutput(const std::string& line) {
        auto& panel = panels_[PanelType::Output];
        panel.content.push_back(line);

        // Auto-scroll to bottom
        if (panel.content.size() > static_cast<size_t>(panel.height - 2)) {
            panel.scrollOffset = panel.content.size() - (panel.height - 2);
        }

#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            drawPanel(PanelType::Output);
            refresh();
        } else
#endif
        {
            std::cout << line << std::endl;
        }
    }

    void printStyled(const std::string& text, Color fg, Style style) {
#if HAS_NCURSES
        if (!fallbackMode_ && active_) {
            auto& panel = panels_[PanelType::Output];
            panel.content.push_back(text);

            if (windows_.count(PanelType::Output) &&
                windows_[PanelType::Output]) {
                WINDOW* win = windows_[PanelType::Output];
                wattron(win, colorToNcurses(fg));
                if (style == Style::Bold) {
                    wattron(win, A_BOLD);
                }
                // Content will be drawn in drawPanel
                if (style == Style::Bold) {
                    wattroff(win, A_BOLD);
                }
                wattroff(win, colorToNcurses(fg));
            }

            drawPanel(PanelType::Output);
            refresh();
        } else
#endif
        {
            (void)style;
            renderer_->println(text, fg);
        }
    }
};

// TuiManager implementation

TuiManager::TuiManager() : impl_(std::make_unique<Impl>()) {}

TuiManager::TuiManager(std::shared_ptr<ConsoleRenderer> renderer,
                       std::shared_ptr<InputController> input,
                       std::shared_ptr<HistoryManager> history)
    : impl_(std::make_unique<Impl>(renderer, input, history)) {}

TuiManager::~TuiManager() = default;

TuiManager::TuiManager(TuiManager&&) noexcept = default;
TuiManager& TuiManager::operator=(TuiManager&&) noexcept = default;

bool TuiManager::isAvailable() {
#if HAS_NCURSES
    return true;
#else
    return false;
#endif
}

bool TuiManager::initialize() { return impl_->initialize(); }

void TuiManager::shutdown() { impl_->shutdown(); }

bool TuiManager::isActive() const { return impl_->active_; }

void TuiManager::setLayout(const LayoutConfig& config) {
    impl_->layout_ = config;
}

const LayoutConfig& TuiManager::getLayout() const { return impl_->layout_; }

void TuiManager::setTheme(const Theme& theme) {
    impl_->theme_ = theme;
    if (impl_->renderer_) {
        impl_->renderer_->setTheme(theme);
    }
}

void TuiManager::applyLayout() { impl_->applyLayout(); }

Panel& TuiManager::createPanel(PanelType type, const std::string& title) {
    impl_->panels_[type] = Panel{type, title, 0, 0, 0, 0, true, false};
    return impl_->panels_[type];
}

Panel* TuiManager::getPanel(PanelType type) {
    auto it = impl_->panels_.find(type);
    if (it != impl_->panels_.end()) {
        return &it->second;
    }
    return nullptr;
}

void TuiManager::showPanel(PanelType type, bool show) {
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].visible = show;
        impl_->applyLayout();
    }
}

void TuiManager::togglePanel(PanelType type) { impl_->togglePanel(type); }

void TuiManager::focusPanel(PanelType type) {
    // Unfocus current
    if (impl_->panels_.count(impl_->focusedPanel_)) {
        impl_->panels_[impl_->focusedPanel_].focused = false;
    }

    // Focus new
    impl_->focusedPanel_ = type;
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].focused = true;
    }

    impl_->redraw();
}

PanelType TuiManager::getFocusedPanel() const { return impl_->focusedPanel_; }

void TuiManager::focusNext() {
    std::vector<PanelType> visiblePanels;
    for (const auto& [type, panel] : impl_->panels_) {
        if (panel.visible) {
            visiblePanels.push_back(type);
        }
    }

    if (visiblePanels.empty())
        return;

    auto it = std::find(visiblePanels.begin(), visiblePanels.end(),
                        impl_->focusedPanel_);
    if (it != visiblePanels.end()) {
        ++it;
        if (it == visiblePanels.end()) {
            it = visiblePanels.begin();
        }
        focusPanel(*it);
    }
}

void TuiManager::focusPrevious() {
    std::vector<PanelType> visiblePanels;
    for (const auto& [type, panel] : impl_->panels_) {
        if (panel.visible) {
            visiblePanels.push_back(type);
        }
    }

    if (visiblePanels.empty())
        return;

    auto it = std::find(visiblePanels.begin(), visiblePanels.end(),
                        impl_->focusedPanel_);
    if (it != visiblePanels.end()) {
        if (it == visiblePanels.begin()) {
            it = visiblePanels.end();
        }
        --it;
        focusPanel(*it);
    }
}

void TuiManager::setPanelContent(PanelType type,
                                 const std::vector<std::string>& lines) {
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].content = lines;
        impl_->panels_[type].scrollOffset = 0;
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::appendToPanel(PanelType type, const std::string& line) {
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].content.push_back(line);
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::clearPanel(PanelType type) {
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].content.clear();
        impl_->panels_[type].scrollOffset = 0;
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::scrollPanel(PanelType type, int delta) {
    if (impl_->panels_.count(type)) {
        auto& panel = impl_->panels_[type];
        int newOffset = panel.scrollOffset + delta;
        int maxOffset = std::max(
            0, static_cast<int>(panel.content.size()) - panel.height + 2);
        panel.scrollOffset = std::clamp(newOffset, 0, maxOffset);
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::scrollToTop(PanelType type) {
    if (impl_->panels_.count(type)) {
        impl_->panels_[type].scrollOffset = 0;
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::scrollToBottom(PanelType type) {
    if (impl_->panels_.count(type)) {
        auto& panel = impl_->panels_[type];
        panel.scrollOffset = std::max(
            0, static_cast<int>(panel.content.size()) - panel.height + 2);
#if HAS_NCURSES
        impl_->drawPanel(type);
        impl_->refresh();
#endif
    }
}

void TuiManager::setStatusItems(const std::vector<StatusItem>& items) {
    impl_->statusItems_ = items;
#if HAS_NCURSES
    impl_->drawStatusBar();
    impl_->refresh();
#endif
}

void TuiManager::updateStatus(const std::string& label,
                              const std::string& value) {
    for (auto& item : impl_->statusItems_) {
        if (item.label == label) {
            item.value = value;
#if HAS_NCURSES
            impl_->drawStatusBar();
            impl_->refresh();
#endif
            return;
        }
    }

    // Add new item
    impl_->statusItems_.push_back({label, value, Color::Default});
#if HAS_NCURSES
    impl_->drawStatusBar();
    impl_->refresh();
#endif
}

void TuiManager::setStatusMessage(const std::string& message, Color color,
                                  int durationMs) {
    (void)color;
    impl_->statusMessage_ = message;
    impl_->statusMessageExpiry_ = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(durationMs);
#if HAS_NCURSES
    impl_->drawStatusBar();
    impl_->refresh();
#endif
}

void TuiManager::setPrompt(const std::string& prompt) {
    impl_->prompt_ = prompt;
}

std::string TuiManager::getInput() const { return impl_->currentInput_; }

void TuiManager::setInput(const std::string& content) {
    impl_->currentInput_ = content;
    impl_->cursorPos_ = content.size();
#if HAS_NCURSES
    impl_->drawInput();
    impl_->refresh();
#endif
}

void TuiManager::clearInput() {
    impl_->currentInput_.clear();
    impl_->cursorPos_ = 0;
#if HAS_NCURSES
    impl_->drawInput();
    impl_->refresh();
#endif
}

void TuiManager::showSuggestions(const std::vector<std::string>& suggestions) {
    impl_->suggestions_ = suggestions;
    impl_->suggestionsVisible_ = true;
    impl_->selectedSuggestion_ = -1;

    setPanelContent(PanelType::Suggestions, suggestions);
    showPanel(PanelType::Suggestions, true);
}

void TuiManager::hideSuggestions() {
    impl_->suggestionsVisible_ = false;
    showPanel(PanelType::Suggestions, false);
}

void TuiManager::selectSuggestion(int index) {
    if (index >= 0 && index < static_cast<int>(impl_->suggestions_.size())) {
        impl_->selectedSuggestion_ = index;
        setInput(impl_->suggestions_[index]);
    }
}

void TuiManager::print(const std::string& text) { impl_->appendToOutput(text); }

void TuiManager::println(const std::string& text) {
    impl_->appendToOutput(text);
}

void TuiManager::printStyled(const std::string& text, Color fg, Style style) {
    impl_->printStyled(text, fg, style);
}

void TuiManager::success(const std::string& message) {
    printStyled(impl_->theme_.successSymbol + " " + message,
                impl_->theme_.successColor, Style::Normal);
}

void TuiManager::error(const std::string& message) {
    printStyled(impl_->theme_.errorSymbol + " " + message,
                impl_->theme_.errorColor, Style::Bold);
}

void TuiManager::warning(const std::string& message) {
    printStyled(impl_->theme_.warningSymbol + " " + message,
                impl_->theme_.warningColor, Style::Normal);
}

void TuiManager::info(const std::string& message) {
    printStyled(impl_->theme_.infoSymbol + " " + message,
                impl_->theme_.infoColor, Style::Normal);
}

TuiEvent TuiManager::processEvents() { return impl_->processEvents(); }

TuiEvent TuiManager::waitForEvent(int timeoutMs) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto event = processEvents();
        if (event != TuiEvent::None) {
            return event;
        }

        if (timeoutMs >= 0) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
            if (elapsed >= timeoutMs) {
                return TuiEvent::None;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TuiManager::handleResize() { impl_->handleResize(); }

void TuiManager::setKeyHandler(std::function<bool(const InputEvent&)> handler) {
    impl_->keyHandler_ = std::move(handler);
}

void TuiManager::refresh() { impl_->refresh(); }

void TuiManager::redraw() { impl_->redraw(); }

void TuiManager::clear() {
    for (auto& [type, panel] : impl_->panels_) {
        panel.content.clear();
        panel.scrollOffset = 0;
    }
    impl_->redraw();
}

void TuiManager::messageBox(const std::string& title,
                            const std::string& message) {
#if HAS_NCURSES
    if (!impl_->fallbackMode_ && impl_->active_) {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        int width = std::min(static_cast<int>(message.size()) + 4, maxX - 4);
        int height = 5;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        WINDOW* msgWin = newwin(height, width, startY, startX);
        box(msgWin, 0, 0);

        mvwprintw(msgWin, 0, 2, " %s ", title.c_str());
        mvwprintw(msgWin, 2, 2, "%s", message.c_str());
        mvwprintw(msgWin, height - 1, width / 2 - 5, " [OK] ");

        wrefresh(msgWin);

        // Wait for key
        nodelay(stdscr, FALSE);
        getch();
        nodelay(stdscr, TRUE);

        delwin(msgWin);
        impl_->redraw();
    } else
#endif
    {
        impl_->renderer_->box({message}, title);
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
}

bool TuiManager::confirm(const std::string& title, const std::string& message) {
#if HAS_NCURSES
    if (!impl_->fallbackMode_ && impl_->active_) {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        int width = std::min(static_cast<int>(message.size()) + 4, maxX - 4);
        int height = 5;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        WINDOW* msgWin = newwin(height, width, startY, startX);
        box(msgWin, 0, 0);

        mvwprintw(msgWin, 0, 2, " %s ", title.c_str());
        mvwprintw(msgWin, 2, 2, "%s", message.c_str());
        mvwprintw(msgWin, height - 1, width / 2 - 8, " [Y]es  [N]o ");

        wrefresh(msgWin);

        nodelay(stdscr, FALSE);
        int ch = getch();
        nodelay(stdscr, TRUE);

        delwin(msgWin);
        impl_->redraw();

        return (ch == 'y' || ch == 'Y');
    }
#endif

    std::cout << title << ": " << message << " [y/N] ";
    std::string response;
    std::getline(std::cin, response);
    return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
}

std::string TuiManager::inputDialog(const std::string& title,
                                    const std::string& prompt,
                                    const std::string& defaultValue) {
#if HAS_NCURSES
    if (!impl_->fallbackMode_ && impl_->active_) {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        int width = std::max(40, static_cast<int>(prompt.size()) + 10);
        int height = 5;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        WINDOW* inputWin = newwin(height, width, startY, startX);
        box(inputWin, 0, 0);

        mvwprintw(inputWin, 0, 2, " %s ", title.c_str());
        mvwprintw(inputWin, 2, 2, "%s: ", prompt.c_str());

        echo();
        nodelay(stdscr, FALSE);

        char buffer[256] = {0};
        if (!defaultValue.empty()) {
            strncpy(buffer, defaultValue.c_str(), sizeof(buffer) - 1);
        }

        mvwgetnstr(inputWin, 2, prompt.size() + 4, buffer, sizeof(buffer) - 1);

        noecho();
        nodelay(stdscr, TRUE);

        delwin(inputWin);
        impl_->redraw();

        return std::string(buffer);
    }
#endif

    std::cout << title << " - " << prompt;
    if (!defaultValue.empty()) {
        std::cout << " [" << defaultValue << "]";
    }
    std::cout << ": ";

    std::string input;
    std::getline(std::cin, input);

    return input.empty() ? defaultValue : input;
}

int TuiManager::showMenu(const std::string& title,
                         const std::vector<MenuItem>& items) {
#if HAS_NCURSES
    if (!impl_->fallbackMode_ && impl_->active_) {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        // Calculate menu size
        int width = title.size() + 4;
        for (const auto& item : items) {
            width = std::max(width, static_cast<int>(item.label.size() +
                                                     item.shortcut.size() + 6));
        }
        width = std::min(width, maxX - 4);

        int height = items.size() + 2;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        WINDOW* menuWin = newwin(height, width, startY, startX);
        keypad(menuWin, TRUE);

        int selected = 0;

        while (true) {
            werase(menuWin);
            box(menuWin, 0, 0);
            mvwprintw(menuWin, 0, 2, " %s ", title.c_str());

            for (size_t i = 0; i < items.size(); ++i) {
                if (items[i].separator) {
                    mvwhline(menuWin, i + 1, 1, ACS_HLINE, width - 2);
                    continue;
                }

                if (static_cast<int>(i) == selected) {
                    wattron(menuWin, A_REVERSE);
                }

                if (!items[i].enabled) {
                    wattron(menuWin, A_DIM);
                }

                mvwprintw(
                    menuWin, static_cast<int>(i) + 1, 2, "%-*s %s",
                    width - static_cast<int>(items[i].shortcut.size()) - 6,
                    items[i].label.c_str(), items[i].shortcut.c_str());

                wattroff(menuWin, A_REVERSE | A_DIM);
            }

            wrefresh(menuWin);

            int ch = wgetch(menuWin);

            switch (ch) {
                case KEY_UP:
                    do {
                        selected = (selected - 1 + items.size()) % items.size();
                    } while (items[selected].separator);
                    break;

                case KEY_DOWN:
                    do {
                        selected = (selected + 1) % items.size();
                    } while (items[selected].separator);
                    break;

                case '\n':
                case '\r':
                    if (items[selected].enabled && !items[selected].separator) {
                        delwin(menuWin);
                        impl_->redraw();
                        if (items[selected].action) {
                            items[selected].action();
                        }
                        return selected;
                    }
                    break;

                case 27:  // Escape
                    delwin(menuWin);
                    impl_->redraw();
                    return -1;
            }
        }
    }
#endif

    // Fallback: simple text menu
    std::cout << "\n" << title << "\n";
    std::cout << std::string(title.size(), '-') << "\n";

    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].separator) {
            std::cout << "---\n";
        } else {
            std::cout << (i + 1) << ". " << items[i].label;
            if (!items[i].shortcut.empty()) {
                std::cout << " (" << items[i].shortcut << ")";
            }
            std::cout << "\n";
        }
    }

    std::cout << "Enter choice (0 to cancel): ";
    int choice;
    std::cin >> choice;
    std::cin.ignore();

    if (choice > 0 && choice <= static_cast<int>(items.size())) {
        if (items[choice - 1].action) {
            items[choice - 1].action();
        }
        return choice - 1;
    }

    return -1;
}

void TuiManager::showHelp() {
    std::vector<std::string> helpLines;
    for (const auto& [shortcut, description] : impl_->helpContent_) {
        helpLines.push_back(shortcut + "  " + description);
    }

    setPanelContent(PanelType::Help, helpLines);
    showPanel(PanelType::Help, true);
}

void TuiManager::hideHelp() { showPanel(PanelType::Help, false); }

void TuiManager::setHelpContent(
    const std::vector<std::pair<std::string, std::string>>& shortcuts) {
    impl_->helpContent_ = shortcuts;
}

bool TuiManager::isFallbackMode() const { return impl_->fallbackMode_; }

void TuiManager::setFallbackMode(bool fallback) {
    impl_->fallbackMode_ = fallback;
}

}  // namespace lithium::debug::terminal
