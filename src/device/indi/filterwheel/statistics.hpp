/*
 * statistics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel statistics and monitoring

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_STATISTICS_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_STATISTICS_HPP

#include "base.hpp"

class INDIFilterwheelStatistics : public virtual INDIFilterwheelBase {
public:
    explicit INDIFilterwheelStatistics(std::string name);
    ~INDIFilterwheelStatistics() override = default;

    // Statistics
    auto getTotalMoves() -> uint64_t override;
    auto resetTotalMoves() -> bool override;
    auto getLastMoveTime() -> int override;

    // Temperature (if supported)
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Additional statistics
    auto getAverageMoveTime() -> double;
    auto getMovesPerHour() -> double;
    auto getUptimeSeconds() -> uint64_t;

protected:
    void recordMove();
    void updateTemperature(double temp);
    
private:
    std::chrono::steady_clock::time_point startTime_;
    std::vector<std::chrono::milliseconds> moveTimes_;
    static constexpr size_t MAX_MOVE_HISTORY = 100;
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_STATISTICS_HPP
