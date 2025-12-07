/*
 * resource_limits.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file resource_limits.cpp
 * @brief Resource limit management implementation
 * @date 2024-1-13
 */

#include "resource_limits.hpp"

#include <atomic>
#include <mutex>

#include <spdlog/spdlog.h>

namespace lithium::shell {

class ResourceManager::Impl {
public:
    // Limits
    std::atomic<size_t> maxMemoryMB_{1024};
    std::atomic<int> maxCpuPercent_{100};
    std::chrono::seconds maxExecutionTime_{3600};
    std::atomic<size_t> maxOutputSize_{10 * 1024 * 1024};
    std::atomic<int> maxConcurrent_{4};

    // Current usage
    std::atomic<size_t> currentMemoryMB_{0};
    std::atomic<double> currentCpuPercent_{0.0};
    std::atomic<size_t> runningCount_{0};
    std::atomic<size_t> totalScripts_{0};
    std::atomic<size_t> outputSizeBytes_{0};

    mutable std::mutex timeMutex_;
};

ResourceManager::ResourceManager() : pImpl_(std::make_unique<Impl>()) {}

ResourceManager::ResourceManager(size_t maxMemoryMB, int maxCpuPercent,
                                 std::chrono::seconds maxExecutionTime,
                                 size_t maxOutputSize, int maxConcurrent)
    : pImpl_(std::make_unique<Impl>()) {
    pImpl_->maxMemoryMB_ = maxMemoryMB;
    pImpl_->maxCpuPercent_ = maxCpuPercent;
    pImpl_->maxExecutionTime_ = maxExecutionTime;
    pImpl_->maxOutputSize_ = maxOutputSize;
    pImpl_->maxConcurrent_ = maxConcurrent;
}

ResourceManager::~ResourceManager() = default;

ResourceManager::ResourceManager(ResourceManager&&) noexcept = default;
ResourceManager& ResourceManager::operator=(ResourceManager&&) noexcept =
    default;

auto ResourceManager::canExecute() const -> bool {
    auto running = pImpl_->runningCount_.load();
    auto maxConcurrent = pImpl_->maxConcurrent_.load();
    auto currentMem = pImpl_->currentMemoryMB_.load();
    auto maxMem = pImpl_->maxMemoryMB_.load();

    return running < static_cast<size_t>(maxConcurrent) && currentMem < maxMem;
}

auto ResourceManager::acquire() -> bool {
    auto running = pImpl_->runningCount_.load();
    auto maxConcurrent = pImpl_->maxConcurrent_.load();

    if (running >= static_cast<size_t>(maxConcurrent)) {
        spdlog::warn("ResourceManager: max concurrent scripts reached ({}/{})",
                     running, maxConcurrent);
        return false;
    }

    pImpl_->runningCount_++;
    spdlog::debug("ResourceManager: acquired resource (now {} running)",
                  pImpl_->runningCount_.load());
    return true;
}

void ResourceManager::release() {
    if (pImpl_->runningCount_ > 0) {
        pImpl_->runningCount_--;
        spdlog::debug("ResourceManager: released resource (now {} running)",
                      pImpl_->runningCount_.load());
    }
}

auto ResourceManager::getUsage() const -> ResourceUsage {
    ResourceUsage usage;
    usage.currentMemoryMB = pImpl_->currentMemoryMB_.load();
    usage.cpuPercent = pImpl_->currentCpuPercent_.load();
    usage.runningScripts = pImpl_->runningCount_.load();
    usage.totalScripts = pImpl_->totalScripts_.load();
    usage.outputSizeBytes = pImpl_->outputSizeBytes_.load();
    return usage;
}

auto ResourceManager::getUsageMap() const
    -> std::unordered_map<std::string, double> {
    auto usage = getUsage();
    return {{"running_scripts", static_cast<double>(usage.runningScripts)},
            {"total_scripts", static_cast<double>(usage.totalScripts)},
            {"memory_usage_mb", static_cast<double>(usage.currentMemoryMB)},
            {"memory_usage_percent",
             (static_cast<double>(usage.currentMemoryMB) /
              static_cast<double>(pImpl_->maxMemoryMB_.load())) *
                 100.0},
            {"cpu_percent", usage.cpuPercent},
            {"output_size_bytes", static_cast<double>(usage.outputSizeBytes)}};
}

void ResourceManager::setMaxMemory(size_t megabytes) {
    pImpl_->maxMemoryMB_ = megabytes;
    spdlog::debug("ResourceManager: set max memory to {}MB", megabytes);
}

auto ResourceManager::getMaxMemory() const -> size_t {
    return pImpl_->maxMemoryMB_.load();
}

void ResourceManager::setMaxCpuPercent(int percent) {
    pImpl_->maxCpuPercent_ = std::clamp(percent, 0, 100);
    spdlog::debug("ResourceManager: set max CPU to {}%", percent);
}

auto ResourceManager::getMaxCpuPercent() const -> int {
    return pImpl_->maxCpuPercent_.load();
}

void ResourceManager::setMaxExecutionTime(std::chrono::seconds duration) {
    std::lock_guard lock(pImpl_->timeMutex_);
    pImpl_->maxExecutionTime_ = duration;
    spdlog::debug("ResourceManager: set max execution time to {}s",
                  duration.count());
}

auto ResourceManager::getMaxExecutionTime() const -> std::chrono::seconds {
    std::lock_guard lock(pImpl_->timeMutex_);
    return pImpl_->maxExecutionTime_;
}

void ResourceManager::setMaxOutputSize(size_t bytes) {
    pImpl_->maxOutputSize_ = bytes;
    spdlog::debug("ResourceManager: set max output size to {} bytes", bytes);
}

auto ResourceManager::getMaxOutputSize() const -> size_t {
    return pImpl_->maxOutputSize_.load();
}

void ResourceManager::setMaxConcurrent(int count) {
    pImpl_->maxConcurrent_ = std::max(1, count);
    spdlog::debug("ResourceManager: set max concurrent to {}", count);
}

auto ResourceManager::getMaxConcurrent() const -> int {
    return pImpl_->maxConcurrent_.load();
}

auto ResourceManager::getRunningCount() const -> size_t {
    return pImpl_->runningCount_.load();
}

void ResourceManager::updateMemoryUsage(size_t megabytes) {
    pImpl_->currentMemoryMB_ = megabytes;
}

void ResourceManager::updateCpuUsage(double percent) {
    pImpl_->currentCpuPercent_ = percent;
}

auto ResourceManager::isMemoryExceeded() const -> bool {
    return pImpl_->currentMemoryMB_.load() >= pImpl_->maxMemoryMB_.load();
}

auto ResourceManager::isCpuExceeded() const -> bool {
    return pImpl_->currentCpuPercent_.load() >=
           static_cast<double>(pImpl_->maxCpuPercent_.load());
}

}  // namespace lithium::shell
