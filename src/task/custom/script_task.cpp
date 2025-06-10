#include "script_task.hpp"
#include "atom/log/loguru.hpp"
#include "factory.hpp"
#include "task_registry.hpp"

namespace lithium::sequencer::task {

ScriptTask::ScriptTask(const std::string& name,
                       const std::string& scriptConfigPath,
                       const std::string& analyzerConfigPath)
    : Task(name, [this](const json& params) { execute(params); }),
      scriptConfigPath_(scriptConfigPath) {
    // 初始化组件
    scriptManager_ = std::make_shared<ScriptManager>();
    if (!analyzerConfigPath.empty()) {
        scriptAnalyzer_ = std::make_unique<ScriptAnalyzer>(analyzerConfigPath);
    }

    setupDefaults();
}

void ScriptTask::setupDefaults() {
    // 基本参数配置
    addParamDefinition("scriptName", "string", true, nullptr, "脚本名称");
    addParamDefinition("scriptContent", "string", false, nullptr, "脚本内容");
    addParamDefinition("allowUnsafe", "boolean", false, false,
                       "允许不安全脚本");
    addParamDefinition("timeout", "number", false, 30, "超时时间(秒)");
    addParamDefinition("args", "object", false, json::object(), "脚本参数");
    addParamDefinition("retryCount", "number", false, 0, "重试次数");

    // 任务属性设置
    setTimeout(std::chrono::seconds(300));
    // setPriority(8);
    setLogLevel(3);

    // 设置异常处理
    setExceptionCallback([this](const std::exception& e) {
        LOG_F(ERROR, "Script task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
    });
}

void ScriptTask::execute(const json& params) {
    LOG_F(INFO, "Starting script task with params: {}", params.dump());

    try {
        // 验证参数
        validateParameters(params);

        // 提取基本参数
        std::string scriptName = params["scriptName"].get<std::string>();
        bool allowUnsafe = params.value("allowUnsafe", false);
        int timeout = params.value("timeout", 30);
        auto args = params.value("args", json::object())
                        .get<std::unordered_map<std::string, std::string>>();

        // 处理脚本内容
        if (params.contains("scriptContent")) {
            std::string scriptContent =
                params["scriptContent"].get<std::string>();
            if (!scriptContent.empty()) {
                // 分析和验证脚本
                if (scriptAnalyzer_) {
                    auto analysisResult = analyzeScript(scriptContent);
                    if (!analysisResult.isValid && !allowUnsafe) {
                        THROW_INVALID_FORMAT("Script validation failed: " +
                                             scriptName);
                    }
                    scriptContent = allowUnsafe ? scriptContent
                                                : analysisResult.safeVersion;
                }
                registerScript(scriptName, scriptContent);
            }
        }

        // 配置执行环境
        scriptManager_->setExecutionEnvironment(scriptName, "production");
        // scriptManager_->setTimeout(std::chrono::seconds(timeout));

        // 执行脚本
        executeScript(scriptName, args);

        LOG_F(INFO, "Script task completed successfully: {}", scriptName);
        addHistoryEntry("Script executed: " + scriptName);

    } catch (const std::exception& e) {
        handleScriptError("unknown", e.what());
        throw;
    }
}

// ... 实现其他公共接口方法(registerScript, analyzeScript等) ...
// 注意：这里省略了其他方法的实现，它们的实现逻辑与之前的 TaskScript
// 和 ScriptManagerTask 中的相应方法基本相同

void ScriptTask::executeScript(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args) {
    LOG_F(INFO, "Executing script: {}", scriptName);

    try {
        // 启动监控
        auto monitor =
            std::async(std::launch::async, &ScriptTask::monitorExecution, this,
                       scriptName);
        auto perfMonitor =
            std::async(std::launch::async, &ScriptTask::monitorPerformance,
                       this, scriptName);

        // 执行脚本
        auto result = scriptManager_->runScript(scriptName, args, true);

        // 验证结果
        validateResults(scriptName, result);

        // 更新任务状态
        updateTaskStatus(scriptName, true);

    } catch (const std::exception& e) {
        handleScriptError(scriptName, e.what());
        throw;
    }
}

void ScriptTask::setPriority(const std::string& name, ScriptPriority priority) {
    std::unique_lock lock(statusMutex_);
    priorities_[name] = priority;
}

void ScriptTask::setConcurrencyLimit(int limit) {
    if (limit > 0) {
        concurrencyLimit_ = limit;
    }
}

auto ScriptTask::getScriptStatus(const std::string& name) const
    -> ScriptStatus {
    std::shared_lock lock(statusMutex_);
    if (auto it = scriptStatuses_.find(name); it != scriptStatuses_.end()) {
        return it->second;
    }
    return ScriptStatus{};
}

void ScriptTask::pauseScript(const std::string& name) {
    scriptManager_->abortScript(name);
    updateScriptStatus(name, {.isRunning = false});
}

std::string ScriptTask::validateAndPreprocessScript(
    const std::string& content) {
    // 检查脚本有效性
    if (!validateScript(content)) {
        THROW_INVALID_FORMAT("Invalid script content");
    }

    // 分析脚本并获取安全版本
    auto result = analyzeScript(content);
    if (!result.isValid) {
        THROW_INVALID_FORMAT("Script analysis failed");
    }

    return result.safeVersion;
}

void ScriptTask::updateScriptStatus(const std::string& name,
                                    const ScriptStatus& status) {
    std::unique_lock lock(statusMutex_);
    scriptStatuses_[name] = status;
}

// ... 实现其他私有辅助方法 ...
// 注意：其他辅助方法的实现可以参考之前两个类中的相应实现

void ScriptTask::registerScript(const std::string& name,
                                const std::string& content) {
    LOG_F(INFO, "Registering script: {}", name);

    // 预处理和验证脚本
    std::string validated_content = validateAndPreprocessScript(content);

    try {
        // 注册脚本到管理器
        scriptManager_->registerScript(name, validated_content);

        // 初始化状态
        ScriptStatus status{.progress = 0.0f,
                            .currentStage = "Registered",
                            .startTime = std::chrono::system_clock::now(),
                            .isRunning = false};
        updateScriptStatus(name, status);

        // 添加日志记录
        LOG_F(INFO, "Script registered successfully: {}", name);
        addHistoryEntry("Script registered: " + name);

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to register script {}: {}", name, e.what());
        throw;
    }
}

void ScriptTask::updateScript(const std::string& name,
                              const std::string& content) {
    std::string validated_content = validateAndPreprocessScript(content);
    scriptManager_->updateScript(name, validated_content);
}

void ScriptTask::deleteScript(const std::string& name) {
    scriptManager_->deleteScript(name);
    std::unique_lock lock(statusMutex_);
    scriptStatuses_.erase(name);
    priorities_.erase(name);
}

bool ScriptTask::validateScript(const std::string& content) {
    if (!scriptAnalyzer_) {
        return true;  // 如果没有配置分析器,默认认为安全
    }
    return scriptAnalyzer_->validateScript(content);
}

auto ScriptTask::analyzeScript(const std::string& content)
    -> ScriptAnalysisResult {
    if (!scriptAnalyzer_) {
        return ScriptAnalysisResult{true, {}, 0, content};
    }

    // 配置分析选项
    AnalyzerOptions options;
    options.deep_analysis = true;
    options.timeout_seconds = 30;

    // 执行分析
    auto result = scriptAnalyzer_->analyzeWithOptions(content, options);

    return ScriptAnalysisResult{
        !result.timeout_occurred && result.dangers.empty(), result.dangers,
        result.complexity, scriptAnalyzer_->getSafeVersion(content)};
}

void ScriptTask::setScriptTimeout(std::chrono::seconds timeout) {
    // scriptManager_->setTimeout(timeout.count());
}

void ScriptTask::setScriptRetryCount(int count) {
    // 保存到配置中
    config_["retryCount"] = count;
}

void ScriptTask::setScriptEnvironment(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& env) {
    scriptManager_->setScriptEnvironmentVars(name, env);
}

void ScriptTask::setRetryStrategy(const std::string& name,
                                  RetryStrategy strategy) {
    // 转换为ScriptManager内部策略枚举
    auto managerStrategy = static_cast<lithium::RetryStrategy>(strategy);
    scriptManager_->setRetryStrategy(name, managerStrategy);
}

auto ScriptTask::getScriptProgress(const std::string& name) -> float {
    return scriptManager_->getScriptProgress(name);
}

void ScriptTask::abortScript(const std::string& name) {
    scriptManager_->abortScript(name);
    updateScriptStatus(name, {.isRunning = false});
}

auto ScriptTask::getScriptLogs(const std::string& name)
    -> std::vector<std::string> {
    return scriptManager_->getScriptLogs(name);
}

void ScriptTask::addPreExecutionHook(
    const std::string& name, std::function<void(const std::string&)> hook) {
    scriptManager_->addPreExecutionHook(name, hook);
}

void ScriptTask::addPostExecutionHook(
    const std::string& name,
    std::function<void(const std::string&, int)> hook) {
    scriptManager_->addPostExecutionHook(name, hook);
}

void ScriptTask::setResourceLimit(const std::string& name, size_t memoryLimit,
                                  int cpuLimit) {
    // 保存资源限制到配置中
    std::unique_lock lock(statusMutex_);
    resourceLimits_[name] = {memoryLimit, cpuLimit};
}

auto ScriptTask::getActiveScripts() const -> std::vector<std::string> {
    std::shared_lock lock(statusMutex_);
    std::vector<std::string> active;
    for (const auto& [name, status] : scriptStatuses_) {
        if (status.isRunning) {
            active.push_back(name);
        }
    }
    return active;
}

void ScriptTask::resumeScript(const std::string& name) {
    // 检查脚本是否存在挂起状态
    {
        std::shared_lock lock(statusMutex_);
        auto it = scriptStatuses_.find(name);
        if (it == scriptStatuses_.end() || it->second.isRunning) {
            return;
        }
    }

    // 重新执行脚本
    executeScript(name, {});
}

auto ScriptTask::getDependencies(const std::string& name) const
    -> std::vector<std::string> {
    // 分析脚本获取依赖
    std::vector<std::string> deps;
    if (scriptAnalyzer_) {
        auto result = scriptAnalyzer_->analyzeWithOptions(name, {});
        // 解析分析结果中的依赖信息
        // ...
    }
    return deps;
}

auto ScriptTask::getResourceUsage(const std::string& name) const -> float {
    // 获取脚本资源使用情况
    std::shared_lock lock(statusMutex_);
    if (auto it = scriptStatuses_.find(name); it != scriptStatuses_.end()) {
        // 计算CPU和内存使用率的加权平均
        return 0.5f;  // 示例值
    }
    return 0.0f;
}

auto ScriptTask::getExecutionTime(const std::string& name) const
    -> std::chrono::milliseconds {
    std::shared_lock lock(statusMutex_);
    if (auto it = scriptStatuses_.find(name); it != scriptStatuses_.end()) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.startTime);
    }
    return std::chrono::milliseconds(0);
}

void ScriptTask::checkResourceLimits(const std::string& name) {
    std::shared_lock lock(statusMutex_);
    auto it = resourceLimits_.find(name);
    if (it == resourceLimits_.end()) {
        return;
    }

    // 检查是否超过资源限制
    if (getResourceUsage(name) > 0.9f) {  // 90%资源使用率作为警戒线
        LOG_F(WARNING, "Script {} is approaching resource limits", name);
    }
}

void ScriptTask::cleanupScript(const std::string& name) {
    std::unique_lock lock(statusMutex_);
    scriptStatuses_.erase(name);
    priorities_.erase(name);
    resourceLimits_.erase(name);
}

void ScriptTask::validateParameters(const json& params) {
    // 验证必需参数
    if (!params.contains("scriptName")) {
        THROW_INVALID_FORMAT("Missing required parameter: scriptName");
    }

    // 验证可选参数类型
    if (params.contains("timeout")) {
        if (!params["timeout"].is_number()) {
            THROW_INVALID_FORMAT("Invalid timeout parameter type");
        }
    }

    if (params.contains("args")) {
        if (!params["args"].is_object()) {
            THROW_INVALID_FORMAT("Invalid args parameter type");
        }
    }
}

void ScriptTask::handleScriptError(const std::string& scriptName,
                                   const std::string& error) {
    LOG_F(ERROR, "Script execution error: {} - {}", scriptName, error);
    updateScriptStatus(scriptName, {.isRunning = false, .exitCode = -1});
    cleanupScript(scriptName);
}

void ScriptTask::monitorExecution(const std::string& scriptName) {
    LOG_F(INFO, "Starting execution monitor for script: {}", scriptName);

    const auto startTime = std::chrono::steady_clock::now();
    bool resourceWarningIssued = false;

    while (true) {
        {
            std::shared_lock lock(statusMutex_);
            auto status = scriptStatuses_.find(scriptName);
            if (status == scriptStatuses_.end() || !status->second.isRunning) {
                break;
            }

            // 检查超时
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(
                                   currentTime - startTime)
                                   .count();

            if (elapsedTime > config_["timeout"].get<int>()) {
                LOG_F(WARNING, "Script {} exceeded timeout limit", scriptName);
                abortScript(scriptName);
                break;
            }

            // 检查资源限制
            checkResourceLimits(scriptName);

            // 更新进度
            float progress = scriptManager_->getScriptProgress(scriptName);
            updateScriptStatus(
                scriptName,
                {.progress = progress,
                 .currentStage = "Running",
                 .startTime = status->second.startTime,
                 .outputs = scriptManager_->getScriptLogs(scriptName),
                 .isRunning = true});

            // 资源使用预警
            float resourceUsage = getResourceUsage(scriptName);
            if (resourceUsage > 0.8f && !resourceWarningIssued) {
                LOG_F(WARNING, "Script {} high resource usage: {:.1f}%",
                      scriptName, resourceUsage * 100);
                resourceWarningIssued = true;
            }
        }

        // 降低监控频率以减少系统负担
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOG_F(INFO, "Execution monitor ended for script: {}", scriptName);
}

void ScriptTask::monitorPerformance(const std::string& scriptName) {
    LOG_F(INFO, "Starting performance monitor for script: {}", scriptName);

    std::vector<float> usageHistory;
    const size_t HISTORY_SIZE = 10;

    while (true) {
        {
            std::shared_lock lock(statusMutex_);
            if (!scriptStatuses_[scriptName].isRunning) {
                break;
            }

            // 获取当前资源使用情况
            float usage = getResourceUsage(scriptName);
            usageHistory.push_back(usage);

            // 保持历史记录大小
            if (usageHistory.size() > HISTORY_SIZE) {
                usageHistory.erase(usageHistory.begin());
            }

            // 计算平均使用率
            float avgUsage = std::accumulate(usageHistory.begin(),
                                             usageHistory.end(), 0.0f) /
                             usageHistory.size();

            // 记录性能数据
            LOG_F(INFO, "Script {} performance metrics:", scriptName);
            LOG_F(INFO, "  - Current usage: {:.1f}%", usage * 100);
            LOG_F(INFO, "  - Average usage: {:.1f}%", avgUsage * 100);
            LOG_F(INFO, "  - Execution time: {}ms",
                  getExecutionTime(scriptName).count());
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    LOG_F(INFO, "Performance monitor ended for script: {}", scriptName);
}

void ScriptTask::updateTaskStatus(const std::string& scriptName, bool success) {
    if (success) {
        status_ = TaskStatus::Completed;  // 使用成员变量而不是函数
    } else {
        status_ = TaskStatus::Failed;  // 使用成员变量而不是函数
    }
    updateScriptStatus(scriptName,
                       {.isRunning = false, .exitCode = success ? 0 : -1});
}

void ScriptTask::validateResults(
    const std::string& scriptName,
    const std::optional<std::pair<std::string, int>>& result) {
    if (!result) {
        LOG_F(ERROR, "Script {} returned no result", scriptName);
        THROW_INVALID_FORMAT("Script execution returned no result: " +
                             scriptName);
    }

    const auto& [output, exitCode] = *result;

    // 检查执行结果
    if (exitCode != 0) {
        LOG_F(ERROR, "Script {} failed with exit code {}: {}", scriptName,
              exitCode, output);

        // 更新状态
        updateTaskStatus(scriptName, false);

        // 记录失败信息
        addHistoryEntry("Script execution failed: " + scriptName +
                        " (Exit code: " + std::to_string(exitCode) + ")");

        throw std::runtime_error("Script execution failed with code " +
                                 std::to_string(exitCode));
    }

    // 执行成功
    LOG_F(INFO, "Script {} completed successfully", scriptName);

    // 更新状态
    updateTaskStatus(scriptName, true);

    // 记录成功信息
    addHistoryEntry("Script executed successfully: " + scriptName);
}

// Register ScriptTask with factory
#include "factory.hpp"

namespace {
    static auto script_task_registrar = TaskRegistrar<ScriptTask>(
        "script_task",
        TaskInfo{
            .name = "script_task",
            .description = "Execute custom scripts with error handling and monitoring",
            .category = "automation",
            .requiredParameters = {"scriptName"},
            .parameterSchema = json{
                {"scriptName", {{"type", "string"}, {"description", "Name or path of script to execute"}}},
                {"scriptContent", {{"type", "string"}, {"description", "Inline script content"}}},
                {"allowUnsafe", {{"type", "boolean"}, {"description", "Allow unsafe script execution"}, {"default", false}}},
                {"timeout", {{"type", "number"}, {"description", "Execution timeout in seconds"}, {"default", 30}}},
                {"args", {{"type", "object"}, {"description", "Script arguments"}, {"default", json::object()}}},
                {"retryCount", {{"type", "number"}, {"description", "Number of retry attempts"}, {"default", 0}}}
            },
            .version = "1.0.0",
            .dependencies = {},
            .isEnabled = true
        }
    );
}

}  // namespace lithium::sequencer::task
