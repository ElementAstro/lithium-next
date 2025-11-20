#ifndef LITHIUM_APP_COMMAND_HPP
#define LITHIUM_APP_COMMAND_HPP

#include <any>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/memory/memory.hpp"
#include "eventloop.hpp"


namespace lithium::app {

/**
 * @brief Command execution status enumeration
 */
enum class CommandStatus {
    PENDING,    ///< Command awaiting execution
    RUNNING,    ///< Command currently executing
    COMPLETED,  ///< Command completed successfully
    FAILED,     ///< Command execution failed
    CANCELLED   ///< Command was cancelled
};

/**
 * @brief Command execution result container
 */
struct CommandResult {
    CommandStatus status;      ///< Execution status
    std::any result;           ///< Execution result
    std::string errorMessage;  ///< Error description
    std::chrono::system_clock::time_point
        executionTime;  ///< Execution timestamp
};

/**
 * @brief High-performance command dispatcher with async execution, priority
 * handling, and memory pooling
 */
class CommandDispatcher {
public:
    using CommandID = std::string;
    using CommandHandler = std::function<void(const std::any&)>;
    using ResultType = std::variant<std::any, std::exception_ptr>;
    using CommandCallback =
        std::function<void(const CommandID&, const ResultType&)>;
    using EventCallback =
        std::function<void(const CommandID&, const std::any&)>;

    /**
     * @brief CommandDispatcher configuration parameters
     */
    struct Config {
        std::size_t maxHistorySize = 100;  ///< Maximum command history entries
        std::chrono::milliseconds defaultTimeout{
            5000};                           ///< Default command timeout
        size_t maxConcurrentCommands = 100;  ///< Maximum concurrent commands
        bool enablePriority = true;  ///< Enable priority-based execution
    };

    /**
     * @brief Constructs CommandDispatcher with event loop and configuration
     *
     * @param eventLoop Shared event loop instance
     * @param config Dispatcher configuration
     */
    explicit CommandDispatcher(std::shared_ptr<EventLoop> eventLoop,
                               const Config& config);

    /**
     * @brief Registers typed command handler with optional undo capability
     *
     * @tparam CommandType Command data type
     * @param id Unique command identifier
     * @param handler Command execution function
     * @param undoHandler Optional command undo function
     */
    template <typename CommandType>
    void registerCommand(const CommandID& id,
                         std::function<void(const CommandType&)> handler,
                         std::optional<std::function<void(const CommandType&)>>
                             undoHandler = std::nullopt);

    /**
     * @brief Unregisters command handler
     *
     * @param id Command identifier to unregister
     */
    void unregisterCommand(const CommandID& id);

    /**
     * @brief Dispatches command for asynchronous execution with priority and
     * delay support
     *
     * @tparam CommandType Command data type
     * @param id Command identifier
     * @param command Command data
     * @param priority Execution priority (higher values execute first)
     * @param delay Optional execution delay
     * @param callback Optional completion callback
     * @return Future containing execution result
     */
    template <typename CommandType>
    auto dispatch(const CommandID& id, const CommandType& command,
                  int priority = 0,
                  std::optional<std::chrono::milliseconds> delay = std::nullopt,
                  CommandCallback callback = nullptr)
        -> std::future<ResultType>;

    /**
     * @brief Dispatches multiple commands as a batch operation
     *
     * @tparam CommandType Command data type
     * @param commands Vector of command ID and data pairs
     * @param priority Batch execution priority
     * @return Vector of futures for each command result
     */
    template <typename CommandType>
    auto batchDispatch(
        const std::vector<std::pair<CommandID, CommandType>>& commands,
        int priority = 0) -> std::vector<std::future<ResultType>>;

    /**
     * @brief Cancels pending or running command
     *
     * @param id Command identifier to cancel
     */
    void cancelCommand(const CommandID& id);

    /**
     * @brief Retrieves current command execution status
     *
     * @param id Command identifier
     * @return Current command status
     */
    CommandStatus getCommandStatus(const CommandID& id) const;

    /**
     * @brief Sets timeout for specific command
     *
     * @param id Command identifier
     * @param timeout Timeout duration
     */
    void setTimeout(const CommandID& id,
                    const std::chrono::milliseconds& timeout);

    /**
     * @brief Synchronously dispatches and waits for command completion
     *
     * @tparam CommandType Command data type
     * @param id Command identifier
     * @param command Command data
     * @return Command execution result
     */
    template <typename CommandType>
    auto quickDispatch(const CommandID& id, const CommandType& command)
        -> CommandType;

    /**
     * @brief Extracts typed result from future
     *
     * @tparam CommandType Expected result type
     * @param resultFuture Future containing command result
     * @return Typed command result
     */
    template <typename CommandType>
    auto getResult(std::future<ResultType>& resultFuture) -> CommandType;

    /**
     * @brief Executes command undo operation
     *
     * @tparam CommandType Command data type
     * @param id Command identifier
     * @param command Command data to undo
     */
    template <typename CommandType>
    void undo(const CommandID& id, const CommandType& command);

    /**
     * @brief Re-executes previously executed command
     *
     * @tparam CommandType Command data type
     * @param id Command identifier
     * @param command Command data to redo
     */
    template <typename CommandType>
    void redo(const CommandID& id, const CommandType& command);

    /**
     * @brief Subscribes to command execution events
     *
     * @param id Command identifier to monitor
     * @param callback Event notification callback
     * @return Subscription token for unsubscribing
     */
    int subscribe(const CommandID& id, EventCallback callback);

    /**
     * @brief Unsubscribes from command events
     *
     * @param id Command identifier
     * @param token Subscription token
     */
    void unsubscribe(const CommandID& id, int token);

    /**
     * @brief Retrieves command execution history
     *
     * @tparam CommandType Command data type
     * @param id Command identifier
     * @return Vector of historical command instances
     */
    template <typename CommandType>
    auto getCommandHistory(const CommandID& id) -> std::vector<CommandType>;

    /**
     * @brief Clears all command history
     */
    void clearHistory();

    /**
     * @brief Clears history for specific command
     *
     * @param id Command identifier
     */
    void clearCommandHistory(const CommandID& id);

    /**
     * @brief Retrieves list of currently active commands
     *
     * @return Vector of active command identifiers
     */
    auto getActiveCommands() const -> std::vector<CommandID>;

private:
    void recordCommandHistory(const CommandID& id, const std::any& command);
    void notifyEventSubscribers(const CommandID& id, const std::any& command);
    void updateCommandStatus(const CommandID& id, CommandStatus status);
    bool checkCommandTimeout(const CommandID& id) const;
    void cleanupCommandResources(const CommandID& id);

    static constexpr size_t COMMAND_POOL_BLOCK_SIZE = 4096;
    mutable MemoryPool<std::byte, COMMAND_POOL_BLOCK_SIZE> commandMemoryPool_;

    std::unordered_map<CommandID, CommandHandler> commandHandlers_;
    std::unordered_map<CommandID, CommandHandler> undoHandlers_;
    std::unordered_map<CommandID, std::vector<std::any>> commandHistory_;
    std::unordered_map<CommandID, std::unordered_map<int, EventCallback>>
        eventSubscribers_;
    std::unordered_map<CommandID, CommandStatus> commandStatusMap_;
    std::unordered_map<CommandID, std::chrono::milliseconds> commandTimeouts_;

    mutable std::shared_mutex accessMutex_;
    std::shared_ptr<EventLoop> eventLoop_;
    Config configuration_;
    int nextSubscriberToken_ = 0;

    struct CommandExecutionInfo {
        CommandStatus status;
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds timeout;
    };

    std::unordered_map<CommandID, CommandExecutionInfo> executionInfoMap_;
};

template <typename CommandType>
auto CommandDispatcher::dispatch(const CommandID& id,
                                 const CommandType& command, int priority,
                                 std::optional<std::chrono::milliseconds> delay,
                                 CommandCallback callback)
    -> std::future<ResultType> {
    size_t commandSize = sizeof(CommandType);
    void* memory = commandMemoryPool_.allocate(commandSize);
    auto* commandCopy = new (memory) CommandType(command);

    updateCommandStatus(id, CommandStatus::PENDING);

    auto executionTask = [this, id, commandCopy, callback]() -> ResultType {
        try {
            updateCommandStatus(id, CommandStatus::RUNNING);

            if (checkCommandTimeout(id)) {
                commandCopy->~CommandType();
                commandMemoryPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));
                THROW_RUNTIME_ERROR("Command execution timeout");
            }

            std::shared_lock lock(accessMutex_);
            auto handlerIt = commandHandlers_.find(id);
            if (handlerIt != commandHandlers_.end()) {
                handlerIt->second(*commandCopy);
                recordCommandHistory(id, *commandCopy);
                notifyEventSubscribers(id, *commandCopy);

                updateCommandStatus(id, CommandStatus::COMPLETED);

                ResultType result = *commandCopy;
                commandCopy->~CommandType();
                commandMemoryPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));

                if (callback) {
                    callback(id, result);
                }
                return result;
            } else {
                commandCopy->~CommandType();
                commandMemoryPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));
                THROW_RUNTIME_ERROR("Command handler not found: " + id);
            }
        } catch (const std::exception& e) {
            updateCommandStatus(id, CommandStatus::FAILED);
            auto exception = std::current_exception();
            if (callback) {
                callback(id, exception);
            }
            return exception;
        }
    };

    if (delay) {
        return eventLoop_->postDelayed(*delay, priority,
                                       std::move(executionTask));
    }
    return eventLoop_->post(priority, std::move(executionTask));
}

template <typename CommandType>
void CommandDispatcher::registerCommand(
    const CommandID& id, std::function<void(const CommandType&)> handler,
    std::optional<std::function<void(const CommandType&)>> undoHandler) {
    std::unique_lock lock(accessMutex_);
    commandHandlers_[id] = [handler](const std::any& cmd) {
        handler(std::any_cast<const CommandType&>(cmd));
    };
    if (undoHandler) {
        undoHandlers_[id] = [undoHandler](const std::any& cmd) {
            (*undoHandler)(std::any_cast<const CommandType&>(cmd));
        };
    }
}

template <typename CommandType>
auto CommandDispatcher::getResult(std::future<ResultType>& resultFuture)
    -> CommandType {
    auto result = resultFuture.get();
    if (std::holds_alternative<std::any>(result)) {
        return std::any_cast<CommandType>(std::get<std::any>(result));
    }
    std::rethrow_exception(std::get<std::exception_ptr>(result));
}

template <typename CommandType>
void CommandDispatcher::undo(const CommandID& id, const CommandType& command) {
    std::shared_lock lock(accessMutex_);
    auto handlerIt = undoHandlers_.find(id);
    if (handlerIt != undoHandlers_.end()) {
        handlerIt->second(command);
    }
}

template <typename CommandType>
void CommandDispatcher::redo(const CommandID& id, const CommandType& command) {
    dispatch(id, command, 0, std::nullopt).get();
}

template <typename CommandType>
auto CommandDispatcher::getCommandHistory(const CommandID& id)
    -> std::vector<CommandType> {
    std::shared_lock lock(accessMutex_);
    std::vector<CommandType> history;
    if (auto historyIt = commandHistory_.find(id);
        historyIt != commandHistory_.end()) {
        history.reserve(historyIt->second.size());
        for (const auto& cmd : historyIt->second) {
            history.push_back(std::any_cast<CommandType>(cmd));
        }
    }
    return history;
}

template <typename CommandType>
auto CommandDispatcher::batchDispatch(
    const std::vector<std::pair<CommandID, CommandType>>& commands,
    int priority) -> std::vector<std::future<ResultType>> {
    std::vector<std::future<ResultType>> results;
    results.reserve(commands.size());

    for (const auto& [id, cmd] : commands) {
        results.push_back(dispatch(id, cmd, priority));
    }
    return results;
}

template <typename CommandType>
auto CommandDispatcher::quickDispatch(const CommandID& id,
                                      const CommandType& command)
    -> CommandType {
    auto future = dispatch(id, command);
    return getResult<CommandType>(future);
}

}  // namespace lithium::app

#endif  // LITHIUM_APP_COMMAND_HPP