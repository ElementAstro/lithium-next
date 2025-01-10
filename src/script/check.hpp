#ifndef LITHIUM_SCRIPT_CHECKER_HPP
#define LITHIUM_SCRIPT_CHECKER_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/noncopyable.hpp"

class InvalidFormatException : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_INVALID_FORMAT(...)                                \
    throw InvalidFormatException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__);

namespace lithium {
class ScriptAnalyzerImpl;

enum class ReportFormat { TEXT, JSON, XML };

// 分析配置选项
struct AnalyzerOptions {
    bool async_mode{true};                     // 是否使用异步分析
    bool deep_analysis{false};                 // 是否进行深度分析
    size_t thread_count{4};                    // 分析线程数
    int timeout_seconds{30};                   // 分析超时时间
    std::vector<std::string> ignore_patterns;  // 忽略的模式
};

// 危险项
struct DangerItem {
    std::string category;
    std::string command;
    std::string reason;
    int line;
    std::optional<std::string> context;
};

// 分析结果
struct AnalysisResult {
    int complexity;
    std::vector<DangerItem> dangers;
    double execution_time;
    bool timeout_occurred;
};

class ScriptAnalyzer : public NonCopyable {
public:
    explicit ScriptAnalyzer(const std::string& config_file);
    ~ScriptAnalyzer() override;

    // 原有分析方法
    void analyze(const std::string& script, bool output_json = false,
                 ReportFormat format = ReportFormat::TEXT);

    // 新增接口
    AnalysisResult analyzeWithOptions(const std::string& script,
                                      const AnalyzerOptions& options);
    void updateConfig(const std::string& config_file);
    void addCustomPattern(const std::string& pattern,
                          const std::string& category);
    void setCallback(std::function<void(const DangerItem&)> callback);
    bool validateScript(const std::string& script);
    std::string getSafeVersion(const std::string& script);

    // 统计信息
    size_t getTotalAnalyzed() const;
    double getAverageAnalysisTime() const;

private:
    std::unique_ptr<ScriptAnalyzerImpl> impl_;  // 指向实现类的智能指针
};
}  // namespace lithium

#endif  // LITHIUM_SCRIPT_CHECKER_HPP
