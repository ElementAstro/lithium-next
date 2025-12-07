#ifndef LITHIUM_TASK_COMPONENTS_SCRIPT_TASK_HPP
#define LITHIUM_TASK_COMPONENTS_SCRIPT_TASK_HPP

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <queue>
#include <shared_mutex>
#include <string>
#include <vector>
#include "../core/task.hpp"
#include "atom/type/json.hpp"
#include "script/check.hpp"
#include "script/python_caller.hpp"
#include "script/sheller.hpp"

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

// 新增脚本类型枚举
enum class ScriptType {
    Shell,   // Shell/PowerShell脚本
    Python,  // Python脚本
    Mixed,   // 同时包含Shell和Python组件的脚本
    Auto     // 自动检测脚本类型
};

// 扩展的脚本执行上下文
struct ScriptExecutionContext {
    ScriptType type;
    std::string workingDirectory;
    std::unordered_map<std::string, std::string> environment;
    std::vector<std::string> dependencies;
    bool requiresElevation;
    std::chrono::seconds maxExecutionTime;
    size_t maxMemoryUsage;
    int maxCpuUsage;
};

namespace lithium::task::task {
using json = nlohmann::json;
// 脚本分析结果结构
struct ScriptAnalysisResult {
    bool isValid;
    std::vector<DangerItem> dangers;
    int complexity;
    std::string safeVersion;
};

/**
 * @class ScriptTask
 * @brief Enhanced unified script execution task with Python and shell support
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

    // 新增Python特定方法
    void registerPythonScript(const std::string& name,
                              const std::string& content);
    void loadPythonModule(const std::string& moduleName,
                          const std::string& alias = "");

    template <typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                  const std::string& functionName,
                                  Args... args);

    template <typename T>
    T getPythonVariable(const std::string& alias, const std::string& varName);

    void setPythonVariable(const std::string& alias, const std::string& varName,
                           const py::object& value);

    // 扩展的执行方法
    void executeWithContext(const std::string& scriptName,
                            const ScriptExecutionContext& context);

    std::future<ScriptStatus> executeAsync(const std::string& scriptName,
                                           const json& params = {});

    void executePipeline(const std::vector<std::string>& scriptNames,
                         const json& sharedContext = {});

    // 脚本工作流管理
    void createWorkflow(const std::string& workflowName,
                        const std::vector<std::string>& scriptSequence);

    void executeWorkflow(const std::string& workflowName,
                         const json& params = {});

    void pauseWorkflow(const std::string& workflowName);
    void resumeWorkflow(const std::string& workflowName);
    void abortWorkflow(const std::string& workflowName);

    // 资源管理
    void setResourcePool(size_t maxConcurrentScripts, size_t totalMemoryLimit);

    void reserveResources(const std::string& scriptName, size_t memoryMB,
                          int cpuPercent);

    void releaseResources(const std::string& scriptName);

    // 依赖管理
    void addScriptDependency(const std::string& scriptName,
                             const std::string& dependencyName);

    bool checkDependencies(const std::string& scriptName);
    void resolveDependencies(const std::string& scriptName);

    // 事件处理
    void addEventListener(const std::string& eventType,
                          std::function<void(const json&)> handler);

    void removeEventListener(const std::string& eventType);
    void fireEvent(const std::string& eventType, const json& data);

    // 缓存和优化
    void enableScriptCaching(bool enable = true);
    void precompileScript(const std::string& scriptName);
    void clearScriptCache();

    // 调试和性能分析
    void enableDebugMode(const std::string& scriptName, bool enable = true);
    void setBreakpoint(const std::string& scriptName, int lineNumber);
    void stepExecution(const std::string& scriptName);
    void getCallStack(const std::string& scriptName);

    struct ProfilingData {
        std::chrono::milliseconds executionTime;
        size_t memoryUsage;
        float cpuUsage;
        size_t ioOperations;
        std::map<std::string, std::chrono::milliseconds> functionTimes;
    };

    ProfilingData getProfilingData(const std::string& scriptName);

    // 脚本模板和参数化
    void registerTemplate(const std::string& templateName,
                          const std::string& templateContent);

    void instantiateFromTemplate(const std::string& templateName,
                                 const std::string& scriptName,
                                 const json& parameters);

    // 多语言支持
    void executeHybridScript(const std::string& scriptName,
                             const std::vector<ScriptType>& languages);

    void bridgeLanguages(const std::string& fromScript,
                         const std::string& toScript, const json& data);

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

    // 新组件
    std::unique_ptr<PythonWrapper> pythonWrapper_;
    std::map<std::string, ScriptExecutionContext> executionContexts_;
    std::map<std::string, std::vector<std::string>> workflows_;
    std::map<std::string, std::vector<std::string>> dependencies_;
    std::map<std::string, std::function<void(const json&)>> eventHandlers_;
    std::map<std::string, std::string> scriptTemplates_;

    // 资源管理
    struct ResourcePool {
        size_t maxConcurrentScripts;
        size_t totalMemoryLimit;
        size_t usedMemory;
        std::queue<std::string> waitingQueue;
        std::condition_variable resourceAvailable;
        std::mutex resourceMutex;
    } resourcePool_;

    // 缓存
    std::map<std::string, py::object> compiledPythonScripts_;
    std::map<std::string, std::string> cachedShellScripts_;
    bool cachingEnabled_{false};

    // 调试
    std::map<std::string, std::set<int>> breakpoints_;
    std::map<std::string, bool> debugModeEnabled_;

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

    // 新增方法实现
    ScriptType detectScriptType(const std::string& content);
    void initializePythonEnvironment();
    void setupResourcePool();
    void validateExecutionContext(const ScriptExecutionContext& context);
    void waitForResources(const std::string& scriptName);
    void executeScriptWithType(const std::string& scriptName, ScriptType type,
                               const json& params);
    void handleLanguageBridge(const std::string& fromLang,
                              const std::string& toLang, const json& data);
    void processTemplate(const std::string& templateContent,
                         const json& parameters, std::string& result);
};

// 模板实现
template <typename ReturnType, typename... Args>
ReturnType ScriptTask::callPythonFunction(const std::string& alias,
                                          const std::string& functionName,
                                          Args... args) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        addHistoryEntry("Calling Python function: " + alias +
                        "::" + functionName);
        return pythonWrapper_->call_function<ReturnType>(alias, functionName,
                                                         args...);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Python function call failed: " + std::string(e.what()));
        throw;
    }
}

template <typename T>
T ScriptTask::getPythonVariable(const std::string& alias,
                                const std::string& varName) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        return pythonWrapper_->get_variable<T>(alias, varName);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Failed to get Python variable: " + std::string(e.what()));
        throw;
    }
}

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_COMPONENTS_SCRIPT_TASK_HPP
