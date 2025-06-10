#include "script_task.hpp"
#include "atom/log/loguru.hpp"
#include "factory.hpp"
#include "spdlog/spdlog.h"

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
    // 基本参数配置 - 使用新的Task API
    addParamDefinition("scriptName", "string", true, nullptr, "脚本名称");
    addParamDefinition("scriptContent", "string", false, nullptr, "脚本内容");
    addParamDefinition("allowUnsafe", "boolean", false, false,
                       "允许不安全脚本");
    addParamDefinition("timeout", "number", false, 30, "超时时间(秒)");
    addParamDefinition("args", "object", false, json::object(), "脚本参数");
    addParamDefinition("retryCount", "number", false, 0, "重试次数");

    // 任务属性设置 - 使用Task基类方法
    setTimeout(std::chrono::seconds(300));
    Task::setPriority(8);
    setLogLevel(3);

    // 设置异常处理
    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Script task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Exception occurred: " + std::string(e.what()));
    });
}

void ScriptTask::execute(const json& params) {
    spdlog::info("Starting script task with params: {}", params.dump());
    addHistoryEntry("Starting script execution");

    try {
        // 使用新的Task验证API
        if (!validateParams(params)) {
            setErrorType(TaskErrorType::InvalidParameter);
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            spdlog::error("Parameter validation failed: {}", errorMsg);
            addHistoryEntry("Parameter validation failed");
            throw std::invalid_argument(errorMsg);
        }

        // 提取基本参数
        std::string scriptName = params["scriptName"].get<std::string>();
        bool allowUnsafe = params.value("allowUnsafe", false);
        int timeout = params.value("timeout", 30);
        auto args = params.value("args", json::object())
                        .get<std::unordered_map<std::string, std::string>>();

        addHistoryEntry("Processing script: " + scriptName);

        // 处理脚本内容
        if (params.contains("scriptContent")) {
            std::string scriptContent =
                params["scriptContent"].get<std::string>();
            if (!scriptContent.empty()) {
                // 分析和验证脚本
                if (scriptAnalyzer_) {
                    auto analysisResult = analyzeScript(scriptContent);
                    if (!analysisResult.isValid && !allowUnsafe) {
                        setErrorType(TaskErrorType::InvalidParameter);
                        addHistoryEntry("Script validation failed: " +
                                        scriptName);
                        throw std::invalid_argument(
                            "Script validation failed: " + scriptName);
                    }
                    scriptContent = allowUnsafe ? scriptContent
                                                : analysisResult.safeVersion;
                }
                registerScript(scriptName, scriptContent);
            }
        }

        // 配置执行环境
        scriptManager_->setExecutionEnvironment(scriptName, "production");

        // 执行脚本
        executeScript(scriptName, args);

        spdlog::info("Script task completed successfully: {}", scriptName);
        addHistoryEntry("Script executed successfully: " + scriptName);

    } catch (const std::exception& e) {
        handleScriptError(params.value("scriptName", "unknown"), e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Script execution failed: " + std::string(e.what()));
        throw;
    }
}

void ScriptTask::executeScript(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args) {
    spdlog::info("Executing script: {}", scriptName);
    addHistoryEntry("Starting script execution: " + scriptName);

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
        setErrorType(TaskErrorType::SystemError);
        throw;
    }
}

void ScriptTask::setScriptPriority(const std::string& name,
                                   ScriptPriority priority) {
    std::unique_lock lock(statusMutex_);
    priorities_[name] = priority;
    addHistoryEntry("Set priority for script " + name + " to level " +
                    std::to_string(priority.level));
}

void ScriptTask::setConcurrencyLimit(int limit) {
    if (limit > 0) {
        concurrencyLimit_ = limit;
        addHistoryEntry("Set concurrency limit to " + std::to_string(limit));
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
    addHistoryEntry("Paused script: " + name);
}

std::string ScriptTask::validateAndPreprocessScript(
    const std::string& content) {
    // 检查脚本有效性
    if (!validateScript(content)) {
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument("Invalid script content");
    }

    // 分析脚本并获取安全版本
    auto result = analyzeScript(content);
    if (!result.isValid) {
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument("Script analysis failed");
    }

    return result.safeVersion;
}

void ScriptTask::updateScriptStatus(const std::string& name,
                                    const ScriptStatus& status) {
    std::unique_lock lock(statusMutex_);
    scriptStatuses_[name] = status;
}

void ScriptTask::registerScript(const std::string& name,
                                const std::string& content) {
    spdlog::info("Registering script: {}", name);
    addHistoryEntry("Registering script: " + name);

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

        spdlog::info("Script registered successfully: {}", name);
        addHistoryEntry("Script registered successfully: " + name);

    } catch (const std::exception& e) {
        spdlog::error("Failed to register script {}: {}", name, e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Failed to register script " + name + ": " + e.what());
        throw;
    }
}

void ScriptTask::updateScript(const std::string& name,
                              const std::string& content) {
    std::string validated_content = validateAndPreprocessScript(content);
    scriptManager_->updateScript(name, validated_content);
    addHistoryEntry("Updated script: " + name);
}

void ScriptTask::deleteScript(const std::string& name) {
    scriptManager_->deleteScript(name);
    std::unique_lock lock(statusMutex_);
    scriptStatuses_.erase(name);
    priorities_.erase(name);
    addHistoryEntry("Deleted script: " + name);
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
    setTimeout(timeout);  // 使用Task基类方法
    addHistoryEntry("Set script timeout to " + std::to_string(timeout.count()) +
                    " seconds");
}

void ScriptTask::setScriptRetryCount(int count) {
    config_["retryCount"] = count;
    addHistoryEntry("Set retry count to " + std::to_string(count));
}

void ScriptTask::setScriptEnvironment(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& env) {
    scriptManager_->setScriptEnvironmentVars(name, env);
    addHistoryEntry("Set environment variables for script: " + name);
}

void ScriptTask::setRetryStrategy(const std::string& name,
                                  RetryStrategy strategy) {
    auto managerStrategy = static_cast<lithium::RetryStrategy>(strategy);
    scriptManager_->setRetryStrategy(name, managerStrategy);
    addHistoryEntry("Set retry strategy for script: " + name);
}

auto ScriptTask::getScriptProgress(const std::string& name) -> float {
    return scriptManager_->getScriptProgress(name);
}

void ScriptTask::abortScript(const std::string& name) {
    scriptManager_->abortScript(name);
    updateScriptStatus(name, {.isRunning = false});
    addHistoryEntry("Aborted script: " + name);
}

auto ScriptTask::getScriptLogs(const std::string& name)
    -> std::vector<std::string> {
    return scriptManager_->getScriptLogs(name);
}

void ScriptTask::addPreExecutionHook(
    const std::string& name, std::function<void(const std::string&)> hook) {
    scriptManager_->addPreExecutionHook(name, hook);
    addHistoryEntry("Added pre-execution hook for script: " + name);
}

void ScriptTask::addPostExecutionHook(
    const std::string& name,
    std::function<void(const std::string&, int)> hook) {
    scriptManager_->addPostExecutionHook(name, hook);
    addHistoryEntry("Added post-execution hook for script: " + name);
}

void ScriptTask::setResourceLimit(const std::string& name, size_t memoryLimit,
                                  int cpuLimit) {
    std::unique_lock lock(statusMutex_);
    resourceLimits_[name] = {memoryLimit, cpuLimit};
    addHistoryEntry("Set resource limits for script " + name);
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
    {
        std::shared_lock lock(statusMutex_);
        auto it = scriptStatuses_.find(name);
        if (it == scriptStatuses_.end() || it->second.isRunning) {
            return;
        }
    }

    executeScript(name, {});
    addHistoryEntry("Resumed script: " + name);
}

auto ScriptTask::getDependencies(const std::string& name) const
    -> std::vector<std::string> {
    std::vector<std::string> deps;
    if (scriptAnalyzer_) {
        auto result = scriptAnalyzer_->analyzeWithOptions(name, {});
    }
    return deps;
}

auto ScriptTask::getResourceUsage(const std::string& name) const -> float {
    std::shared_lock lock(statusMutex_);
    if (auto it = scriptStatuses_.find(name); it != scriptStatuses_.end()) {
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

    if (getResourceUsage(name) > 0.9f) {
        spdlog::warn("Script {} is approaching resource limits", name);
        addHistoryEntry("Script " + name + " approaching resource limits");
    }
}

void ScriptTask::cleanupScript(const std::string& name) {
    std::unique_lock lock(statusMutex_);
    scriptStatuses_.erase(name);
    priorities_.erase(name);
    resourceLimits_.erase(name);
    addHistoryEntry("Cleaned up script resources: " + name);
}

void ScriptTask::validateParameters(const json& params) {
    // 使用新的Task验证系统
    if (!validateParams(params)) {
        auto errors = getParamErrors();
        std::string errorMsg = "Parameter validation failed: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument(errorMsg);
    }
}

void ScriptTask::handleScriptError(const std::string& scriptName,
                                   const std::string& error) {
    spdlog::error("Script execution error: {} - {}", scriptName, error);
    updateScriptStatus(scriptName, {.isRunning = false, .exitCode = -1});
    cleanupScript(scriptName);
    setErrorType(TaskErrorType::SystemError);
    addHistoryEntry("Script error (" + scriptName + "): " + error);
}

void ScriptTask::monitorExecution(const std::string& scriptName) {
    spdlog::info("Starting execution monitor for script: {}", scriptName);

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

            if (elapsedTime > config_.value("timeout", 30)) {
                spdlog::warn("Script {} exceeded timeout limit", scriptName);
                abortScript(scriptName);
                setErrorType(TaskErrorType::Timeout);
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
                spdlog::warn("Script {} high resource usage: {:.1f}%",
                      scriptName, resourceUsage * 100);
                resourceWarningIssued = true;
                addHistoryEntry("High resource usage warning for script: " +
                                scriptName);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    spdlog::info("Execution monitor ended for script: {}", scriptName);
}

void ScriptTask::monitorPerformance(const std::string& scriptName) {
    spdlog::info("Starting performance monitor for script: {}", scriptName);

    std::vector<float> usageHistory;
    const size_t HISTORY_SIZE = 10;

    while (true) {
        {
            std::shared_lock lock(statusMutex_);
            if (!scriptStatuses_[scriptName].isRunning) {
                break;
            }

            float usage = getResourceUsage(scriptName);
            usageHistory.push_back(usage);

            if (usageHistory.size() > HISTORY_SIZE) {
                usageHistory.erase(usageHistory.begin());
            }

            float avgUsage = std::accumulate(usageHistory.begin(),
                                             usageHistory.end(), 0.0f) /
                             usageHistory.size();

            spdlog::info("Script {} performance metrics:", scriptName);
            spdlog::info("  - Current usage: {:.1f}%", usage * 100);
            spdlog::info("  - Average usage: {:.1f}%", avgUsage * 100);
            spdlog::info("  - Execution time: {}ms",
                  getExecutionTime(scriptName).count());
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    spdlog::info("Performance monitor ended for script: {}", scriptName);
}

void ScriptTask::updateTaskStatus(const std::string& scriptName, bool success) {
    // Task基类会自动管理状态，这里只需要更新脚本特定状态
    updateScriptStatus(scriptName,
                       {.isRunning = false, .exitCode = success ? 0 : -1});
    addHistoryEntry("Script " + scriptName +
                    (success ? " completed successfully" : " failed"));
}

void ScriptTask::validateResults(
    const std::string& scriptName,
    const std::optional<std::pair<std::string, int>>& result) {
    if (!result) {
        spdlog::error("Script {} returned no result", scriptName);
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Script " + scriptName + " returned no result");
        throw std::runtime_error("Script execution returned no result: " +
                                 scriptName);
    }

    const auto& [output, exitCode] = *result;

    if (exitCode != 0) {
        spdlog::error("Script {} failed with exit code {}: {}", scriptName,
              exitCode, output);
        updateTaskStatus(scriptName, false);
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Script execution failed: " + scriptName +
                        " (Exit code: " + std::to_string(exitCode) + ")");
        throw std::runtime_error("Script execution failed with code " +
                                 std::to_string(exitCode));
    }

    spdlog::info("Script {} completed successfully", scriptName);
    updateTaskStatus(scriptName, true);
    addHistoryEntry("Script executed successfully: " + scriptName);
}

// Register ScriptTask with factory
namespace {
static auto script_task_registrar = TaskRegistrar<ScriptTask>(
    "script_task",
    TaskInfo{.name = "script_task",
             .description =
                 "Execute custom scripts with error handling and monitoring",
             .category = "automation",
             .requiredParameters = {"scriptName"},
             .parameterSchema =
                 json{{"scriptName",
                       {{"type", "string"},
                        {"description", "Name or path of script to execute"}}},
                      {"scriptContent",
                       {{"type", "string"},
                        {"description", "Inline script content"}}},
                      {"allowUnsafe",
                       {{"type", "boolean"},
                        {"description", "Allow unsafe script execution"},
                        {"default", false}}},
                      {"timeout",
                       {{"type", "number"},
                        {"description", "Execution timeout in seconds"},
                        {"default", 30}}},
                      {"args",
                       {{"type", "object"},
                        {"description", "Script arguments"},
                        {"default", json::object()}}},
                      {"retryCount",
                       {{"type", "number"},
                        {"description", "Number of retry attempts"},
                        {"default", 0}}}},
             .version = "1.0.0",
             .dependencies = {},
             .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<ScriptTask> {
        return std::make_unique<ScriptTask>(
            name, config.value("scriptConfigPath", ""),
            config.value("analyzerConfigPath", ""));
    });
}  // namespace

}  // namespace lithium::sequencer::task