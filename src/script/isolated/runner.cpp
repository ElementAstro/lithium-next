/*
 * runner.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "runner.hpp"
#include "execution_engine.hpp"
#include "config_discovery.hpp"

#include <future>

namespace lithium::isolated {

// ============================================================================
// PythonRunner::Impl
// ============================================================================

class PythonRunner::Impl {
public:
    explicit Impl(const IsolationConfig& config) : engine_() {
        engine_.setConfig(config);
    }

    ExecutionEngine engine_;
};

// ============================================================================
// PythonRunner
// ============================================================================

PythonRunner::PythonRunner()
    : pImpl_(std::make_unique<Impl>(IsolationConfig{})) {}

PythonRunner::PythonRunner(const IsolationConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

PythonRunner::~PythonRunner() = default;

PythonRunner::PythonRunner(PythonRunner&&) noexcept = default;
PythonRunner& PythonRunner::operator=(PythonRunner&&) noexcept = default;

void PythonRunner::setConfig(const IsolationConfig& config) {
    pImpl_->engine_.setConfig(config);
}

const IsolationConfig& PythonRunner::getConfig() const {
    return pImpl_->engine_.getConfig();
}

void PythonRunner::setPythonExecutable(const std::filesystem::path& path) {
    auto config = pImpl_->engine_.getConfig();
    config.pythonExecutable = path;
    pImpl_->engine_.setConfig(config);
}

void PythonRunner::setExecutorScript(const std::filesystem::path& path) {
    auto config = pImpl_->engine_.getConfig();
    config.executorScript = path;
    pImpl_->engine_.setConfig(config);
}

void PythonRunner::setProgressCallback(ProgressCallback callback) {
    pImpl_->engine_.setProgressCallback(std::move(callback));
}

void PythonRunner::setLogCallback(LogCallback callback) {
    pImpl_->engine_.setLogCallback(std::move(callback));
}

ExecutionResult PythonRunner::execute(
    std::string_view scriptContent, const nlohmann::json& args) {
    return pImpl_->engine_.execute(scriptContent, args);
}

ExecutionResult PythonRunner::executeFile(
    const std::filesystem::path& scriptPath, const nlohmann::json& args) {
    return pImpl_->engine_.executeFile(scriptPath, args);
}

ExecutionResult PythonRunner::executeFunction(
    std::string_view moduleName, std::string_view functionName,
    const nlohmann::json& args) {
    return pImpl_->engine_.executeFunction(moduleName, functionName, args);
}

std::future<ExecutionResult> PythonRunner::executeAsync(
    std::string_view scriptContent, const nlohmann::json& args) {
    std::string content(scriptContent);
    return std::async(std::launch::async, [this, content, args] {
        return execute(content, args);
    });
}

std::future<ExecutionResult> PythonRunner::executeFileAsync(
    const std::filesystem::path& scriptPath, const nlohmann::json& args) {
    return std::async(std::launch::async, [this, scriptPath, args] {
        return executeFile(scriptPath, args);
    });
}

std::future<ExecutionResult> PythonRunner::executeFunctionAsync(
    std::string_view moduleName, std::string_view functionName,
    const nlohmann::json& args) {
    std::string mod(moduleName);
    std::string func(functionName);
    return std::async(std::launch::async, [this, mod, func, args] {
        return executeFunction(mod, func, args);
    });
}

bool PythonRunner::cancel() {
    return pImpl_->engine_.cancel();
}

bool PythonRunner::isRunning() const {
    return pImpl_->engine_.isRunning();
}

std::optional<int> PythonRunner::getProcessId() const {
    return pImpl_->engine_.getProcessId();
}

std::optional<size_t> PythonRunner::getCurrentMemoryUsage() const {
    return pImpl_->engine_.getCurrentMemoryUsage();
}

std::optional<double> PythonRunner::getCurrentCpuUsage() const {
    return std::nullopt;  // Not implemented yet
}

void PythonRunner::kill() {
    pImpl_->engine_.kill();
}

Result<void> PythonRunner::validateConfig() const {
    return ConfigDiscovery::validateConfig(pImpl_->engine_.getConfig());
}

std::optional<std::filesystem::path> PythonRunner::findExecutorScript() {
    return ConfigDiscovery::findExecutorScript();
}

std::optional<std::filesystem::path> PythonRunner::findPythonExecutable() {
    return ConfigDiscovery::findPythonExecutable();
}

std::optional<std::string> PythonRunner::getPythonVersion() const {
    const auto& config = pImpl_->engine_.getConfig();
    auto pythonPath = config.pythonExecutable.empty() ?
        ConfigDiscovery::findPythonExecutable() :
        std::optional{config.pythonExecutable};

    if (!pythonPath) return std::nullopt;
    return ConfigDiscovery::getPythonVersion(*pythonPath);
}

// ============================================================================
// RunnerFactory
// ============================================================================

std::unique_ptr<PythonRunner> RunnerFactory::create() {
    return std::make_unique<PythonRunner>();
}

std::unique_ptr<PythonRunner> RunnerFactory::createQuick() {
    IsolationConfig config;
    config.level = IsolationLevel::Subprocess;
    config.timeout = std::chrono::seconds{60};
    config.maxMemoryMB = 256;
    return std::make_unique<PythonRunner>(config);
}

std::unique_ptr<PythonRunner> RunnerFactory::createSecure() {
    IsolationConfig config;
    config.level = IsolationLevel::Sandboxed;
    config.allowNetwork = false;
    config.allowFilesystem = false;
    config.maxMemoryMB = 128;
    config.maxCpuPercent = 50;
    config.timeout = std::chrono::seconds{30};
    config.blockedImports = {"os", "subprocess", "socket", "sys"};
    return std::make_unique<PythonRunner>(config);
}

std::unique_ptr<PythonRunner> RunnerFactory::createScientific() {
    IsolationConfig config;
    config.level = IsolationLevel::Subprocess;
    config.maxMemoryMB = 4096;
    config.timeout = std::chrono::seconds{3600};
    config.allowedImports = {"numpy", "scipy", "pandas", "astropy", "matplotlib"};
    return std::make_unique<PythonRunner>(config);
}

std::unique_ptr<PythonRunner> RunnerFactory::create(const IsolationConfig& config) {
    return std::make_unique<PythonRunner>(config);
}

}  // namespace lithium::isolated
