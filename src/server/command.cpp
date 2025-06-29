#include "command.hpp"
#include "eventloop.hpp"

#include <spdlog/spdlog.h>

namespace lithium::app {

CommandDispatcher::CommandDispatcher(std::shared_ptr<EventLoop> eventLoop,
                                     const Config& config)
    : eventLoop_(std::move(eventLoop)), configuration_(config) {
    spdlog::info(
        "CommandDispatcher initialized with max_history={}, "
        "default_timeout={}ms, max_concurrent={}",
        config.maxHistorySize, config.defaultTimeout.count(),
        config.maxConcurrentCommands);
}

void CommandDispatcher::unregisterCommand(const CommandID& id) {
    std::unique_lock lock(accessMutex_);
    commandHandlers_.erase(id);
    undoHandlers_.erase(id);
    spdlog::debug("Unregistered command handler: {}", id);
}

void CommandDispatcher::recordCommandHistory(const CommandID& id,
                                             const std::any& command) {
    auto& history = commandHistory_[id];
    history.push_back(command);
    if (history.size() > configuration_.maxHistorySize) {
        history.erase(history.begin());
    }
    spdlog::trace("Recorded command history for: {} (total: {})", id,
                  history.size());
}

void CommandDispatcher::notifyEventSubscribers(const CommandID& id,
                                               const std::any& command) {
    auto subscriberIt = eventSubscribers_.find(id);
    if (subscriberIt != eventSubscribers_.end()) {
        for (const auto& [token, callback] : subscriberIt->second) {
            try {
                callback(id, command);
            } catch (const std::exception& e) {
                spdlog::warn(
                    "Event subscriber callback failed for command {}: {}", id,
                    e.what());
            }
        }
        spdlog::trace("Notified {} subscribers for command: {}",
                      subscriberIt->second.size(), id);
    }
}

int CommandDispatcher::subscribe(const CommandID& id, EventCallback callback) {
    std::unique_lock lock(accessMutex_);
    int token = nextSubscriberToken_++;
    eventSubscribers_[id][token] = std::move(callback);
    spdlog::debug("Subscribed to command events: {} (token: {})", id, token);
    return token;
}

void CommandDispatcher::unsubscribe(const CommandID& id, int token) {
    std::unique_lock lock(accessMutex_);
    auto& callbacks = eventSubscribers_[id];
    callbacks.erase(token);
    if (callbacks.empty()) {
        eventSubscribers_.erase(id);
    }
    spdlog::debug("Unsubscribed from command events: {} (token: {})", id,
                  token);
}

void CommandDispatcher::clearHistory() {
    std::unique_lock lock(accessMutex_);
    size_t totalCleared = 0;
    for (const auto& [id, history] : commandHistory_) {
        totalCleared += history.size();
    }
    commandHistory_.clear();
    spdlog::info("Cleared all command history ({} entries)", totalCleared);
}

void CommandDispatcher::clearCommandHistory(const CommandID& id) {
    std::unique_lock lock(accessMutex_);
    auto historyIt = commandHistory_.find(id);
    if (historyIt != commandHistory_.end()) {
        size_t cleared = historyIt->second.size();
        commandHistory_.erase(historyIt);
        spdlog::debug("Cleared command history for: {} ({} entries)", id,
                      cleared);
    }
}

auto CommandDispatcher::getActiveCommands() const -> std::vector<CommandID> {
    std::shared_lock lock(accessMutex_);
    std::vector<CommandID> activeCommands;
    activeCommands.reserve(commandHandlers_.size());

    for (const auto& [id, handler] : commandHandlers_) {
        auto statusIt = commandStatusMap_.find(id);
        if (statusIt != commandStatusMap_.end() &&
            (statusIt->second == CommandStatus::PENDING ||
             statusIt->second == CommandStatus::RUNNING)) {
            activeCommands.push_back(id);
        }
    }

    spdlog::trace("Retrieved {} active commands", activeCommands.size());
    return activeCommands;
}

void CommandDispatcher::cancelCommand(const CommandID& id) {
    std::unique_lock lock(accessMutex_);
    updateCommandStatus(id, CommandStatus::CANCELLED);
    spdlog::info("Command cancelled: {}", id);
}

void CommandDispatcher::setTimeout(const CommandID& id,
                                   const std::chrono::milliseconds& timeout) {
    std::unique_lock lock(accessMutex_);
    commandTimeouts_[id] = timeout;
    spdlog::debug("Set timeout for command {}: {}ms", id, timeout.count());
}

CommandStatus CommandDispatcher::getCommandStatus(const CommandID& id) const {
    std::shared_lock lock(accessMutex_);
    auto statusIt = commandStatusMap_.find(id);
    return statusIt != commandStatusMap_.end() ? statusIt->second
                                               : CommandStatus::PENDING;
}

void CommandDispatcher::updateCommandStatus(const CommandID& id,
                                            CommandStatus status) {
    auto now = std::chrono::system_clock::now();
    auto timeout = commandTimeouts_.find(id) != commandTimeouts_.end()
                       ? commandTimeouts_[id]
                       : configuration_.defaultTimeout;

    executionInfoMap_[id] = {status, now, timeout};
    commandStatusMap_[id] = status;

    // **Notify subscribers about status change**
    notifyEventSubscribers(id, static_cast<int>(status));

    if (status == CommandStatus::COMPLETED || status == CommandStatus::FAILED ||
        status == CommandStatus::CANCELLED) {
        cleanupCommandResources(id);
    }

    spdlog::trace("Updated command status: {} -> {}", id,
                  static_cast<int>(status));
}

bool CommandDispatcher::checkCommandTimeout(const CommandID& id) const {
    auto infoIt = executionInfoMap_.find(id);
    if (infoIt == executionInfoMap_.end()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - infoIt->second.startTime);

    bool timedOut = duration > infoIt->second.timeout;
    if (timedOut) {
        spdlog::warn("Command timeout detected: {} ({}ms > {}ms)", id,
                     duration.count(), infoIt->second.timeout.count());
    }

    return timedOut;
}

void CommandDispatcher::cleanupCommandResources(const CommandID& id) {
    commandTimeouts_.erase(id);
    commandStatusMap_.erase(id);
    executionInfoMap_.erase(id);

    // **Preserve last history entry for potential undo/redo operations**
    auto historyIt = commandHistory_.find(id);
    if (historyIt != commandHistory_.end() && !historyIt->second.empty()) {
        auto lastRecord = std::move(historyIt->second.back());
        historyIt->second.clear();
        historyIt->second.push_back(std::move(lastRecord));
    }

    spdlog::trace("Cleaned up resources for command: {}", id);
}

}  // namespace lithium::app
