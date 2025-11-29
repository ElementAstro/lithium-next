#include "check.hpp"
#include <fstream>

#include <algorithm>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

#include "atom/type/json.hpp"

namespace lithium::debug {

class CommandChecker::CommandCheckerImpl {
public:
    std::vector<CheckRule> rules;
    std::map<std::string, size_t> ruleNameToIndex;  // Map for fast rule lookup
    std::vector<std::string> dangerousCommands{"rm", "mkfs", "dd", "format"};
    size_t maxLineLength{80};
    size_t maxNestingDepth{5};
    std::vector<std::string> forbiddenPatterns;
    bool checkPrivilegedCommands{true};
    size_t maxMemoryMB{1024};
    size_t maxFileSize{100};
    bool sandboxEnabled{false};
    std::chrono::milliseconds timeoutLimit{5000};  // Default 5 seconds
    std::vector<std::function<bool(const std::string&)>> customSecurityRules;

    CommandCheckerImpl() { initializeDefaultRules(); }

    void addRule(
        const std::string& name,
        std::function<std::optional<Error>(const std::string&, size_t)> check) {
        // Prevent duplicate rule names
        if (ruleNameToIndex.count(name))
            return;
        rules.push_back({name, std::move(check)});
        ruleNameToIndex[name] = rules.size() - 1;
    }

    bool removeRule(const std::string& name) {
        auto it = ruleNameToIndex.find(name);
        if (it == ruleNameToIndex.end())
            return false;
        size_t idx = it->second;
        rules.erase(rules.begin() + idx);
        ruleNameToIndex.erase(it);
        // Rebuild index map
        for (size_t i = 0; i < rules.size(); ++i)
            ruleNameToIndex[rules[i].name] = i;
        return true;
    }

    std::vector<std::string> listRules() const {
        std::vector<std::string> names;
        for (const auto& rule : rules)
            names.push_back(rule.name);
        return names;
    }

    void setDangerousCommands(const std::vector<std::string>& commands) {
        dangerousCommands = commands;
    }

    void setMaxLineLength(size_t length) { maxLineLength = length; }

    void loadConfig(const std::string& configPath) {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + configPath);
        }

        try {
            json config = json::parse(file);

            if (config.contains("maxLineLength")) {
                maxLineLength = config["maxLineLength"];
            }
            if (config.contains("dangerousCommands")) {
                dangerousCommands =
                    config["dangerousCommands"].get<std::vector<std::string>>();
            }
            if (config.contains("maxNestingDepth")) {
                maxNestingDepth = config["maxNestingDepth"];
            }
            if (config.contains("forbiddenPatterns")) {
                forbiddenPatterns =
                    config["forbiddenPatterns"].get<std::vector<std::string>>();
            }
            if (config.contains("checkPrivilegedCommands")) {
                checkPrivilegedCommands = config["checkPrivilegedCommands"];
            }
            if (config.contains("resourceLimits")) {
                maxMemoryMB = config["resourceLimits"]["maxMemoryMB"];
                maxFileSize = config["resourceLimits"]["maxFileSize"];
            }
            if (config.contains("sandboxEnabled")) {
                sandboxEnabled = config["sandboxEnabled"];
            }
            if (config.contains("timeoutLimit")) {
                timeoutLimit =
                    std::chrono::milliseconds(config["timeoutLimit"]);
            }
        } catch (const json::exception& e) {
            throw std::runtime_error("Error parsing config file: " +
                                     std::string(e.what()));
        }
    }

    void saveConfig(const std::string& configPath) const {
        json config;
        config["maxLineLength"] = maxLineLength;
        config["dangerousCommands"] = dangerousCommands;
        config["maxNestingDepth"] = maxNestingDepth;
        config["forbiddenPatterns"] = forbiddenPatterns;
        config["checkPrivilegedCommands"] = checkPrivilegedCommands;
        config["resourceLimits"] = {{"maxMemoryMB", maxMemoryMB},
                                    {"maxFileSize", maxFileSize}};
        config["sandboxEnabled"] = sandboxEnabled;
        config["timeoutLimit"] = timeoutLimit.count();

        try {
            std::ofstream file(configPath);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot open file for writing: " +
                                         configPath);
            }
            file << config.dump(4);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error saving config: " +
                                     std::string(e.what()));
        }
    }

    auto check(std::string_view command) const -> std::vector<Error> {
        std::vector<Error> errors;
        std::vector<std::string> lines;

        std::istringstream iss{std::string(command)};
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }

        for (size_t i = 0; i < lines.size(); ++i) {
            checkLine(lines[i], i + 1, errors);
        }

        // Apply custom security rules if any
        if (!customSecurityRules.empty()) {
            for (size_t i = 0; i < lines.size(); ++i) {
                for (size_t j = 0; j < customSecurityRules.size(); ++j) {
                    const auto& rule = customSecurityRules[j];
                    if (!rule(lines[i])) {
                        errors.push_back(Error{"Custom security rule " +
                                                   std::to_string(j) +
                                                   " violated",
                                               i + 1, 0, ErrorSeverity::ERROR});
                    }
                }
            }
        }

        // Check for nesting depth
        checkNestingDepth(command, errors);

        return errors;
    }

    void checkNestingDepth(std::string_view command,
                           std::vector<Error>& errors) const {
        size_t maxDepth = 0;
        size_t currentDepth = 0;
        size_t line = 1;
        size_t column = 0;
        size_t deepestLine = 1;
        size_t deepestColumn = 0;

        for (size_t i = 0; i < command.size(); ++i) {
            column++;
            if (command[i] == '\n') {
                line++;
                column = 0;
                continue;
            }

            if (command[i] == '{' || command[i] == '(' || command[i] == '[') {
                currentDepth++;
                if (currentDepth > maxDepth) {
                    maxDepth = currentDepth;
                    deepestLine = line;
                    deepestColumn = column;
                }
            } else if (command[i] == '}' || command[i] == ')' ||
                       command[i] == ']') {
                if (currentDepth > 0) {
                    currentDepth--;
                }
            }
        }

        if (maxDepth > maxNestingDepth) {
            errors.push_back(Error{
                "Maximum nesting depth exceeded: " + std::to_string(maxDepth) +
                    " > " + std::to_string(maxNestingDepth),
                deepestLine, deepestColumn, ErrorSeverity::ERROR});
        }
    }

    void initializeDefaultRules() {
        rules.emplace_back("forkbomb",
                           [](const std::string& line,
                              size_t lineNumber) -> std::optional<Error> {
                               auto pos = line.find(":(){ :|:& };:");
                               if (pos != std::string::npos) {
                                   return Error{"Potential forkbomb detected",
                                                lineNumber, pos,
                                                ErrorSeverity::CRITICAL};
                               }
                               return std::nullopt;
                           });

        rules.emplace_back(
            "dangerous_commands",
            [this](const std::string& line,
                   size_t lineNumber) -> std::optional<Error> {
                for (const auto& cmd : dangerousCommands) {
                    auto pos = line.find(cmd);
                    if (pos != std::string::npos) {
                        return Error{"Dangerous command detected: " + cmd,
                                     lineNumber, pos, ErrorSeverity::ERROR};
                    }
                }
                return std::nullopt;
            });

        rules.emplace_back("line_length",
                           [this](const std::string& line,
                                  size_t lineNumber) -> std::optional<Error> {
                               if (line.length() > maxLineLength) {
                                   return Error{"Line exceeds maximum length",
                                                lineNumber, maxLineLength,
                                                ErrorSeverity::WARNING};
                               }
                               return std::nullopt;
                           });

        rules.emplace_back(
            "unmatched_quotes_and_brackets",
            [](const std::string& line,
               size_t lineNumber) -> std::optional<Error> {
                auto doubleQuoteCount =
                    std::count(line.begin(), line.end(), '"');
                auto singleQuoteCount =
                    std::count(line.begin(), line.end(), '\'');
                auto openParenCount = std::count(line.begin(), line.end(), '(');
                auto closeParenCount =
                    std::count(line.begin(), line.end(), ')');
                auto openBraceCount = std::count(line.begin(), line.end(), '{');
                auto closeBraceCount =
                    std::count(line.begin(), line.end(), '}');
                auto openBracketCount =
                    std::count(line.begin(), line.end(), '[');
                auto closeBracketCount =
                    std::count(line.begin(), line.end(), ']');

                if (doubleQuoteCount % 2 != 0) {
                    return Error{"Unmatched double quotes detected", lineNumber,
                                 line.find('"'), ErrorSeverity::ERROR};
                }
                if (singleQuoteCount % 2 != 0) {
                    return Error{"Unmatched single quotes detected", lineNumber,
                                 line.find('\''), ErrorSeverity::ERROR};
                }
                if (openParenCount != closeParenCount) {
                    return Error{"Unmatched parentheses detected", lineNumber,
                                 line.find('('), ErrorSeverity::ERROR};
                }
                if (openBraceCount != closeBraceCount) {
                    return Error{"Unmatched braces detected", lineNumber,
                                 line.find('{'), ErrorSeverity::ERROR};
                }
                if (openBracketCount != closeBracketCount) {
                    return Error{"Unmatched brackets detected", lineNumber,
                                 line.find('['), ErrorSeverity::ERROR};
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "backtick_usage",
            [](const std::string& line,
               size_t lineNumber) -> std::optional<Error> {
                auto pos = line.find('`');
                if (pos != std::string::npos) {
                    return Error{
                        "Use of backticks detected, consider using $() instead",
                        lineNumber, pos, ErrorSeverity::WARNING};
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "unused_variables",
            [](const std::string& line,
               size_t lineNumber) -> std::optional<Error> {
                static std::unordered_map<std::string, size_t> variableUsage;
                std::regex varRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
                std::smatch match;
                std::string::const_iterator searchStart(line.cbegin());
                while (std::regex_search(searchStart, line.cend(), match,
                                         varRegex)) {
                    std::string var = match[1];
                    if (line.find(var + "=") != std::string::npos) {
                        variableUsage[var] = lineNumber;
                    } else if (variableUsage.find(var) == variableUsage.end()) {
                        return Error{"Unused variable detected: " + var,
                                     lineNumber,
                                     static_cast<size_t>(match.position()),
                                     ErrorSeverity::WARNING};
                    }
                    searchStart = match.suffix().first;
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "potential_infinite_loop",
            [](const std::string& line,
               size_t lineNumber) -> std::optional<Error> {
                if (line.find("while (true)") != std::string::npos ||
                    line.find("for (;;)") != std::string::npos) {
                    return Error{"Potential infinite loop detected", lineNumber,
                                 line.find("while (true)") != std::string::npos
                                     ? line.find("while (true)")
                                     : line.find("for (;;)"),
                                 ErrorSeverity::WARNING};
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "privileged_commands",
            [this](const std::string& line,
                   size_t lineNumber) -> std::optional<Error> {
                if (!checkPrivilegedCommands)
                    return std::nullopt;

                static const std::vector<std::string> privilegedCmds = {
                    "sudo", "su", "passwd", "chown", "chmod"};

                for (const auto& cmd : privilegedCmds) {
                    auto pos = line.find(cmd);
                    if (pos != std::string::npos) {
                        return Error{"Privileged command detected: " + cmd,
                                     lineNumber, pos, ErrorSeverity::WARNING};
                    }
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "resource_limits",
            [this](const std::string& line,
                   size_t lineNumber) -> std::optional<Error> {
                std::regex memoryPattern(R"(\b(\d+)[mg]b?\b)",
                                         std::regex::icase);
                std::smatch match;
                if (std::regex_search(line, match, memoryPattern)) {
                    size_t value = std::stoull(match[1].str());
                    if (value > maxMemoryMB) {
                        return Error{
                            "Memory limit exceeded: " + std::to_string(value) +
                                "MB",
                            lineNumber, static_cast<size_t>(match.position()),
                            ErrorSeverity::ERROR};
                    }
                }
                return std::nullopt;
            });

        rules.emplace_back(
            "forbidden_patterns",
            [this](const std::string& line,
                   size_t lineNumber) -> std::optional<Error> {
                for (const auto& pattern : forbiddenPatterns) {
                    try {
                        std::regex rx(pattern);
                        std::smatch match;
                        if (std::regex_search(line, match, rx)) {
                            return Error{
                                "Forbidden pattern detected: " + pattern,
                                lineNumber,
                                static_cast<size_t>(match.position()),
                                ErrorSeverity::ERROR};
                        }
                    } catch (const std::regex_error& e) {
                        // Invalid regex pattern - log this but continue
                        // checking
                        std::cerr << "Invalid regex pattern: " << pattern
                                  << " - " << e.what() << std::endl;
                    }
                }
                return std::nullopt;
            });
    }

    void checkLine(const std::string& line, size_t lineNumber,
                   std::vector<Error>& errors) const {
        for (const auto& rule : rules) {
            if (auto error = rule.check(line, lineNumber)) {
                errors.push_back(*error);
            }
        }
    }

    auto severityToString(ErrorSeverity severity) const -> std::string {
        switch (severity) {
            case ErrorSeverity::WARNING:
                return "warning";
            case ErrorSeverity::ERROR:
                return "error";
            case ErrorSeverity::CRITICAL:
                return "critical";
            default:
                return "unknown";
        }
    }

    auto toJson(const std::vector<Error>& errors) const -> json {
        json j = json::array();
        for (const auto& error : errors) {
            j.push_back({{"message", error.message},
                         {"line", error.line},
                         {"column", error.column},
                         {"severity", severityToString(error.severity)}});
        }
        return j;
    }
};

CommandChecker::CommandChecker()
    : impl_(std::make_unique<CommandCheckerImpl>()) {}

CommandChecker::~CommandChecker() = default;

void CommandChecker::addRule(
    const std::string& name,
    std::function<std::optional<Error>(const std::string&, size_t)> check) {
    std::unique_lock lock(ruleMutex_);
    impl_->addRule(name, std::move(check));
}

bool CommandChecker::removeRule(const std::string& name) {
    std::unique_lock lock(ruleMutex_);
    return impl_->removeRule(name);
}

std::vector<std::string> CommandChecker::listRules() const {
    std::shared_lock lock(ruleMutex_);
    return impl_->listRules();
}

void CommandChecker::setDangerousCommands(
    const std::vector<std::string>& commands) {
    std::unique_lock lock(ruleMutex_);
    impl_->setDangerousCommands(commands);
}

void CommandChecker::setMaxLineLength(size_t length) {
    std::unique_lock lock(ruleMutex_);
    impl_->setMaxLineLength(length);
}

void CommandChecker::loadConfig(const std::string& configPath) {
    std::unique_lock lock(ruleMutex_);
    impl_->loadConfig(configPath);
}

void CommandChecker::saveConfig(const std::string& configPath) const {
    std::shared_lock lock(ruleMutex_);
    impl_->saveConfig(configPath);
}

void CommandChecker::setMaxNestingDepth(size_t depth) {
    std::unique_lock lock(ruleMutex_);
    impl_->maxNestingDepth = depth;
}

void CommandChecker::setForbiddenPatterns(
    const std::vector<std::string>& patterns) {
    std::unique_lock lock(ruleMutex_);
    impl_->forbiddenPatterns = patterns;
}

void CommandChecker::enablePrivilegedCommandCheck(bool enable) {
    std::unique_lock lock(ruleMutex_);
    impl_->checkPrivilegedCommands = enable;
}

void CommandChecker::setResourceLimits(size_t maxMemoryMB, size_t maxFileSize) {
    std::unique_lock lock(ruleMutex_);
    impl_->maxMemoryMB = maxMemoryMB;
    impl_->maxFileSize = maxFileSize;
}

void CommandChecker::enableSandbox(bool enable) {
    std::unique_lock lock(ruleMutex_);

    impl_->sandboxEnabled = enable;

    // If enabling, initialize sandbox environment
    if (enable) {
        // Add sandbox-specific rules
        impl_->addRule(
            "sandbox_filesystem",
            [](const std::string& line,
               size_t lineNumber) -> std::optional<Error> {
                static const std::regex fsPattern(
                    R"((^|[^\w])(\/etc\/|\/var\/|\/root\/|\/boot\/))");
                std::smatch match;
                if (std::regex_search(line, match, fsPattern)) {
                    return Error{"Access to protected filesystem location: " +
                                     std::string(match[2]),
                                 lineNumber,
                                 static_cast<size_t>(match.position()),
                                 ErrorSeverity::ERROR};
                }
                return std::nullopt;
            });
    }
}

void CommandChecker::addSecurityRule(
    std::function<bool(const std::string&)> rule) {
    std::unique_lock lock(ruleMutex_);

    // Create a unique rule name based on the current number of custom rules
    std::string ruleName =
        "custom_security_rule_" + std::to_string(rules_.get_size());

    // Create a wrapper that adapts the boolean function to the CheckRule
    // interface
    auto ruleWrapper = [rule](const std::string& line,
                              size_t lineNumber) -> std::optional<Error> {
        if (!rule(line)) {
            return Error{"Custom security rule violated", lineNumber, 0,
                         ErrorSeverity::ERROR};
        }
        return std::nullopt;
    };

    // Add to concurrent vector safely
    auto newRule = std::make_unique<CheckRule>();
    newRule->name = ruleName;
    newRule->check = ruleWrapper;
    rules_.push_back(std::move(newRule));

    // Also add to impl's custom security rules
    impl_->customSecurityRules.push_back(rule);
}

void CommandChecker::setTimeoutLimit(std::chrono::milliseconds timeout) {
    std::unique_lock lock(ruleMutex_);
    impl_->timeoutLimit = timeout;

    // Add a rule to check for potential timeout issues in commands
    impl_->addRule(
        "timeout_check",
        [timeout](const std::string& line,
                  size_t lineNumber) -> std::optional<Error> {
            // Check for patterns that might cause timeouts (infinite loops,
            // etc.)
            static const std::regex sleepPattern(
                R"((sleep|timeout|wait)\s+(\d+))");
            std::smatch match;
            if (std::regex_search(line, match, sleepPattern)) {
                try {
                    auto waitTime = std::stoi(match[2]);
                    auto msTimeout = timeout.count();

                    // If command explicitly waits longer than our timeout
                    if (waitTime * 1000 > msTimeout) {
                        return Error{
                            "Potential timeout issue: wait time exceeds limit",
                            lineNumber, static_cast<size_t>(match.position()),
                            ErrorSeverity::WARNING};
                    }
                } catch (...) {
                    // Number parsing failed, ignore
                }
            }
            return std::nullopt;
        });
}

auto CommandChecker::check(std::string_view command) const
    -> std::vector<Error> {
    // Use shared lock for reading rules
    std::shared_lock lock(ruleMutex_);

    std::vector<Error> errors;

    // First apply the PIMPL built-in rules
    auto implErrors = impl_->check(command);
    errors.insert(errors.end(), implErrors.begin(), implErrors.end());

    // Then apply the concurrent vector rules
    std::vector<std::string> lines;
    std::istringstream iss{std::string(command)};
    std::string line;
    size_t lineNum = 1;

    while (std::getline(iss, line)) {
        for (const auto& rule : rules_) {
            if (auto error = rule->check(line, lineNum)) {
                errors.push_back(*error);
            }
        }
        lineNum++;
    }

    return errors;
}

auto CommandChecker::toJson(const std::vector<Error>& errors) const -> json {
    std::shared_lock lock(ruleMutex_);
    return impl_->toJson(errors);
}

void printErrors(const std::vector<CommandChecker::Error>& errors,
                 std::string_view command, bool useColor) {
    if (errors.empty()) {
        std::cout << (useColor ? "\033[32m" : "")
                  << "✓ Command passed all checks"
                  << (useColor ? "\033[0m" : "") << std::endl;
        return;
    }

    std::vector<std::string> lines;
    std::istringstream iss{std::string(command)};
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    // Group errors by line
    std::map<size_t, std::vector<CommandChecker::Error>> errorsByLine;
    for (const auto& error : errors) {
        errorsByLine[error.line].push_back(error);
    }

    // Count errors by severity
    int warnings = 0, errors_count = 0, critical = 0;
    for (const auto& error : errors) {
        switch (error.severity) {
            case ErrorSeverity::WARNING:
                warnings++;
                break;
            case ErrorSeverity::ERROR:
                errors_count++;
                break;
            case ErrorSeverity::CRITICAL:
                critical++;
                break;
        }
    }

    // Print summary
    std::cout << (useColor ? "\033[1m" : "") << "Found " << errors.size()
              << " issues: " << (useColor ? "\033[33m" : "") << warnings
              << " warnings, " << (useColor ? "\033[31m" : "") << errors_count
              << " errors, " << (useColor ? "\033[35m" : "") << critical
              << " critical" << (useColor ? "\033[0m" : "") << "\n\n";

    // Print errors by line
    for (const auto& [lineNum, lineErrors] : errorsByLine) {
        if (lineNum > 0 && lineNum <= lines.size()) {
            // Print the line
            std::cout << (useColor ? "\033[1m" : "") << "Line " << lineNum
                      << ":" << (useColor ? "\033[0m" : "") << "\n";
            std::cout << "  " << lines[lineNum - 1] << "\n";

            // Sort errors by column for consistent output
            auto sortedErrors = lineErrors;
            std::sort(sortedErrors.begin(), sortedErrors.end(),
                      [](const auto& a, const auto& b) {
                          return a.column < b.column;
                      });

            // Print all errors for this line
            for (const auto& error : sortedErrors) {
                std::string severityStr;
                std::string colorCode;
                switch (error.severity) {
                    case ErrorSeverity::WARNING:
                        severityStr = "warning";
                        colorCode = "\033[33m";
                        break;
                    case ErrorSeverity::ERROR:
                        severityStr = "error";
                        colorCode = "\033[31m";
                        break;
                    case ErrorSeverity::CRITICAL:
                        severityStr = "CRITICAL";
                        colorCode = "\033[35m";
                        break;
                }

                std::cout << "  " << std::string(error.column, ' ') << "^\n";
                std::cout << "  " << (useColor ? colorCode : "") << severityStr
                          << ": " << error.message
                          << (useColor ? "\033[0m" : "") << "\n\n";
            }
        }
    }
}

}  // namespace lithium::debug
