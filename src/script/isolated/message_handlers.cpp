/*
 * message_handlers.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "message_handlers.hpp"

#include <spdlog/spdlog.h>

namespace lithium::isolated {

void MessageHandler::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

void MessageHandler::setLogCallback(LogCallback callback) {
    logCallback_ = std::move(callback);
}

MessageHandlerResult MessageHandler::processMessage(
    const ipc::Message& message, ExecutionResult& currentResult) {
    auto payloadResult = message.getPayloadAsJson();
    if (!payloadResult) {
        MessageHandlerResult result;
        result.shouldContinue = true;
        return result;
    }

    const auto& payload = *payloadResult;

    switch (message.header.type) {
        case ipc::MessageType::Result:
            return handleResult(payload, currentResult);

        case ipc::MessageType::Progress:
            return handleProgress(payload);

        case ipc::MessageType::Log:
            return handleLog(payload);

        case ipc::MessageType::Error:
            return handleError(payload, currentResult);

        default:
            spdlog::warn("Unexpected message type: {}",
                         static_cast<int>(message.header.type));
            MessageHandlerResult result;
            result.shouldContinue = true;
            return result;
    }
}

MessageHandlerResult MessageHandler::handleResult(const nlohmann::json& payload,
                                                  ExecutionResult& result) {
    auto execResult = ipc::ExecuteResult::fromJson(payload);
    if (execResult) {
        result.success = execResult->success;
        result.result = execResult->result;
        result.output = execResult->output;
        result.errorOutput = execResult->errorOutput;
        result.exception = execResult->exception;
        result.exceptionType = execResult->exceptionType;
        result.traceback = execResult->traceback;
        result.peakMemoryUsage = execResult->peakMemoryBytes;
    }

    MessageHandlerResult handlerResult;
    handlerResult.shouldContinue = false;
    handlerResult.executionComplete = true;
    handlerResult.result = result;
    return handlerResult;
}

MessageHandlerResult MessageHandler::handleProgress(
    const nlohmann::json& payload) {
    if (progressCallback_) {
        auto progress = ipc::ProgressUpdate::fromJson(payload);
        if (progress) {
            progressCallback_(progress->percentage, progress->message,
                              progress->currentStep);
        }
    }

    MessageHandlerResult result;
    result.shouldContinue = true;
    return result;
}

MessageHandlerResult MessageHandler::handleLog(const nlohmann::json& payload) {
    if (logCallback_) {
        std::string level = payload.value("level", "info");
        std::string message = payload.value("message", "");
        logCallback_(level, message);
    }

    MessageHandlerResult result;
    result.shouldContinue = true;
    return result;
}

MessageHandlerResult MessageHandler::handleError(const nlohmann::json& payload,
                                                 ExecutionResult& result) {
    result.success = false;
    result.exception = payload.value("message", "Unknown error");
    result.error = RunnerError::ExecutionFailed;

    MessageHandlerResult handlerResult;
    handlerResult.shouldContinue = false;
    handlerResult.executionComplete = true;
    handlerResult.result = result;
    return handlerResult;
}

}  // namespace lithium::isolated
