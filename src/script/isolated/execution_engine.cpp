/*
 * execution_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "execution_engine.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>

namespace lithium::isolated {

ExecutionEngine::ExecutionEngine() = default;
ExecutionEngine::~ExecutionEngine() = default;

ExecutionEngine::ExecutionEngine(ExecutionEngine&&) noexcept = default;
ExecutionEngine& ExecutionEngine::operator=(ExecutionEngine&&) noexcept =
    default;

void ExecutionEngine::setConfig(const IsolationConfig& config) {
    config_ = config;
}

const IsolationConfig& ExecutionEngine::getConfig() const { return config_; }

void ExecutionEngine::setProgressCallback(ProgressCallback callback) {
    messageHandler_.setProgressCallback(std::move(callback));
}

void ExecutionEngine::setLogCallback(LogCallback callback) {
    messageHandler_.setLogCallback(std::move(callback));
}

ExecutionResult ExecutionEngine::execute(std::string_view scriptContent,
                                         const nlohmann::json& args) {
    return executeInternal(scriptContent, "", "", "", args);
}

ExecutionResult ExecutionEngine::executeFile(
    const std::filesystem::path& scriptPath, const nlohmann::json& args) {
    if (!std::filesystem::exists(scriptPath)) {
        ExecutionResult result;
        result.success = false;
        result.error = RunnerError::ExecutionFailed;
        result.exception = "Script file not found: " + scriptPath.string();
        return result;
    }

    // Read script content
    std::ifstream file(scriptPath);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    return executeInternal(content, scriptPath.string(), "", "", args);
}

ExecutionResult ExecutionEngine::executeFunction(std::string_view moduleName,
                                                 std::string_view functionName,
                                                 const nlohmann::json& args) {
    return executeInternal("", "", std::string(moduleName),
                           std::string(functionName), args);
}

bool ExecutionEngine::cancel() { return lifecycle_.cancel(); }

bool ExecutionEngine::isRunning() const { return lifecycle_.isRunning(); }

std::optional<int> ExecutionEngine::getProcessId() const {
    if (!lifecycle_.isRunning())
        return std::nullopt;
    return lifecycle_.getProcessId();
}

std::optional<size_t> ExecutionEngine::getCurrentMemoryUsage() const {
    if (!lifecycle_.isRunning())
        return std::nullopt;
    return ResourceMonitor::getMemoryUsage(lifecycle_.getProcessId());
}

void ExecutionEngine::kill() { lifecycle_.kill(); }

ExecutionResult ExecutionEngine::executeInternal(
    std::string_view scriptContent, const std::string& scriptPath,
    const std::string& moduleName, const std::string& functionName,
    const nlohmann::json& args) {
    ExecutionResult result;
    auto startTime = std::chrono::steady_clock::now();

    // Validate configuration
    auto validateResult = ConfigDiscovery::validateConfig(config_);
    if (!validateResult) {
        result.success = false;
        result.error = validateResult.error();
        result.exception =
            std::string(runnerErrorToString(validateResult.error()));
        return result;
    }

    // Get paths
    auto pythonPath = config_.pythonExecutable.empty()
                          ? *ConfigDiscovery::findPythonExecutable()
                          : config_.pythonExecutable;
    auto executorPath = config_.executorScript.empty()
                            ? *ConfigDiscovery::findExecutorScript()
                            : config_.executorScript;

    // Create IPC channel
    channel_ = std::make_shared<ipc::BidirectionalChannel>();
    auto channelResult = channel_->create();
    if (!channelResult) {
        result.success = false;
        result.error = RunnerError::CommunicationError;
        result.exception = "Failed to create IPC channel";
        return result;
    }

    // Spawn subprocess
    auto subprocessFds = channel_->getSubprocessFds();
    auto spawnResult =
        ProcessSpawner::spawn(pythonPath, executorPath, config_, subprocessFds);
    if (!spawnResult) {
        result.success = false;
        result.error = RunnerError::ProcessSpawnFailed;
        result.exception = "Failed to spawn Python process";
        channel_.reset();
        return result;
    }

    lifecycle_.setProcessId(*spawnResult);
    lifecycle_.setChannel(channel_);
    lifecycle_.setRunning(true);
    lifecycle_.resetCancellation();

    // Setup parent side of channel
    channel_->setupParent();

    // Perform handshake
    auto handshakeResult = channel_->performHandshake(std::chrono::seconds{10});
    if (!handshakeResult) {
        result.success = false;
        result.error = RunnerError::HandshakeFailed;
        result.exception = "Handshake with Python process failed";
        lifecycle_.kill();
        return result;
    }

    spdlog::debug("Handshake successful, Python version: {}",
                  handshakeResult->pythonVersion);

    // Send execute request
    ipc::ExecuteRequest request;
    request.scriptContent = std::string(scriptContent);
    request.scriptPath = scriptPath;
    request.functionName = functionName;
    request.arguments = args;
    request.timeout = config_.timeout;
    request.captureOutput = config_.captureOutput;
    request.allowedImports = config_.allowedImports;

    if (!config_.workingDirectory.empty()) {
        request.workingDirectory = config_.workingDirectory.string();
    }

    auto msg =
        ipc::Message::create(ipc::MessageType::Execute, request.toJson());
    auto sendResult = channel_->send(msg);
    if (!sendResult) {
        result.success = false;
        result.error = RunnerError::CommunicationError;
        result.exception = "Failed to send execute request";
        lifecycle_.kill();
        return result;
    }

    // Wait for result with timeout
    auto deadline = std::chrono::steady_clock::now() + config_.timeout;

    while (lifecycle_.isRunning() && !lifecycle_.isCancelled()) {
        if (std::chrono::steady_clock::now() > deadline) {
            result.success = false;
            result.error = RunnerError::Timeout;
            result.exception = "Execution timed out";
            lifecycle_.kill();
            return result;
        }

        // Check memory limit
        if (config_.maxMemoryMB > 0) {
            if (ResourceMonitor::isMemoryLimitExceeded(
                    lifecycle_.getProcessId(), config_.maxMemoryMB)) {
                result.success = false;
                result.error = RunnerError::MemoryLimitExceeded;
                result.exception = "Memory limit exceeded";
                result.peakMemoryUsage =
                    ResourceMonitor::getMemoryUsage(lifecycle_.getProcessId())
                        .value_or(0);
                lifecycle_.kill();
                return result;
            }
        }

        // Try to receive message
        auto msgResult = channel_->receive(std::chrono::milliseconds{100});
        if (!msgResult) {
            if (msgResult.error() == ipc::IPCError::Timeout) {
                continue;  // Keep waiting
            }
            result.success = false;
            result.error = RunnerError::CommunicationError;
            result.exception =
                std::string(ipc::ipcErrorToString(msgResult.error()));
            lifecycle_.kill();
            return result;
        }

        // Process message
        auto handlerResult = messageHandler_.processMessage(*msgResult, result);
        if (!handlerResult.shouldContinue) {
            lifecycle_.setRunning(false);
            result = handlerResult.result;
            break;
        }
    }

    // Calculate execution time
    result.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

    // Wait for process to exit
    lifecycle_.waitForExit();

    return result;
}

}  // namespace lithium::isolated
