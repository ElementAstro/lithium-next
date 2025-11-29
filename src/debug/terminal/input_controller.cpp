/**
 * @file input_controller.cpp
 * @brief Implementation of cross-platform input controller
 */

#include "input_controller.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#if __has_include(<readline/readline.h>)
#include <readline/history.h>
#include <readline/readline.h>
#define HAS_READLINE 1
#else
#define HAS_READLINE 0
#endif

namespace lithium::debug::terminal {

class InputController::Impl {
public:
    InputConfig config_;
    std::string buffer_;
    size_t cursorPos_{0};

    std::deque<std::string> history_;
    size_t historyIndex_{0};
    size_t maxHistorySize_{1000};
    std::string savedBuffer_;  // Buffer saved when navigating history

    CompletionHandler completionHandler_;
    ValidationHandler validationHandler_;
    KeyHandler keyHandler_;

    bool initialized_{false};
    bool rawMode_{false};

#ifdef _WIN32
    HANDLE hStdin_{INVALID_HANDLE_VALUE};
    DWORD originalMode_{0};
#else
    struct termios originalTermios_{};
    bool termiosSet_{false};
#endif

    std::mutex mutex_;

    Impl(const InputConfig& config) : config_(config) {}

    ~Impl() { restore(); }

    void initialize() {
        if (initialized_)
            return;

#ifdef _WIN32
        hStdin_ = GetStdHandle(STD_INPUT_HANDLE);
        if (hStdin_ != INVALID_HANDLE_VALUE) {
            GetConsoleMode(hStdin_, &originalMode_);
        }
#else
        if (isatty(STDIN_FILENO)) {
            tcgetattr(STDIN_FILENO, &originalTermios_);
            termiosSet_ = true;
        }
#endif

#if HAS_READLINE
        // Initialize readline
        rl_initialize();
        using_history();
#endif

        initialized_ = true;
    }

    void restore() {
        if (!initialized_)
            return;

        if (rawMode_) {
            setRawMode(false);
        }

#ifdef _WIN32
        if (hStdin_ != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hStdin_, originalMode_);
        }
#else
        if (termiosSet_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios_);
        }
#endif

        initialized_ = false;
    }

    void setRawMode(bool enable) {
        if (enable == rawMode_)
            return;

#ifdef _WIN32
        if (hStdin_ != INVALID_HANDLE_VALUE) {
            DWORD mode = originalMode_;
            if (enable) {
                mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
                mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            }
            SetConsoleMode(hStdin_, mode);
        }
#else
        if (termiosSet_) {
            struct termios t = originalTermios_;
            if (enable) {
                t.c_lflag &= ~(ICANON | ECHO | ISIG);
                t.c_cc[VMIN] = 1;
                t.c_cc[VTIME] = 0;
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &t);
        }
#endif

        rawMode_ = enable;
    }

    std::optional<std::string> readLine(const std::string& prompt) {
        std::lock_guard<std::mutex> lock(mutex_);

#if HAS_READLINE
        char* line = readline(prompt.c_str());
        if (line == nullptr) {
            return std::nullopt;
        }

        std::string result(line);
        free(line);

        // Add to readline history if not empty
        if (!result.empty()) {
            add_history(result.c_str());
        }

        return result;
#else
        // Fallback implementation
        std::cout << prompt;
        std::cout.flush();

        buffer_.clear();
        cursorPos_ = 0;
        historyIndex_ = history_.size();

#ifdef _WIN32
        return readLineWindows();
#else
        return readLineUnix();
#endif
#endif
    }

#ifdef _WIN32
    std::optional<std::string> readLineWindows() {
        while (true) {
            int ch = _getch();

            if (ch == '\r' || ch == '\n') {
                std::cout << std::endl;
                return buffer_;
            }

            if (ch == 3) {  // Ctrl+C
                std::cout << "^C" << std::endl;
                return std::nullopt;
            }

            if (ch == 4) {  // Ctrl+D
                if (buffer_.empty()) {
                    return std::nullopt;
                }
            }

            if (ch == '\b' || ch == 127) {  // Backspace
                if (cursorPos_ > 0) {
                    buffer_.erase(cursorPos_ - 1, 1);
                    cursorPos_--;
                    std::cout << "\b \b";
                    // Redraw rest of line
                    std::cout << buffer_.substr(cursorPos_) << " ";
                    for (size_t i = cursorPos_; i <= buffer_.size(); ++i) {
                        std::cout << "\b";
                    }
                    std::cout.flush();
                }
                continue;
            }

            if (ch == 0 || ch == 224) {  // Extended key
                ch = _getch();
                handleExtendedKey(ch);
                continue;
            }

            if (ch == '\t') {  // Tab completion
                if (completionHandler_) {
                    triggerCompletionInternal();
                }
                continue;
            }

            // Regular character
            if (ch >= 32 && ch < 127) {
                buffer_.insert(cursorPos_, 1, static_cast<char>(ch));
                cursorPos_++;

                if (config_.echoInput) {
                    std::cout << buffer_.substr(cursorPos_ - 1);
                    for (size_t i = cursorPos_; i < buffer_.size(); ++i) {
                        std::cout << "\b";
                    }
                    std::cout.flush();
                }
            }
        }
    }

    void handleExtendedKey(int ch) {
        switch (ch) {
            case 72:  // Up arrow
                historyPrevious();
                break;
            case 80:  // Down arrow
                historyNext();
                break;
            case 75:  // Left arrow
                if (cursorPos_ > 0) {
                    cursorPos_--;
                    std::cout << "\b";
                    std::cout.flush();
                }
                break;
            case 77:  // Right arrow
                if (cursorPos_ < buffer_.size()) {
                    std::cout << buffer_[cursorPos_];
                    cursorPos_++;
                    std::cout.flush();
                }
                break;
            case 71:  // Home
                while (cursorPos_ > 0) {
                    std::cout << "\b";
                    cursorPos_--;
                }
                std::cout.flush();
                break;
            case 79:  // End
                while (cursorPos_ < buffer_.size()) {
                    std::cout << buffer_[cursorPos_];
                    cursorPos_++;
                }
                std::cout.flush();
                break;
            case 83:  // Delete
                if (cursorPos_ < buffer_.size()) {
                    buffer_.erase(cursorPos_, 1);
                    std::cout << buffer_.substr(cursorPos_) << " ";
                    for (size_t i = cursorPos_; i <= buffer_.size(); ++i) {
                        std::cout << "\b";
                    }
                    std::cout.flush();
                }
                break;
        }
    }
#else
    std::optional<std::string> readLineUnix() {
        setRawMode(true);

        while (true) {
            int ch = getchar();

            if (ch == EOF || ch == 4) {  // EOF or Ctrl+D
                setRawMode(false);
                if (buffer_.empty()) {
                    std::cout << std::endl;
                    return std::nullopt;
                }
                std::cout << std::endl;
                return buffer_;
            }

            if (ch == '\n' || ch == '\r') {
                setRawMode(false);
                std::cout << std::endl;
                return buffer_;
            }

            if (ch == 3) {  // Ctrl+C
                setRawMode(false);
                std::cout << "^C" << std::endl;
                return std::nullopt;
            }

            if (ch == 127 || ch == '\b') {  // Backspace
                if (cursorPos_ > 0) {
                    buffer_.erase(cursorPos_ - 1, 1);
                    cursorPos_--;
                    refreshLine();
                }
                continue;
            }

            if (ch == '\t') {  // Tab
                if (completionHandler_) {
                    triggerCompletionInternal();
                }
                continue;
            }

            if (ch == 27) {  // Escape sequence
                int next1 = getchar();
                if (next1 == '[') {
                    int next2 = getchar();
                    handleEscapeSequence(next2);
                }
                continue;
            }

            if (ch == 21) {  // Ctrl+U - clear line
                buffer_.clear();
                cursorPos_ = 0;
                refreshLine();
                continue;
            }

            if (ch == 11) {  // Ctrl+K - kill to end
                buffer_.erase(cursorPos_);
                refreshLine();
                continue;
            }

            if (ch == 1) {  // Ctrl+A - beginning
                cursorPos_ = 0;
                refreshLine();
                continue;
            }

            if (ch == 5) {  // Ctrl+E - end
                cursorPos_ = buffer_.size();
                refreshLine();
                continue;
            }

            // Regular character
            if (ch >= 32 && ch < 127) {
                buffer_.insert(cursorPos_, 1, static_cast<char>(ch));
                cursorPos_++;
                refreshLine();
            }
        }
    }

    void handleEscapeSequence(int ch) {
        switch (ch) {
            case 'A':  // Up
                historyPrevious();
                break;
            case 'B':  // Down
                historyNext();
                break;
            case 'C':  // Right
                if (cursorPos_ < buffer_.size()) {
                    cursorPos_++;
                    refreshLine();
                }
                break;
            case 'D':  // Left
                if (cursorPos_ > 0) {
                    cursorPos_--;
                    refreshLine();
                }
                break;
            case 'H':  // Home
                cursorPos_ = 0;
                refreshLine();
                break;
            case 'F':  // End
                cursorPos_ = buffer_.size();
                refreshLine();
                break;
            case '3':       // Delete (followed by ~)
                getchar();  // consume ~
                if (cursorPos_ < buffer_.size()) {
                    buffer_.erase(cursorPos_, 1);
                    refreshLine();
                }
                break;
        }
    }

    void refreshLine() {
        // Clear line and redraw
        std::cout << "\r\033[K" << config_.prompt << buffer_;

        // Move cursor to correct position
        size_t moveBack = buffer_.size() - cursorPos_;
        if (moveBack > 0) {
            std::cout << "\033[" << moveBack << "D";
        }
        std::cout.flush();
    }
#endif

    void historyPrevious() {
        if (history_.empty())
            return;

        if (historyIndex_ == history_.size()) {
            savedBuffer_ = buffer_;
        }

        if (historyIndex_ > 0) {
            historyIndex_--;
            buffer_ = history_[historyIndex_];
            cursorPos_ = buffer_.size();

#ifdef _WIN32
            // Clear and redraw line
            std::cout << "\r" << config_.prompt
                      << std::string(buffer_.size() + 10, ' ');
            std::cout << "\r" << config_.prompt << buffer_;
            std::cout.flush();
#else
            refreshLine();
#endif
        }
    }

    void historyNext() {
        if (historyIndex_ < history_.size()) {
            historyIndex_++;

            if (historyIndex_ == history_.size()) {
                buffer_ = savedBuffer_;
            } else {
                buffer_ = history_[historyIndex_];
            }
            cursorPos_ = buffer_.size();

#ifdef _WIN32
            std::cout << "\r" << config_.prompt
                      << std::string(buffer_.size() + 10, ' ');
            std::cout << "\r" << config_.prompt << buffer_;
            std::cout.flush();
#else
            refreshLine();
#endif
        }
    }

    void triggerCompletionInternal() {
        if (!completionHandler_)
            return;

        auto result = completionHandler_(buffer_, cursorPos_);

        if (result.matches.empty()) {
            bell();
            return;
        }

        if (result.matches.size() == 1) {
            // Single match - complete it
            buffer_ = result.matches[0];
            cursorPos_ = buffer_.size();
#ifdef _WIN32
            std::cout << "\r" << config_.prompt << buffer_;
            std::cout.flush();
#else
            refreshLine();
#endif
        } else {
            // Multiple matches - show them
            std::cout << std::endl;
            for (const auto& match : result.matches) {
                std::cout << match << "  ";
            }
            std::cout << std::endl << config_.prompt << buffer_;
            std::cout.flush();
        }
    }

    void bell() {
        std::cout << '\a';
        std::cout.flush();
    }

    std::optional<char> readChar() {
        setRawMode(true);

#ifdef _WIN32
        int ch = _getch();
#else
        int ch = getchar();
#endif

        setRawMode(false);

        if (ch == EOF) {
            return std::nullopt;
        }

        return static_cast<char>(ch);
    }

    bool hasInput() const {
#ifdef _WIN32
        return _kbhit() != 0;
#else
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        struct timeval tv = {0, 0};
        return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
#endif
    }

    std::optional<std::string> readPassword(const std::string& prompt) {
        std::cout << prompt;
        std::cout.flush();

        std::string password;

#ifdef _WIN32
        int ch;
        while ((ch = _getch()) != '\r' && ch != '\n') {
            if (ch == '\b' || ch == 127) {
                if (!password.empty()) {
                    password.pop_back();
                }
            } else if (ch >= 32 && ch < 127) {
                password += static_cast<char>(ch);
            }
        }
#else
        setRawMode(true);

        int ch;
        while ((ch = getchar()) != '\n' && ch != '\r' && ch != EOF) {
            if (ch == 127 || ch == '\b') {
                if (!password.empty()) {
                    password.pop_back();
                }
            } else if (ch >= 32 && ch < 127) {
                password += static_cast<char>(ch);
            }
        }

        setRawMode(false);
#endif

        std::cout << std::endl;
        return password;
    }

    void addToHistory(const std::string& entry) {
        if (entry.empty())
            return;

        // Don't add duplicates of the last entry
        if (!history_.empty() && history_.back() == entry) {
            return;
        }

        history_.push_back(entry);

        // Trim to max size
        while (history_.size() > maxHistorySize_) {
            history_.pop_front();
        }

        historyIndex_ = history_.size();

#if HAS_READLINE
        add_history(entry.c_str());
#endif
    }

    bool loadHistory(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        history_.clear();
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                history_.push_back(line);
            }
        }

        historyIndex_ = history_.size();
        return true;
    }

    bool saveHistory(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        for (const auto& entry : history_) {
            file << entry << "\n";
        }

        return true;
    }

    std::vector<std::string> searchHistory(const std::string& pattern) const {
        std::vector<std::string> results;

        for (const auto& entry : history_) {
            if (entry.find(pattern) != std::string::npos) {
                results.push_back(entry);
            }
        }

        return results;
    }
};

// InputController implementation

InputController::InputController(const InputConfig& config)
    : impl_(std::make_unique<Impl>(config)) {
    impl_->initialize();
}

InputController::~InputController() = default;

InputController::InputController(InputController&&) noexcept = default;
InputController& InputController::operator=(InputController&&) noexcept =
    default;

void InputController::setConfig(const InputConfig& config) {
    impl_->config_ = config;
}

const InputConfig& InputController::getConfig() const { return impl_->config_; }

void InputController::setPrompt(const std::string& prompt) {
    impl_->config_.prompt = prompt;
}

void InputController::setMode(InputMode mode) { impl_->config_.mode = mode; }

std::optional<std::string> InputController::readLine() {
    return impl_->readLine(impl_->config_.prompt);
}

std::optional<std::string> InputController::readLine(
    const std::string& prompt) {
    return impl_->readLine(prompt);
}

std::optional<char> InputController::readChar() { return impl_->readChar(); }

std::optional<InputEvent> InputController::readKey() {
    auto ch = impl_->readChar();
    if (!ch) {
        return std::nullopt;
    }

    InputEvent event;
    event.character = *ch;

    // Map common keys
    switch (*ch) {
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
        case 127:
            event.key = Key::Backspace;
            event.isSpecialKey = true;
            break;
        default:
            if (*ch >= 1 && *ch <= 26) {
                event.ctrl = true;
                event.hasModifier = true;
                event.key = static_cast<Key>(*ch);
            }
            break;
    }

    return event;
}

bool InputController::hasInput() const { return impl_->hasInput(); }

std::optional<std::string> InputController::readPassword(
    const std::string& prompt) {
    return impl_->readPassword(prompt);
}

std::string InputController::getBuffer() const { return impl_->buffer_; }

void InputController::setBuffer(const std::string& content) {
    impl_->buffer_ = content;
    impl_->cursorPos_ = content.size();
}

void InputController::clearBuffer() {
    impl_->buffer_.clear();
    impl_->cursorPos_ = 0;
}

size_t InputController::getCursorPosition() const { return impl_->cursorPos_; }

void InputController::setCursorPosition(size_t pos) {
    impl_->cursorPos_ = std::min(pos, impl_->buffer_.size());
}

void InputController::insertText(const std::string& text) {
    impl_->buffer_.insert(impl_->cursorPos_, text);
    impl_->cursorPos_ += text.size();
}

void InputController::deleteChar() {
    if (impl_->cursorPos_ < impl_->buffer_.size()) {
        impl_->buffer_.erase(impl_->cursorPos_, 1);
    }
}

void InputController::backspace() {
    if (impl_->cursorPos_ > 0) {
        impl_->buffer_.erase(impl_->cursorPos_ - 1, 1);
        impl_->cursorPos_--;
    }
}

void InputController::addToHistory(const std::string& entry) {
    impl_->addToHistory(entry);
}

std::vector<std::string> InputController::getHistory() const {
    return std::vector<std::string>(impl_->history_.begin(),
                                    impl_->history_.end());
}

void InputController::clearHistory() {
    impl_->history_.clear();
    impl_->historyIndex_ = 0;
}

void InputController::setMaxHistorySize(size_t size) {
    impl_->maxHistorySize_ = size;
}

bool InputController::loadHistory(const std::string& path) {
    return impl_->loadHistory(path);
}

bool InputController::saveHistory(const std::string& path) const {
    return impl_->saveHistory(path);
}

void InputController::historyPrevious() { impl_->historyPrevious(); }

void InputController::historyNext() { impl_->historyNext(); }

std::vector<std::string> InputController::searchHistory(
    const std::string& pattern) const {
    return impl_->searchHistory(pattern);
}

void InputController::setCompletionHandler(CompletionHandler handler) {
    impl_->completionHandler_ = std::move(handler);
}

void InputController::triggerCompletion() {
    impl_->triggerCompletionInternal();
}

CompletionResult InputController::getCompletions() const {
    if (impl_->completionHandler_) {
        return impl_->completionHandler_(impl_->buffer_, impl_->cursorPos_);
    }
    return {};
}

void InputController::setKeyHandler(KeyHandler handler) {
    impl_->keyHandler_ = std::move(handler);
}

void InputController::setValidationHandler(ValidationHandler handler) {
    impl_->validationHandler_ = std::move(handler);
}

void InputController::initialize() { impl_->initialize(); }

void InputController::restore() { impl_->restore(); }

bool InputController::isRawMode() const { return impl_->rawMode_; }

void InputController::setRawMode(bool enable) { impl_->setRawMode(enable); }

void InputController::refresh() {
#ifndef _WIN32
    impl_->refreshLine();
#endif
}

void InputController::bell() { impl_->bell(); }

}  // namespace lithium::debug::terminal
