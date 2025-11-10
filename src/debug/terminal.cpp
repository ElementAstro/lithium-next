#include "terminal.hpp"

#include <chrono>
#include <csignal>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

#if __has_include(<ncurses.h>)
#include <ncurses.h>  // ncurses library
#elif __has_include(<ncurses/ncurses.h>)
#include <ncurses/ncurses.h>
#else
#include <readline/history.h>
#include <readline/readline.h>
#endif

#undef ERROR

#include "check.hpp"
#include "suggestion.hpp"

#include "atom/components/component.hpp"

namespace lithium::debug {

// Define the global console terminal pointer
ConsoleTerminal* globalConsoleTerminal = nullptr;

// Signal handler for graceful termination
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived termination signal. Exiting..." << std::endl;

        // Clean up terminal state
        if (globalConsoleTerminal) {
            // Perform necessary cleanup
        }

        exit(0);
    }
}

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

#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    bool ncursesEnabled_ = false;
#endif

    bool historyEnabled_ = true;
    bool suggestionsEnabled_ = true;
    bool syntaxHighlightEnabled_ = true;
    std::chrono::milliseconds commandTimeout_ =
        std::chrono::milliseconds(DEFAULT_COMMAND_TIMEOUT_MS);

    bool commandCheckEnabled_{true};
};

ConsoleTerminal::ConsoleTerminal()
    : impl_(std::make_unique<ConsoleTerminalImpl>()) {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Set global terminal pointer
    globalConsoleTerminal = this;
}

ConsoleTerminal::~ConsoleTerminal() = default;

ConsoleTerminal::ConsoleTerminal(ConsoleTerminal&& other) noexcept
    : impl_(std::move(other.impl_)),
      historyEnabled_(other.historyEnabled_),
      suggestionsEnabled_(other.suggestionsEnabled_),
      syntaxHighlightEnabled_(other.syntaxHighlightEnabled_),
      commandTimeout_(other.commandTimeout_),
      commandCheckEnabled_(other.commandCheckEnabled_),
      commandChecker_(std::move(other.commandChecker_)),
      suggestionEngine_(std::move(other.suggestionEngine_)) {
    // Transfer ownership of the global pointer if necessary
    if (globalConsoleTerminal == &other) {
        globalConsoleTerminal = this;
    }
}

auto ConsoleTerminal::operator=(ConsoleTerminal&& other) noexcept -> ConsoleTerminal& {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        historyEnabled_ = other.historyEnabled_;
        suggestionsEnabled_ = other.suggestionsEnabled_;
        syntaxHighlightEnabled_ = other.syntaxHighlightEnabled_;
        commandTimeout_ = other.commandTimeout_;
        commandCheckEnabled_ = other.commandCheckEnabled_;
        commandChecker_ = std::move(other.commandChecker_);
        suggestionEngine_ = std::move(other.suggestionEngine_);

        // Transfer ownership of the global pointer if necessary
        if (globalConsoleTerminal == &other) {
            globalConsoleTerminal = this;
        }
    }
    return *this;
}

auto ConsoleTerminal::getRegisteredCommands() const
    -> std::vector<std::string> {
    return impl_->getRegisteredCommands();
}

void ConsoleTerminal::callCommand(std::string_view name,
                                  const std::vector<std::any>& args) {
    impl_->callCommand(name, args);
}

void ConsoleTerminal::run() { impl_->run(); }

void ConsoleTerminal::setCommandTimeout(std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0) {
        std::cerr << "Warning: Invalid timeout value, using default" << std::endl;
        commandTimeout_ = std::chrono::milliseconds(5000);
    } else {
        commandTimeout_ = timeout;
    }

    if (impl_) {
        impl_->commandTimeout_ = commandTimeout_;
    }
}

void ConsoleTerminal::enableHistory(bool enable) {
    historyEnabled_ = enable;
    if (impl_) {
        impl_->historyEnabled_ = enable;
    }
}

void ConsoleTerminal::enableSuggestions(bool enable) {
    suggestionsEnabled_ = enable;
    if (impl_) {
        impl_->suggestionsEnabled_ = enable;
    }
}

void ConsoleTerminal::enableSyntaxHighlight(bool enable) {
    syntaxHighlightEnabled_ = enable;
    if (impl_) {
        impl_->syntaxHighlightEnabled_ = enable;
    }
}

void ConsoleTerminal::loadConfig(const std::string& configPath) {
    if (configPath.empty()) {
        std::cerr << "Error: Config path is empty" << std::endl;
        return;
    }

    try {
        std::cout << "Loading configuration from: " << configPath << std::endl;

        // Load configuration from file
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            std::cerr << "Failed to open config file: " << configPath << std::endl;
            return;
        }

        // In a production environment, parse JSON/XML/YAML here
        // For now, we'll set some default values
        enableHistory(true);
        enableSuggestions(true);
        enableSyntaxHighlight(true);
        setCommandTimeout(std::chrono::milliseconds(5000));

        // Load command checker configuration if available
        if (commandChecker_) {
            commandChecker_->loadConfig(configPath);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
    }
}

ConsoleTerminal::ConsoleTerminalImpl::ConsoleTerminalImpl()
    : commandChecker_(std::make_shared<CommandChecker>()) {
    component_ = std::make_shared<Component>("lithium.terminal");

    suggestionEngine_ =
        std::make_shared<SuggestionEngine>(getRegisteredCommands());

    initializeNcurses();
    // Initialize command checker
    if (commandChecker_) {
        commandChecker_->loadConfig("config/command_check.json");
    }
}

ConsoleTerminal::ConsoleTerminalImpl::~ConsoleTerminalImpl() {
    shutdownNcurses();
}

void ConsoleTerminal::ConsoleTerminalImpl::initializeNcurses() {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    ncursesEnabled_ = true;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
#endif
}

void ConsoleTerminal::ConsoleTerminalImpl::shutdownNcurses() const {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    if (ncursesEnabled_) {
        endwin();
    }
#endif
}

std::string ConsoleTerminal::ConsoleTerminalImpl::readInput() {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    // Using ncurses for input
    char inputBuffer[BUFFER_SIZE] = {0};
    echo();  // Enable character echo
    int result = getnstr(inputBuffer, BUFFER_SIZE - 1);
    noecho();  // Disable character echo

    if (result == ERR) {
        // Handle input error
        return "";
    }

    return std::string(inputBuffer);
#elif defined(_WIN32)
    // Windows-specific input handling without readline
    std::string input;
    std::cout << "> ";
    std::cout.flush();

    char c;
    while ((c = _getch()) != '\r') {
        if (c == '\b') {  // Backspace
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";  // Erase character from display
                std::cout.flush();
            }
        } else if (c == 3) {  // Ctrl+C
            std::cout << "^C" << std::endl;
            return "";
        } else if (c >= 32 && c <= 126) {  // Printable character
            input += c;
            std::cout << c;
            std::cout.flush();
        }
    }
    std::cout << std::endl;
    return input;
#else
    // Unix/Linux input handling with readline
    char* line = readline("> ");
    if (line) {
        std::string input(line);
        free(line);
        return input;
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
#ifndef _WIN32
    add_history(input.c_str());
#endif
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
    if (component_) {
        for (const auto& name : component_->getAllCommands()) {
            commands.push_back(name);
        }
    }
    return commands;
}

void ConsoleTerminal::ConsoleTerminalImpl::callCommand(
    std::string_view name, const std::vector<std::any>& args) {
    if (component_ && component_->has(name.data())) {
        try {
            if (!args.empty()) {
                component_->dispatch(name.data(), args);
            } else {
                component_->dispatch(name.data());
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    } else {
        std::cout << "Command '" << name << "' not found.\n";
        if (suggestionEngine_) {
            auto possibleCommand = suggestionEngine_->suggest(name);
            if (!possibleCommand.empty()) {
                std::cout << "Did you mean: \n";
                for (const auto& cmd : possibleCommand) {
                    std::cout << "- " << cmd << "\n";
                }
            }
        }
    }
}

void ConsoleTerminal::ConsoleTerminalImpl::run() {
    std::string input;
    bool running = true;

    // Print welcome message
    printHeader();

    while (running) {
        try {
            // Display the command prompt
            displayPrompt();

            // Read input from the user
            input = readInput();

            // Check if the input is empty
            if (input.empty()) {
                continue;
            }

            // Check for exit commands
            if (input == "exit" || input == "quit") {
                std::cout << "Exiting console terminal..." << std::endl;
                break;
            }

            // Add the command to history if enabled
            if (historyEnabled_) {
                addToHistory(input);
            }

            // Check the command if checking is enabled
            if (commandCheckEnabled_ && commandChecker_) {
                auto errors = commandChecker_->check(input);
                if (!errors.empty()) {
                    printErrors(errors, input, false);

                    // Provide suggestions if enabled
                    if (suggestionsEnabled_ && suggestionEngine_) {
                        auto suggestions = suggestionEngine_->suggest(input);
                        if (!suggestions.empty()) {
                            std::cout << "Did you mean:" << std::endl;
                            for (const auto& suggestion : suggestions) {
                                std::cout << "  - " << suggestion << std::endl;
                            }
                        }
                    }
                    continue;
                }
            }

            // Extract command name and parse arguments
            std::string cmdName;
            std::istringstream iss(input);
            iss >> cmdName;

            // Extract the remaining string for argument parsing
            std::string argsStr;
            std::getline(iss >> std::ws, argsStr);

            // Parse arguments
            std::vector<std::any> args;
            if (!argsStr.empty()) {
                args = parseArguments(argsStr);
            }

            // Execute the command with timeout
            auto future = std::async(
                std::launch::async,
                [this, cmdName, args]() {
                    try {
                        callCommand(cmdName, args);
                    } catch (const std::exception& e) {
                        std::cerr << "Error executing command: " << e.what() << std::endl;
                    }
                }
            );

            // Wait for the command to complete or timeout
            if (future.wait_for(commandTimeout_) == std::future_status::timeout) {
                std::cout << "Command execution timed out after "
                          << commandTimeout_.count() << "ms" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

void ConsoleTerminal::ConsoleTerminalImpl::printErrors(
    const std::vector<CommandChecker::Error>& errors,
    const std::string& input,
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
            if (error.column < static_cast<int>(input.length())) {
                errorLine[error.column] = '^';
            }
        }

        // Print each error with its severity and location
        for (const auto& error : errors) {
            std::string severityColor =
                error.severity == ErrorSeverity::WARNING ? YELLOW : RED;

            std::cout << severityColor
                      << (error.severity == ErrorSeverity::WARNING ? "Warning"
                         : error.severity == ErrorSeverity::ERROR ? "Error"
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

auto ConsoleTerminal::ConsoleTerminalImpl::parseArguments(const std::string& input) -> std::vector<std::any> {
    std::vector<std::any> args;
    std::string token;
    bool inQuotes = false;
    char quoteChar = '\0';
    bool escape = false;

    // Parse the input character by character
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (escape) {
            // Handle escaped character
            token += c;
            escape = false;
        } else if (c == '\\') {
            // Start of escape sequence
            escape = true;
        } else if (inQuotes) {
            // Inside quoted string
            if (c == quoteChar) {
                // End of quoted string
                inQuotes = false;
                args.push_back(processToken(token));
                token.clear();
            } else {
                // Add character to token
                token += c;
            }
        } else if (c == '"' || c == '\'') {
            // Start of quoted string
            inQuotes = true;
            quoteChar = c;

            // Process any token before the quote
            if (!token.empty()) {
                args.push_back(processToken(token));
                token.clear();
            }
        } else if (std::isspace(c)) {
            // End of token
            if (!token.empty()) {
                args.push_back(processToken(token));
                token.clear();
            }
        } else {
            // Add character to token
            token += c;
        }
    }

    // Process the last token if there is one
    if (!token.empty()) {
        args.push_back(processToken(token));
    }

    // If still in quotes at the end, there's an error
    if (inQuotes) {
        std::cerr << "Warning: Unmatched quote in input" << std::endl;
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
        return token.substr(1, token.size() - 2);  // Remove quotes
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
    return token;  // Default is string
}

void ConsoleTerminal::ConsoleTerminalImpl::handleInput(
    const std::string& input) {
    if (commandCheckEnabled_ && commandChecker_) {
        auto errors = commandChecker_->check(input);
        if (!errors.empty()) {
            printErrors(errors, input, true);
            // Provide command suggestions
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

    // Parse and execute command
    try {
        // Extract command name and arguments
        std::istringstream iss(input);
        std::string cmdName;
        iss >> cmdName;

        // Parse remaining arguments
        std::string argsStr;
        std::getline(iss >> std::ws, argsStr);
        auto args = parseArguments(argsStr);

        // Use timeout control to execute command
        auto future = std::async(std::launch::async, [this, cmdName, args]() {
            callCommand(cmdName, args);
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
#ifndef _WIN32
    rl_attempted_completion_over = 1;  // Disable default filename completion
    return rl_completion_matches(text, commandGenerator);
#else
    return nullptr;
#endif
#endif
}

char* ConsoleTerminal::ConsoleTerminalImpl::commandGenerator(const char* text,
                                                             int state) {
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    (void)text;
    (void)state;
    return nullptr;
#elif defined(_WIN32)
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

        if (globalConsoleTerminal) {
            auto registeredCommands = globalConsoleTerminal->getRegisteredCommands();
            for (const auto& command : registeredCommands) {
                if (command.find(prefix) == 0) {
                    matches.push_back(command);
                }
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
#if __has_include(<ncurses.h>) || __has_include(<ncurses/ncurses.h>)
    printw("> ");
    refresh();
#else
    std::cout << "\n> ";
    std::cout.flush();
#endif
}

void ConsoleTerminal::setCommandChecker(
    std::shared_ptr<CommandChecker> checker) {
    commandChecker_ = checker;
    if (impl_) {
        impl_->commandChecker_ = checker;
    }
}

void ConsoleTerminal::setSuggestionEngine(
    std::shared_ptr<SuggestionEngine> engine) {
    suggestionEngine_ = engine;
    if (impl_) {
        impl_->suggestionEngine_ = engine;
    }
}

void ConsoleTerminal::enableCommandCheck(bool enable) {
    commandCheckEnabled_ = enable;
    if (impl_) {
        impl_->commandCheckEnabled_ = enable;
    }
}

std::vector<std::string> ConsoleTerminal::getCommandSuggestions(
    const std::string& prefix) {
    if (impl_ && impl_->suggestionEngine_) {
        return impl_->suggestionEngine_->suggest(prefix);
    }
    return {};
}

// --- Unified Debugging Integration Implementation ---
void ConsoleTerminal::loadDebugConfig(const std::string& configPath) {
    if (commandChecker_) commandChecker_->loadConfig(configPath);
    // TODO: load suggestion config if needed
}

void ConsoleTerminal::saveDebugConfig(const std::string& configPath) const {
    if (commandChecker_) commandChecker_->saveConfig(configPath);
    // TODO: save suggestion config if needed
}

std::string ConsoleTerminal::exportDebugStateJson() const {
    // Export checker errors and suggestion stats/config as JSON
    std::string result = "{";
    if (commandChecker_) {
        auto rules = commandChecker_->listRules();
        result += "\"rules\":[";
        for (size_t i = 0; i < rules.size(); ++i) {
            result += '"' + rules[i] + '"';
            if (i + 1 < rules.size()) result += ",";
        }
        result += "]";
    }
    if (suggestionEngine_) {
        result += ",\"suggestionStats\":{\"size\":1}";
    }
    result += "}";
    return result;
}

void ConsoleTerminal::importDebugStateJson(const std::string& jsonStr) {
    // TODO: parse and apply JSON config to checker and suggestion engine
}

void ConsoleTerminal::addCommandCheckRule(const std::string& name, std::function<std::optional<CommandCheckerErrorProxy::Error>(const std::string&, size_t)> check) {
    if (commandChecker_) {
        // Adapter: convert proxy Error to real Error if needed
        commandChecker_->addRule(name, [check](const std::string& line, size_t lineNum) -> std::optional<CommandChecker::Error> {
            auto res = check(line, lineNum);
            if (res) {
                CommandChecker::Error err;
                err.message = res->message;
                err.line = res->line;
                err.column = res->column;
                err.severity = static_cast<ErrorSeverity>(res->severity);
                return err;
            }
            return std::nullopt;
        });
    }
}

bool ConsoleTerminal::removeCommandCheckRule(const std::string& name) {
    return commandChecker_ ? commandChecker_->removeRule(name) : false;
}

void ConsoleTerminal::addSuggestionFilter(std::function<bool(const std::string&)> filter) {
    if (suggestionEngine_) suggestionEngine_->addFilter(filter);
}

void ConsoleTerminal::clearSuggestionFilters() {
    if (suggestionEngine_) suggestionEngine_->clearFilters();
}

void ConsoleTerminal::updateSuggestionDataset(const std::vector<std::string>& newItems) {
    if (suggestionEngine_) suggestionEngine_->updateDataset(newItems);
}

void ConsoleTerminal::updateDangerousCommands(const std::vector<std::string>& commands) {
    if (commandChecker_) commandChecker_->setDangerousCommands(commands);
}

void ConsoleTerminal::printDebugReport(const std::string& input, bool useColor) const {
    if (commandChecker_) {
        auto errors = commandChecker_->check(input);
        printErrors(errors, input, useColor);
    }
    if (suggestionEngine_) {
        auto suggestions = suggestionEngine_->suggest(input);
        if (!suggestions.empty()) {
            std::cout << (useColor ? "\033[36m" : "") << "Suggestions: ";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                std::cout << suggestions[i];
                if (i + 1 < suggestions.size()) std::cout << ", ";
            }
            std::cout << (useColor ? "\033[0m" : "") << std::endl;
        }
    }
}

}  // namespace lithium::debug
