#include "terminal.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#if __has_include(<ncurses.h>)
#include <ncurses.h>  // ncurses library
#elif __has_include(<ncurses/ncurses.h>)
#include <ncurses/ncurses.h>
#else
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "check.hpp"
#include "suggestion.hpp"

#include "atom/components/component.hpp"

namespace lithium::debug {

constexpr int BUFFER_SIZE = 256;
constexpr int DEFAULT_COMMAND_TIMEOUT_MS = 5000;

auto ctermid() -> std::string {
#ifdef _WIN32
    char buffer[BUFFER_SIZE];
    DWORD length = GetConsoleTitleA(buffer, BUFFER_SIZE);
    if (length > 0) {
        return std::string(buffer, length);
    }
#else
    std::array<char, L_ctermid> buffer;
    if (::ctermid(buffer.data()) != nullptr) {
        return std::string(buffer.data());
    }
#endif
    return "";
}

class ConsoleTerminal::ConsoleTerminalImpl {
public:
    ConsoleTerminalImpl();
    ~ConsoleTerminalImpl();

    ConsoleTerminalImpl(const ConsoleTerminalImpl&) = delete;
    ConsoleTerminalImpl& operator=(const ConsoleTerminalImpl&) = delete;
    ConsoleTerminalImpl(ConsoleTerminalImpl&&) noexcept = delete;
    ConsoleTerminalImpl& operator=(ConsoleTerminalImpl&&) noexcept = delete;

    [[nodiscard]] auto getRegisteredCommands() const
        -> std::vector<std::string>;
    void callCommand(std::string_view name, const std::vector<std::any>& args);
    void run();

    void initializeNcurses();
    void shutdownNcurses() const;
    std::string readInput();
    void addToHistory(const std::string& input);
    void printHeader() const;
    auto processToken(const std::string& token) -> std::any;
    auto parseArguments(const std::string& input) -> std::vector<std::any>;

    static auto commandCompletion(const char* text, int start,
                                  int end) -> char**;
    static auto commandGenerator(const char* text, int state) -> char*;

    void handleInput(const std::string& input);
    void displayPrompt() const;
    void printErrors(const std::vector<CommandChecker::Error>& errors,
                     const std::string& input, bool continueRun) const;

    std::shared_ptr<SuggestionEngine> suggestionEngine_;
    std::shared_ptr<CommandChecker> commandChecker_;
    std::shared_ptr<Component> component_;

    static constexpr int MAX_HISTORY_SIZE = 100;

    std::vector<std::string> commandHistoryQueue_;

#if __has_include(<ncurses.h>)
    bool ncursesEnabled_ = false;
#endif

    bool historyEnabled_ = false;
    bool suggestionsEnabled_ = false;
    bool syntaxHighlightEnabled_ = false;
    std::chrono::milliseconds commandTimeout_ =
        std::chrono::milliseconds(DEFAULT_COMMAND_TIMEOUT_MS);

    bool commandCheckEnabled_{true};
};

ConsoleTerminal::ConsoleTerminal()
    : impl_(std::make_unique<ConsoleTerminalImpl>()) {}

ConsoleTerminal::~ConsoleTerminal() = default;

auto ConsoleTerminal::getRegisteredCommands() const
    -> std::vector<std::string> {
    return impl_->getRegisteredCommands();
}

void ConsoleTerminal::callCommand(std::string_view name,
                                  const std::vector<std::any>& args) {
    impl_->callCommand(name, args);
}

void ConsoleTerminal::run() { impl_->run(); }

ConsoleTerminal::ConsoleTerminalImpl::ConsoleTerminalImpl()
    : commandChecker_(std::make_shared<CommandChecker>()) {
    component_ = std::make_shared<Component>("lithium.terminal");

    suggestionEngine_ =
        std::make_shared<SuggestionEngine>(getRegisteredCommands());

    initializeNcurses();
    // 初始化命令检查器
    commandChecker_->loadConfig("config/command_check.json");
}

ConsoleTerminal::ConsoleTerminalImpl::~ConsoleTerminalImpl() {
    shutdownNcurses();
}

void ConsoleTerminal::ConsoleTerminalImpl::initializeNcurses() {
#if __has_include(<ncurses.h>)
    ncursesEnabled_ = true;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
#endif
}

void ConsoleTerminal::ConsoleTerminalImpl::shutdownNcurses() const {
#if __has_include(<ncurses.h>)
    if (ncursesEnabled_) {
        endwin();
    }
#endif
}

std::string ConsoleTerminal::ConsoleTerminalImpl::readInput() {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    char inputBuffer[BUFFER_SIZE];
    echo();
    getnstr(inputBuffer, BUFFER_SIZE - 1);
    noecho();
    return std::string(inputBuffer);
#else
    char* input = readline("> ");
    if (input) {
        std::string inputStr(input);
        free(input);
        return inputStr;
    }
    return "";
#endif
}

void ConsoleTerminal::ConsoleTerminalImpl::addToHistory(
    const std::string& input) {
    if (commandHistoryQueue_.size() >= MAX_HISTORY_SIZE) {
        commandHistoryQueue_.erase(commandHistoryQueue_.begin());
    }
    commandHistoryQueue_.push_back(input);
#if !(__has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>))
    add_history(input.c_str());
#endif
}

void ConsoleTerminal::ConsoleTerminalImpl::printHeader() const {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    printw("*** Welcome to Lithium Command Line Tool v1.0 ***\n");
    printw("Type 'help' to see a list of available commands.\n");
#else
    const int BORDER_WIDTH = 60;
    const std::string RESET = "\033[0m";
    const std::string GREEN = "\033[1;32m";
    const std::string BLUE = "\033[1;34m";
    const std::string CYAN = "\033[1;36m";

    // Print top border
    std::cout << BLUE << std::string(BORDER_WIDTH, '*') << RESET << std::endl;

    // Print title
    std::cout << BLUE << "* " << GREEN << std::setw(BORDER_WIDTH - 4)
              << std::left << "Welcome to Lithium Command Line Tool v1.0"
              << " *" << RESET << std::endl;

    // Print description
    std::cout << BLUE << "* " << GREEN << std::setw(BORDER_WIDTH - 4)
              << std::left << "A debugging tool for Lithium Engine" << " *"
              << RESET << std::endl;

    // Print separator line
    std::cout << BLUE << std::string(BORDER_WIDTH, '*') << RESET << std::endl;

    // Print command hint
    std::cout << BLUE << "* " << CYAN << std::setw(BORDER_WIDTH - 4)
              << std::left << "Type 'help' to see a list of available commands."
              << " *" << RESET << std::endl;

    // Print bottom border
    std::cout << BLUE << std::string(BORDER_WIDTH, '*') << RESET << std::endl;
#endif
}

auto ConsoleTerminal::ConsoleTerminalImpl::getRegisteredCommands() const
    -> std::vector<std::string> {
    std::vector<std::string> commands;
    for (const auto& name : component_->getAllCommands()) {
        commands.push_back(name);
    }
    return commands;
}

void ConsoleTerminal::ConsoleTerminalImpl::callCommand(
    std::string_view name, const std::vector<std::any>& args) {
    if (component_->has(name.data())) {
        try {
            if (!args.empty()) {
                component_->dispatch(name.data(), args);
            } else {
                component_->dispatch(name.data());
            }
        } catch (
            const std::runtime_error& e) {  // Catch a more specific exception
            std::cout << "Error: " << e.what() << "\n";
        }
    } else {
        std::cout << "Command '" << name << "' not found.\n";
        auto possibleCommand = suggestionEngine_->suggest(name);
        if (!possibleCommand.empty()) {
            std::cout << "Did you mean: \n";
            for (const auto& cmd : possibleCommand) {
                std::cout << "- " << cmd << "\n";
            }
        }
    }
}

void ConsoleTerminal::ConsoleTerminalImpl::run() {
    std::string input;
    std::deque<std::string> history;

    printHeader();

    while (true) {
        try {
            displayPrompt();

            // 读取输入并处理
            input = readInput();
            if (input.empty())
                continue;

            // 添加到历史记录
            if (historyEnabled_) {
                addToHistory(input);
            }

            // 检查命令
            auto errors = commandChecker_->check(input);
            if (!errors.empty()) {
                printErrors(errors, input, true);
                continue;
            }

            // 解析参数
            auto args = parseArguments(input);

            // 执行命令(添加超时控制)
            auto future = std::async(
                std::launch::async,
                [this, &input, &args]() { callCommand(input, args); });

            if (future.wait_for(commandTimeout_) ==
                std::future_status::timeout) {
                std::cout << "Command execution timed out\n";
                continue;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}

void ConsoleTerminal::ConsoleTerminalImpl::printErrors(
    const std::vector<CommandChecker::Error>& errors, const std::string& input,
    bool continueRun) const {
    // ANSI color codes for formatting
    const std::string RED = "\033[1;31m";
    const std::string YELLOW = "\033[1;33m";
    const std::string RESET = "\033[0m";

    if (!errors.empty()) {
        // Display the input command
        std::cout << "Command: " << input << "\n";

        // Create positional markers for each error
        std::string errorLine(input.length(), ' ');
        for (const auto& error : errors) {
            if (error.column < input.length()) {
                errorLine[error.column] = '^';
            }
        }

        // Print each error with its severity and location
        for (const auto& error : errors) {
            std::string severityColor =
                error.severity == ErrorSeverity::WARNING ? YELLOW : RED;

            std::cout << severityColor
                      << (error.severity == ErrorSeverity::WARNING ? "Warning"
                          : error.severity == ErrorSeverity::ERROR
                              ? "Error"
                              : "Critical Error")
                      << " at line " << error.line << ", column "
                      << error.column << ": " << error.message << RESET << "\n";
        }

        // Display the error markers beneath the command
        std::cout << errorLine << "\n";

        // Abort execution if continueRun is false
        if (!continueRun) {
            std::cout << YELLOW << "Command execution aborted due to errors."
                      << RESET << "\n";
        }
    }
}

auto ConsoleTerminal::ConsoleTerminalImpl::parseArguments(
    const std::string& input) -> std::vector<std::any> {
    std::vector<std::any> args;
    std::string token;
    bool inQuotes = false;
    std::istringstream iss(input);

    for (char character : input) {
        if (character == '"' && !inQuotes) {
            inQuotes = true;
            if (!token.empty()) {
                args.push_back(processToken(token));
                token.clear();
            }
            token += character;
        } else if (character == '"' && inQuotes) {
            token += character;
            args.push_back(processToken(token));
            token.clear();
            inQuotes = false;
        } else if ((std::isspace(character) != 0) && !inQuotes) {
            if (!token.empty()) {
                args.push_back(processToken(token));
                token.clear();
            }
        } else {
            token += character;
        }
    }

    if (!token.empty()) {
        args.push_back(processToken(token));
    }

    return args;
}

auto ConsoleTerminal::ConsoleTerminalImpl::processToken(
    const std::string& token) -> std::any {
    std::regex intRegex("^-?\\d+$");
    std::regex uintRegex("^\\d+u$");
    std::regex longRegex("^-?\\d+l$");
    std::regex ulongRegex("^\\d+ul$");
    std::regex floatRegex(R"(^-?\d*\.\d+f$)");
    std::regex doubleRegex(R"(^-?\d*\.\d+$)");
    std::regex ldoubleRegex(R"(^-?\d*\.\d+ld$)");
    std::regex dateRegex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$)");

    if (token.front() == '"' && token.back() == '"') {
        return token.substr(1, token.size() - 2);  // 去掉引号
    }
    if (std::regex_match(token, intRegex)) {
        return std::stoi(token);
    }
    if (std::regex_match(token, uintRegex)) {
        return static_cast<unsigned int>(
            std::stoul(token.substr(0, token.size() - 1)));
    }
    if (std::regex_match(token, longRegex)) {
        return std::stol(token.substr(0, token.size() - 1));
    }
    if (std::regex_match(token, ulongRegex)) {
        return std::stoul(token.substr(0, token.size() - 2));
    }
    if (std::regex_match(token, floatRegex)) {
        return std::stof(token.substr(0, token.size() - 1));
    }
    if (std::regex_match(token, doubleRegex)) {
        return std::stod(token);
    }
    if (std::regex_match(token, ldoubleRegex)) {
        return std::stold(token.substr(0, token.size() - 2));
    }
    if (token == "true" || token == "false") {
        return token == "true";
    }
    if (std::regex_match(token, dateRegex)) {
        std::tm timeStruct = {};
        std::istringstream stream(token);
        stream >> std::get_time(&timeStruct, "%Y-%m-%d %H:%M:%S");
        return timeStruct;
    }
    return token;  // 默认是字符串
}

void ConsoleTerminal::ConsoleTerminalImpl::handleInput(
    const std::string& input) {
    if (commandCheckEnabled_ && commandChecker_) {
        auto errors = commandChecker_->check(input);
        if (!errors.empty()) {
            printErrors(errors, input, true);
            // 提供命令建议
            if (suggestionEngine_) {
                auto suggestions = suggestionEngine_->suggest(input);
                if (!suggestions.empty()) {
                    std::cout << "\nDid you mean:\n";
                    for (const auto& suggestion : suggestions) {
                        std::cout << "  " << suggestion << "\n";
                    }
                }
            }
            return;
        }
    }

    // 解析和执行命令
    try {
        auto args = parseArguments(input);

        // 使用超时控制执行命令
        auto future = std::async(std::launch::async, [this, &input, &args]() {
            callCommand(input, args);
        });

        if (future.wait_for(commandTimeout_) == std::future_status::timeout) {
            std::cout << "Command execution timed out\n";
            return;
        }
    } catch (const std::exception& e) {
        std::cout << "Error executing command: " << e.what() << "\n";
    }
}

char** ConsoleTerminal::ConsoleTerminalImpl::commandCompletion(const char* text,
                                                               int start,
                                                               int end) {
    (void)start;
    (void)end;
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    return nullptr;
#else
    rl_attempted_completion_over = 1;  // Disable default filename completion
    if (suggestionEngine_) {
        std::string prefix(text);
        auto suggestions = suggestionEngine_->suggest(prefix);
        // 转换建议为readline需要的格式
        // ...remaining completion code...
    }
    return nullptr;
#endif
}

char* ConsoleTerminal::ConsoleTerminalImpl::commandGenerator(const char* text,
                                                             int state) {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    (void)text;
    (void)state;
    return nullptr;
#else
    static std::vector<std::string> matches;
    static size_t matchIndex;

    if (state == 0) {
        matches.clear();
        matchIndex = 0;
        std::string prefix(text);

        auto registeredCommands =
            globalConsoleTerminal->getRegisteredCommands();
        for (const auto& command : registeredCommands) {
            if (command.find(prefix) == 0) {
                matches.push_back(command);
            }
        }
    }

    if (matchIndex < matches.size()) {
        char* result = strdup(matches[matchIndex].c_str());
        matchIndex++;
        return result;
    }
    return nullptr;
#endif
}

void ConsoleTerminal::ConsoleTerminalImpl::displayPrompt() const {
    std::cout << "\n> ";
}

void ConsoleTerminal::setCommandChecker(
    std::shared_ptr<CommandChecker> checker) {
    impl_->commandChecker_ = checker;
}

void ConsoleTerminal::setSuggestionEngine(
    std::shared_ptr<SuggestionEngine> engine) {
    impl_->suggestionEngine_ = engine;
}

void ConsoleTerminal::enableCommandCheck(bool enable) {
    impl_->commandCheckEnabled_ = enable;
}

std::vector<std::string> ConsoleTerminal::getCommandSuggestions(
    const std::string& prefix) {
    if (impl_->suggestionEngine_) {
        return impl_->suggestionEngine_->suggest(prefix);
    }
    return {};
}

}  // namespace lithium::debug
