/**
 * @file renderer.cpp
 * @brief Implementation of console renderer
 */

#include "renderer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <sys/ioctl.h>
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace lithium::debug::terminal {

class ConsoleRenderer::Impl {
public:
    Theme theme_;
    bool colorsEnabled_{true};
    bool unicodeEnabled_{true};
    std::mutex outputMutex_;

    // Spinner state
    std::atomic<bool> spinnerRunning_{false};
    std::thread spinnerThread_;
    std::string spinnerMessage_;
    SpinnerStyle spinnerStyle_;

    // Progress state
    float lastProgress_{0.0f};
    std::string lastProgressLabel_;

    Impl(const Theme& theme) : theme_(theme) {
        detectCapabilities();
        enableWindowsAnsi();
    }

    ~Impl() { stopSpinner(); }

    void detectCapabilities() {
        // Check if stdout is a terminal
        bool isTty = ISATTY(FILENO(stdout));

        // Check for color support
        colorsEnabled_ = isTty;

#ifndef _WIN32
        // Check TERM environment variable on Unix
        const char* term = std::getenv("TERM");
        if (term) {
            std::string termStr(term);
            colorsEnabled_ = colorsEnabled_ &&
                             (termStr.find("color") != std::string::npos ||
                              termStr.find("xterm") != std::string::npos ||
                              termStr.find("screen") != std::string::npos ||
                              termStr.find("tmux") != std::string::npos ||
                              termStr == "linux");
        }

        // Check for Unicode support
        const char* lang = std::getenv("LANG");
        unicodeEnabled_ =
            lang && (std::string(lang).find("UTF-8") != std::string::npos ||
                     std::string(lang).find("utf8") != std::string::npos);
#else
        // Windows 10+ generally supports Unicode
        unicodeEnabled_ = true;
#endif

        // Apply theme settings
        if (!colorsEnabled_) {
            theme_.useColors = false;
        }
        if (!unicodeEnabled_) {
            theme_.useUnicode = false;
            // Switch to ASCII theme if Unicode not supported
            if (theme_.name == "default") {
                theme_ = Theme::ascii();
            }
        }
    }

    void enableWindowsAnsi() {
#ifdef _WIN32
        // Enable ANSI escape codes on Windows 10+
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }

        // Set console to UTF-8
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

    TerminalSize getTerminalSize() const {
        TerminalSize size;

#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                       &csbi)) {
            size.width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            size.height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
#else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            size.width = w.ws_col;
            size.height = w.ws_row;
        }
#endif

        return size;
    }

    std::string colorCode(Color fg, std::optional<Color> bg,
                          Style style) const {
        if (!theme_.useColors) {
            return "";
        }

        std::ostringstream oss;
        oss << "\033[" << static_cast<int>(style);

        if (fg != Color::Default) {
            oss << ";" << static_cast<int>(fg);
        }

        if (bg.has_value() && bg.value() != Color::Default) {
            oss << ";" << (static_cast<int>(bg.value()) + 10);
        }

        oss << "m";
        return oss.str();
    }

    std::string resetCode() const { return theme_.useColors ? "\033[0m" : ""; }

    void print(const std::string& text, Color fg, std::optional<Color> bg,
               Style style) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << colorCode(fg, bg, style) << text << resetCode();
    }

    void println(const std::string& text, Color fg, std::optional<Color> bg,
                 Style style) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << colorCode(fg, bg, style) << text << resetCode()
                  << std::endl;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[2J\033[H";
        std::cout.flush();
    }

    void clearLine() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[2K\r";
        std::cout.flush();
    }

    void moveCursor(int x, int y) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[" << y << ";" << x << "H";
        std::cout.flush();
    }

    void moveCursorUp(int lines) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[" << lines << "A";
        std::cout.flush();
    }

    void moveCursorDown(int lines) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[" << lines << "B";
        std::cout.flush();
    }

    void saveCursor() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[s";
        std::cout.flush();
    }

    void restoreCursor() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[u";
        std::cout.flush();
    }

    void hideCursor() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[?25l";
        std::cout.flush();
    }

    void showCursor() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout << "\033[?25h";
        std::cout.flush();
    }

    std::string repeatString(const std::string& str, int count) const {
        std::string result;
        result.reserve(str.size() * count);
        for (int i = 0; i < count; ++i) {
            result += str;
        }
        return result;
    }

    void startSpinner() {
        if (spinnerRunning_)
            return;

        spinnerRunning_ = true;
        spinnerThread_ = std::thread([this]() {
            size_t frameIndex = 0;
            hideCursor();

            while (spinnerRunning_) {
                {
                    std::lock_guard<std::mutex> lock(outputMutex_);
                    std::cout << "\r"
                              << colorCode(spinnerStyle_.color, std::nullopt,
                                           Style::Normal)
                              << spinnerStyle_.frames[frameIndex] << resetCode()
                              << " " << spinnerMessage_;
                    std::cout.flush();
                }

                frameIndex = (frameIndex + 1) % spinnerStyle_.frames.size();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(spinnerStyle_.intervalMs));
            }

            showCursor();
        });
    }

    void stopSpinner() {
        if (!spinnerRunning_)
            return;

        spinnerRunning_ = false;
        if (spinnerThread_.joinable()) {
            spinnerThread_.join();
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lock(outputMutex_);
        std::cout.flush();
    }
};

// ConsoleRenderer implementation

ConsoleRenderer::ConsoleRenderer(const Theme& theme)
    : impl_(std::make_unique<Impl>(theme)) {}

ConsoleRenderer::~ConsoleRenderer() = default;

ConsoleRenderer::ConsoleRenderer(ConsoleRenderer&&) noexcept = default;
ConsoleRenderer& ConsoleRenderer::operator=(ConsoleRenderer&&) noexcept =
    default;

void ConsoleRenderer::setTheme(const Theme& theme) { impl_->theme_ = theme; }

const Theme& ConsoleRenderer::getTheme() const { return impl_->theme_; }

bool ConsoleRenderer::loadTheme(const std::string& path) {
    // TODO: Implement JSON theme loading
    (void)path;
    return false;
}

bool ConsoleRenderer::saveTheme(const std::string& path) const {
    // TODO: Implement JSON theme saving
    (void)path;
    return false;
}

void ConsoleRenderer::print(const std::string& text, Color fg,
                            std::optional<Color> bg, Style style) {
    impl_->print(text, fg, bg, style);
}

void ConsoleRenderer::println(const std::string& text, Color fg,
                              std::optional<Color> bg, Style style) {
    impl_->println(text, fg, bg, style);
}

void ConsoleRenderer::clear() { impl_->clear(); }

void ConsoleRenderer::clearLine() { impl_->clearLine(); }

void ConsoleRenderer::moveCursor(int x, int y) { impl_->moveCursor(x, y); }

void ConsoleRenderer::moveCursorUp(int lines) { impl_->moveCursorUp(lines); }

void ConsoleRenderer::moveCursorDown(int lines) {
    impl_->moveCursorDown(lines);
}

void ConsoleRenderer::saveCursor() { impl_->saveCursor(); }

void ConsoleRenderer::restoreCursor() { impl_->restoreCursor(); }

void ConsoleRenderer::hideCursor() { impl_->hideCursor(); }

void ConsoleRenderer::showCursor() { impl_->showCursor(); }

void ConsoleRenderer::success(const std::string& message) {
    const auto& t = impl_->theme_;
    println(t.successSymbol + " " + message, t.successColor, std::nullopt,
            Style::Normal);
}

void ConsoleRenderer::error(const std::string& message) {
    const auto& t = impl_->theme_;
    println(t.errorSymbol + " " + message, t.errorColor, std::nullopt,
            t.errorStyle);
}

void ConsoleRenderer::warning(const std::string& message) {
    const auto& t = impl_->theme_;
    println(t.warningSymbol + " " + message, t.warningColor);
}

void ConsoleRenderer::info(const std::string& message) {
    const auto& t = impl_->theme_;
    println(t.infoSymbol + " " + message, t.infoColor);
}

void ConsoleRenderer::debug(const std::string& message) {
    const auto& t = impl_->theme_;
    println("[DEBUG] " + message, t.debugColor);
}

void ConsoleRenderer::header(const std::string& title, char fillChar) {
    const auto& t = impl_->theme_;
    auto size = impl_->getTerminalSize();
    int width = std::min(size.width, 80);

    int titleLen = static_cast<int>(title.length()) + 2;  // +2 for spaces
    int sideLen = (width - titleLen) / 2;

    std::string line = std::string(sideLen, fillChar) + " " + title + " " +
                       std::string(width - sideLen - titleLen, fillChar);

    println(line, t.headerColor, std::nullopt, t.headerStyle);
}

void ConsoleRenderer::subheader(const std::string& title) {
    const auto& t = impl_->theme_;
    println(t.arrowSymbol + " " + title, t.headerColor, std::nullopt,
            Style::Bold);
}

void ConsoleRenderer::horizontalRule(char ch, int width) {
    auto size = impl_->getTerminalSize();
    int w = (width > 0) ? width : std::min(size.width, 80);
    println(std::string(w, ch), impl_->theme_.borderColor);
}

void ConsoleRenderer::box(const std::string& content,
                          const std::string& title) {
    box(std::vector<std::string>{content}, title);
}

void ConsoleRenderer::box(const std::vector<std::string>& lines,
                          const std::string& title) {
    const auto& t = impl_->theme_;

    // Find max line width
    size_t maxWidth = title.length();
    for (const auto& line : lines) {
        maxWidth = std::max(maxWidth, visibleLength(line));
    }
    maxWidth += 2;  // Padding

    // Top border
    std::string topBorder = t.borderTopLeft;
    if (!title.empty()) {
        topBorder += " " + title + " ";
        topBorder += impl_->repeatString(t.borderHorizontal,
                                         maxWidth - title.length() - 2);
    } else {
        topBorder += impl_->repeatString(t.borderHorizontal, maxWidth);
    }
    topBorder += t.borderTopRight;
    println(topBorder, t.borderColor);

    // Content lines
    for (const auto& line : lines) {
        size_t visLen = visibleLength(line);
        std::string padding(maxWidth - visLen, ' ');
        println(t.borderVertical + " " + line + padding + t.borderVertical,
                t.borderColor);
    }

    // Bottom border
    std::string bottomBorder =
        t.borderBottomLeft + impl_->repeatString(t.borderHorizontal, maxWidth) +
        t.borderBottomRight;
    println(bottomBorder, t.borderColor);
}

void ConsoleRenderer::bulletList(const std::vector<std::string>& items,
                                 int indent) {
    const auto& t = impl_->theme_;
    std::string indentStr(indent * 2, ' ');

    for (const auto& item : items) {
        println(indentStr + t.bulletSymbol + " " + item);
    }
}

void ConsoleRenderer::numberedList(const std::vector<std::string>& items,
                                   int startNum) {
    int num = startNum;
    for (const auto& item : items) {
        println(std::to_string(num++) + ". " + item);
    }
}

void ConsoleRenderer::keyValue(const std::string& key, const std::string& value,
                               int keyWidth) {
    const auto& t = impl_->theme_;
    std::ostringstream oss;
    oss << std::left << std::setw(keyWidth) << key;

    print(oss.str(), t.highlightColor, std::nullopt, Style::Bold);
    println(": " + value);
}

void ConsoleRenderer::keyValueList(
    const std::vector<std::pair<std::string, std::string>>& pairs,
    int keyWidth) {
    for (const auto& [key, value] : pairs) {
        keyValue(key, value, keyWidth);
    }
}

void ConsoleRenderer::table(const std::vector<TableColumn>& columns,
                            const std::vector<std::vector<std::string>>& rows) {
    const auto& t = impl_->theme_;

    // Calculate column widths
    std::vector<int> widths;
    for (const auto& col : columns) {
        widths.push_back(col.width > 0 ? col.width
                                       : static_cast<int>(col.header.length()));
    }

    // Adjust widths based on content
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            widths[i] =
                std::max(widths[i], static_cast<int>(visibleLength(row[i])));
        }
    }

    // Print header
    std::string headerLine;
    for (size_t i = 0; i < columns.size(); ++i) {
        std::ostringstream oss;
        oss << std::left << std::setw(widths[i]) << columns[i].header;
        headerLine += oss.str();
        if (i < columns.size() - 1)
            headerLine += " | ";
    }
    println(headerLine, t.headerColor, std::nullopt, Style::Bold);

    // Print separator
    std::string separator;
    for (size_t i = 0; i < widths.size(); ++i) {
        separator += std::string(widths[i], '-');
        if (i < widths.size() - 1)
            separator += "-+-";
    }
    println(separator, t.borderColor);

    // Print rows
    for (const auto& row : rows) {
        std::string rowLine;
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            std::ostringstream oss;
            oss << std::left << std::setw(widths[i]) << row[i];
            rowLine += oss.str();
            if (i < row.size() - 1)
                rowLine += " | ";
        }
        println(rowLine);
    }
}

void ConsoleRenderer::simpleTable(
    const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows) {
    std::vector<TableColumn> columns;
    for (const auto& h : headers) {
        columns.push_back({h, 0, Alignment::Left});
    }
    table(columns, rows);
}

void ConsoleRenderer::progressBar(float progress, const std::string& label,
                                  const ProgressStyle& style) {
    progress = std::clamp(progress, 0.0f, 1.0f);

    int filled = static_cast<int>(progress * style.width);
    int empty = style.width - filled;

    std::ostringstream oss;
    oss << style.leftBracket;
    oss << impl_->colorCode(style.fillColor, std::nullopt, Style::Normal);
    oss << impl_->repeatString(style.fillChar, filled);
    oss << impl_->colorCode(style.emptyColor, std::nullopt, Style::Normal);
    oss << impl_->repeatString(style.emptyChar, empty);
    oss << impl_->resetCode();
    oss << style.rightBracket;

    if (style.showPercentage) {
        oss << " " << std::fixed << std::setprecision(1) << (progress * 100)
            << "%";
    }

    if (!label.empty()) {
        oss << " " << label;
    }

    println(oss.str());

    impl_->lastProgress_ = progress;
    impl_->lastProgressLabel_ = label;
}

void ConsoleRenderer::updateProgress(float progress, const std::string& label) {
    moveCursorUp(1);
    clearLine();
    progressBar(progress, label.empty() ? impl_->lastProgressLabel_ : label);
}

void ConsoleRenderer::startSpinner(const std::string& message,
                                   const SpinnerStyle& style) {
    impl_->spinnerMessage_ = message;
    impl_->spinnerStyle_ = style;
    impl_->startSpinner();
}

void ConsoleRenderer::stopSpinner(bool successFlag,
                                  const std::string& message) {
    impl_->stopSpinner();
    clearLine();

    if (successFlag) {
        success(message.empty() ? impl_->spinnerMessage_ : message);
    } else {
        error(message.empty() ? impl_->spinnerMessage_ : message);
    }
}

void ConsoleRenderer::prompt(const std::string& prefix) {
    const auto& t = impl_->theme_;

    if (!prefix.empty()) {
        print(prefix + " ", t.promptColor, std::nullopt, t.promptStyle);
    }

    print(t.promptSymbol + " ", t.promptSymbolColor, std::nullopt,
          t.promptStyle);
    flush();
}

void ConsoleRenderer::welcomeHeader(const std::string& title,
                                    const std::string& version,
                                    const std::string& description) {
    const auto& t = impl_->theme_;
    auto size = impl_->getTerminalSize();
    int width = std::min(size.width, 70);

    println();

    // Top border
    std::string border = t.borderTopLeft +
                         impl_->repeatString(t.borderHorizontal, width - 2) +
                         t.borderTopRight;
    println(border, t.borderColor);

    // Title line
    std::string titleLine = title + " v" + version;
    int padding = (width - 2 - static_cast<int>(titleLine.length())) / 2;
    std::string paddedTitle =
        std::string(padding, ' ') + titleLine +
        std::string(width - 2 - padding - titleLine.length(), ' ');
    println(t.borderVertical + paddedTitle + t.borderVertical, t.borderColor);

    // Description if provided
    if (!description.empty()) {
        int descPadding =
            (width - 2 - static_cast<int>(description.length())) / 2;
        std::string paddedDesc =
            std::string(descPadding, ' ') + description +
            std::string(width - 2 - descPadding - description.length(), ' ');
        println(t.borderVertical + paddedDesc + t.borderVertical,
                t.borderColor);
    }

    // Bottom border
    border = t.borderBottomLeft +
             impl_->repeatString(t.borderHorizontal, width - 2) +
             t.borderBottomRight;
    println(border, t.borderColor);

    println();
}

void ConsoleRenderer::suggestions(const std::vector<std::string>& items,
                                  const std::string& prefix) {
    const auto& t = impl_->theme_;

    if (items.empty())
        return;

    println(prefix, t.infoColor);
    for (const auto& item : items) {
        println("  " + t.bulletSymbol + " " + item, t.suggestionColor);
    }
}

void ConsoleRenderer::commandHelp(
    const std::string& command, const std::string& description,
    const std::vector<std::pair<std::string, std::string>>& options) {
    const auto& t = impl_->theme_;

    // Command name
    print(command, t.highlightColor, std::nullopt, Style::Bold);
    println(" - " + description);

    // Options
    if (!options.empty()) {
        println("\nOptions:", t.headerColor);
        for (const auto& [opt, desc] : options) {
            print("  " + opt, t.suggestionColor);
            println("  " + desc);
        }
    }
}

void ConsoleRenderer::highlightedCommand(
    const std::string& command, const std::vector<std::string>& keywords) {
    const auto& t = impl_->theme_;

    // Simple keyword highlighting
    std::string result = command;

    // Extract first word as command name
    size_t spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        print(command.substr(0, spacePos), t.highlightColor, std::nullopt,
              Style::Bold);
        println(command.substr(spacePos));
    } else {
        println(command, t.highlightColor, std::nullopt, Style::Bold);
    }

    (void)keywords;  // TODO: Implement full keyword highlighting
}

void ConsoleRenderer::errorWithPosition(const std::string& input,
                                        size_t position,
                                        const std::string& message) {
    const auto& t = impl_->theme_;

    // Print the input
    println(input);

    // Print position indicator
    std::string indicator(position, ' ');
    indicator += "^";
    println(indicator, t.errorColor);

    // Print error message
    error(message);
}

TerminalSize ConsoleRenderer::getTerminalSize() const {
    return impl_->getTerminalSize();
}

bool ConsoleRenderer::supportsColors() const { return impl_->colorsEnabled_; }

bool ConsoleRenderer::supportsUnicode() const { return impl_->unicodeEnabled_; }

void ConsoleRenderer::enableColors(bool enable) {
    impl_->colorsEnabled_ = enable;
    impl_->theme_.useColors = enable;
}

void ConsoleRenderer::enableUnicode(bool enable) {
    impl_->unicodeEnabled_ = enable;
    impl_->theme_.useUnicode = enable;
}

void ConsoleRenderer::flush() { impl_->flush(); }

std::string ConsoleRenderer::colorCode(Color fg, std::optional<Color> bg,
                                       Style style) const {
    return impl_->colorCode(fg, bg, style);
}

std::string ConsoleRenderer::resetCode() const { return impl_->resetCode(); }

std::string ConsoleRenderer::stripAnsi(const std::string& text) {
    static const std::regex ansiRegex("\033\\[[0-9;]*m");
    return std::regex_replace(text, ansiRegex, "");
}

size_t ConsoleRenderer::visibleLength(const std::string& text) {
    return stripAnsi(text).length();
}

}  // namespace lithium::debug::terminal
