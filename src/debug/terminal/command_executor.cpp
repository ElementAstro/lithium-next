/**
 * @file command_executor.cpp
 * @brief Implementation of command executor
 */

#include "command_executor.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace lithium::debug::terminal {

class CommandExecutor::Impl {
public:
    ExecutorConfig config_;
    std::unordered_map<std::string, CommandDef> commands_;
    std::unordered_map<std::string, std::string> aliases_;

    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::mutex executionMutex_;

    std::function<void()> exitCallback_;
    std::function<void(const std::string&)> helpCallback_;
    std::function<bool(const ParsedCommand&)> preExecuteHook_;
    std::function<void(const ParsedCommand&, const CommandResult&)>
        postExecuteHook_;
    std::function<void(const std::string&)> outputHandler_;
    std::function<void(const std::string&)> errorHandler_;

    Impl(const ExecutorConfig& config) : config_(config) {}

    ParsedCommand parse(const std::string& input) const {
        ParsedCommand cmd;
        cmd.rawInput = input;

        std::string trimmed = input;

        // Check for background execution
        if (!trimmed.empty() && trimmed.back() == '&') {
            cmd.isBackground = true;
            trimmed.pop_back();
            // Trim trailing spaces
            while (!trimmed.empty() && std::isspace(trimmed.back())) {
                trimmed.pop_back();
            }
        }

        // Check for output redirection
        size_t redirectPos = trimmed.find('>');
        if (redirectPos != std::string::npos && config_.allowRedirection) {
            cmd.redirectOutput = trimmed.substr(redirectPos + 1);
            trimmed = trimmed.substr(0, redirectPos);

            // Trim whitespace from redirect path
            size_t start = cmd.redirectOutput.find_first_not_of(" \t");
            size_t end = cmd.redirectOutput.find_last_not_of(" \t");
            if (start != std::string::npos) {
                cmd.redirectOutput =
                    cmd.redirectOutput.substr(start, end - start + 1);
            }
        }

        // Check for input redirection
        size_t inputRedirectPos = trimmed.find('<');
        if (inputRedirectPos != std::string::npos && config_.allowRedirection) {
            cmd.redirectInput = trimmed.substr(inputRedirectPos + 1);
            trimmed = trimmed.substr(0, inputRedirectPos);

            // Trim whitespace
            size_t start = cmd.redirectInput.find_first_not_of(" \t");
            size_t end = cmd.redirectInput.find_last_not_of(" \t");
            if (start != std::string::npos) {
                cmd.redirectInput =
                    cmd.redirectInput.substr(start, end - start + 1);
            }
        }

        // Parse command and arguments
        std::istringstream iss(trimmed);
        iss >> cmd.name;

        // Parse arguments with quote handling
        std::string remaining;
        std::getline(iss >> std::ws, remaining);

        if (!remaining.empty()) {
            parseArguments(remaining, cmd.args, cmd.typedArgs);
        }

        return cmd;
    }

    void parseArguments(const std::string& input,
                        std::vector<std::string>& args,
                        std::vector<std::any>& typedArgs) const {
        std::string token;
        bool inQuotes = false;
        char quoteChar = '\0';
        bool escape = false;

        for (size_t i = 0; i < input.length(); ++i) {
            char c = input[i];

            if (escape) {
                token += c;
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (inQuotes) {
                if (c == quoteChar) {
                    inQuotes = false;
                    args.push_back(token);
                    typedArgs.push_back(parseArgument(token));
                    token.clear();
                } else {
                    token += c;
                }
            } else if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
                if (!token.empty()) {
                    args.push_back(token);
                    typedArgs.push_back(parseArgument(token));
                    token.clear();
                }
            } else if (std::isspace(c)) {
                if (!token.empty()) {
                    args.push_back(token);
                    typedArgs.push_back(parseArgument(token));
                    token.clear();
                }
            } else {
                token += c;
            }
        }

        if (!token.empty()) {
            args.push_back(token);
            typedArgs.push_back(parseArgument(token));
        }
    }

    std::any parseArgument(const std::string& arg) const {
        // Try to parse as different types

        // Boolean
        if (arg == "true" || arg == "True" || arg == "TRUE") {
            return true;
        }
        if (arg == "false" || arg == "False" || arg == "FALSE") {
            return false;
        }

        // Integer
        static const std::regex intRegex("^-?\\d+$");
        if (std::regex_match(arg, intRegex)) {
            try {
                return std::stoi(arg);
            } catch (...) {
                try {
                    return std::stol(arg);
                } catch (...) {
                }
            }
        }

        // Float/Double
        static const std::regex floatRegex("^-?\\d*\\.\\d+$");
        if (std::regex_match(arg, floatRegex)) {
            try {
                return std::stod(arg);
            } catch (...) {
            }
        }

        // Default: string
        return arg;
    }

    std::optional<std::string> validate(const ParsedCommand& cmd) const {
        if (cmd.name.empty()) {
            return "Empty command";
        }

        // Resolve alias
        std::string resolvedName = cmd.name;
        if (aliases_.count(cmd.name)) {
            resolvedName = aliases_.at(cmd.name);
        }

        // Check if command exists
        if (!commands_.count(resolvedName)) {
            return "Unknown command: " + cmd.name;
        }

        const auto& def = commands_.at(resolvedName);

        // Check argument count
        int argCount = static_cast<int>(cmd.args.size());

        if (def.requiresArgs && argCount == 0) {
            return "Command '" + cmd.name + "' requires arguments";
        }

        if (argCount < def.minArgs) {
            return "Command '" + cmd.name + "' requires at least " +
                   std::to_string(def.minArgs) + " argument(s)";
        }

        if (def.maxArgs >= 0 && argCount > def.maxArgs) {
            return "Command '" + cmd.name + "' accepts at most " +
                   std::to_string(def.maxArgs) + " argument(s)";
        }

        // Check background execution
        if (cmd.isBackground && !config_.allowBackground) {
            return "Background execution is not allowed";
        }

        return std::nullopt;
    }

    CommandResult execute(const ParsedCommand& cmd) {
        CommandResult result;
        auto startTime = std::chrono::steady_clock::now();

        // Resolve alias
        std::string resolvedName = cmd.name;
        if (aliases_.count(cmd.name)) {
            resolvedName = aliases_.at(cmd.name);
        }

        // Get command handler
        if (!commands_.count(resolvedName)) {
            result.success = false;
            result.error = "Unknown command: " + cmd.name;
            return result;
        }

        const auto& def = commands_.at(resolvedName);

        // Pre-execute hook
        if (preExecuteHook_ && !preExecuteHook_(cmd)) {
            result.success = false;
            result.error = "Command execution blocked by pre-execute hook";
            return result;
        }

        // Execute
        try {
            running_ = true;
            cancelled_ = false;

            result = def.handler(cmd.typedArgs);

            running_ = false;
        } catch (const std::exception& e) {
            running_ = false;
            result.success = false;
            result.error = std::string("Exception: ") + e.what();
        }

        auto endTime = std::chrono::steady_clock::now();
        result.executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        // Post-execute hook
        if (postExecuteHook_) {
            postExecuteHook_(cmd, result);
        }

        return result;
    }

    CommandResult executeWithTimeout(const ParsedCommand& cmd,
                                     std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(executionMutex_);

        auto future = std::async(std::launch::async,
                                 [this, cmd]() { return execute(cmd); });

        if (future.wait_for(timeout) == std::future_status::timeout) {
            cancelled_ = true;
            running_ = false;

            CommandResult result;
            result.success = false;
            result.error = "Command execution timed out after " +
                           std::to_string(timeout.count()) + "ms";
            result.executionTime = timeout;
            return result;
        }

        return future.get();
    }

    void registerBuiltins() {
        // Help command
        commands_["help"] = CommandDef{
            "help",
            "Display help information",
            "help [command]",
            {"?", "h"},
            [this](const std::vector<std::any>& args) -> CommandResult {
                CommandResult result;
                result.success = true;

                if (args.empty()) {
                    // List all commands
                    std::ostringstream oss;
                    oss << "Available commands:\n";

                    std::vector<std::pair<std::string, std::string>> cmds;
                    for (const auto& [name, def] : commands_) {
                        cmds.emplace_back(name, def.description);
                    }

                    std::sort(cmds.begin(), cmds.end());

                    for (const auto& [name, desc] : cmds) {
                        oss << "  " << name << " - " << desc << "\n";
                    }

                    result.output = oss.str();

                    if (helpCallback_) {
                        helpCallback_("");
                    }
                } else {
                    // Show help for specific command
                    std::string cmdName;
                    try {
                        cmdName = std::any_cast<std::string>(args[0]);
                    } catch (...) {
                        result.success = false;
                        result.error = "Invalid argument";
                        return result;
                    }

                    if (commands_.count(cmdName)) {
                        const auto& def = commands_.at(cmdName);
                        std::ostringstream oss;
                        oss << def.name << " - " << def.description << "\n";
                        oss << "Usage: " << def.usage << "\n";

                        if (!def.aliases.empty()) {
                            oss << "Aliases: ";
                            for (size_t i = 0; i < def.aliases.size(); ++i) {
                                if (i > 0)
                                    oss << ", ";
                                oss << def.aliases[i];
                            }
                            oss << "\n";
                        }

                        result.output = oss.str();

                        if (helpCallback_) {
                            helpCallback_(cmdName);
                        }
                    } else {
                        result.success = false;
                        result.error = "Unknown command: " + cmdName;
                    }
                }

                return result;
            },
            false,
            0,
            1};

        // Register aliases for help
        aliases_["?"] = "help";
        aliases_["h"] = "help";

        // Exit command
        commands_["exit"] =
            CommandDef{"exit",
                       "Exit the terminal",
                       "exit",
                       {"quit", "q"},
                       [this](const std::vector<std::any>&) -> CommandResult {
                           CommandResult result;
                           result.success = true;
                           result.output = "Exiting...";

                           if (exitCallback_) {
                               exitCallback_();
                           }

                           return result;
                       },
                       false,
                       0,
                       0};

        aliases_["quit"] = "exit";
        aliases_["q"] = "exit";

        // Clear command
        commands_["clear"] =
            CommandDef{"clear",
                       "Clear the screen",
                       "clear",
                       {"cls"},
                       [](const std::vector<std::any>&) -> CommandResult {
                           CommandResult result;
                           result.success = true;

                           // ANSI clear screen
                           std::cout << "\033[2J\033[H";
                           std::cout.flush();

                           return result;
                       },
                       false,
                       0,
                       0};

        aliases_["cls"] = "clear";

        // Echo command
        commands_["echo"] = CommandDef{
            "echo",
            "Print text to output",
            "echo [text...]",
            {},
            [](const std::vector<std::any>& args) -> CommandResult {
                CommandResult result;
                result.success = true;

                std::ostringstream oss;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0)
                        oss << " ";
                    try {
                        oss << std::any_cast<std::string>(args[i]);
                    } catch (...) {
                        try {
                            oss << std::any_cast<int>(args[i]);
                        } catch (...) {
                            try {
                                oss << std::any_cast<double>(args[i]);
                            } catch (...) {
                                try {
                                    oss << (std::any_cast<bool>(args[i])
                                                ? "true"
                                                : "false");
                                } catch (...) {
                                    oss << "[unknown]";
                                }
                            }
                        }
                    }
                }

                result.output = oss.str();
                return result;
            },
            false,
            0,
            -1};

        // History command (placeholder - actual implementation in terminal)
        commands_["history"] = CommandDef{
            "history",
            "Show command history",
            "history [count]",
            {},
            [](const std::vector<std::any>&) -> CommandResult {
                CommandResult result;
                result.success = true;
                result.output = "History command - implement in terminal";
                return result;
            },
            false,
            0,
            1};
    }
};

// CommandExecutor implementation

CommandExecutor::CommandExecutor(const ExecutorConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

CommandExecutor::~CommandExecutor() = default;

CommandExecutor::CommandExecutor(CommandExecutor&&) noexcept = default;
CommandExecutor& CommandExecutor::operator=(CommandExecutor&&) noexcept =
    default;

void CommandExecutor::setConfig(const ExecutorConfig& config) {
    impl_->config_ = config;
}

const ExecutorConfig& CommandExecutor::getConfig() const {
    return impl_->config_;
}

void CommandExecutor::setTimeout(std::chrono::milliseconds timeout) {
    impl_->config_.defaultTimeout = timeout;
}

void CommandExecutor::registerCommand(const CommandDef& command) {
    impl_->commands_[command.name] = command;

    // Register aliases
    for (const auto& alias : command.aliases) {
        impl_->aliases_[alias] = command.name;
    }
}

void CommandExecutor::registerCommand(
    const std::string& name, const std::string& description,
    std::function<CommandResult(const std::vector<std::any>&)> handler) {
    CommandDef def;
    def.name = name;
    def.description = description;
    def.usage = name;
    def.handler = std::move(handler);

    registerCommand(def);
}

void CommandExecutor::registerAlias(const std::string& alias,
                                    const std::string& command) {
    impl_->aliases_[alias] = command;
}

bool CommandExecutor::unregisterCommand(const std::string& name) {
    if (impl_->commands_.count(name)) {
        // Remove aliases
        const auto& def = impl_->commands_[name];
        for (const auto& alias : def.aliases) {
            impl_->aliases_.erase(alias);
        }

        impl_->commands_.erase(name);
        return true;
    }
    return false;
}

bool CommandExecutor::hasCommand(const std::string& name) const {
    if (impl_->commands_.count(name)) {
        return true;
    }
    if (impl_->aliases_.count(name)) {
        return impl_->commands_.count(impl_->aliases_.at(name)) > 0;
    }
    return false;
}

std::optional<CommandDef> CommandExecutor::getCommand(
    const std::string& name) const {
    std::string resolvedName = name;
    if (impl_->aliases_.count(name)) {
        resolvedName = impl_->aliases_.at(name);
    }

    if (impl_->commands_.count(resolvedName)) {
        return impl_->commands_.at(resolvedName);
    }

    return std::nullopt;
}

std::vector<std::string> CommandExecutor::getCommands() const {
    std::vector<std::string> commands;
    commands.reserve(impl_->commands_.size());

    for (const auto& [name, def] : impl_->commands_) {
        commands.push_back(name);
    }

    std::sort(commands.begin(), commands.end());
    return commands;
}

std::vector<std::pair<std::string, std::string>>
CommandExecutor::getCommandDescriptions() const {
    std::vector<std::pair<std::string, std::string>> descriptions;
    descriptions.reserve(impl_->commands_.size());

    for (const auto& [name, def] : impl_->commands_) {
        descriptions.emplace_back(name, def.description);
    }

    std::sort(descriptions.begin(), descriptions.end());
    return descriptions;
}

ParsedCommand CommandExecutor::parse(const std::string& input) const {
    return impl_->parse(input);
}

std::any CommandExecutor::parseArgument(const std::string& arg) const {
    return impl_->parseArgument(arg);
}

std::optional<std::string> CommandExecutor::validate(
    const ParsedCommand& cmd) const {
    return impl_->validate(cmd);
}

CommandResult CommandExecutor::execute(const std::string& input) {
    auto cmd = parse(input);

    // Validate
    auto error = validate(cmd);
    if (error) {
        CommandResult result;
        result.success = false;
        result.error = *error;
        return result;
    }

    // Execute with timeout
    return impl_->executeWithTimeout(cmd, impl_->config_.defaultTimeout);
}

CommandResult CommandExecutor::execute(const ParsedCommand& cmd) {
    auto error = validate(cmd);
    if (error) {
        CommandResult result;
        result.success = false;
        result.error = *error;
        return result;
    }

    return impl_->executeWithTimeout(cmd, impl_->config_.defaultTimeout);
}

CommandResult CommandExecutor::execute(const std::string& input,
                                       std::chrono::milliseconds timeout) {
    auto cmd = parse(input);

    auto error = validate(cmd);
    if (error) {
        CommandResult result;
        result.success = false;
        result.error = *error;
        return result;
    }

    return impl_->executeWithTimeout(cmd, timeout);
}

std::future<CommandResult> CommandExecutor::executeAsync(
    const std::string& input) {
    return std::async(std::launch::async,
                      [this, input]() { return execute(input); });
}

void CommandExecutor::executeBackground(const std::string& input) {
    std::thread([this, input]() { execute(input); }).detach();
}

bool CommandExecutor::cancel() {
    if (impl_->running_) {
        impl_->cancelled_ = true;
        return true;
    }
    return false;
}

bool CommandExecutor::isRunning() const { return impl_->running_; }

void CommandExecutor::registerBuiltins() { impl_->registerBuiltins(); }

void CommandExecutor::setExitCallback(std::function<void()> callback) {
    impl_->exitCallback_ = std::move(callback);
}

void CommandExecutor::setHelpCallback(
    std::function<void(const std::string&)> callback) {
    impl_->helpCallback_ = std::move(callback);
}

void CommandExecutor::setPreExecuteHook(
    std::function<bool(const ParsedCommand&)> hook) {
    impl_->preExecuteHook_ = std::move(hook);
}

void CommandExecutor::setPostExecuteHook(
    std::function<void(const ParsedCommand&, const CommandResult&)> hook) {
    impl_->postExecuteHook_ = std::move(hook);
}

void CommandExecutor::setOutputHandler(
    std::function<void(const std::string&)> handler) {
    impl_->outputHandler_ = std::move(handler);
}

void CommandExecutor::setErrorHandler(
    std::function<void(const std::string&)> handler) {
    impl_->errorHandler_ = std::move(handler);
}

}  // namespace lithium::debug::terminal
