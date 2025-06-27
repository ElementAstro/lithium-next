#ifndef LITHIUM_INDI_FILTERWHEEL_STATISTICS_MANAGER_HPP
#define LITHIUM_INDI_FILTERWHEEL_STATISTICS_MANAGER_HPP

#include "component_base.hpp"
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Manages statistics and usage tracking for INDI FilterWheel
 * 
 * This component tracks filter wheel usage statistics including position
 * changes, movement times, and filter usage patterns.
 */
class StatisticsManager : public FilterWheelComponentBase {
public:
    explicit StatisticsManager(std::shared_ptr<INDIFilterWheelCore> core);
    ~StatisticsManager() override = default;

    bool initialize() override;
    void shutdown() override;
    std::string getComponentName() const override { return "StatisticsManager"; }

    // Statistics recording
    void recordPositionChange(int fromPosition, int toPosition);
    void recordMoveTime(std::chrono::milliseconds duration);
    void startSession();
    void endSession();

    // Statistics retrieval
    uint64_t getTotalPositionChanges() const;
    uint64_t getPositionUsageCount(int position) const;
    std::chrono::milliseconds getAverageMoveTime() const;
    std::chrono::milliseconds getTotalMoveTime() const;
    uint64_t getSessionPositionChanges() const;
    std::chrono::duration<double> getSessionDuration() const;
    
    // Statistics management
    bool resetStatistics();
    bool resetSessionStatistics();

    // Most/least used filters
    int getMostUsedPosition() const;
    int getLeastUsedPosition() const;
    std::unordered_map<int, uint64_t> getPositionUsageMap() const;

private:
    bool initialized_{false};
    
    // Total statistics
    std::atomic<uint64_t> totalPositionChanges_{0};
    std::atomic<uint64_t> totalMoveTimeMs_{0};
    std::unordered_map<int, std::atomic<uint64_t>> positionUsage_;
    
    // Session statistics
    std::atomic<uint64_t> sessionPositionChanges_{0};
    std::chrono::steady_clock::time_point sessionStartTime_;
    std::chrono::steady_clock::time_point sessionEndTime_;
    bool sessionActive_{false};
    
    // Thread safety
    mutable std::mutex statisticsMutex_;
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_STATISTICS_MANAGER_HPP
