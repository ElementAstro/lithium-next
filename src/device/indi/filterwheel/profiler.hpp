#ifndef LITHIUM_INDI_FILTERWHEEL_PROFILER_HPP
#define LITHIUM_INDI_FILTERWHEEL_PROFILER_HPP

#include "component_base.hpp"
#include <chrono>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Performance profiling data for filter wheel operations.
 */
struct FilterProfileData {
    int fromSlot = 0;
    int toSlot = 0;
    std::chrono::milliseconds duration{0};
    bool success = false;
    std::chrono::system_clock::time_point timestamp;
    double temperature = 0.0;  // Temperature during move (if available)
};

/**
 * @brief Performance statistics for filter wheel operations.
 */
struct FilterPerformanceStats {
    size_t totalMoves = 0;
    std::chrono::milliseconds averageMoveTime{0};
    std::chrono::milliseconds fastestMove{std::chrono::milliseconds::max()};
    std::chrono::milliseconds slowestMove{0};
    double successRate = 100.0;
    std::unordered_map<int, std::chrono::milliseconds> slotAverages;
    std::vector<FilterProfileData> recentMoves;
};

/**
 * @brief Advanced profiler for filter wheel performance monitoring and optimization.
 *
 * This component provides detailed performance analytics, predictive timing,
 * and optimization recommendations for filter wheel operations. It can help
 * identify performance degradation and suggest maintenance intervals.
 */
class FilterWheelProfiler : public FilterWheelComponentBase {
public:
    /**
     * @brief Constructor with shared core.
     * @param core Shared pointer to the INDIFilterWheelCore
     */
    explicit FilterWheelProfiler(std::shared_ptr<INDIFilterWheelCore> core);
    
    /**
     * @brief Virtual destructor.
     */
    ~FilterWheelProfiler() override = default;

    /**
     * @brief Initialize the profiler.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize() override;

    /**
     * @brief Cleanup resources and shutdown the component.
     */
    void shutdown() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override { return "FilterWheelProfiler"; }

    /**
     * @brief Start profiling a filter wheel move.
     * @param fromSlot Starting filter slot.
     * @param toSlot Target filter slot.
     */
    void startMove(int fromSlot, int toSlot);

    /**
     * @brief Complete profiling a filter wheel move.
     * @param success Whether the move was successful.
     * @param actualSlot The actual slot reached (may differ from target if failed).
     */
    void completeMove(bool success, int actualSlot);

    /**
     * @brief Predict move duration based on historical data.
     * @param fromSlot Starting filter slot.
     * @param toSlot Target filter slot.
     * @return Predicted duration in milliseconds.
     */
    std::chrono::milliseconds predictMoveDuration(int fromSlot, int toSlot) const;

    /**
     * @brief Get comprehensive performance statistics.
     * @return Performance statistics structure.
     */
    FilterPerformanceStats getPerformanceStats() const;

    /**
     * @brief Get performance data for a specific slot transition.
     * @param fromSlot Starting slot.
     * @param toSlot Target slot.
     * @return Vector of historical move data for this transition.
     */
    std::vector<FilterProfileData> getSlotTransitionData(int fromSlot, int toSlot) const;

    /**
     * @brief Check if filter wheel performance has degraded.
     * @return true if performance degradation is detected, false otherwise.
     */
    bool hasPerformanceDegraded() const;

    /**
     * @brief Get optimization recommendations.
     * @return Vector of recommendation strings.
     */
    std::vector<std::string> getOptimizationRecommendations() const;

    /**
     * @brief Reset all profiling data.
     */
    void resetProfileData();

    /**
     * @brief Export profiling data to CSV file.
     * @param filePath Path to output CSV file.
     * @return true if export was successful, false otherwise.
     */
    bool exportToCSV(const std::string& filePath) const;

    /**
     * @brief Enable/disable continuous profiling.
     * @param enabled Whether to enable profiling.
     */
    void setProfiling(bool enabled) { profilingEnabled_ = enabled; }

    /**
     * @brief Check if profiling is enabled.
     * @return true if profiling is enabled, false otherwise.
     */
    bool isProfilingEnabled() const { return profilingEnabled_; }

private:
    bool initialized_{false};
    std::atomic_bool profilingEnabled_{true};
    
    // Current move tracking
    std::chrono::steady_clock::time_point moveStartTime_;
    int moveFromSlot_ = -1;
    int moveToSlot_ = -1;
    bool moveInProgress_ = false;
    
    // Historical data
    std::vector<FilterProfileData> moveHistory_;
    static constexpr size_t MAX_HISTORY_SIZE = 10000;
    static constexpr size_t RECENT_MOVES_COUNT = 100;
    
    // Performance analysis
    mutable std::mutex dataAccessMutex_;
    
    // Helper methods
    void pruneOldData();
    double calculateSuccessRate() const;
    std::chrono::milliseconds calculateAverageTime() const;
    std::chrono::milliseconds calculateSlotAverage(int fromSlot, int toSlot) const;
    bool detectPerformanceTrend() const;
    void logPerformanceAlert(const std::string& message) const;
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_PROFILER_HPP
