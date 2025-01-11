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

class EventLoop;

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

    void updateCommandStatus(const CommandID& id, CommandStatus status);
    bool checkTimeout(const CommandID& id) const;
    void cleanupCommand(const CommandID& id);
};
}  // namespace lithium::app

#endif  // LITHIUM_APP_COMMAND_HPP