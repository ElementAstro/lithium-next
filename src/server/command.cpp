#include "command.hpp"
#include "eventloop.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::app {
CommandDispatcher::CommandDispatcher(std::shared_ptr<EventLoop> eventLoop,
                                     const Config& config)
    : eventLoop_(std::move(eventLoop)), config_(config) {
    LOG_F(INFO, "CommandDispatcher initialized with custom config");
}

void CommandDispatcher::unregisterCommand(const CommandID& id) {
    std::unique_lock lock(mutex_);
    handlers_.erase(id);
    undoHandlers_.erase(id);
    LOG_F(INFO, "Unregistered command: {}", id);
}

void CommandDispatcher::recordHistory(const CommandID& id,
                                      const std::any& command) {
    auto& commandHistory = history_[id];
    commandHistory.push_back(command);
    if (commandHistory.size() > maxHistorySize_) {
        commandHistory.erase(commandHistory.begin());
    }
    LOG_F(INFO, "Recorded history for command: {}", id);
}

void CommandDispatcher::notifySubscribers(const CommandID& id,
                                          const std::any& command) {
    auto it = subscribers_.find(id);
    if (it != subscribers_.end()) {
        for (auto& [_, callback] : it->second) {
            callback(id, command);
        }
        LOG_F(INFO, "Notified subscribers for command: {}", id);
    }
}

int CommandDispatcher::subscribe(const CommandID& id, EventCallback callback) {
    std::unique_lock lock(mutex_);
    int token = nextSubscriberId_++;
    subscribers_[id][token] = std::move(callback);
    LOG_F(INFO, "Subscribed to command: {} with token: {}", id, token);
    return token;
}

void CommandDispatcher::unsubscribe(const CommandID& id, int token) {
    std::unique_lock lock(mutex_);
    auto& callbacks = subscribers_[id];
    callbacks.erase(token);
    if (callbacks.empty()) {
        subscribers_.erase(id);
    }
    LOG_F(INFO, "Unsubscribed from command: {} with token: {}", id, token);
}

void CommandDispatcher::clearHistory() {
    std::unique_lock lock(mutex_);
    history_.clear();
    LOG_F(INFO, "Cleared all command history");
}

void CommandDispatcher::clearCommandHistory(const CommandID& id) {
    std::unique_lock lock(mutex_);
    history_.erase(id);
    LOG_F(INFO, "Cleared history for command: {}", id);
}

auto CommandDispatcher::getActiveCommands() const -> std::vector<CommandID> {
    std::shared_lock lock(mutex_);
    std::vector<CommandID> activeCommands;
    activeCommands.reserve(handlers_.size());
    for (const auto& [id, _] : handlers_) {
        activeCommands.push_back(id);
    }
    LOG_F(INFO, "Retrieved active commands");
    return activeCommands;
}

void CommandDispatcher::cancelCommand(const CommandID& id) {
    std::unique_lock lock(mutex_);
    updateCommandStatus(id, CommandStatus::CANCELLED);
    LOG_F(INFO, "Command cancelled: {}", id);
}

void CommandDispatcher::setTimeout(const CommandID& id,
                                   const std::chrono::milliseconds& timeout) {
    std::unique_lock lock(mutex_);
    timeouts_[id] = timeout;
}

CommandStatus CommandDispatcher::getCommandStatus(const CommandID& id) const {
    std::shared_lock lock(mutex_);
    auto it = commandStatus_.find(id);
    return it != commandStatus_.end() ? it->second : CommandStatus::PENDING;
}

void CommandDispatcher::updateCommandStatus(const CommandID& id,
                                            CommandStatus status) {
    auto now = std::chrono::system_clock::now();
    auto timeout = timeouts_.find(id) != timeouts_.end()
                       ? timeouts_[id]
                       : config_.defaultTimeout;

    commandInfo_[id] = {status, now, timeout};

    commandStatus_[id] = status;
    notifySubscribers(id, status);

    if (status == CommandStatus::COMPLETED || status == CommandStatus::FAILED ||
        status == CommandStatus::CANCELLED) {
        cleanupCommand(id);
    }
}

bool CommandDispatcher::checkTimeout(const CommandID& id) const {
    auto it = commandInfo_.find(id);
    if (it == commandInfo_.end()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.startTime);

    return duration > it->second.timeout;
}

void CommandDispatcher::cleanupCommand(const CommandID& id) {
    timeouts_.erase(id);
    commandStatus_.erase(id);
    commandInfo_.erase(id);

    // 清理相关的内存和资源
    auto historyIt = history_.find(id);
    if (historyIt != history_.end() && !historyIt->second.empty()) {
        // 保留最后一条历史记录
        auto lastRecord = historyIt->second.back();
        historyIt->second.clear();
        historyIt->second.push_back(std::move(lastRecord));
    }

    // 清理优先队列中的相关命令
    std::priority_queue<std::pair<int, CommandID>> tempQueue;
    while (!priorityQueue_.empty()) {
        auto item = priorityQueue_.top();
        priorityQueue_.pop();
        if (item.second != id) {
            tempQueue.push(item);
        }
    }
    priorityQueue_ = std::move(tempQueue);

    LOG_F(INFO, "Cleaned up resources for command: {}", id);
}

}  // namespace lithium::app
