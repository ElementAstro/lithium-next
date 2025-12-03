/*
 * script_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "script_service.hpp"

#include "check.hpp"
#include "interpreter_pool.hpp"
#include "python_caller.hpp"

#include "isolated/runner.hpp"
#include "shell/script_manager.hpp"
#include "tools/tool_registry.hpp"
#include "venv/venv_manager.hpp"

#include "numpy/array_ops.hpp"
#include "numpy/astro_ops.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

#include <future>
#include <mutex>
#include <shared_mutex>

namespace lithium {

// ============================================================================
// ScriptService::Impl
// ============================================================================

class ScriptService::Impl {
public:
    explicit Impl(const ScriptServiceConfig& config) : config_(config) {}

    ScriptResult<void> initialize() {
        std::unique_lock lock(mutex_);

        if (initialized_) {
            return {};
        }

        LOG_INFO("Initializing ScriptService...");

        try {
            // Initialize PythonWrapper
            pythonWrapper_ = std::make_shared<PythonWrapper>();
            LOG_DEBUG("PythonWrapper initialized");

            // Initialize InterpreterPool
            InterpreterPoolConfig poolConfig;
            poolConfig.poolSize = config_.poolSize;
            poolConfig.maxQueuedTasks = config_.maxQueuedTasks;
            interpreterPool_ = std::make_shared<InterpreterPool>(poolConfig);

            auto poolResult = interpreterPool_->initialize();
            if (!poolResult) {
                LOG_ERROR("Failed to initialize InterpreterPool: {}",
                          interpreterPoolErrorToString(poolResult.error()));
                return std::unexpected(ScriptServiceError::InternalError);
            }
            LOG_DEBUG("InterpreterPool initialized with {} interpreters",
                      config_.poolSize);

            // Initialize IsolatedRunner
            isolatedRunner_ = std::make_shared<isolated::PythonRunner>();
            LOG_DEBUG("IsolatedRunner initialized");

            // Initialize ScriptManager
            scriptManager_ = std::make_shared<shell::ScriptManager>();
            LOG_DEBUG("ScriptManager initialized");

            // Initialize ToolRegistry
            toolRegistry_ = std::make_shared<tools::PythonToolRegistry>();
            if (config_.autoDiscoverTools &&
                !config_.toolsDirectory.empty()) {
                toolRegistry_->setSearchPath(config_.toolsDirectory);
                auto discoverResult = toolRegistry_->discoverTools();
                if (discoverResult) {
                    LOG_INFO("Discovered {} Python tools", *discoverResult);
                }
            }
            LOG_DEBUG("ToolRegistry initialized");

            // Initialize VenvManager
            venvManager_ = std::make_shared<venv::VenvManager>();
            if (config_.autoActivateVenv && !config_.defaultVenvPath.empty()) {
                auto activateResult = venvManager_->activateVenv(
                    config_.defaultVenvPath.string());
                if (activateResult) {
                    LOG_INFO("Activated default venv: {}",
                             config_.defaultVenvPath.string());
                }
            }
            LOG_DEBUG("VenvManager initialized");

            // Initialize ScriptAnalyzer
            if (config_.enableSecurityAnalysis) {
                scriptAnalyzer_ = std::make_shared<ScriptAnalyzer>(
                    config_.analysisConfigPath.string());
                LOG_DEBUG("ScriptAnalyzer initialized");
            }

            initialized_ = true;
            LOG_INFO("ScriptService initialized successfully");
            return {};

        } catch (const std::exception& e) {
            LOG_ERROR("ScriptService initialization failed: {}", e.what());
            return std::unexpected(ScriptServiceError::InternalError);
        }
    }

    void shutdown(bool waitForPending) {
        std::unique_lock lock(mutex_);

        if (!initialized_) {
            return;
        }

        LOG_INFO("Shutting down ScriptService...");

        if (interpreterPool_) {
            interpreterPool_->shutdown(waitForPending);
        }

        if (isolatedRunner_ && isolatedRunner_->isRunning()) {
            isolatedRunner_->kill();
        }

        pythonWrapper_.reset();
        interpreterPool_.reset();
        isolatedRunner_.reset();
        scriptManager_.reset();
        toolRegistry_.reset();
        venvManager_.reset();
        scriptAnalyzer_.reset();

        initialized_ = false;
        LOG_INFO("ScriptService shutdown complete");
    }

    bool isInitialized() const noexcept {
        std::shared_lock lock(mutex_);
        return initialized_;
    }

    ScriptExecutionResult executePython(
        std::string_view code,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        ScriptExecutionResult result;
        auto startTime = std::chrono::steady_clock::now();

        if (!isInitialized()) {
            result.errorMessage = "Service not initialized";
            return result;
        }

        // Validate script if required
        if (config.validateBeforeExecution && scriptAnalyzer_) {
            if (!scriptAnalyzer_->validateScript(std::string(code))) {
                result.errorMessage = "Script failed security validation";
                return result;
            }
        }

        // Determine execution mode
        ExecutionMode mode = config.mode;
        if (mode == ExecutionMode::Auto) {
            mode = selectExecutionMode(code, args);
        }

        result.actualMode = mode;

        try {
            switch (mode) {
                case ExecutionMode::InProcess:
                    result = executeInProcess(code, args, config);
                    break;
                case ExecutionMode::Pooled:
                    result = executePooled(code, args, config);
                    break;
                case ExecutionMode::Isolated:
                    result = executeIsolated(code, args, config);
                    break;
                default:
                    result = executeInProcess(code, args, config);
                    break;
            }
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
        }

        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        updateStatistics(result);
        return result;
    }

    ScriptExecutionResult executePythonFile(
        const std::filesystem::path& path,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        if (!std::filesystem::exists(path)) {
            ScriptExecutionResult result;
            result.errorMessage = "File not found: " + path.string();
            return result;
        }

        // Read file content
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        return executePython(content, args, config);
    }

    ScriptExecutionResult executePythonFunction(
        std::string_view moduleName,
        std::string_view functionName,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        ScriptExecutionResult result;
        auto startTime = std::chrono::steady_clock::now();

        if (!isInitialized()) {
            result.errorMessage = "Service not initialized";
            return result;
        }

        ExecutionMode mode = config.mode;
        if (mode == ExecutionMode::Auto) {
            mode = ExecutionMode::Isolated;  // Default to isolated for functions
        }

        result.actualMode = mode;

        try {
            if (mode == ExecutionMode::Isolated) {
                auto isolatedResult = isolatedRunner_->executeFunction(
                    moduleName, functionName, args);

                result.success = isolatedResult.success;
                result.result = isolatedResult.result;
                result.stdoutOutput = isolatedResult.stdoutOutput;
                result.stderrOutput = isolatedResult.stderrOutput;
                if (isolatedResult.errorMessage) {
                    result.errorMessage = *isolatedResult.errorMessage;
                }
            } else {
                // Use PythonWrapper for in-process execution
                pythonWrapper_->load_script(std::string(moduleName),
                                           std::string(moduleName));
                auto pyResult = pythonWrapper_->invoke_export(
                    std::string(moduleName), std::string(functionName),
                    py::cast(args));

                result.success = true;
                result.result = py::cast<nlohmann::json>(pyResult);
            }
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
        }

        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        updateStatistics(result);
        return result;
    }

    std::future<ScriptExecutionResult> executePythonAsync(
        std::string_view code,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        std::string codeCopy(code);
        return std::async(std::launch::async, [this, codeCopy, args, config] {
            return executePython(codeCopy, args, config);
        });
    }

    ScriptResult<std::pair<std::string, int>> executeShellScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args,
        bool safe) {

        if (!scriptManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = scriptManager_->runScript(scriptName, args, safe);
        if (!result) {
            return std::unexpected(ScriptServiceError::ExecutionFailed);
        }

        return *result;
    }

    std::vector<std::string> listShellScripts() const {
        if (!scriptManager_) {
            return {};
        }

        std::vector<std::string> scripts;
        auto allScripts = scriptManager_->getAllScripts();
        scripts.reserve(allScripts.size());
        for (const auto& [name, _] : allScripts) {
            scripts.push_back(name);
        }
        return scripts;
    }

    ScriptResult<nlohmann::json> invokeTool(
        const std::string& toolName,
        const std::string& functionName,
        const nlohmann::json& args) {

        if (!toolRegistry_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = toolRegistry_->invoke(toolName, functionName, args);
        if (!result) {
            return std::unexpected(ScriptServiceError::ExecutionFailed);
        }

        return result->result;
    }

    std::vector<std::string> listTools() const {
        if (!toolRegistry_) {
            return {};
        }
        return toolRegistry_->getToolNames();
    }

    ScriptResult<size_t> discoverTools() {
        if (!toolRegistry_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = toolRegistry_->discoverTools();
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        return *result;
    }

    ScriptResult<nlohmann::json> createVirtualEnv(
        const std::filesystem::path& path,
        const std::string& pythonVersion) {

        if (!venvManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = venvManager_->createVenv(path.string(), pythonVersion);
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        nlohmann::json info;
        info["path"] = result->path.string();
        info["pythonVersion"] = result->pythonVersion;
        return info;
    }

    ScriptResult<void> activateVirtualEnv(const std::filesystem::path& path) {
        if (!venvManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = venvManager_->activateVenv(path.string());
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        return {};
    }

    ScriptResult<void> deactivateVirtualEnv() {
        if (!venvManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = venvManager_->deactivateVenv();
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        return {};
    }

    ScriptResult<void> installPackage(const std::string& package, bool upgrade) {
        if (!venvManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = venvManager_->installPackage(package, upgrade);
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        return {};
    }

    ScriptResult<std::vector<nlohmann::json>> listPackages() const {
        if (!venvManager_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        auto result = venvManager_->listInstalledPackages();
        if (!result) {
            return std::unexpected(ScriptServiceError::InternalError);
        }

        std::vector<nlohmann::json> packages;
        for (const auto& pkg : *result) {
            nlohmann::json j;
            j["name"] = pkg.name;
            j["version"] = pkg.version;
            j["location"] = pkg.location.string();
            packages.push_back(std::move(j));
        }

        return packages;
    }

    nlohmann::json analyzeScript(const std::string& script) {
        nlohmann::json result;
        result["valid"] = true;
        result["dangers"] = nlohmann::json::array();

        if (!scriptAnalyzer_) {
            return result;
        }

        AnalyzerOptions options;
        options.async_mode = false;
        options.deep_analysis = true;

        auto analysisResult = scriptAnalyzer_->analyzeWithOptions(script, options);

        result["valid"] = analysisResult.dangers.empty();
        result["complexity"] = analysisResult.complexity;
        result["executionTime"] = analysisResult.execution_time;

        for (const auto& danger : analysisResult.dangers) {
            nlohmann::json d;
            d["category"] = danger.category;
            d["command"] = danger.command;
            d["reason"] = danger.reason;
            d["line"] = danger.line;
            if (danger.context) {
                d["context"] = *danger.context;
            }
            result["dangers"].push_back(std::move(d));
        }

        return result;
    }

    bool validateScript(const std::string& script) {
        if (!scriptAnalyzer_) {
            return true;  // No analyzer, assume safe
        }
        return scriptAnalyzer_->validateScript(script);
    }

    std::string getSafeScript(const std::string& script) {
        if (!scriptAnalyzer_) {
            return script;
        }
        return scriptAnalyzer_->getSafeVersion(script);
    }

    ScriptResult<nlohmann::json> executeNumpyOp(
        const std::string& operation,
        const nlohmann::json& arrays,
        const nlohmann::json& params) {

        if (!pythonWrapper_) {
            return std::unexpected(ScriptServiceError::NotInitialized);
        }

        try {
            // Build Python code for NumPy operation
            std::string code = R"(
import numpy as np
import json

def execute_numpy_op(op, arrays, params):
    result = {}
    # Convert JSON arrays to numpy arrays
    np_arrays = [np.array(a) for a in arrays]

    if op == "stack":
        axis = params.get("axis", 0)
        result["data"] = np.stack(np_arrays, axis=axis).tolist()
    elif op == "concatenate":
        axis = params.get("axis", 0)
        result["data"] = np.concatenate(np_arrays, axis=axis).tolist()
    elif op == "mean":
        result["data"] = [np.mean(a) for a in np_arrays]
    elif op == "std":
        result["data"] = [np.std(a) for a in np_arrays]
    elif op == "sum":
        result["data"] = [np.sum(a) for a in np_arrays]
    elif op == "reshape":
        shape = tuple(params.get("shape", [-1]))
        result["data"] = [a.reshape(shape).tolist() for a in np_arrays]
    else:
        raise ValueError(f"Unknown operation: {op}")

    return result

result = execute_numpy_op(op, arrays, params)
)";

            // Execute using isolated runner for safety
            nlohmann::json args;
            args["op"] = operation;
            args["arrays"] = arrays;
            args["params"] = params;

            auto execResult = isolatedRunner_->execute(code, args);
            if (!execResult.success) {
                return std::unexpected(ScriptServiceError::ExecutionFailed);
            }

            return execResult.result;

        } catch (const std::exception& e) {
            LOG_ERROR("NumPy operation failed: {}", e.what());
            return std::unexpected(ScriptServiceError::ExecutionFailed);
        }
    }

    nlohmann::json getStatistics() const {
        std::shared_lock lock(mutex_);

        nlohmann::json stats;
        stats["totalExecutions"] = stats_.totalExecutions;
        stats["successfulExecutions"] = stats_.successfulExecutions;
        stats["failedExecutions"] = stats_.failedExecutions;
        stats["totalExecutionTimeMs"] = stats_.totalExecutionTimeMs;

        if (stats_.totalExecutions > 0) {
            stats["averageExecutionTimeMs"] =
                stats_.totalExecutionTimeMs / stats_.totalExecutions;
        }

        // Add subsystem stats
        if (interpreterPool_) {
            auto poolStats = interpreterPool_->getStatistics();
            stats["interpreterPool"]["availableInterpreters"] =
                poolStats.availableInterpreters;
            stats["interpreterPool"]["busyInterpreters"] =
                poolStats.busyInterpreters;
            stats["interpreterPool"]["queuedTasks"] =
                poolStats.currentQueuedTasks;
        }

        return stats;
    }

    void resetStatistics() {
        std::unique_lock lock(mutex_);
        stats_ = Statistics{};

        if (interpreterPool_) {
            interpreterPool_->resetStatistics();
        }
    }

    // Getters for subsystems
    std::shared_ptr<PythonWrapper> getPythonWrapper() const { return pythonWrapper_; }
    std::shared_ptr<InterpreterPool> getInterpreterPool() const { return interpreterPool_; }
    std::shared_ptr<isolated::PythonRunner> getIsolatedRunner() const { return isolatedRunner_; }
    std::shared_ptr<shell::ScriptManager> getScriptManager() const { return scriptManager_; }
    std::shared_ptr<tools::PythonToolRegistry> getToolRegistry() const { return toolRegistry_; }
    std::shared_ptr<venv::VenvManager> getVenvManager() const { return venvManager_; }
    std::shared_ptr<ScriptAnalyzer> getScriptAnalyzer() const { return scriptAnalyzer_; }

    void setProgressCallback(ScriptProgressCallback callback) {
        progressCallback_ = std::move(callback);
    }

    void setLogCallback(ScriptLogCallback callback) {
        logCallback_ = std::move(callback);
    }

private:
    ExecutionMode selectExecutionMode(std::string_view code,
                                      const nlohmann::json& /*args*/) {
        // Simple heuristics for mode selection
        std::string codeStr(code);

        // Use isolated for potentially dangerous operations
        if (codeStr.find("import os") != std::string::npos ||
            codeStr.find("import subprocess") != std::string::npos ||
            codeStr.find("import socket") != std::string::npos ||
            codeStr.find("open(") != std::string::npos ||
            codeStr.find("exec(") != std::string::npos ||
            codeStr.find("eval(") != std::string::npos) {
            return ExecutionMode::Isolated;
        }

        // Use pooled for medium complexity
        if (codeStr.length() > 1000 ||
            codeStr.find("import numpy") != std::string::npos ||
            codeStr.find("import pandas") != std::string::npos) {
            return ExecutionMode::Pooled;
        }

        // Simple scripts can run in-process
        return ExecutionMode::InProcess;
    }

    ScriptExecutionResult executeInProcess(
        std::string_view code,
        const nlohmann::json& args,
        const ScriptExecutionConfig& /*config*/) {

        ScriptExecutionResult result;

        try {
            // Inject args into Python namespace
            pythonWrapper_->sync_variable_to_python("args", py::cast(args));

            // Execute the code
            pythonWrapper_->inject_code(std::string(code));

            // Get result
            auto pyResult = pythonWrapper_->sync_variable_from_python("result");
            if (!pyResult.is_none()) {
                result.result = py::cast<nlohmann::json>(pyResult);
            }

            result.success = true;

        } catch (const py::error_already_set& e) {
            result.errorMessage = e.what();
            PythonWrapper::handle_exception(e);
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
        }

        return result;
    }

    ScriptExecutionResult executePooled(
        std::string_view code,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        ScriptExecutionResult result;

        auto future = interpreterPool_->executeScript(
            code,
            py::dict(),  // globals
            py::cast(args),  // locals with args
            TaskPriority::Normal);

        auto taskResult = future.get();

        result.success = taskResult.success;
        if (taskResult.success && !taskResult.result.is_none()) {
            try {
                result.result = py::cast<nlohmann::json>(taskResult.result);
            } catch (...) {
                // Result cannot be converted to JSON
            }
        }
        result.errorMessage = taskResult.error;
        result.executionTime = taskResult.executionTime;

        return result;
    }

    ScriptExecutionResult executeIsolated(
        std::string_view code,
        const nlohmann::json& args,
        const ScriptExecutionConfig& config) {

        ScriptExecutionResult result;

        // Configure isolation
        isolated::IsolationConfig isoConfig;
        isoConfig.timeout = config.timeout;
        isoConfig.maxMemoryMB = config.maxMemoryMB;
        isoConfig.captureOutput = config.captureOutput;
        isoConfig.allowedImports = config.allowedImports;
        isoConfig.blockedImports = config.blockedImports;

        if (!config.workingDirectory.empty()) {
            isoConfig.workingDirectory = config.workingDirectory;
        }

        isolatedRunner_->setConfig(isoConfig);

        auto execResult = isolatedRunner_->execute(code, args);

        result.success = execResult.success;
        result.result = execResult.result;
        result.stdoutOutput = execResult.stdoutOutput;
        result.stderrOutput = execResult.stderrOutput;
        if (execResult.errorMessage) {
            result.errorMessage = *execResult.errorMessage;
        }
        result.memoryUsed = execResult.peakMemoryUsage;

        return result;
    }

    void updateStatistics(const ScriptExecutionResult& result) {
        std::unique_lock lock(mutex_);
        stats_.totalExecutions++;
        if (result.success) {
            stats_.successfulExecutions++;
        } else {
            stats_.failedExecutions++;
        }
        stats_.totalExecutionTimeMs += result.executionTime.count();
    }

    ScriptServiceConfig config_;
    bool initialized_{false};
    mutable std::shared_mutex mutex_;

    // Subsystems
    std::shared_ptr<PythonWrapper> pythonWrapper_;
    std::shared_ptr<InterpreterPool> interpreterPool_;
    std::shared_ptr<isolated::PythonRunner> isolatedRunner_;
    std::shared_ptr<shell::ScriptManager> scriptManager_;
    std::shared_ptr<tools::PythonToolRegistry> toolRegistry_;
    std::shared_ptr<venv::VenvManager> venvManager_;
    std::shared_ptr<ScriptAnalyzer> scriptAnalyzer_;

    // Callbacks
    ScriptProgressCallback progressCallback_;
    ScriptLogCallback logCallback_;

    // Statistics
    struct Statistics {
        size_t totalExecutions{0};
        size_t successfulExecutions{0};
        size_t failedExecutions{0};
        size_t totalExecutionTimeMs{0};
    } stats_;
};

// ============================================================================
// ScriptService
// ============================================================================

ScriptService::ScriptService()
    : pImpl_(std::make_unique<Impl>(ScriptServiceConfig{})) {}

ScriptService::ScriptService(const ScriptServiceConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

ScriptService::~ScriptService() = default;

ScriptService::ScriptService(ScriptService&&) noexcept = default;
ScriptService& ScriptService::operator=(ScriptService&&) noexcept = default;

ScriptResult<void> ScriptService::initialize() {
    return pImpl_->initialize();
}

void ScriptService::shutdown(bool waitForPending) {
    pImpl_->shutdown(waitForPending);
}

bool ScriptService::isInitialized() const noexcept {
    return pImpl_->isInitialized();
}

ScriptExecutionResult ScriptService::executePython(
    std::string_view code,
    const nlohmann::json& args,
    const ScriptExecutionConfig& config) {
    return pImpl_->executePython(code, args, config);
}

ScriptExecutionResult ScriptService::executePythonFile(
    const std::filesystem::path& path,
    const nlohmann::json& args,
    const ScriptExecutionConfig& config) {
    return pImpl_->executePythonFile(path, args, config);
}

ScriptExecutionResult ScriptService::executePythonFunction(
    std::string_view moduleName,
    std::string_view functionName,
    const nlohmann::json& args,
    const ScriptExecutionConfig& config) {
    return pImpl_->executePythonFunction(moduleName, functionName, args, config);
}

std::future<ScriptExecutionResult> ScriptService::executePythonAsync(
    std::string_view code,
    const nlohmann::json& args,
    const ScriptExecutionConfig& config) {
    return pImpl_->executePythonAsync(code, args, config);
}

ScriptResult<std::pair<std::string, int>> ScriptService::executeShellScript(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args,
    bool safe) {
    return pImpl_->executeShellScript(scriptName, args, safe);
}

std::vector<std::string> ScriptService::listShellScripts() const {
    return pImpl_->listShellScripts();
}

ScriptResult<nlohmann::json> ScriptService::invokeTool(
    const std::string& toolName,
    const std::string& functionName,
    const nlohmann::json& args) {
    return pImpl_->invokeTool(toolName, functionName, args);
}

std::vector<std::string> ScriptService::listTools() const {
    return pImpl_->listTools();
}

ScriptResult<size_t> ScriptService::discoverTools() {
    return pImpl_->discoverTools();
}

ScriptResult<nlohmann::json> ScriptService::createVirtualEnv(
    const std::filesystem::path& path,
    const std::string& pythonVersion) {
    return pImpl_->createVirtualEnv(path, pythonVersion);
}

ScriptResult<void> ScriptService::activateVirtualEnv(
    const std::filesystem::path& path) {
    return pImpl_->activateVirtualEnv(path);
}

ScriptResult<void> ScriptService::deactivateVirtualEnv() {
    return pImpl_->deactivateVirtualEnv();
}

ScriptResult<void> ScriptService::installPackage(
    const std::string& package,
    bool upgrade) {
    return pImpl_->installPackage(package, upgrade);
}

ScriptResult<std::vector<nlohmann::json>> ScriptService::listPackages() const {
    return pImpl_->listPackages();
}

nlohmann::json ScriptService::analyzeScript(const std::string& script) {
    return pImpl_->analyzeScript(script);
}

bool ScriptService::validateScript(const std::string& script) {
    return pImpl_->validateScript(script);
}

std::string ScriptService::getSafeScript(const std::string& script) {
    return pImpl_->getSafeScript(script);
}

ScriptResult<nlohmann::json> ScriptService::executeNumpyOp(
    const std::string& operation,
    const nlohmann::json& arrays,
    const nlohmann::json& params) {
    return pImpl_->executeNumpyOp(operation, arrays, params);
}

std::shared_ptr<PythonWrapper> ScriptService::getPythonWrapper() const {
    return pImpl_->getPythonWrapper();
}

std::shared_ptr<InterpreterPool> ScriptService::getInterpreterPool() const {
    return pImpl_->getInterpreterPool();
}

std::shared_ptr<isolated::PythonRunner> ScriptService::getIsolatedRunner() const {
    return pImpl_->getIsolatedRunner();
}

std::shared_ptr<shell::ScriptManager> ScriptService::getScriptManager() const {
    return pImpl_->getScriptManager();
}

std::shared_ptr<tools::PythonToolRegistry> ScriptService::getToolRegistry() const {
    return pImpl_->getToolRegistry();
}

std::shared_ptr<venv::VenvManager> ScriptService::getVenvManager() const {
    return pImpl_->getVenvManager();
}

std::shared_ptr<ScriptAnalyzer> ScriptService::getScriptAnalyzer() const {
    return pImpl_->getScriptAnalyzer();
}

void ScriptService::setProgressCallback(ScriptProgressCallback callback) {
    pImpl_->setProgressCallback(std::move(callback));
}

void ScriptService::setLogCallback(ScriptLogCallback callback) {
    pImpl_->setLogCallback(std::move(callback));
}

nlohmann::json ScriptService::getStatistics() const {
    return pImpl_->getStatistics();
}

void ScriptService::resetStatistics() {
    pImpl_->resetStatistics();
}

}  // namespace lithium
