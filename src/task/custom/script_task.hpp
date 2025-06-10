#ifndef LITHIUM_TASK_SCRIPT_TASK_HPP
#define LITHIUM_TASK_SCRIPT_TASK_HPP

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>
#include "script/check.hpp"
#include "script/sheller.hpp"
#include "task.hpp"

// 重试策略枚举
enum class RetryStrategy {
    None,         // 不重试
    Linear,       // 线性重试间隔
    Exponential,  // 指数级重试间隔
    Custom        // 自定义重试策略
};

// 脚本优先级配置
struct ScriptPriority {
    int level;     // 优先级级别
    bool preempt;  // 是否可抢占
    int timeout;   // 超时时间(秒)
};

// 脚本运行状态
struct ScriptStatus {
    float progress;                                   // 执行进度
    std::string currentStage;                         // 当前阶段
    std::chrono::system_clock::time_point startTime;  // 开始时间
    std::vector<std::string> outputs;                 // 输出记录
    bool isRunning;                                   // 是否在运行
    std::optional<int> exitCode;                      // 退出码
};

namespace lithium::sequencer::task {

// 脚本分析结果结构
struct ScriptAnalysisResult {
    bool isValid;
    std::vector<DangerItem> dangers;
    int complexity;
    std::string safeVersion;
};

/**
 * @class ScriptTask
 * @brief 统一的脚本执行任务类，整合了所有脚本管理和执行功能
 */
class ScriptTask : public Task {
public:
    ScriptTask(const std::string& name,
               const std::string& scriptConfigPath = "",
               const std::string& analyzerConfigPath = "");

    // 核心功能
    void execute(const json& params) override;

    // 脚本管理方法
    void registerScript(const std::string& name, const std::string& content);
    void updateScript(const std::string& name, const std::string& content);
    void deleteScript(const std::string& name);
    bool validateScript(const std::string& content);
    ScriptAnalysisResult analyzeScript(const std::string& content);

    // 执行控制方法
    void setScriptTimeout(std::chrono::seconds timeout);
    void setScriptRetryCount(int count);
    void setScriptEnvironment(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& env);
    void setRetryStrategy(const std::string& name, RetryStrategy strategy);

    // 监控和控制方法
    float getScriptProgress(const std::string& name);
    void abortScript(const std::string& name);
    auto getScriptLogs(const std::string& name) -> std::vector<std::string>;

    // 钩子方法
    void addPreExecutionHook(const std::string& name,
                             std::function<void(const std::string&)> hook);
    void addPostExecutionHook(
        const std::string& name,
        std::function<void(const std::string&, int)> hook);

    // 新增配置方法
    void setScriptPriority(const std::string& name, ScriptPriority priority);
    void setConcurrencyLimit(int limit);
    void setResourceLimit(const std::string& name, size_t memoryLimit,
                          int cpuLimit);

    // 新增监控方法
    ScriptStatus getScriptStatus(const std::string& name) const;
    std::vector<std::string> getActiveScripts() const;
    void pauseScript(const std::string& name);
    void resumeScript(const std::string& name);

    // 新增分析方法
    std::vector<std::string> getDependencies(const std::string& name) const;
    float getResourceUsage(const std::string& name) const;
    std::chrono::milliseconds getExecutionTime(const std::string& name) const;

private:
    std::string scriptConfigPath_;
    std::shared_ptr<ScriptManager> scriptManager_;
    std::unique_ptr<ScriptAnalyzer> scriptAnalyzer_;
    json config_;
    std::map<std::string, std::pair<size_t, int>> resourceLimits_;

    // 新增成员变量
    std::unordered_map<std::string, ScriptPriority> priorities_;
    std::unordered_map<std::string, ScriptStatus> scriptStatuses_;
    int concurrencyLimit_{4};
    std::shared_mutex statusMutex_;
    std::atomic<bool> shouldStop_{false};

    // 内部方法
    void setupDefaults();
    void validateParameters(const json& params);
    void handleScriptError(const std::string& scriptName,
                           const std::string& error);
    void monitorExecution(const std::string& scriptName);
    void monitorPerformance(const std::string& scriptName);
    void updateTaskStatus(const std::string& scriptName, bool success);
    void validateResults(
        const std::string& scriptName,
        const std::optional<std::pair<std::string, int>>& result);
    void executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args);

    // 新增辅助方法
    void checkResourceLimits(const std::string& name);
    void updateScriptStatus(const std::string& name,
                            const ScriptStatus& status);
    void cleanupScript(const std::string& name);
    std::string validateAndPreprocessScript(const std::string& content);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_SCRIPT_TASK_HPP