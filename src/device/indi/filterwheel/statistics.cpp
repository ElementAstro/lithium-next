/*
 * statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel statistics and monitoring implementation

*************************************************/

#include "statistics.hpp"

#include <chrono>
#include <numeric>

INDIFilterwheelStatistics::INDIFilterwheelStatistics(std::string name) 
    : INDIFilterwheelBase(name),
      startTime_(std::chrono::steady_clock::now()) {
}

auto INDIFilterwheelStatistics::getTotalMoves() -> uint64_t {
    return total_moves_;
}

auto INDIFilterwheelStatistics::resetTotalMoves() -> bool {
    logger_->info("Resetting total moves counter (was: {})", total_moves_);
    total_moves_ = 0;
    moveTimes_.clear();
    startTime_ = std::chrono::steady_clock::now();
    return true;
}

auto INDIFilterwheelStatistics::getLastMoveTime() -> int {
    return last_move_time_;
}

auto INDIFilterwheelStatistics::getTemperature() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("FILTER_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }

    double temp = property[0].getValue();
    logger_->debug("Filter wheel temperature: {:.2f}°C", temp);
    return temp;
}

auto INDIFilterwheelStatistics::hasTemperatureSensor() -> bool {
    INDI::PropertyNumber property = device_.getProperty("FILTER_TEMPERATURE");
    bool hasTemp = property.isValid();
    logger_->debug("Temperature sensor available: {}", hasTemp ? "Yes" : "No");
    return hasTemp;
}

auto INDIFilterwheelStatistics::getAverageMoveTime() -> double {
    if (moveTimes_.empty()) {
        return 0.0;
    }

    auto total = std::accumulate(moveTimes_.begin(), moveTimes_.end(), 
                                std::chrono::milliseconds(0));
    
    double average = static_cast<double>(total.count()) / moveTimes_.size();
    logger_->debug("Average move time: {:.2f}ms", average);
    return average;
}

auto INDIFilterwheelStatistics::getMovesPerHour() -> double {
    auto uptime = getUptimeSeconds();
    if (uptime == 0) {
        return 0.0;
    }

    double hours = static_cast<double>(uptime) / 3600.0;
    double movesPerHour = static_cast<double>(total_moves_) / hours;
    
    logger_->debug("Moves per hour: {:.2f}", movesPerHour);
    return movesPerHour;
}

auto INDIFilterwheelStatistics::getUptimeSeconds() -> uint64_t {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
    return uptime.count();
}

void INDIFilterwheelStatistics::recordMove() {
    auto now = std::chrono::steady_clock::now();
    auto moveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    
    // Calculate time since last move if we have a previous move
    if (last_move_time_ > 0) {
        auto lastMoveTimePoint = std::chrono::milliseconds(last_move_time_);
        auto timeDiff = moveTime - lastMoveTimePoint;
        
        // Store the move time (limit history size)
        moveTimes_.push_back(timeDiff);
        if (moveTimes_.size() > MAX_MOVE_HISTORY) {
            moveTimes_.erase(moveTimes_.begin());
        }
    }
    
    last_move_time_ = moveTime.count();
    total_moves_++;
    
    logger_->debug("Move recorded: total moves = {}, last move time = {}", 
                  total_moves_, last_move_time_);
}

void INDIFilterwheelStatistics::updateTemperature(double temp) {
    logger_->debug("Temperature updated: {:.2f}°C", temp);
    
    // Call temperature callback if set
    if (temperature_callback_) {
        temperature_callback_(temp);
    }
}
