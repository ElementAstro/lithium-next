#include "check.hpp"

#include <fstream>
#include <future>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef ATOM_USE_BOOST_REGEX
#include <boost/regex.hpp>
#else
#include <regex>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "atom/async/pool.hpp"
#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium {
class ScriptAnalyzerImpl {
public:
    explicit ScriptAnalyzerImpl(const std::string& config_file) {
        try {
            config_ = loadConfig(config_file);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize ScriptAnalyzerImpl: {}",
                  e.what());
            throw;
        }
    }

    void analyze(const std::string& script, bool output_json,
                 ReportFormat format) {
        try {
            std::vector<DangerItem> dangers;
            std::vector<std::future<void>> futures;
            futures.emplace_back(
                std::async(std::launch::async,
                           &ScriptAnalyzerImpl::detectScriptTypeAndAnalyze,
                           this, std::cref(script), std::ref(dangers)));
            futures.emplace_back(
                std::async(std::launch::async,
                           &ScriptAnalyzerImpl::suggestSafeReplacements, this,
                           std::cref(script), std::ref(dangers)));
            futures.emplace_back(std::async(
                std::launch::async, &ScriptAnalyzerImpl::detectExternalCommands,
                this, std::cref(script), std::ref(dangers)));
            futures.emplace_back(
                std::async(std::launch::async,
                           &ScriptAnalyzerImpl::detectEnvironmentVariables,
                           this, std::cref(script), std::ref(dangers)));
            futures.emplace_back(std::async(
                std::launch::async, &ScriptAnalyzerImpl::detectFileOperations,
                this, std::cref(script), std::ref(dangers)));
            for (auto& fut : futures) {
                fut.get();
            }
            int complexity = calculateComplexity(script);
            generateReport(dangers, complexity, output_json, format);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Analysis failed: {}", e.what());
            throw;
        }
    }

    json config_;
    mutable std::shared_mutex config_mutex_;

    std::atomic<size_t> total_analyzed_{0};
    std::atomic<double> total_analysis_time_{0.0};
    std::function<void(const DangerItem&)> callback_;
    atom::async::ThreadPool<> thread_pool_;
    std::unordered_map<std::string, std::string> safe_replacements_;

    static auto loadConfig(const std::string& config_file) -> json {
        if (!atom::io::isFileExists(config_file)) {
            THROW_FILE_NOT_FOUND("Config file not found: " + config_file);
        }
        std::ifstream file(config_file);
        if (!file.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Unable to open config file: " +
                                    config_file);
        }
        json config;
        try {
            file >> config;
        } catch (const json::parse_error& e) {
            THROW_INVALID_FORMAT("Invalid JSON format in config file: " +
                                 config_file);
        }
        return config;
    }

    static auto loadConfigFromDatabase(const std::string& db_file) -> json {
        if (!atom::io::isFileExists(db_file)) {
            THROW_FILE_NOT_FOUND("Database file not found: " + db_file);
        }
        std::ifstream file(db_file);
        if (!file.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Unable to open database file: " + db_file);
        }
        json db;
        try {
            file >> db;
        } catch (const json::parse_error& e) {
            THROW_INVALID_FORMAT("Invalid JSON format in database file: " +
                                 db_file);
        }
        return db;
    }

#ifdef ATOM_USE_BOOST_REGEX
    using Regex = boost::regex;
#else
    using Regex = std::regex;
#endif

    static auto isSkippableLine(const std::string& line) -> bool {
        return line.empty() || std::regex_match(line, Regex(R"(^\s*#.*)")) ||
               std::regex_match(line, Regex(R"(^\s*//.*)"));
    }

    void detectScriptTypeAndAnalyze(const std::string& script,
                                    std::vector<DangerItem>& dangers) {
        std::shared_lock lock(config_mutex_);

#ifdef _WIN32
        bool isPowerShell = detectPowerShell(script);
        if (isPowerShell) {
            checkPattern(script, config_["powershell_danger_patterns"],
                         "PowerShell Security Issue", dangers);
        } else {
            checkPattern(script, config_["windows_cmd_danger_patterns"],
                         "CMD Security Issue", dangers);
        }
#else
        if (detectPython(script)) {
            checkPattern(script, config_["python_danger_patterns"],
                         "Python Script Security Issue", dangers);
        } else if (detectRuby(script)) {
            checkPattern(script, config_["ruby_danger_patterns"],
                         "Ruby Script Security Issue", dangers);
        } else {
            checkPattern(script, config_["bash_danger_patterns"],
                         "Shell Script Security Issue", dangers);
        }
#endif
    }

    static bool detectPowerShell(const std::string& script) {
        return script.find("param(") != std::string::npos ||
               script.find("$PSVersionTable") != std::string::npos;
    }

    static bool detectPython(const std::string& script) {
        return script.find("import ") != std::string::npos ||
               script.find("def ") != std::string::npos;
    }

    static bool detectRuby(const std::string& script) {
        return script.find("require ") != std::string::npos ||
               script.find("def ") != std::string::npos;
    }

    void suggestSafeReplacements(const std::string& script,
                                 std::vector<DangerItem>& dangers) {
        std::unordered_map<std::string, std::string> replacements = {
#ifdef _WIN32
            {"Remove-Item -Recurse -Force", "Remove-Item -Recurse"},
            {"Stop-Process -Force", "Stop-Process"},
#else
            {"rm -rf /", "find . -type f -delete"},
            {"kill -9", "kill -TERM"},
#endif
        };
        checkReplacements(script, replacements, dangers);
    }

    void detectExternalCommands(const std::string& script,
                                std::vector<DangerItem>& dangers) {
        std::unordered_set<std::string> externalCommands = {
#ifdef _WIN32
            "Invoke-WebRequest",
            "Invoke-RestMethod",
#else
            "curl",
            "wget",
#endif
        };
        checkExternalCommands(script, externalCommands, dangers);
    }

    void detectEnvironmentVariables(const std::string& script,
                                    std::vector<DangerItem>& dangers) {
#ifdef ATOM_USE_BOOST_REGEX
        boost::regex envVarPattern(R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)");
#else
        std::regex envVarPattern(R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)");
#endif
        checkPattern(script, envVarPattern, "Environment Variable Usage",
                     dangers);
    }

    void detectFileOperations(const std::string& script,
                              std::vector<DangerItem>& dangers) {
#ifdef ATOM_USE_BOOST_REGEX
        boost::regex fileOpPattern(
            R"(\b(open|read|write|close|unlink|rename)\b)");
#else
        std::regex fileOpPattern(
            R"(\b(open|read|write|close|unlink|rename)\b)");
#endif
        checkPattern(script, fileOpPattern, "File Operation", dangers);
    }

    static auto calculateComplexity(const std::string& script) -> int {
#ifdef ATOM_USE_BOOST_REGEX
        boost::regex complexityPatterns(R"(if\b|while\b|for\b|case\b|&&|\|\|)");
#else
        std::regex complexityPatterns(R"(if\b|while\b|for\b|case\b|&&|\|\|)");
#endif
        std::istringstream scriptStream(script);
        std::string line;
        int complexity = 0;

        while (std::getline(scriptStream, line)) {
            if (std::regex_search(line, complexityPatterns)) {
                complexity++;
            }
        }

        return complexity;
    }

    static void generateReport(const std::vector<DangerItem>& dangers,
                               int complexity, bool output_json,
                               ReportFormat format) {
        switch (format) {
            case ReportFormat::JSON:
                if (output_json) {
                    json report = json::object();
                    report["complexity"] = complexity;
                    report["issues"] = json::array();

                    for (const auto& item : dangers) {
                        report["issues"].push_back(
                            {{"category", item.category},
                             {"line", item.line},
                             {"command", item.command},
                             {"reason", item.reason},
                             {"context", item.context.value_or("")}});
                    }
                    LOG_F(INFO, "Generating JSON report: {}", report.dump(4));
                }
                break;
            case ReportFormat::XML:
                LOG_F(INFO, "<Report>");
                LOG_F(INFO, "  <Complexity>{}</Complexity>", complexity);
                LOG_F(INFO, "  <Issues>");
                for (const auto& item : dangers) {
                    LOG_F(INFO, "    <Issue>");
                    LOG_F(INFO, "      <Category>{}</Category>", item.category);
                    LOG_F(INFO, "      <Line>{}</Line>", item.line);
                    LOG_F(INFO, "      <Command>{}</Command>", item.command);
                    LOG_F(INFO, "      <Reason>{}</Reason>", item.reason);
                    LOG_F(INFO, "      <Context>{}</Context>",
                          item.context.value_or(""));
                    LOG_F(INFO, "    </Issue>");
                }
                LOG_F(INFO, "  </Issues>");
                LOG_F(INFO, "</Report>");
                break;
            case ReportFormat::TEXT:
            default:
                LOG_F(INFO, "Shell Script Analysis Report");
                LOG_F(INFO, "============================");
                LOG_F(INFO, "Code Complexity: {}", complexity);

                if (dangers.empty()) {
                    LOG_F(INFO, "No potential dangers found.");
                } else {
                    for (const auto& item : dangers) {
                        LOG_F(INFO,
                              "Category: {}\nLine: {}\nCommand: {}\nReason: "
                              "{}\nContext: {}\n",
                              item.category, item.line, item.command,
                              item.reason, item.context.value_or(""));
                    }
                }
                break;
        }
    }

    static void checkPattern(const std::string& script, const json& patterns,
                             const std::string& category,
                             std::vector<DangerItem>& dangers) {
        std::unordered_set<std::string> detectedIssues;
        std::istringstream scriptStream(script);
        std::string line;
        int lineNum = 0;
        std::vector<std::future<void>> futures;

        while (std::getline(scriptStream, line)) {
            lineNum++;
            if (isSkippableLine(line)) {
                continue;
            }

            for (const auto& item : patterns) {
#ifdef ATOM_USE_BOOST_REGEX
                boost::regex pattern(item["pattern"]);
#else
                std::regex pattern(item["pattern"]);
#endif
                std::string reason = item["reason"];

                if (std::regex_search(line, pattern)) {
                    std::string key = std::to_string(lineNum) + ":" + reason;
                    if (!detectedIssues.contains(key)) {
                        dangers.emplace_back(
                            DangerItem{category, line, reason, lineNum, {}});
                        detectedIssues.insert(key);
                    }
                }
            }
        }
    }

    static void checkPattern(const std::string& script, const Regex& pattern,
                             const std::string& category,
                             std::vector<DangerItem>& dangers) {
        std::unordered_set<std::string> detectedIssues;
        std::istringstream scriptStream(script);
        std::string line;
        int lineNum = 0;

        while (std::getline(scriptStream, line)) {
            lineNum++;
            if (isSkippableLine(line)) {
                continue;
            }

            if (std::regex_search(line, pattern)) {
                std::string key = std::to_string(lineNum) + ":" + category;
                if (!detectedIssues.contains(key)) {
                    dangers.emplace_back(DangerItem{
                        category, line, "Detected usage", lineNum, {}});
                    detectedIssues.insert(key);
                }
            }
        }
    }

    static void checkExternalCommands(
        const std::string& script,
        const std::unordered_set<std::string>& externalCommands,
        std::vector<DangerItem>& dangers) {
        std::istringstream scriptStream(script);
        std::string line;
        int lineNum = 0;
        std::unordered_set<std::string> detectedIssues;

        while (std::getline(scriptStream, line)) {
            lineNum++;
            if (isSkippableLine(line)) {
                continue;
            }

            for (const auto& command : externalCommands) {
                if (line.find(command) != std::string::npos) {
                    std::string key = std::to_string(lineNum) + ":" + command;
                    if (!detectedIssues.contains(key)) {
                        dangers.emplace_back(
                            DangerItem{"External Command",
                                       command,
                                       "Use of external command",
                                       lineNum,
                                       {}});
                        detectedIssues.insert(key);
                    }
                }
            }
        }
    }

    static void checkReplacements(
        const std::string& script,
        const std::unordered_map<std::string, std::string>& replacements,
        std::vector<DangerItem>& dangers) {
        std::istringstream scriptStream(script);
        std::string line;
        int lineNum = 0;
        std::unordered_set<std::string> detectedIssues;

        while (std::getline(scriptStream, line)) {
            lineNum++;
            if (isSkippableLine(line)) {
                continue;
            }

            for (const auto& [unsafe_command, safe_command] : replacements) {
                if (line.find(unsafe_command) != std::string::npos) {
                    std::string key =
                        std::to_string(lineNum) + ":" + unsafe_command;
                    if (!detectedIssues.contains(key)) {
                        dangers.emplace_back(
                            DangerItem{"Unsafe Command",
                                       unsafe_command,
                                       "Suggested replacement: " + safe_command,
                                       lineNum,
                                       {}});
                        detectedIssues.insert(key);
                    }
                }
            }
        }
    }

    bool detectVulnerablePatterns(const std::string& script) {
        static const std::vector<std::regex> vulnerable_patterns = {
            std::regex(R"(eval\s*\()"), std::regex(R"(exec\s*\()"),
            std::regex(R"(system\s*\()")};

        return std::any_of(vulnerable_patterns.begin(),
                           vulnerable_patterns.end(), [&](const auto& pattern) {
                               return std::regex_search(script, pattern);
                           });
    }

    void analyzeSecurityContext(const std::string& script,
                                std::vector<DangerItem>& dangers) {
        static const std::unordered_set<std::string> sensitive_operations = {
            "chmod", "chown", "sudo", "su", "passwd", "mkfs"};

        std::istringstream iss(script);
        std::string line;
        int line_num = 0;

        while (std::getline(iss, line)) {
            line_num++;
            for (const auto& op : sensitive_operations) {
                if (line.find(op) != std::string::npos) {
                    dangers.emplace_back(DangerItem{
                        "Security Context", op, "Sensitive operation detected",
                        line_num, line});
                }
            }
        }
    }

    std::string sanitizeScript(const std::string& script) {
        std::string result = script;
        for (const auto& [unsafe, safe] : safe_replacements_) {
            size_t pos = 0;
            while ((pos = result.find(unsafe, pos)) != std::string::npos) {
                result.replace(pos, unsafe.length(), safe);
                pos += safe.length();
            }
        }
        return result;
    }

    auto validateScript(const std::string& script) -> bool {
        return !detectVulnerablePatterns(script);
    }

public:
    AnalysisResult analyzeWithOptions(const std::string& script,
                                      const AnalyzerOptions& options) {
        auto start_time = std::chrono::steady_clock::now();
        AnalysisResult result;

        try {
            std::vector<std::future<std::vector<DangerItem>>> futures;

            // 并行执行多个分析任务
            futures.push_back(thread_pool_.enqueue([&]() {
                std::vector<DangerItem> items;
                detectScriptTypeAndAnalyze(script, items);
                return items;
            }));

            if (options.deep_analysis) {
                futures.push_back(thread_pool_.enqueue([&]() {
                    std::vector<DangerItem> items;
                    analyzeSecurityContext(script, items);
                    return items;
                }));
            }

            // 使用超时机制收集结果
            auto timeout = std::chrono::seconds(options.timeout_seconds);
            result.timeout_occurred = false;

            for (auto& future : futures) {
                if (future.wait_for(timeout) == std::future_status::timeout) {
                    result.timeout_occurred = true;
                    break;
                }

                auto items = future.get();
                result.dangers.insert(result.dangers.end(), items.begin(),
                                      items.end());
            }

            result.complexity = calculateComplexity(script);

            auto end_time = std::chrono::steady_clock::now();
            result.execution_time =
                std::chrono::duration<double>(end_time - start_time).count();

            // 更新统计信息
            total_analyzed_++;
            total_analysis_time_ += result.execution_time;

            // 触发回调
            if (callback_) {
                for (const auto& danger : result.dangers) {
                    callback_(danger);
                }
            }

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Analysis failed with error: {}", e.what());
            throw;
        }

        return result;
    }

    auto setCallback(std::function<void(const DangerItem&)> callback) -> void {
        callback_ = std::move(callback);
    }

    auto getTotalAnalyzed() const -> size_t { return total_analyzed_; }

    auto getAverageAnalysisTime() const -> double {
        return total_analyzed_ > 0 ? total_analysis_time_ / total_analyzed_
                                   : 0.0;
    }
};

ScriptAnalyzer::ScriptAnalyzer(const std::string& config_file)
    : impl_(std::make_unique<ScriptAnalyzerImpl>(config_file)) {}

ScriptAnalyzer::~ScriptAnalyzer() = default;

void ScriptAnalyzer::analyze(const std::string& script, bool output_json,
                             ReportFormat format) {
    impl_->analyze(script, output_json, format);
}

void ScriptAnalyzer::updateConfig(const std::string& config_file) {
    impl_ = std::make_unique<ScriptAnalyzerImpl>(config_file);
}

void ScriptAnalyzer::setCallback(
    std::function<void(const DangerItem&)> callback) {
    impl_->setCallback(std::move(callback));
}

size_t ScriptAnalyzer::getTotalAnalyzed() const {
    return impl_->getTotalAnalyzed();
}

double ScriptAnalyzer::getAverageAnalysisTime() const {
    return impl_->getAverageAnalysisTime();
}

bool ScriptAnalyzer::validateScript(const std::string& script) {
    return impl_->validateScript(script);
}

std::string ScriptAnalyzer::getSafeVersion(const std::string& script) {
    if (!validateScript(script)) {
        THROW_INVALID_FORMAT("Script contains unsafe patterns");
    }

    // 创建一个临时分析结果
    AnalyzerOptions options;
    options.deep_analysis = true;
    auto result = analyzeWithOptions(script, options);

    // 如果没有发现危险项，返回原始脚本
    if (result.dangers.empty()) {
        return script;
    }

    // 否则返回经过处理的安全版本
    return impl_->sanitizeScript(script);
}

void ScriptAnalyzer::addCustomPattern(const std::string& pattern,
                                      const std::string& category) {
    if (pattern.empty() || category.empty()) {
        THROW_INVALID_FORMAT("Pattern and category cannot be empty");
    }
    try {
        std::regex test(pattern);
    } catch (const std::regex_error& e) {
        THROW_INVALID_FORMAT("Invalid regex pattern: " + std::string(e.what()));
    }

    // 添加到配置中
    json pattern_obj;
    pattern_obj["pattern"] = pattern;
    pattern_obj["category"] = category;
    pattern_obj["reason"] = "Custom pattern match";

    std::lock_guard<std::shared_mutex> lock(impl_->config_mutex_);
    impl_->config_["custom_patterns"].push_back(pattern_obj);
}

AnalysisResult ScriptAnalyzer::analyzeWithOptions(
    const std::string& script, const AnalyzerOptions& options) {
    return impl_->analyzeWithOptions(script, options);
}

}  // namespace lithium