#ifndef LITHIUM_APP_COMMAND_HPP
#define LITHIUM_APP_COMMAND_HPP

#include <any>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <chrono>
#include <queue>

namespace lithium::app {

// 新增命令执行状态枚举
enum class CommandStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

// 新增命令执行结果结构
struct CommandResult {
    CommandStatus status;
    std::any result;
    std::string error;
    std::chrono::system_clock::time_point timestamp;
};

class EventLoop;

class CommandDispatcher {
public:
    using CommandID = std::string;
    using CommandHandler = std::function<void(const std::any&)>;
    using ResultType = std::variant<std::any, std::exception_ptr>;
    using CommandCallback =
        std::function<void(const CommandID&, const ResultType&)>;
    using EventCallback =
        std::function<void(const CommandID&, const std::any&)>;

    // 新增配置结构
    struct Config {
        std::size_t maxHistorySize = 100;
        std::chrono::milliseconds defaultTimeout{5000};
        size_t maxConcurrentCommands = 100;
        bool enablePriority = true;
    };

    explicit CommandDispatcher(std::shared_ptr<EventLoop> eventLoop, 
                             const Config& config);

    template <typename CommandType>
    void registerCommand(const CommandID& id,
                         std::function<void(const CommandType&)> handler,
                         std::optional<std::function<void(const CommandType&)>>
                             undoHandler = std::nullopt);

    void unregisterCommand(const CommandID& id);

    template <typename CommandType>
    auto dispatch(
        const CommandID& id, const CommandType& command, int priority = 0,
        std::optional<std::chrono::milliseconds> delay = std::nullopt,
        CommandCallback callback = nullptr) -> std::future<ResultType>;

    // 批量执行命令
    template <typename CommandType>
    auto batchDispatch(
        const std::vector<std::pair<CommandID, CommandType>>& commands,
        int priority = 0) -> std::vector<std::future<ResultType>>;

    // 取消正在执行的命令
    void cancelCommand(const CommandID& id);

    // 获取命令执行状态
    CommandStatus getCommandStatus(const CommandID& id) const;

    // 设置命令执行超时
    void setTimeout(const CommandID& id, 
                   const std::chrono::milliseconds& timeout);

    // 简化的快速执行接口
    template <typename CommandType>
    auto quickDispatch(const CommandID& id, 
                      const CommandType& command) -> CommandType;

    template <typename CommandType>
    auto getResult(std::future<ResultType>& resultFuture) -> CommandType;

    template <typename CommandType>
    void undo(const CommandID& id, const CommandType& command);

    template <typename CommandType>
    void redo(const CommandID& id, const CommandType& command);

    int subscribe(const CommandID& id, EventCallback callback);
    void unsubscribe(const CommandID& id, int token);

    template <typename CommandType>
    auto getCommandHistory(const CommandID& id) -> std::vector<CommandType>;

    void clearHistory();
    void clearCommandHistory(const CommandID& id);
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

    // 新增成员
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
