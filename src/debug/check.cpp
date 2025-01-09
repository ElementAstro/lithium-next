#include "check.hpp"
#include <fstream>

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <utility>

#include "atom/type/json.hpp"

namespace lithium::debug {

class CommandChecker::CommandCheckerImpl {
public:
    std::vector<CheckRule> rules;
    std::vector<std::string> dangerousCommands{"rm", "mkfs", "dd", "format"};
    size_t maxLineLength{80};
    size_t maxNestingDepth{5};
    std::vector<std::string> forbiddenPatterns;
    bool checkPrivilegedCommands{true};
    size_t maxMemoryMB{1024};
    size_t maxFileSize{100};

    CommandCheckerImpl() { initializeDefaultRules(); }

    void addRule(
        const std::string& name,
        std::function<std::optional<Error>(const std::string&, size_t)> check) {
        rules.push_back({name, std::move(check)});
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

        std::ofstream file(configPath);
        file << config.dump(4);
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

        return errors;
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
                    std::regex rx(pattern);
                    std::smatch match;
                    if (std::regex_search(line, match, rx)) {
                        return Error{"Forbidden pattern detected: " + pattern,
                                     lineNumber,
                                     static_cast<size_t>(match.position()),
                                     ErrorSeverity::ERROR};
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
    impl_->addRule(name, std::move(check));
}

void CommandChecker::setDangerousCommands(
    const std::vector<std::string>& commands) {
    impl_->setDangerousCommands(commands);
}

void CommandChecker::setMaxLineLength(size_t length) {
    impl_->setMaxLineLength(length);
}

void CommandChecker::loadConfig(const std::string& configPath) {
    impl_->loadConfig(configPath);
}

void CommandChecker::saveConfig(const std::string& configPath) const {
    impl_->saveConfig(configPath);
}

void CommandChecker::setMaxNestingDepth(size_t depth) {
    impl_->maxNestingDepth = depth;
}

void CommandChecker::setForbiddenPatterns(
    const std::vector<std::string>& patterns) {
    impl_->forbiddenPatterns = patterns;
}

void CommandChecker::enablePrivilegedCommandCheck(bool enable) {
    impl_->checkPrivilegedCommands = enable;
}

void CommandChecker::setResourceLimits(size_t maxMemoryMB, size_t maxFileSize) {
    impl_->maxMemoryMB = maxMemoryMB;
    impl_->maxFileSize = maxFileSize;
}

auto CommandChecker::check(std::string_view command) const
    -> std::vector<Error> {
    return impl_->check(command);
}

auto CommandChecker::toJson(const std::vector<Error>& errors) const -> json {
    return impl_->toJson(errors);
}

void printErrors(const std::vector<CommandChecker::Error>& errors,
                 std::string_view command, bool useColor) {
    std::vector<std::string> lines;
    std::istringstream iss{std::string(command)};
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    for (const auto& error : errors) {
        std::string severityStr;
        std::string colorCode;
        switch (error.severity) {
            case CommandChecker::ErrorSeverity::WARNING:
                severityStr = "warning";
                colorCode = "\033[33m";
                break;
            case CommandChecker::ErrorSeverity::ERROR:
                severityStr = "error";
                colorCode = "\033[31m";
                break;
            case CommandChecker::ErrorSeverity::CRITICAL:
                severityStr = "CRITICAL";
                colorCode = "\033[35m";
                break;
            default:
                colorCode = "";
        }

        if (useColor) {
            std::cout << colorCode;
        }
        std::cout << severityStr << ": " << error.message << "\n";
        std::cout << "  --> line " << error.line << ":" << error.column << "\n";
        std::cout << "   | \n";
        std::cout << " " << error.line << " | " << lines[error.line - 1]
                  << "\n";
        std::cout << "   | " << std::string(error.column, ' ') << "^\n";
        if (useColor) {
            std::cout << "\033[0m";
        }
        std::cout << "\n";
    }
}

}  // namespace lithium::debug
