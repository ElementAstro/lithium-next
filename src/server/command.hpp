#ifndef LITHIUM_APP_COMMAND_HPP
#define LITHIUM_APP_COMMAND_HPP

#include <any>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "eventloop.hpp"

#include "atom/error/exception.hpp"
#include "atom/memory/memory.hpp"

namespace lithium::app {

/**
 * @brief Enum representing the status of a command execution.
 */
enum class CommandStatus {
    PENDING,    ///< Command is pending execution.
    RUNNING,    ///< Command is currently running.
    COMPLETED,  ///< Command has completed successfully.
    FAILED,     ///< Command has failed.
    CANCELLED   ///< Command has been cancelled.
};

/**
 * @brief Structure representing the result of a command execution.
 */
struct CommandResult {
    CommandStatus status;  ///< Status of the command execution.
    std::any result;       ///< Result of the command execution.
    std::string error;     ///< Error message if the command failed.
    std::chrono::system_clock::time_point
        timestamp;  ///< Timestamp of the command execution.
};

/**
 * @brief Class for dispatching and managing commands.
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
     * @brief Configuration structure for CommandDispatcher.
     */
    struct Config {
        std::size_t maxHistorySize =
            100;  ///< Maximum size of the command history.
        std::chrono::milliseconds defaultTimeout{
            5000};  ///< Default timeout for command execution.
        size_t maxConcurrentCommands =
            100;  ///< Maximum number of concurrent commands.
        bool enablePriority =
            true;  ///< Flag to enable or disable command priority.
    };

    /**
     * @brief Constructs a CommandDispatcher instance.
     *
     * @param eventLoop Shared pointer to the event loop.
     * @param config Configuration settings for the command dispatcher.
     */
    explicit CommandDispatcher(std::shared_ptr<EventLoop> eventLoop,
                               const Config& config);

    /**
     * @brief Registers a command handler.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @param handler The handler function to execute the command.
     * @param undoHandler Optional undo handler function for the command.
     */
    template <typename CommandType>
    void registerCommand(const CommandID& id,
                         std::function<void(const CommandType&)> handler,
                         std::optional<std::function<void(const CommandType&)>>
                             undoHandler = std::nullopt);

    /**
     * @brief Unregisters a command handler.
     *
     * @param id The ID of the command.
     */
    void unregisterCommand(const CommandID& id);

    /**
     * @brief Dispatches a command for execution.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @param command The command to execute.
     * @param priority The priority of the command.
     * @param delay Optional delay before executing the command.
     * @param callback Optional callback function to call when the command
     * completes.
     * @return A future representing the result of the command execution.
     */
    template <typename CommandType>
    auto dispatch(
        const CommandID& id, const CommandType& command, int priority = 0,
        std::optional<std::chrono::milliseconds> delay = std::nullopt,
        CommandCallback callback = nullptr) -> std::future<ResultType>;

    /**
     * @brief Dispatches a batch of commands for execution.
     *
     * @tparam CommandType The type of the commands.
     * @param commands A vector of command ID and command pairs.
     * @param priority The priority of the commands.
     * @return A vector of futures representing the results of the command
     * executions.
     */
    template <typename CommandType>
    auto batchDispatch(
        const std::vector<std::pair<CommandID, CommandType>>& commands,
        int priority = 0) -> std::vector<std::future<ResultType>>;

    /**
     * @brief Cancels a command.
     *
     * @param id The ID of the command to cancel.
     */
    void cancelCommand(const CommandID& id);

    /**
     * @brief Gets the status of a command.
     *
     * @param id The ID of the command.
     * @return The status of the command.
     */
    CommandStatus getCommandStatus(const CommandID& id) const;

    /**
     * @brief Sets the timeout for a command.
     *
     * @param id The ID of the command.
     * @param timeout The timeout duration.
     */
    void setTimeout(const CommandID& id,
                    const std::chrono::milliseconds& timeout);

    /**
     * @brief Quickly dispatches a command for execution.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @param command The command to execute.
     * @return The result of the command execution.
     */
    template <typename CommandType>
    auto quickDispatch(const CommandID& id,
                       const CommandType& command) -> CommandType;

    /**
     * @brief Gets the result of a command execution.
     *
     * @tparam CommandType The type of the command result.
     * @param resultFuture The future representing the result of the command
     * execution.
     * @return The result of the command execution.
     */
    template <typename CommandType>
    auto getResult(std::future<ResultType>& resultFuture) -> CommandType;

    /**
     * @brief Undoes a command.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @param command The command to undo.
     */
    template <typename CommandType>
    void undo(const CommandID& id, const CommandType& command);

    /**
     * @brief Redoes a command.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @param command The command to redo.
     */
    template <typename CommandType>
    void redo(const CommandID& id, const CommandType& command);

    /**
     * @brief Subscribes to command events.
     *
     * @param id The ID of the command.
     * @param callback The callback function to execute when the command event
     * occurs.
     * @return A token representing the subscription.
     */
    int subscribe(const CommandID& id, EventCallback callback);

    /**
     * @brief Unsubscribes from command events.
     *
     * @param id The ID of the command.
     * @param token The token representing the subscription.
     */
    void unsubscribe(const CommandID& id, int token);

    /**
     * @brief Gets the history of a command.
     *
     * @tparam CommandType The type of the command.
     * @param id The ID of the command.
     * @return A vector of command instances representing the history of the
     * command.
     */
    template <typename CommandType>
    auto getCommandHistory(const CommandID& id) -> std::vector<CommandType>;

    /**
     * @brief Clears the command history.
     */
    void clearHistory();

    /**
     * @brief Clears the history of a specific command.
     *
     * @param id The ID of the command.
     */
    void clearCommandHistory(const CommandID& id);

    /**
     * @brief Gets the list of active commands.
     *
     * @return A vector of command IDs representing the active commands.
     */
    auto getActiveCommands() const -> std::vector<CommandID>;

private:
    void recordHistory(const CommandID& id, const std::any& command);
    void notifySubscribers(const CommandID& id, const std::any& command);

    static constexpr size_t COMMAND_POOL_BLOCK_SIZE = 4096;
    MemoryPool<std::byte, COMMAND_POOL_BLOCK_SIZE> commandPool_;  // 新增内存池

    std::unordered_map<CommandID, CommandHandler> handlers_;
    std::unordered_map<CommandID, CommandHandler> undoHandlers_;
    std::unordered_map<CommandID, std::vector<std::any>> history_;
    std::unordered_map<CommandID, std::unordered_map<int, EventCallback>>
        subscribers_;
    mutable std::shared_mutex mutex_;
    std::size_t maxHistorySize_ = 100;
    std::shared_ptr<EventLoop> eventLoop_;
    int nextSubscriberId_ = 0;

    Config config_;
    std::unordered_map<CommandID, CommandStatus> commandStatus_;
    std::unordered_map<CommandID, std::chrono::milliseconds> timeouts_;
    std::priority_queue<std::pair<int, CommandID>> priorityQueue_;

    struct CommandInfo {
        CommandStatus status;
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds timeout;
    };

    std::unordered_map<CommandID, CommandInfo> commandInfo_;

    void updateCommandStatus(const CommandID& id, CommandStatus status);
    bool checkTimeout(const CommandID& id) const;
    void cleanupCommand(const CommandID& id);
};

template <typename CommandType>
auto CommandDispatcher::dispatch(
    const CommandID& id, const CommandType& command, int priority,
    std::optional<std::chrono::milliseconds> delay,
    CommandCallback callback) -> std::future<ResultType> {
    // 使用内存池分配命令副本
    size_t commandSize = sizeof(CommandType);
    void* memory = commandPool_.allocate(commandSize);
    auto* commandCopy = new (memory) CommandType(command);

    updateCommandStatus(id, CommandStatus::PENDING);

    auto task = [this, id, commandCopy, callback]() -> ResultType {
        try {
            updateCommandStatus(id, CommandStatus::RUNNING);

            if (checkTimeout(id)) {
                // 释放内存
                commandCopy->~CommandType();
                commandPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));
                THROW_RUNTIME_ERROR("Command timeout");
            }

            std::shared_lock lock(mutex_);
            auto it = handlers_.find(id);
            if (it != handlers_.end()) {
                it->second(*commandCopy);
                recordHistory(id, *commandCopy);
                notifySubscribers(id, *commandCopy);

                updateCommandStatus(id, CommandStatus::COMPLETED);

                ResultType result = *commandCopy;
                // 释放内存
                commandCopy->~CommandType();
                commandPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));

                if (callback) {
                    callback(id, result);
                }
                return result;
            } else {
                // 释放内存
                commandCopy->~CommandType();
                commandPool_.deallocate(
                    reinterpret_cast<std::byte*>(commandCopy),
                    sizeof(CommandType));
                THROW_RUNTIME_ERROR("Command not found: " + id);
            }
        } catch (const std::exception& e) {
            updateCommandStatus(id, CommandStatus::FAILED);
            auto ex = std::current_exception();
            if (callback) {
                callback(id, ex);
            }
            return ex;
        }
    };

    if (delay) {
        return eventLoop_->postDelayed(*delay, priority, std::move(task));
    } else {
        return eventLoop_->post(priority, std::move(task));
    }
}

// Template implementations
template <typename CommandType>
void CommandDispatcher::registerCommand(
    const CommandID& id, std::function<void(const CommandType&)> handler,
    std::optional<std::function<void(const CommandType&)>> undoHandler) {
    std::unique_lock lock(mutex_);
    handlers_[id] = [handler](const std::any& cmd) {
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
    } else {
        std::rethrow_exception(std::get<std::exception_ptr>(result));
    }
}

template <typename CommandType>
void CommandDispatcher::undo(const CommandID& id, const CommandType& command) {
    std::unique_lock lock(mutex_);
    auto it = undoHandlers_.find(id);
    if (it != undoHandlers_.end()) {
        it->second(command);
    }
}

template <typename CommandType>
void CommandDispatcher::redo(const CommandID& id, const CommandType& command) {
    dispatch(id, command, 0, std::nullopt).get();
}

template <typename CommandType>
auto CommandDispatcher::getCommandHistory(const CommandID& id)
    -> std::vector<CommandType> {
    std::shared_lock lock(mutex_);
    std::vector<CommandType> history;
    if (auto it = history_.find(id); it != history_.end()) {
        for (const auto& cmd : it->second) {
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
auto CommandDispatcher::quickDispatch(
    const CommandID& id, const CommandType& command) -> CommandType {
    auto future = dispatch(id, command);
    return getResult<CommandType>(future);
}
}  // namespace lithium::app

#endif  // LITHIUM_APP_COMMAND_HPP