/**
 * @file terminal.cpp
 * @brief Console terminal implementation using modular components
 */
#include "terminal.hpp"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "atom/components/component.hpp"
#include "check.hpp"
#include "suggestion.hpp"
#include "terminal/command_executor.hpp"
#include "terminal/history_manager.hpp"
#include "terminal/input_controller.hpp"
#include "terminal/renderer.hpp"
#include "terminal/tui_manager.hpp"

namespace lithium::debug {

ConsoleTerminal* globalConsoleTerminal = nullptr;

static void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        // Signal handled
    }
}

class ConsoleTerminal::ConsoleTerminalImpl {
public:
    std::unique_ptr<terminal::ConsoleRenderer> renderer_;
    std::unique_ptr<terminal::InputController> input_;
    std::unique_ptr<terminal::HistoryManager> history_;
    std::unique_ptr<terminal::CommandExecutor> executor_;
    std::unique_ptr<terminal::TuiManager> tui_;
    std::shared_ptr<SuggestionEngine> suggestionEngine_;
    std::shared_ptr<CommandChecker> commandChecker_;
    std::shared_ptr<Component> component_;
    bool historyEnabled_{true};
    bool suggestionsEnabled_{true};
    bool syntaxHighlightEnabled_{true};
    bool commandCheckEnabled_{true};
    bool tuiEnabled_{false};
    bool running_{false};
    std::chrono::milliseconds commandTimeout_{5000};
    terminal::Theme currentTheme_;

    ConsoleTerminalImpl() {
        currentTheme_ = terminal::Theme{};
        renderer_ = std::make_unique<terminal::ConsoleRenderer>(currentTheme_);
        terminal::InputConfig inputConfig;
        inputConfig.enableHistory = true;
        inputConfig.enableCompletion = true;
        input_ = std::make_unique<terminal::InputController>(inputConfig);
        terminal::HistoryConfig historyConfig;
        historyConfig.maxEntries = 1000;
        historyConfig.historyFile = "lithium_history.json";
        history_ = std::make_unique<terminal::HistoryManager>(historyConfig);
        terminal::ExecutorConfig execConfig;
        execConfig.defaultTimeout = commandTimeout_;
        executor_ = std::make_unique<terminal::CommandExecutor>(execConfig);
        auto rp = std::shared_ptr<terminal::ConsoleRenderer>(renderer_.get(),
                                                             [](auto*) {});
        auto ip = std::shared_ptr<terminal::InputController>(input_.get(),
                                                             [](auto*) {});
        auto hp = std::shared_ptr<terminal::HistoryManager>(history_.get(),
                                                            [](auto*) {});
        tui_ = std::make_unique<terminal::TuiManager>(rp, ip, hp);
        commandChecker_ = std::make_shared<CommandChecker>();
        component_ = std::make_shared<Component>("lithium.terminal");
        suggestionEngine_ =
            std::make_shared<SuggestionEngine>(getRegisteredCommands());
        initialize();
    }
    ~ConsoleTerminalImpl() { shutdown(); }

    void initialize() {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        input_->initialize();
        input_->setCompletionHandler(
            [this](const std::string& text, size_t pos) {
                terminal::CompletionResult result;
                std::string prefix = text.substr(0, pos);
                size_t lastSpace = prefix.rfind(' ');
                std::string word = (lastSpace == std::string::npos)
                                       ? prefix
                                       : prefix.substr(lastSpace + 1);
                for (const auto& cmd : getRegisteredCommands())
                    if (cmd.find(word) == 0)
                        result.matches.push_back(cmd);
                if (suggestionEngine_)
                    for (const auto& s : suggestionEngine_->suggest(word)) {
                        bool found = false;
                        for (const auto& m : result.matches)
                            if (m == s) {
                                found = true;
                                break;
                            }
                        if (!found)
                            result.matches.push_back(s);
                    }
                result.hasMultiple = result.matches.size() > 1;
                return result;
            });
        executor_->registerBuiltins();
        for (const auto& cmdName : getRegisteredCommands()) {
            terminal::CommandDef def;
            def.name = cmdName;
            def.description = "Component command: " + cmdName;
            def.usage = cmdName;
            def.handler = [this, cmdName](const std::vector<std::any>& args)
                -> terminal::CommandResult {
                terminal::CommandResult result;
                try {
                    callCommand(cmdName, args);
                    result.success = true;
                } catch (const std::exception& e) {
                    result.success = false;
                    result.error = e.what();
                }
                return result;
            };
            executor_->registerCommand(def);
        }
        executor_->setExitCallback([this]() { running_ = false; });
        history_->load();
        if (commandChecker_)
            try {
                commandChecker_->loadConfig("config/command_check.json");
            } catch (...) {
            }
    }

    void shutdown() {
        history_->save();
        if (tui_ && tui_->isActive())
            tui_->shutdown();
        input_->restore();
    }

    std::vector<std::string> getRegisteredCommands() const {
        std::vector<std::string> commands;
        if (component_)
            for (const auto& name : component_->getAllCommands())
                commands.push_back(name);
        return commands;
    }

    void callCommand(std::string_view name, const std::vector<std::any>& args) {
        if (component_ && component_->has(name.data())) {
            try {
                if (!args.empty())
                    component_->dispatch(name.data(), args);
                else
                    component_->dispatch(name.data());
            } catch (const std::exception& e) {
                renderer_->error("Error: " + std::string(e.what()));
            }
        } else {
            renderer_->error("Command '" + std::string(name) + "' not found.");
            if (suggestionEngine_) {
                auto suggestions = suggestionEngine_->suggest(name);
                if (!suggestions.empty())
                    renderer_->suggestions(suggestions, "Did you mean");
            }
        }
    }

    void printHeader() {
        if (tuiEnabled_ && tui_->isActive())
            return;
        renderer_->welcomeHeader("Lithium Command Line Tool", "1.0.0",
                                 "Type 'help' for commands");
    }

    void displayPrompt() {
        if (tuiEnabled_ && tui_->isActive())
            return;
        renderer_->prompt(">");
    }

    std::string readInput() {
        if (tuiEnabled_ && tui_->isActive())
            return tui_->getInput();
        return input_->readLine().value_or("");
    }

    void processInput(const std::string& inputStr) {
        if (historyEnabled_)
            history_->add(inputStr);
        if (commandCheckEnabled_ && commandChecker_) {
            auto errors = commandChecker_->check(inputStr);
            if (!errors.empty()) {
                printErrors(errors, inputStr, false);
                if (suggestionsEnabled_ && suggestionEngine_) {
                    auto suggestions = suggestionEngine_->suggest(inputStr);
                    if (!suggestions.empty())
                        renderer_->suggestions(suggestions, "Did you mean");
                }
                return;
            }
        }
        auto result = executor_->execute(inputStr);
        if (!result.output.empty())
            renderer_->println(result.output);
        if (!result.success && !result.error.empty())
            renderer_->error(result.error);
    }

    void printErrors(const std::vector<CommandChecker::Error>& errors,
                     const std::string& inputStr, bool continueRun) {
        if (errors.empty())
            return;
        renderer_->println("Command: " + inputStr);
        std::string errorLine(inputStr.length(), ' ');
        for (const auto& error : errors)
            if (error.column < static_cast<int>(inputStr.length()))
                errorLine[error.column] = '^';
        for (const auto& error : errors) {
            terminal::Color color = (error.severity == ErrorSeverity::WARNING)
                                        ? terminal::Color::Yellow
                                        : terminal::Color::Red;
            std::string sev =
                (error.severity == ErrorSeverity::WARNING) ? "Warning"
                : (error.severity == ErrorSeverity::ERROR) ? "Error"
                                                           : "Critical";
            std::ostringstream oss;
            oss << sev << " at line " << error.line << ", column "
                << error.column << ": " << error.message;
            renderer_->println(oss.str(), color);
        }
        renderer_->println(errorLine);
        if (!continueRun)
            renderer_->warning("Command execution aborted.");
    }

    std::vector<std::any> parseArguments(const std::string& inputStr) {
        std::vector<std::any> args;
        std::string token;
        bool inQuotes = false, escape = false;
        char quoteChar = '\0';
        for (char c : inputStr) {
            if (escape) {
                token += c;
                escape = false;
            } else if (c == '\\')
                escape = true;
            else if (inQuotes) {
                if (c == quoteChar) {
                    inQuotes = false;
                    args.push_back(processToken(token));
                    token.clear();
                } else
                    token += c;
            } else if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
                if (!token.empty()) {
                    args.push_back(processToken(token));
                    token.clear();
                }
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                if (!token.empty()) {
                    args.push_back(processToken(token));
                    token.clear();
                }
            } else
                token += c;
        }
        if (!token.empty())
            args.push_back(processToken(token));
        return args;
    }

    std::any processToken(const std::string& token) {
        if (token == "true" || token == "True" || token == "TRUE")
            return true;
        if (token == "false" || token == "False" || token == "FALSE")
            return false;
        try {
            size_t pos;
            int v = std::stoi(token, &pos);
            if (pos == token.length())
                return v;
        } catch (...) {
        }
        try {
            size_t pos;
            double v = std::stod(token, &pos);
            if (pos == token.length())
                return v;
        } catch (...) {
        }
        return token;
    }

    void run() {
        running_ = true;
        printHeader();
        if (tuiEnabled_ && terminal::TuiManager::isAvailable()) {
            tui_->initialize();
            while (running_) {
                auto event = tui_->waitForEvent(100);
                if (event == terminal::TuiEvent::KeyPress) {
                    std::string inputStr = tui_->getInput();
                    if (!inputStr.empty()) {
                        tui_->clearInput();
                        processInput(inputStr);
                    }
                }
            }
            tui_->shutdown();
        } else {
            while (running_) {
                try {
                    displayPrompt();
                    std::string inputStr = readInput();
                    if (inputStr.empty())
                        continue;
                    if (inputStr == "exit" || inputStr == "quit") {
                        renderer_->info("Exiting...");
                        break;
                    }
                    processInput(inputStr);
                } catch (const std::exception& e) {
                    renderer_->error("Error: " + std::string(e.what()));
                }
            }
        }
        shutdown();
    }

    std::vector<std::string> getCompletions(const std::string& prefix) {
        std::vector<std::string> completions;
        for (const auto& cmd : getRegisteredCommands())
            if (cmd.find(prefix) == 0)
                completions.push_back(cmd);
        if (suggestionEngine_)
            for (const auto& s : suggestionEngine_->suggest(prefix)) {
                bool found = false;
                for (const auto& c : completions)
                    if (c == s) {
                        found = true;
                        break;
                    }
                if (!found)
                    completions.push_back(s);
            }
        return completions;
    }
};

// Public Interface
ConsoleTerminal::ConsoleTerminal()
    : impl_(std::make_unique<ConsoleTerminalImpl>()) {
    globalConsoleTerminal = this;
}
ConsoleTerminal::~ConsoleTerminal() {
    if (globalConsoleTerminal == this)
        globalConsoleTerminal = nullptr;
}
ConsoleTerminal::ConsoleTerminal(ConsoleTerminal&& o) noexcept
    : impl_(std::move(o.impl_)),
      historyEnabled_(o.historyEnabled_),
      suggestionsEnabled_(o.suggestionsEnabled_),
      syntaxHighlightEnabled_(o.syntaxHighlightEnabled_),
      commandTimeout_(o.commandTimeout_),
      commandCheckEnabled_(o.commandCheckEnabled_),
      commandChecker_(std::move(o.commandChecker_)),
      suggestionEngine_(std::move(o.suggestionEngine_)) {
    if (globalConsoleTerminal == &o)
        globalConsoleTerminal = this;
}
auto ConsoleTerminal::operator=(ConsoleTerminal&& o) noexcept
    -> ConsoleTerminal& {
    if (this != &o) {
        impl_ = std::move(o.impl_);
        historyEnabled_ = o.historyEnabled_;
        suggestionsEnabled_ = o.suggestionsEnabled_;
        syntaxHighlightEnabled_ = o.syntaxHighlightEnabled_;
        commandTimeout_ = o.commandTimeout_;
        commandCheckEnabled_ = o.commandCheckEnabled_;
        commandChecker_ = std::move(o.commandChecker_);
        suggestionEngine_ = std::move(o.suggestionEngine_);
        if (globalConsoleTerminal == &o)
            globalConsoleTerminal = this;
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
    commandTimeout_ = timeout;
    if (impl_) {
        impl_->commandTimeout_ = timeout;
        impl_->executor_->setTimeout(timeout);
    }
}
void ConsoleTerminal::enableHistory(bool enable) {
    historyEnabled_ = enable;
    if (impl_)
        impl_->historyEnabled_ = enable;
}
void ConsoleTerminal::enableSuggestions(bool enable) {
    suggestionsEnabled_ = enable;
    if (impl_)
        impl_->suggestionsEnabled_ = enable;
}
void ConsoleTerminal::enableSyntaxHighlight(bool enable) {
    syntaxHighlightEnabled_ = enable;
    if (impl_)
        impl_->syntaxHighlightEnabled_ = enable;
}
void ConsoleTerminal::loadConfig(const std::string& configPath) {
    if (configPath.empty()) {
        impl_->renderer_->error("Config path is empty");
        return;
    }
    try {
        std::ifstream f(configPath);
        if (!f.is_open()) {
            impl_->renderer_->error("Failed to open: " + configPath);
            return;
        }
        enableHistory(true);
        enableSuggestions(true);
        enableSyntaxHighlight(true);
        setCommandTimeout(std::chrono::milliseconds(5000));
        if (commandChecker_)
            commandChecker_->loadConfig(configPath);
        impl_->renderer_->success("Config loaded: " + configPath);
    } catch (const std::exception& e) {
        impl_->renderer_->error("Error: " + std::string(e.what()));
    }
}
void ConsoleTerminal::setCommandChecker(
    std::shared_ptr<CommandChecker> checker) {
    commandChecker_ = checker;
    if (impl_)
        impl_->commandChecker_ = checker;
}
void ConsoleTerminal::setSuggestionEngine(
    std::shared_ptr<SuggestionEngine> engine) {
    suggestionEngine_ = engine;
    if (impl_)
        impl_->suggestionEngine_ = engine;
}
void ConsoleTerminal::enableCommandCheck(bool enable) {
    commandCheckEnabled_ = enable;
    if (impl_)
        impl_->commandCheckEnabled_ = enable;
}
std::vector<std::string> ConsoleTerminal::getCommandSuggestions(
    const std::string& prefix) {
    return impl_->getCompletions(prefix);
}
void ConsoleTerminal::loadDebugConfig(const std::string& configPath) {
    if (commandChecker_)
        commandChecker_->loadConfig(configPath);
}
void ConsoleTerminal::saveDebugConfig(const std::string& configPath) const {
    if (commandChecker_)
        commandChecker_->saveConfig(configPath);
}
std::string ConsoleTerminal::exportDebugStateJson() const {
    std::ostringstream oss;
    oss << "{";
    if (commandChecker_) {
        auto rules = commandChecker_->listRules();
        oss << "\"rules\":[";
        for (size_t i = 0; i < rules.size(); ++i) {
            oss << '"' << rules[i] << '"';
            if (i + 1 < rules.size())
                oss << ",";
        }
        oss << "]";
    }
    if (suggestionEngine_)
        oss << ",\"suggestionStats\":{\"size\":1}";
    oss << "}";
    return oss.str();
}
void ConsoleTerminal::importDebugStateJson(const std::string&) {}
void ConsoleTerminal::addCommandCheckRule(
    const std::string& name,
    std::function<std::optional<CommandCheckerErrorProxy::Error>(
        const std::string&, size_t)>
        check) {
    if (commandChecker_) {
        commandChecker_->addRule(
            name,
            [check](const std::string& line,
                    size_t lineNum) -> std::optional<CommandChecker::Error> {
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
void ConsoleTerminal::addSuggestionFilter(
    std::function<bool(const std::string&)> filter) {
    if (suggestionEngine_)
        suggestionEngine_->addFilter(filter);
}
void ConsoleTerminal::clearSuggestionFilters() {
    if (suggestionEngine_)
        suggestionEngine_->clearFilters();
}
void ConsoleTerminal::updateSuggestionDataset(
    const std::vector<std::string>& newItems) {
    if (suggestionEngine_)
        suggestionEngine_->updateDataset(newItems);
}
void ConsoleTerminal::updateDangerousCommands(
    const std::vector<std::string>& commands) {
    if (commandChecker_)
        commandChecker_->setDangerousCommands(commands);
}
void ConsoleTerminal::printDebugReport(const std::string& inputStr,
                                       bool) const {
    if (commandChecker_) {
        auto errors = commandChecker_->check(inputStr);
        if (!errors.empty())
            impl_->printErrors(errors, inputStr, true);
    }
    if (suggestionEngine_) {
        auto suggestions = suggestionEngine_->suggest(inputStr);
        if (!suggestions.empty())
            impl_->renderer_->suggestions(suggestions, "Suggestions");
    }
}
void ConsoleTerminal::enableTuiMode(bool enable) {
    if (impl_)
        impl_->tuiEnabled_ = enable;
}
bool ConsoleTerminal::isTuiAvailable() const {
    return terminal::TuiManager::isAvailable();
}
terminal::CommandExecutor* ConsoleTerminal::getExecutor() {
    return impl_ ? impl_->executor_.get() : nullptr;
}
terminal::ConsoleRenderer* ConsoleTerminal::getRenderer() {
    return impl_ ? impl_->renderer_.get() : nullptr;
}
terminal::TuiManager* ConsoleTerminal::getTuiManager() {
    return impl_ ? impl_->tui_.get() : nullptr;
}
void ConsoleTerminal::setTheme(const std::string& themeName) {
    if (!impl_)
        return;
    if (themeName == "default")
        impl_->currentTheme_ = terminal::Theme{};
    else if (themeName == "dark")
        impl_->currentTheme_ = terminal::Theme::dark();
    else if (themeName == "light")
        impl_->currentTheme_ = terminal::Theme::light();
    else if (themeName == "ascii")
        impl_->currentTheme_ = terminal::Theme::ascii();
    else {
        impl_->renderer_->warning("Unknown theme: " + themeName);
        return;
    }
    impl_->renderer_->setTheme(impl_->currentTheme_);
    if (impl_->tui_ && impl_->tui_->isActive()) {
        impl_->tui_->setTheme(impl_->currentTheme_);
        impl_->tui_->redraw();
    }
    impl_->renderer_->success("Theme: " + themeName);
}
void ConsoleTerminal::printSuccess(const std::string& message) {
    if (impl_)
        impl_->renderer_->success(message);
}
void ConsoleTerminal::printError(const std::string& message) {
    if (impl_)
        impl_->renderer_->error(message);
}
void ConsoleTerminal::printWarning(const std::string& message) {
    if (impl_)
        impl_->renderer_->warning(message);
}
void ConsoleTerminal::printInfo(const std::string& message) {
    if (impl_)
        impl_->renderer_->info(message);
}
void ConsoleTerminal::clearScreen() {
    if (impl_)
        impl_->renderer_->clear();
}
void ConsoleTerminal::showProgress(float progress, const std::string& label) {
    if (impl_)
        impl_->renderer_->progressBar(progress, 40, label);
}

}  // namespace lithium::debug
