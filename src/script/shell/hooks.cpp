/*
 * hooks.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "hooks.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace lithium::shell {

HookManager::HookManager() {
    spdlog::debug("HookManager initialized");
}

HookManager::~HookManager() {
    spdlog::debug("HookManager destroyed, total history entries: {}",
                  executionHistory_.size());
}

bool HookManager::addPreHook(std::string_view hookId, PreHook hook) {
    if (hookId.empty()) {
        spdlog::warn("Cannot add pre-hook with empty ID");
        throw std::invalid_argument("Hook ID cannot be empty");
    }

    std::unique_lock<std::shared_mutex> lock(hooksMutex_);

    const auto hookIdStr = std::string(hookId);
    if (preHooks_.find(hookIdStr) != preHooks_.end()) {
        spdlog::warn("Pre-hook with ID '{}' already exists", hookId);
        return false;
    }

    preHooks_[hookIdStr] = hook;
    spdlog::debug("Added pre-hook: {}", hookId);
    return true;
}

bool HookManager::addPostHook(std::string_view hookId, PostHook hook) {
    if (hookId.empty()) {
        spdlog::warn("Cannot add post-hook with empty ID");
        throw std::invalid_argument("Hook ID cannot be empty");
    }

    std::unique_lock<std::shared_mutex> lock(hooksMutex_);

    const auto hookIdStr = std::string(hookId);
    if (postHooks_.find(hookIdStr) != postHooks_.end()) {
        spdlog::warn("Post-hook with ID '{}' already exists", hookId);
        return false;
    }

    postHooks_[hookIdStr] = hook;
    spdlog::debug("Added post-hook: {}", hookId);
    return true;
}

bool HookManager::removeHook(std::string_view hookId) {
    return removePreHook(hookId) || removePostHook(hookId);
}

bool HookManager::removePreHook(std::string_view hookId) {
    std::unique_lock<std::shared_mutex> lock(hooksMutex_);

    const auto hookIdStr = std::string(hookId);
    const auto it = preHooks_.find(hookIdStr);
    if (it != preHooks_.end()) {
        preHooks_.erase(it);
        spdlog::debug("Removed pre-hook: {}", hookId);
        return true;
    }

    return false;
}

bool HookManager::removePostHook(std::string_view hookId) {
    std::unique_lock<std::shared_mutex> lock(hooksMutex_);

    const auto hookIdStr = std::string(hookId);
    const auto it = postHooks_.find(hookIdStr);
    if (it != postHooks_.end()) {
        postHooks_.erase(it);
        spdlog::debug("Removed post-hook: {}", hookId);
        return true;
    }

    return false;
}

std::vector<HookResult> HookManager::executePreHooks(std::string_view scriptId) {
    std::vector<HookResult> results;

    if (!enabled_.load(std::memory_order_acquire)) {
        spdlog::debug("Hooks are disabled, skipping pre-hook execution for script: {}",
                      scriptId);
        return results;
    }

    std::shared_lock<std::shared_mutex> lock(hooksMutex_);

    for (const auto& [hookId, hook] : preHooks_) {
        const auto start = std::chrono::high_resolution_clock::now();
        HookResult result;
        result.hookId = hookId;
        result.scriptId = std::string(scriptId);
        result.hookType = "pre";
        result.timestamp = std::chrono::system_clock::now();

        try {
            hook(result.scriptId);
            result.success = true;
            spdlog::debug("Pre-hook '{}' executed successfully for script: {}",
                          hookId, scriptId);
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
            spdlog::error("Pre-hook '{}' failed for script '{}': {}", hookId, scriptId,
                          e.what());
        } catch (...) {
            result.success = false;
            result.errorMessage = "Unknown error";
            spdlog::error("Pre-hook '{}' failed for script '{}' with unknown error",
                          hookId, scriptId);
        }

        const auto end = std::chrono::high_resolution_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);

        results.push_back(result);
        recordHookExecution(result);
    }

    spdlog::debug("Executed {} pre-hooks for script: {}", results.size(), scriptId);
    return results;
}

std::vector<HookResult> HookManager::executePostHooks(std::string_view scriptId,
                                                      int exitCode) {
    std::vector<HookResult> results;

    if (!enabled_.load(std::memory_order_acquire)) {
        spdlog::debug(
            "Hooks are disabled, skipping post-hook execution for script: {}",
            scriptId);
        return results;
    }

    std::shared_lock<std::shared_mutex> lock(hooksMutex_);

    for (const auto& [hookId, hook] : postHooks_) {
        const auto start = std::chrono::high_resolution_clock::now();
        HookResult result;
        result.hookId = hookId;
        result.scriptId = std::string(scriptId);
        result.hookType = "post";
        result.timestamp = std::chrono::system_clock::now();

        try {
            hook(result.scriptId, exitCode);
            result.success = true;
            spdlog::debug(
                "Post-hook '{}' executed successfully for script: {} "
                "(exit code: {})",
                hookId, scriptId, exitCode);
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
            spdlog::error(
                "Post-hook '{}' failed for script '{}' (exit code: {}): {}", hookId,
                scriptId, exitCode, e.what());
        } catch (...) {
            result.success = false;
            result.errorMessage = "Unknown error";
            spdlog::error(
                "Post-hook '{}' failed for script '{}' (exit code: {}) "
                "with unknown error",
                hookId, scriptId, exitCode);
        }

        const auto end = std::chrono::high_resolution_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);

        results.push_back(result);
        recordHookExecution(result);
    }

    spdlog::debug("Executed {} post-hooks for script: {} (exit code: {})",
                  results.size(), scriptId, exitCode);
    return results;
}

size_t HookManager::getPreHookCount() const {
    std::shared_lock<std::shared_mutex> lock(hooksMutex_);
    return preHooks_.size();
}

size_t HookManager::getPostHookCount() const {
    std::shared_lock<std::shared_mutex> lock(hooksMutex_);
    return postHooks_.size();
}

bool HookManager::hasHook(std::string_view hookId) const {
    std::shared_lock<std::shared_mutex> lock(hooksMutex_);
    const auto hookIdStr = std::string(hookId);
    return preHooks_.find(hookIdStr) != preHooks_.end() ||
           postHooks_.find(hookIdStr) != postHooks_.end();
}

void HookManager::clearAllHooks() {
    std::unique_lock<std::shared_mutex> lock(hooksMutex_);
    preHooks_.clear();
    postHooks_.clear();
    spdlog::debug("All hooks cleared");
}

void HookManager::clearPreHooks() {
    std::unique_lock<std::shared_mutex> lock(hooksMutex_);
    preHooks_.clear();
    spdlog::debug("All pre-hooks cleared");
}

void HookManager::clearPostHooks() {
    std::unique_lock<std::shared_mutex> lock(hooksMutex_);
    postHooks_.clear();
    spdlog::debug("All post-hooks cleared");
}

std::vector<HookResult> HookManager::getExecutionHistory(size_t maxEntries) const {
    std::shared_lock<std::shared_mutex> lock(historyMutex_);

    std::vector<HookResult> result;
    const size_t startIdx = maxEntries == 0 ? 0
                                             : (executionHistory_.size() > maxEntries
                                                    ? executionHistory_.size() - maxEntries
                                                    : 0);

    for (size_t i = startIdx; i < executionHistory_.size(); ++i) {
        result.push_back(executionHistory_[i]);
    }

    // Return in reverse chronological order (most recent first)
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<HookResult> HookManager::getScriptHistory(std::string_view scriptId,
                                                      size_t maxEntries) const {
    std::shared_lock<std::shared_mutex> lock(historyMutex_);

    std::vector<HookResult> result;
    const auto scriptIdStr = std::string(scriptId);
    size_t count = 0;

    // Iterate in reverse to get most recent entries first
    for (auto it = executionHistory_.rbegin(); it != executionHistory_.rend();
         ++it) {
        if (it->scriptId == scriptIdStr) {
            result.push_back(*it);
            ++count;
            if (maxEntries > 0 && count >= maxEntries) {
                break;
            }
        }
    }

    return result;
}

void HookManager::clearHistory() {
    std::unique_lock<std::shared_mutex> lock(historyMutex_);
    executionHistory_.clear();
    spdlog::debug("Hook execution history cleared");
}

size_t HookManager::getHistorySize() const {
    std::shared_lock<std::shared_mutex> lock(historyMutex_);
    return executionHistory_.size();
}

void HookManager::setEnabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
    spdlog::debug("Hooks {}", enabled ? "enabled" : "disabled");
}

bool HookManager::isEnabled() const {
    return enabled_.load(std::memory_order_acquire);
}

void HookManager::recordHookExecution(const HookResult& result) {
    std::unique_lock<std::shared_mutex> lock(historyMutex_);

    executionHistory_.push_back(result);

    // Maintain maximum history size
    if (executionHistory_.size() > MAX_HISTORY_SIZE) {
        // Remove oldest entries (front of the vector)
        executionHistory_.erase(executionHistory_.begin(),
                                executionHistory_.begin() +
                                    (executionHistory_.size() - MAX_HISTORY_SIZE));
        spdlog::trace("Hook execution history pruned to {} entries",
                      MAX_HISTORY_SIZE);
    }
}

}  // namespace lithium::shell
