/**
 * @file terminal.cpp
 * @brief Implementation of main terminal facade
 */

#include "terminal.hpp"

#include <atomic>
#include <csignal>
#include <fstream>
#include <mutex>
#include <sstream>

namespace lithium::debug::terminal {

// Global terminal pointer
Terminal* globalTerminal = nullptr;

// Signal handler
static void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        if (globalTerminal) {
            globalTerminal->stop();
        }
    }
}

class Terminal::Impl {
public:
    TerminalConfig config_;
    TerminalMode mode_{TerminalMode::Interactive};

    std::shared_ptr<ConsoleRenderer> renderer_;
    std::shared_ptr<InputController> input_;
    std::shared_ptr<HistoryManager> history_;
    std::shared_ptr<CommandExecutor> executor_;
    std::shared_ptr<TuiManager> tui_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};

    std::function<std::string()> promptCallback_;
    CompletionCallback completionCallback_;
    std::function<bool(const std::string&)> preCommandCallback_;
    std::function<void(const std::string&, const CommandResult&)>
        postCommandCallback_;

    std::mutex mutex_;

    Impl(const TerminalConfig& config) : config_(config) {
        // Create components
        renderer_ = std::make_shared<ConsoleRenderer>(config.theme);

        InputConfig inputConfig;
        inputConfig.enableHistory = config.enableHistory;
        inputConfig.enableCompletion = config.enableCompletion;
        input_ = std::make_shared<InputController>(inputConfig);

        HistoryConfig historyConfig;
        historyConfig.historyFile = config.historyFile;
        history_ = std::make_shared<HistoryManager>(historyConfig);

        ExecutorConfig execConfig;
        execConfig.defaultTimeout = config.commandTimeout;
        executor_ = std::make_shared<CommandExecutor>(execConfig);

        tui_ = std::make_shared<TuiManager>(renderer_, input_, history_);
    }

    bool initialize() {
        if (initialized_)
            return true;

        // Setup signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Initialize components
        input_->initialize();

        // Load history
        if (!config_.historyFile.empty()) {
            history_->load(config_.historyFile);
        }

        // Register built-in commands
        executor_->registerBuiltins();
        registerTerminalCommands();

        // Setup completion
        if (config_.enableCompletion) {
            input_->setCompletionHandler(
                [this](const std::string& input, size_t pos) {
                    return getCompletions(input, pos);
                });
        }

        // Setup executor callbacks
        executor_->setExitCallback([this]() { running_ = false; });

        executor_->setOutputHandler([this](const std::string& output) {
            if (mode_ == TerminalMode::Tui && tui_->isActive()) {
                tui_->println(output);
            } else {
                renderer_->println(output);
            }
        });

        executor_->setErrorHandler([this](const std::string& error) {
            if (mode_ == TerminalMode::Tui && tui_->isActive()) {
                tui_->error(error);
            } else {
                renderer_->error(error);
            }
        });

        // Initialize TUI if enabled
        if (config_.enableTui && TuiManager::isAvailable()) {
            mode_ = TerminalMode::Tui;
            tui_->setLayout(config_.layout);
            tui_->setTheme(config_.theme);
        } else {
            mode_ = TerminalMode::Interactive;
            tui_->setFallbackMode(true);
        }

        initialized_ = true;
        return true;
    }

    void shutdown() {
        if (!initialized_)
            return;

        running_ = false;

        // Save history
        if (!config_.historyFile.empty()) {
            history_->save(config_.historyFile);
        }

        // Shutdown TUI
        if (tui_->isActive()) {
            tui_->shutdown();
        }

        // Restore terminal
        input_->restore();

        initialized_ = false;
    }

    void registerTerminalCommands() {
        // Theme command
        executor_->registerCommand(CommandDef{
            "theme",
            "Change terminal theme",
            "theme [default|dark|light|ascii]",
            {},
            [this](const std::vector<std::any>& args) -> CommandResult {
                CommandResult result;

                if (args.empty()) {
                    result.success = true;
                    result.output = "Current theme: " + config_.theme.name +
                                    "\nAvailable: default, dark, light, ascii";
                    return result;
                }

                std::string themeName;
                try {
                    themeName = std::any_cast<std::string>(args[0]);
                } catch (...) {
                    result.success = false;
                    result.error = "Invalid theme name";
                    return result;
                }

                if (themeName == "default") {
                    config_.theme = Theme{};
                } else if (themeName == "dark") {
                    config_.theme = Theme::dark();
                } else if (themeName == "light") {
                    config_.theme = Theme::light();
                } else if (themeName == "ascii") {
                    config_.theme = Theme::ascii();
                } else {
                    result.success = false;
                    result.error = "Unknown theme: " + themeName;
                    return result;
                }

                renderer_->setTheme(config_.theme);
                if (tui_->isActive()) {
                    tui_->setTheme(config_.theme);
                    tui_->redraw();
                }

                result.success = true;
                result.output = "Theme changed to: " + themeName;
                return result;
            },
            false,
            0,
            1});

        // History command
        executor_->registerCommand(CommandDef{
            "history",
            "Show command history",
            "history [count] | history clear | history search <pattern>",
            {},
            [this](const std::vector<std::any>& args) -> CommandResult {
                CommandResult result;
                result.success = true;

                if (args.empty()) {
                    // Show recent history
                    auto entries = history_->getRecent(20);
                    std::ostringstream oss;

                    size_t idx = history_->size() - entries.size();
                    for (const auto& entry : entries) {
                        oss << "  " << idx++ << "  " << entry.command << "\n";
                    }

                    result.output = oss.str();
                    return result;
                }

                std::string arg;
                try {
                    arg = std::any_cast<std::string>(args[0]);
                } catch (...) {
                    try {
                        int count = std::any_cast<int>(args[0]);
                        auto entries = history_->getRecent(count);
                        std::ostringstream oss;

                        size_t idx = history_->size() - entries.size();
                        for (const auto& entry : entries) {
                            oss << "  " << idx++ << "  " << entry.command
                                << "\n";
                        }

                        result.output = oss.str();
                        return result;
                    } catch (...) {
                        result.success = false;
                        result.error = "Invalid argument";
                        return result;
                    }
                }

                if (arg == "clear") {
                    history_->clear();
                    result.output = "History cleared";
                } else if (arg == "search" && args.size() > 1) {
                    std::string pattern;
                    try {
                        pattern = std::any_cast<std::string>(args[1]);
                    } catch (...) {
                        result.success = false;
                        result.error = "Invalid search pattern";
                        return result;
                    }

                    auto entries = history_->search(pattern);
                    std::ostringstream oss;
                    for (const auto& entry : entries) {
                        oss << "  " << entry.command << "\n";
                    }
                    result.output = oss.str();
                } else if (arg == "save") {
                    if (history_->save()) {
                        result.output = "History saved";
                    } else {
                        result.success = false;
                        result.error = "Failed to save history";
                    }
                } else if (arg == "stats") {
                    auto stats = history_->getStats();
                    std::ostringstream oss;
                    oss << "Total entries: " << stats.totalEntries << "\n";
                    oss << "Unique commands: " << stats.uniqueCommands << "\n";
                    oss << "Favorites: " << stats.favoriteCount << "\n";

                    if (!stats.topCommands.empty()) {
                        oss << "\nTop commands:\n";
                        for (const auto& [cmd, count] : stats.topCommands) {
                            oss << "  " << cmd << " (" << count << ")\n";
                        }
                    }

                    result.output = oss.str();
                } else {
                    result.success = false;
                    result.error = "Unknown history subcommand: " + arg;
                }

                return result;
            },
            false,
            0,
            2});

        // Layout command (TUI only)
        executor_->registerCommand(CommandDef{
            "layout",
            "Configure TUI layout",
            "layout [show|hide] [history|suggestions|status|help]",
            {},
            [this](const std::vector<std::any>& args) -> CommandResult {
                CommandResult result;

                if (!tui_->isActive() || tui_->isFallbackMode()) {
                    result.success = false;
                    result.error = "Layout command requires TUI mode";
                    return result;
                }

                if (args.size() < 2) {
                    result.success = true;
                    result.output =
                        "Usage: layout [show|hide] "
                        "[history|suggestions|status|help]";
                    return result;
                }

                std::string action, panel;
                try {
                    action = std::any_cast<std::string>(args[0]);
                    panel = std::any_cast<std::string>(args[1]);
                } catch (...) {
                    result.success = false;
                    result.error = "Invalid arguments";
                    return result;
                }

                bool show = (action == "show");

                if (panel == "history") {
                    tui_->showPanel(PanelType::History, show);
                } else if (panel == "suggestions") {
                    tui_->showPanel(PanelType::Suggestions, show);
                } else if (panel == "status") {
                    config_.layout.showStatusBar = show;
                    tui_->setLayout(config_.layout);
                    tui_->applyLayout();
                } else if (panel == "help") {
                    if (show) {
                        tui_->showHelp();
                    } else {
                        tui_->hideHelp();
                    }
                } else {
                    result.success = false;
                    result.error = "Unknown panel: " + panel;
                    return result;
                }

                result.success = true;
                result.output = panel + " panel " + (show ? "shown" : "hidden");
                return result;
            },
            false,
            0,
            2});
    }

    CompletionResult getCompletions(const std::string& input, size_t pos) {
        CompletionResult result;

        // Use custom callback if set
        if (completionCallback_) {
            result.matches = completionCallback_(input);
            return result;
        }

        // Default: complete command names
        std::string prefix = input.substr(0, pos);

        // Find last word
        size_t lastSpace = prefix.rfind(' ');
        std::string word = (lastSpace == std::string::npos)
                               ? prefix
                               : prefix.substr(lastSpace + 1);

        if (lastSpace == std::string::npos) {
            // Completing command name
            for (const auto& cmd : executor_->getCommands()) {
                if (cmd.find(word) == 0) {
                    result.matches.push_back(cmd);
                }
            }
        }

        // Find common prefix
        if (!result.matches.empty()) {
            result.commonPrefix = result.matches[0];
            for (const auto& match : result.matches) {
                size_t i = 0;
                while (i < result.commonPrefix.size() && i < match.size() &&
                       result.commonPrefix[i] == match[i]) {
                    ++i;
                }
                result.commonPrefix = result.commonPrefix.substr(0, i);
            }
        }

        result.hasMultiple = result.matches.size() > 1;
        return result;
    }

    void run() {
        if (!initialized_ && !initialize()) {
            return;
        }

        running_ = true;

        // Print welcome header
        printWelcome();

        if (mode_ == TerminalMode::Tui && !tui_->isFallbackMode()) {
            runTuiMode();
        } else {
            runInteractiveMode();
        }

        shutdown();
    }

    void printWelcome() {
        if (mode_ == TerminalMode::Tui && tui_->isActive()) {
            // TUI will handle its own welcome
        } else {
            renderer_->welcomeHeader("Lithium Debug Terminal", "1.0.0",
                                     "Type 'help' for available commands");
        }
    }

    void runInteractiveMode() {
        while (running_) {
            // Get prompt
            std::string prompt = ">";
            if (promptCallback_) {
                prompt = promptCallback_();
            }

            // Display prompt
            renderer_->prompt(prompt);

            // Read input
            auto inputOpt = input_->readLine();
            if (!inputOpt) {
                // EOF or interrupt
                break;
            }

            std::string inputStr = *inputOpt;

            // Skip empty input
            if (inputStr.empty()) {
                continue;
            }

            // Process input
            processInputInternal(inputStr);
        }
    }

    void runTuiMode() {
        if (!tui_->initialize()) {
            // Fallback to interactive mode
            mode_ = TerminalMode::Interactive;
            runInteractiveMode();
            return;
        }

        // Set up TUI status bar
        tui_->setStatusItems(
            {{"Mode", "TUI"},
             {"Commands", std::to_string(executor_->getCommands().size())},
             {"History", std::to_string(history_->size())}});

        // Set key handler
        tui_->setKeyHandler([this](const InputEvent& event) -> bool {
            if (event.key == Key::Enter) {
                std::string input = tui_->getInput();
                if (!input.empty()) {
                    tui_->clearInput();
                    processInputInternal(input);
                }
                return true;
            }

            if (event.key == Key::Tab) {
                // Trigger completion
                std::string input = tui_->getInput();
                auto completions = getCompletions(input, input.size());

                if (completions.matches.size() == 1) {
                    tui_->setInput(completions.matches[0]);
                } else if (!completions.matches.empty()) {
                    tui_->showSuggestions(completions.matches);
                }
                return true;
            }

            if (event.key == Key::CtrlC) {
                running_ = false;
                return true;
            }

            return false;
        });

        // Main TUI loop
        while (running_) {
            auto event = tui_->waitForEvent(100);

            // Update status periodically
            if (event == TuiEvent::None) {
                tui_->updateStatus("History", std::to_string(history_->size()));
            }
        }

        tui_->shutdown();
    }

    void processInputInternal(const std::string& input) {
        // Pre-command callback
        if (preCommandCallback_ && !preCommandCallback_(input)) {
            return;
        }

        // Add to history
        history_->add(input);

        // Execute command
        auto result = executor_->execute(input);

        // Display result
        if (!result.output.empty()) {
            if (mode_ == TerminalMode::Tui && tui_->isActive()) {
                tui_->println(result.output);
            } else {
                renderer_->println(result.output);
            }
        }

        if (!result.success && !result.error.empty()) {
            if (mode_ == TerminalMode::Tui && tui_->isActive()) {
                tui_->error(result.error);
            } else {
                renderer_->error(result.error);
            }
        }

        // Post-command callback
        if (postCommandCallback_) {
            postCommandCallback_(input, result);
        }
    }
};

// Terminal implementation

Terminal::Terminal(const TerminalConfig& config)
    : impl_(std::make_unique<Impl>(config)) {
    globalTerminal = this;
}

Terminal::~Terminal() {
    if (globalTerminal == this) {
        globalTerminal = nullptr;
    }
}

Terminal::Terminal(Terminal&&) noexcept = default;
Terminal& Terminal::operator=(Terminal&&) noexcept = default;

bool Terminal::initialize() { return impl_->initialize(); }

void Terminal::shutdown() { impl_->shutdown(); }

bool Terminal::isInitialized() const { return impl_->initialized_; }

void Terminal::setConfig(const TerminalConfig& config) {
    impl_->config_ = config;
}

const TerminalConfig& Terminal::getConfig() const { return impl_->config_; }

void Terminal::setTheme(const Theme& theme) {
    impl_->config_.theme = theme;
    impl_->renderer_->setTheme(theme);
    if (impl_->tui_->isActive()) {
        impl_->tui_->setTheme(theme);
    }
}

void Terminal::setMode(TerminalMode mode) { impl_->mode_ = mode; }

TerminalMode Terminal::getMode() const { return impl_->mode_; }

bool Terminal::loadConfig(const std::string& path) {
    // TODO: Implement JSON config loading
    (void)path;
    return false;
}

bool Terminal::saveConfig(const std::string& path) const {
    // TODO: Implement JSON config saving
    (void)path;
    return false;
}

void Terminal::run() { impl_->run(); }

void Terminal::stop() { impl_->running_ = false; }

bool Terminal::isRunning() const { return impl_->running_; }

CommandResult Terminal::processInput(const std::string& input) {
    return impl_->executor_->execute(input);
}

bool Terminal::executeScript(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        impl_->processInputInternal(line);
    }

    return true;
}

void Terminal::registerCommand(const CommandDef& command) {
    impl_->executor_->registerCommand(command);
}

void Terminal::registerCommand(
    const std::string& name, const std::string& description,
    std::function<CommandResult(const std::vector<std::any>&)> handler) {
    impl_->executor_->registerCommand(name, description, std::move(handler));
}

bool Terminal::unregisterCommand(const std::string& name) {
    return impl_->executor_->unregisterCommand(name);
}

std::vector<std::string> Terminal::getCommands() const {
    return impl_->executor_->getCommands();
}

void Terminal::print(const std::string& text) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->print(text);
    } else {
        impl_->renderer_->print(text);
    }
}

void Terminal::println(const std::string& text) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->println(text);
    } else {
        impl_->renderer_->println(text);
    }
}

void Terminal::printStyled(const std::string& text, Color fg, Style style) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->printStyled(text, fg, style);
    } else {
        impl_->renderer_->println(text, fg, std::nullopt, style);
    }
}

void Terminal::success(const std::string& message) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->success(message);
    } else {
        impl_->renderer_->success(message);
    }
}

void Terminal::error(const std::string& message) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->error(message);
    } else {
        impl_->renderer_->error(message);
    }
}

void Terminal::warning(const std::string& message) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->warning(message);
    } else {
        impl_->renderer_->warning(message);
    }
}

void Terminal::info(const std::string& message) {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->info(message);
    } else {
        impl_->renderer_->info(message);
    }
}

void Terminal::clear() {
    if (impl_->mode_ == TerminalMode::Tui && impl_->tui_->isActive()) {
        impl_->tui_->clear();
    } else {
        impl_->renderer_->clear();
    }
}

ConsoleRenderer& Terminal::getRenderer() { return *impl_->renderer_; }

InputController& Terminal::getInput() { return *impl_->input_; }

HistoryManager& Terminal::getHistory() { return *impl_->history_; }

CommandExecutor& Terminal::getExecutor() { return *impl_->executor_; }

TuiManager& Terminal::getTui() { return *impl_->tui_; }

void Terminal::setPromptCallback(std::function<std::string()> callback) {
    impl_->promptCallback_ = std::move(callback);
}

void Terminal::setCompletionCallback(CompletionCallback callback) {
    impl_->completionCallback_ = std::move(callback);
}

void Terminal::setPreCommandCallback(
    std::function<bool(const std::string&)> callback) {
    impl_->preCommandCallback_ = std::move(callback);
}

void Terminal::setPostCommandCallback(
    std::function<void(const std::string&, const CommandResult&)> callback) {
    impl_->postCommandCallback_ = std::move(callback);
}

}  // namespace lithium::debug::terminal
