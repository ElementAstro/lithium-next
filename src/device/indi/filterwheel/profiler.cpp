#include "profiler.hpp"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <mutex>

namespace lithium::device::indi::filterwheel {

FilterWheelProfiler::FilterWheelProfiler(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {
    moveHistory_.reserve(MAX_HISTORY_SIZE);
}

bool FilterWheelProfiler::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing FilterWheelProfiler");

    // Clear any existing data
    resetProfileData();

    core->getLogger()->info("FilterWheelProfiler initialized - continuous profiling enabled");

    initialized_ = true;
    return true;
}

void FilterWheelProfiler::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down FilterWheelProfiler");

        // Log final statistics
        if (!moveHistory_.empty()) {
            auto stats = getPerformanceStats();
            core->getLogger()->info("Final profiling stats: {} moves, {:.2f}% success rate, avg {:.0f}ms",
                                   stats.totalMoves, stats.successRate,
                                   static_cast<double>(stats.averageMoveTime.count()));
        }
    }

    profilingEnabled_ = false;
    initialized_ = false;
}

void FilterWheelProfiler::startMove(int fromSlot, int toSlot) {
    if (!profilingEnabled_ || !initialized_) {
        return;
    }

    auto core = getCore();
    if (!core) {
        return;
    }

    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    moveStartTime_ = std::chrono::steady_clock::now();
    moveFromSlot_ = fromSlot;
    moveToSlot_ = toSlot;
    moveInProgress_ = true;

    core->getLogger()->debug("Profiler: Started move {} -> {}", fromSlot, toSlot);
}

void FilterWheelProfiler::completeMove(bool success, int actualSlot) {
    if (!profilingEnabled_ || !initialized_ || !moveInProgress_) {
        return;
    }

    auto core = getCore();
    if (!core) {
        return;
    }

    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - moveStartTime_);

    FilterProfileData data;
    data.fromSlot = moveFromSlot_;
    data.toSlot = moveToSlot_;
    data.duration = duration;
    data.success = success && (actualSlot == moveToSlot_);
    data.timestamp = std::chrono::system_clock::now();

    // Get temperature if available (would need to query TemperatureManager)
    data.temperature = 0.0; // Placeholder

    moveHistory_.push_back(data);

    // Prune old data if necessary
    if (moveHistory_.size() > MAX_HISTORY_SIZE) {
        pruneOldData();
    }

    moveInProgress_ = false;

    core->getLogger()->debug("Profiler: Completed move {} -> {} in {}ms (success: {})",
                            moveFromSlot_, actualSlot, duration.count(), success);

    // Check for performance issues
    if (hasPerformanceDegraded()) {
        logPerformanceAlert("Performance degradation detected");
    }
}

std::chrono::milliseconds FilterWheelProfiler::predictMoveDuration(int fromSlot, int toSlot) const {
    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    // First try to get specific slot-to-slot average
    auto specificAverage = calculateSlotAverage(fromSlot, toSlot);
    if (specificAverage.count() > 0) {
        return specificAverage;
    }

    // Fall back to overall average
    auto overallAverage = calculateAverageTime();
    if (overallAverage.count() > 0) {
        return overallAverage;
    }

    // Default estimate based on slot distance
    int distance = std::abs(toSlot - fromSlot);
    return std::chrono::milliseconds(1000 + distance * 500); // Base 1s + 500ms per slot
}

FilterPerformanceStats FilterWheelProfiler::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    FilterPerformanceStats stats;

    if (moveHistory_.empty()) {
        return stats;
    }

    stats.totalMoves = moveHistory_.size();
    stats.successRate = calculateSuccessRate();
    stats.averageMoveTime = calculateAverageTime();

    // Find fastest and slowest moves
    for (const auto& move : moveHistory_) {
        if (move.success) {
            if (move.duration < stats.fastestMove) {
                stats.fastestMove = move.duration;
            }
            if (move.duration > stats.slowestMove) {
                stats.slowestMove = move.duration;
            }
        }
    }

    // Calculate per-slot averages
    std::unordered_map<std::string, std::vector<std::chrono::milliseconds>> slotPairs;
    for (const auto& move : moveHistory_) {
        if (move.success) {
            std::string key = std::to_string(move.fromSlot) + "->" + std::to_string(move.toSlot);
            slotPairs[key].push_back(move.duration);
        }
    }

    for (const auto& [key, durations] : slotPairs) {
        if (!durations.empty()) {
            auto sum = std::accumulate(durations.begin(), durations.end(), std::chrono::milliseconds(0));
            auto average = sum / static_cast<int>(durations.size());
            // Extract to and from slots (simplified for now)
            stats.slotAverages[0] = average; // Placeholder
        }
    }

    // Get recent moves
    size_t recentStart = moveHistory_.size() > RECENT_MOVES_COUNT ?
                        moveHistory_.size() - RECENT_MOVES_COUNT : 0;
    stats.recentMoves.assign(moveHistory_.begin() + recentStart, moveHistory_.end());

    return stats;
}

std::vector<FilterProfileData> FilterWheelProfiler::getSlotTransitionData(int fromSlot, int toSlot) const {
    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    std::vector<FilterProfileData> result;

    for (const auto& move : moveHistory_) {
        if (move.fromSlot == fromSlot && move.toSlot == toSlot) {
            result.push_back(move);
        }
    }

    return result;
}

bool FilterWheelProfiler::hasPerformanceDegraded() const {
    if (moveHistory_.size() < 50) {
        return false; // Not enough data
    }

    return detectPerformanceTrend();
}

std::vector<std::string> FilterWheelProfiler::getOptimizationRecommendations() const {
    std::vector<std::string> recommendations;
    auto stats = getPerformanceStats();

    if (stats.successRate < 95.0) {
        recommendations.push_back("Success rate is below 95% - consider filter wheel maintenance");
    }

    if (stats.averageMoveTime > std::chrono::milliseconds(5000)) {
        recommendations.push_back("Average move time is high - check for mechanical issues");
    }

    if (stats.slowestMove > std::chrono::milliseconds(10000)) {
        recommendations.push_back("Some moves are very slow - consider lubrication or calibration");
    }

    if (hasPerformanceDegraded()) {
        recommendations.push_back("Performance degradation detected - schedule maintenance");
    }

    if (recommendations.empty()) {
        recommendations.push_back("Filter wheel performance is optimal");
    }

    return recommendations;
}

void FilterWheelProfiler::resetProfileData() {
    std::lock_guard<std::mutex> lock(dataAccessMutex_);

    moveHistory_.clear();
    moveInProgress_ = false;

    auto core = getCore();
    if (core) {
        core->getLogger()->info("Profiler data reset");
    }
}

bool FilterWheelProfiler::exportToCSV(const std::string& filePath) const {
    auto core = getCore();
    if (!core) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(dataAccessMutex_);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            core->getLogger()->error("Failed to open file for export: {}", filePath);
            return false;
        }

        // Write CSV header
        file << "Timestamp,FromSlot,ToSlot,Duration(ms),Success,Temperature\n";

        // Write data
        for (const auto& move : moveHistory_) {
            auto time_t = std::chrono::system_clock::to_time_t(move.timestamp);
            auto tm = *std::localtime(&time_t);

            file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ","
                 << move.fromSlot << ","
                 << move.toSlot << ","
                 << move.duration.count() << ","
                 << (move.success ? "true" : "false") << ","
                 << std::fixed << std::setprecision(2) << move.temperature << "\n";
        }

        core->getLogger()->info("Profiler data exported to: {}", filePath);
        return true;
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to export profiler data: {}", e.what());
        return false;
    }
}

void FilterWheelProfiler::pruneOldData() {
    // Keep only the most recent MAX_HISTORY_SIZE entries
    if (moveHistory_.size() > MAX_HISTORY_SIZE) {
        size_t removeCount = moveHistory_.size() - MAX_HISTORY_SIZE;
        moveHistory_.erase(moveHistory_.begin(), moveHistory_.begin() + removeCount);
    }
}

double FilterWheelProfiler::calculateSuccessRate() const {
    if (moveHistory_.empty()) {
        return 100.0;
    }

    size_t successCount = std::count_if(moveHistory_.begin(), moveHistory_.end(),
                                       [](const FilterProfileData& data) { return data.success; });

    return (static_cast<double>(successCount) / moveHistory_.size()) * 100.0;
}

std::chrono::milliseconds FilterWheelProfiler::calculateAverageTime() const {
    if (moveHistory_.empty()) {
        return std::chrono::milliseconds(0);
    }

    auto total = std::accumulate(moveHistory_.begin(), moveHistory_.end(),
                                std::chrono::milliseconds(0),
                                [](std::chrono::milliseconds sum, const FilterProfileData& data) {
                                    return data.success ? sum + data.duration : sum;
                                });

    size_t successCount = std::count_if(moveHistory_.begin(), moveHistory_.end(),
                                       [](const FilterProfileData& data) { return data.success; });

    return successCount > 0 ? total / static_cast<int>(successCount) : std::chrono::milliseconds(0);
}

std::chrono::milliseconds FilterWheelProfiler::calculateSlotAverage(int fromSlot, int toSlot) const {
    std::vector<std::chrono::milliseconds> durations;

    for (const auto& move : moveHistory_) {
        if (move.fromSlot == fromSlot && move.toSlot == toSlot && move.success) {
            durations.push_back(move.duration);
        }
    }

    if (durations.empty()) {
        return std::chrono::milliseconds(0);
    }

    auto total = std::accumulate(durations.begin(), durations.end(), std::chrono::milliseconds(0));
    return total / static_cast<int>(durations.size());
}

bool FilterWheelProfiler::detectPerformanceTrend() const {
    if (moveHistory_.size() < 100) {
        return false;
    }

    // Compare recent performance to historical average
    size_t recentStart = moveHistory_.size() - 50;
    auto recentMoves = std::vector<FilterProfileData>(moveHistory_.begin() + recentStart, moveHistory_.end());

    auto recentAverage = std::accumulate(recentMoves.begin(), recentMoves.end(),
                                        std::chrono::milliseconds(0),
                                        [](std::chrono::milliseconds sum, const FilterProfileData& data) {
                                            return data.success ? sum + data.duration : sum;
                                        }) / static_cast<int>(recentMoves.size());

    auto overallAverage = calculateAverageTime();

    // Flag degradation if recent moves are 20% slower than overall average
    return recentAverage > overallAverage * 1.2;
}

void FilterWheelProfiler::logPerformanceAlert(const std::string& message) const {
    auto core = getCore();
    if (core) {
        core->getLogger()->warn("PROFILER ALERT: {}", message);
    }
}

}  // namespace lithium::device::indi::filterwheel
