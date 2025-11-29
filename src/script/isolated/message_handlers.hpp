/*
 * message_handlers.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_MESSAGE_HANDLERS_HPP
#define LITHIUM_SCRIPT_ISOLATED_MESSAGE_HANDLERS_HPP

#include "types.hpp"
#include "../ipc/message.hpp"

#include <functional>

namespace lithium::isolated {

/**
 * @brief Handler result after processing a message
 */
struct MessageHandlerResult {
    bool shouldContinue{true};      ///< Continue waiting for messages
    bool executionComplete{false};  ///< Execution has completed
    ExecutionResult result;         ///< Partial or final execution result
};

/**
 * @brief IPC message handler for isolated runner
 */
class MessageHandler {
public:
    /**
     * @brief Set progress callback
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set log callback
     */
    void setLogCallback(LogCallback callback);

    /**
     * @brief Process an incoming IPC message
     * @param message Message to process
     * @param currentResult Current execution result to update
     * @return Handler result indicating what to do next
     */
    [[nodiscard]] MessageHandlerResult processMessage(
        const ipc::Message& message,
        ExecutionResult& currentResult);

private:
    /**
     * @brief Handle Result message
     */
    [[nodiscard]] MessageHandlerResult handleResult(
        const nlohmann::json& payload,
        ExecutionResult& result);

    /**
     * @brief Handle Progress message
     */
    [[nodiscard]] MessageHandlerResult handleProgress(
        const nlohmann::json& payload);

    /**
     * @brief Handle Log message
     */
    [[nodiscard]] MessageHandlerResult handleLog(
        const nlohmann::json& payload);

    /**
     * @brief Handle Error message
     */
    [[nodiscard]] MessageHandlerResult handleError(
        const nlohmann::json& payload,
        ExecutionResult& result);

    ProgressCallback progressCallback_;
    LogCallback logCallback_;
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_MESSAGE_HANDLERS_HPP
