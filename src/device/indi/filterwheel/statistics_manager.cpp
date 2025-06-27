#include "statistics_manager.hpp"

namespace lithium::device::indi::filterwheel {

StatisticsManager::StatisticsManager(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {}

bool StatisticsManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing StatisticsManager");
    
    // Initialize position usage counters for all possible slots
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    for (int i = core->getMinSlot(); i <= core->getMaxSlot(); ++i) {
        positionUsage_[i].store(0);
    }
    
    initialized_ = true;
    return true;
}

void StatisticsManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down StatisticsManager");
    }
    
    if (sessionActive_) {
        endSession();
    }
    
    initialized_ = false;
}

void StatisticsManager::recordPositionChange(int fromPosition, int toPosition) {
    auto core = getCore();
    if (!core || !initialized_) {
        return;
    }

    if (fromPosition == toPosition) {
        return; // No actual change
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    // Record total position changes
    totalPositionChanges_.fetch_add(1);
    
    // Record usage for the destination position
    if (positionUsage_.find(toPosition) != positionUsage_.end()) {
        positionUsage_[toPosition].fetch_add(1);
    }
    
    // Record session statistics
    if (sessionActive_) {
        sessionPositionChanges_.fetch_add(1);
    }
    
    core->getLogger()->debug("Recorded position change: {} -> {}", fromPosition, toPosition);
}

void StatisticsManager::recordMoveTime(std::chrono::milliseconds duration) {
    auto core = getCore();
    if (!core || !initialized_) {
        return;
    }

    totalMoveTimeMs_.fetch_add(duration.count());
    
    core->getLogger()->debug("Recorded move time: {} ms", duration.count());
}

void StatisticsManager::startSession() {
    auto core = getCore();
    if (!core) {
        return;
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    sessionStartTime_ = std::chrono::steady_clock::now();
    sessionPositionChanges_.store(0);
    sessionActive_ = true;
    
    core->getLogger()->info("Statistics session started");
}

void StatisticsManager::endSession() {
    auto core = getCore();
    if (!core) {
        return;
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    if (sessionActive_) {
        sessionEndTime_ = std::chrono::steady_clock::now();
        sessionActive_ = false;
        
        auto duration = getSessionDuration();
        core->getLogger()->info("Statistics session ended. Duration: {:.2f} seconds, Changes: {}", 
                               duration.count(), sessionPositionChanges_.load());
    }
}

uint64_t StatisticsManager::getTotalPositionChanges() const {
    return totalPositionChanges_.load();
}

uint64_t StatisticsManager::getPositionUsageCount(int position) const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    auto it = positionUsage_.find(position);
    if (it != positionUsage_.end()) {
        return it->second.load();
    }
    return 0;
}

std::chrono::milliseconds StatisticsManager::getAverageMoveTime() const {
    uint64_t totalChanges = totalPositionChanges_.load();
    if (totalChanges == 0) {
        return std::chrono::milliseconds(0);
    }
    
    uint64_t totalMs = totalMoveTimeMs_.load();
    return std::chrono::milliseconds(totalMs / totalChanges);
}

std::chrono::milliseconds StatisticsManager::getTotalMoveTime() const {
    return std::chrono::milliseconds(totalMoveTimeMs_.load());
}

uint64_t StatisticsManager::getSessionPositionChanges() const {
    return sessionPositionChanges_.load();
}

std::chrono::duration<double> StatisticsManager::getSessionDuration() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    if (!sessionActive_) {
        return sessionEndTime_ - sessionStartTime_;
    } else {
        return std::chrono::steady_clock::now() - sessionStartTime_;
    }
}

bool StatisticsManager::resetStatistics() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    totalPositionChanges_.store(0);
    totalMoveTimeMs_.store(0);
    
    for (auto& pair : positionUsage_) {
        pair.second.store(0);
    }
    
    core->getLogger()->info("All statistics reset");
    return true;
}

bool StatisticsManager::resetSessionStatistics() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    sessionPositionChanges_.store(0);
    if (sessionActive_) {
        sessionStartTime_ = std::chrono::steady_clock::now();
    }
    
    core->getLogger()->info("Session statistics reset");
    return true;
}

int StatisticsManager::getMostUsedPosition() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    int mostUsed = 1;
    uint64_t maxUsage = 0;
    
    for (const auto& pair : positionUsage_) {
        uint64_t usage = pair.second.load();
        if (usage > maxUsage) {
            maxUsage = usage;
            mostUsed = pair.first;
        }
    }
    
    return mostUsed;
}

int StatisticsManager::getLeastUsedPosition() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    int leastUsed = 1;
    uint64_t minUsage = UINT64_MAX;
    
    for (const auto& pair : positionUsage_) {
        uint64_t usage = pair.second.load();
        if (usage < minUsage) {
            minUsage = usage;
            leastUsed = pair.first;
        }
    }
    
    return leastUsed;
}

std::unordered_map<int, uint64_t> StatisticsManager::getPositionUsageMap() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    std::unordered_map<int, uint64_t> result;
    for (const auto& pair : positionUsage_) {
        result[pair.first] = pair.second.load();
    }
    
    return result;
}

}  // namespace lithium::device::indi::filterwheel
