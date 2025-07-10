/*
 * calibration_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Filter Wheel Calibration System Implementation

*************************************************/

#include "calibration_system.hpp"
#include "hardware_interface.hpp"
#include "position_manager.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace lithium::device::ascom::filterwheel::components {

CalibrationSystem::CalibrationSystem(std::shared_ptr<HardwareInterface> hardware,
                                   std::shared_ptr<PositionManager> position_manager)
    : hardware_(std::move(hardware)), position_manager_(std::move(position_manager)) {
    spdlog::debug("CalibrationSystem constructor called");
}

CalibrationSystem::~CalibrationSystem() {
    spdlog::debug("CalibrationSystem destructor called");
    stopCalibration();
}

auto CalibrationSystem::initialize() -> bool {
    spdlog::info("Initializing Calibration System");
    
    if (!hardware_ || !position_manager_) {
        setError("Hardware or position manager not available");
        return false;
    }
    
    // Initialize default calibration parameters
    calibration_config_.home_position = 0;
    calibration_config_.max_attempts = 3;
    calibration_config_.timeout_ms = 30000;
    calibration_config_.position_tolerance = 0.1;
    calibration_config_.enable_backlash_compensation = true;
    calibration_config_.backlash_compensation_steps = 5;
    calibration_config_.enable_temperature_compensation = false;
    
    return true;
}

auto CalibrationSystem::shutdown() -> void {
    spdlog::info("Shutting down Calibration System");
    stopCalibration();
    clearResults();
}

auto CalibrationSystem::startFullCalibration() -> bool {
    if (is_calibrating_.load()) {
        spdlog::error("Calibration already in progress");
        return false;
    }
    
    if (!hardware_ || !hardware_->isConnected()) {
        setError("Hardware not connected");
        return false;
    }
    
    spdlog::info("Starting full filter wheel calibration");
    
    is_calibrating_.store(true);
    calibration_progress_.store(0.0f);
    current_step_ = CalibrationStep::INITIALIZE;
    
    // Start calibration in a separate thread
    if (calibration_thread_ && calibration_thread_->joinable()) {
        calibration_thread_->join();
    }
    
    calibration_thread_ = std::make_unique<std::thread>(&CalibrationSystem::fullCalibrationLoop, this);
    
    return true;
}

auto CalibrationSystem::startPositionCalibration(int position) -> bool {
    if (is_calibrating_.load()) {
        spdlog::error("Calibration already in progress");
        return false;
    }
    
    if (!isValidPosition(position)) {
        setError("Invalid position for calibration: " + std::to_string(position));
        return false;
    }
    
    spdlog::info("Starting position calibration for position: {}", position);
    
    is_calibrating_.store(true);
    calibration_progress_.store(0.0f);
    current_step_ = CalibrationStep::POSITION_CALIBRATION;
    
    // Start position calibration
    return performPositionCalibration(position);
}

auto CalibrationSystem::startHomeCalibration() -> bool {
    if (is_calibrating_.load()) {
        spdlog::error("Calibration already in progress");
        return false;
    }
    
    spdlog::info("Starting home position calibration");
    
    is_calibrating_.store(true);
    calibration_progress_.store(0.0f);
    current_step_ = CalibrationStep::HOME_CALIBRATION;
    
    return performHomeCalibration();
}

auto CalibrationSystem::stopCalibration() -> bool {
    if (!is_calibrating_.load()) {
        return true;
    }
    
    spdlog::info("Stopping calibration");
    
    is_calibrating_.store(false);
    
    if (calibration_thread_ && calibration_thread_->joinable()) {
        calibration_thread_->join();
    }
    
    current_step_ = CalibrationStep::IDLE;
    calibration_progress_.store(0.0f);
    
    return true;
}

auto CalibrationSystem::isCalibrating() const -> bool {
    return is_calibrating_.load();
}

auto CalibrationSystem::getCurrentStep() const -> CalibrationStep {
    return current_step_;
}

auto CalibrationSystem::getProgress() const -> float {
    return calibration_progress_.load();
}

auto CalibrationSystem::getLastResult() const -> std::optional<CalibrationResult> {
    std::lock_guard<std::mutex> lock(results_mutex_);
    if (!calibration_results_.empty()) {
        return calibration_results_.back();
    }
    return std::nullopt;
}

auto CalibrationSystem::getAllResults() const -> std::vector<CalibrationResult> {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return calibration_results_;
}

auto CalibrationSystem::clearResults() -> void {
    std::lock_guard<std::mutex> lock(results_mutex_);
    calibration_results_.clear();
    spdlog::debug("Calibration results cleared");
}

auto CalibrationSystem::setCalibrationConfig(const CalibrationConfig& config) -> bool {
    if (is_calibrating_.load()) {
        spdlog::error("Cannot change configuration during calibration");
        return false;
    }
    
    if (!validateConfig(config)) {
        setError("Invalid calibration configuration");
        return false;
    }
    
    calibration_config_ = config;
    spdlog::debug("Calibration configuration updated");
    return true;
}

auto CalibrationSystem::getCalibrationConfig() const -> CalibrationConfig {
    return calibration_config_;
}

auto CalibrationSystem::performBacklashTest() -> BacklashResult {
    spdlog::info("Performing backlash test");
    
    BacklashResult result;
    result.start_time = std::chrono::system_clock::now();
    result.success = false;
    
    if (!hardware_ || !hardware_->isConnected()) {
        result.error_message = "Hardware not connected";
        return result;
    }
    
    try {
        // Test backlash by moving in one direction, then back
        auto initial_position = position_manager_->getCurrentPosition();
        if (!initial_position) {
            result.error_message = "Cannot determine current position";
            return result;
        }
        
        int test_position = (*initial_position + 1) % position_manager_->getFilterCount();
        
        // Move forward
        auto move_start = std::chrono::steady_clock::now();
        if (!position_manager_->moveToPosition(test_position)) {
            result.error_message = "Failed to move to test position";
            return result;
        }
        
        // Wait for movement to complete
        while (position_manager_->isMoving() && 
               std::chrono::steady_clock::now() - move_start < std::chrono::milliseconds(calibration_config_.timeout_ms)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto forward_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - move_start);
        
        // Move back
        move_start = std::chrono::steady_clock::now();
        if (!position_manager_->moveToPosition(*initial_position)) {
            result.error_message = "Failed to move back to initial position";
            return result;
        }
        
        // Wait for movement to complete
        while (position_manager_->isMoving() && 
               std::chrono::steady_clock::now() - move_start < std::chrono::milliseconds(calibration_config_.timeout_ms)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto backward_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - move_start);
        
        // Calculate backlash
        result.forward_time = forward_time;
        result.backward_time = backward_time;
        result.backlash_amount = std::abs(forward_time.count() - backward_time.count());
        result.success = true;
        
        spdlog::info("Backlash test completed: forward={}ms, backward={}ms, backlash={}ms",
                     forward_time.count(), backward_time.count(), result.backlash_amount);
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during backlash test: " + std::string(e.what());
        spdlog::error("Backlash test failed: {}", e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

auto CalibrationSystem::performAccuracyTest() -> AccuracyResult {
    spdlog::info("Performing accuracy test");
    
    AccuracyResult result;
    result.start_time = std::chrono::system_clock::now();
    result.success = false;
    
    if (!hardware_ || !hardware_->isConnected()) {
        result.error_message = "Hardware not connected";
        return result;
    }
    
    try {
        int filter_count = position_manager_->getFilterCount();
        result.position_errors.resize(filter_count);
        
        for (int position = 0; position < filter_count; ++position) {
            // Move to position
            if (!position_manager_->moveToPosition(position)) {
                result.error_message = "Failed to move to position " + std::to_string(position);
                return result;
            }
            
            // Wait for movement
            auto move_start = std::chrono::steady_clock::now();
            while (position_manager_->isMoving() && 
                   std::chrono::steady_clock::now() - move_start < std::chrono::milliseconds(calibration_config_.timeout_ms)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Check actual position
            auto actual_position = position_manager_->getCurrentPosition();
            if (actual_position) {
                result.position_errors[position] = std::abs(*actual_position - position);
            } else {
                result.position_errors[position] = 999.0; // Error indicator
            }
            
            spdlog::debug("Position {} accuracy: error = {}", position, result.position_errors[position]);
        }
        
        // Calculate statistics
        double sum = 0.0;
        double max_error = 0.0;
        for (double error : result.position_errors) {
            sum += error;
            max_error = std::max(max_error, error);
        }
        
        result.average_error = sum / filter_count;
        result.max_error = max_error;
        result.success = max_error < calibration_config_.position_tolerance;
        
        spdlog::info("Accuracy test completed: avg_error={}, max_error={}, success={}",
                     result.average_error, result.max_error, result.success);
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during accuracy test: " + std::string(e.what());
        spdlog::error("Accuracy test failed: {}", e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

auto CalibrationSystem::performSpeedTest() -> SpeedResult {
    spdlog::info("Performing speed test");
    
    SpeedResult result;
    result.start_time = std::chrono::system_clock::now();
    result.success = false;
    
    if (!hardware_ || !hardware_->isConnected()) {
        result.error_message = "Hardware not connected";
        return result;
    }
    
    try {
        int filter_count = position_manager_->getFilterCount();
        std::vector<std::chrono::milliseconds> move_times;
        
        auto initial_position = position_manager_->getCurrentPosition();
        if (!initial_position) {
            result.error_message = "Cannot determine current position";
            return result;
        }
        
        // Test moves between adjacent positions
        for (int i = 0; i < filter_count; ++i) {
            int next_position = (i + 1) % filter_count;
            
            auto move_start = std::chrono::steady_clock::now();
            
            if (!position_manager_->moveToPosition(next_position)) {
                result.error_message = "Failed to move to position " + std::to_string(next_position);
                return result;
            }
            
            // Wait for movement
            while (position_manager_->isMoving() && 
                   std::chrono::steady_clock::now() - move_start < std::chrono::milliseconds(calibration_config_.timeout_ms)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            auto move_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - move_start);
            
            move_times.push_back(move_time);
            spdlog::debug("Move {} -> {}: {}ms", i, next_position, move_time.count());
        }
        
        // Calculate statistics
        auto total_time = std::chrono::milliseconds{0};
        auto min_time = move_times[0];
        auto max_time = move_times[0];
        
        for (const auto& time : move_times) {
            total_time += time;
            min_time = std::min(min_time, time);
            max_time = std::max(max_time, time);
        }
        
        result.average_move_time = total_time / move_times.size();
        result.min_move_time = min_time;
        result.max_move_time = max_time;
        result.total_test_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - result.start_time);
        result.success = true;
        
        spdlog::info("Speed test completed: avg={}ms, min={}ms, max={}ms",
                     result.average_move_time.count(), result.min_move_time.count(), result.max_move_time.count());
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during speed test: " + std::string(e.what());
        spdlog::error("Speed test failed: {}", e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

auto CalibrationSystem::setProgressCallback(ProgressCallback callback) -> void {
    progress_callback_ = std::move(callback);
}

auto CalibrationSystem::setCompletionCallback(CompletionCallback callback) -> void {
    completion_callback_ = std::move(callback);
}

auto CalibrationSystem::getLastError() -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto CalibrationSystem::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private implementation methods

auto CalibrationSystem::fullCalibrationLoop() -> void {
    spdlog::debug("Starting full calibration loop");
    
    CalibrationResult result;
    result.type = CalibrationType::FULL_CALIBRATION;
    result.start_time = std::chrono::system_clock::now();
    result.success = false;
    
    try {
        // Step 1: Initialize
        current_step_ = CalibrationStep::INITIALIZE;
        updateProgress(0.1f);
        
        if (!initializeCalibration()) {
            result.error_message = "Failed to initialize calibration";
            storeResult(result);
            return;
        }
        
        // Step 2: Home calibration
        current_step_ = CalibrationStep::HOME_CALIBRATION;
        updateProgress(0.2f);
        
        if (!performHomeCalibration()) {
            result.error_message = "Failed to calibrate home position";
            storeResult(result);
            return;
        }
        
        // Step 3: Position calibration for all positions
        current_step_ = CalibrationStep::POSITION_CALIBRATION;
        
        int filter_count = position_manager_->getFilterCount();
        for (int position = 0; position < filter_count; ++position) {
            updateProgress(0.2f + 0.6f * (float(position) / filter_count));
            
            if (!performPositionCalibration(position)) {
                result.error_message = "Failed to calibrate position " + std::to_string(position);
                storeResult(result);
                return;
            }
        }
        
        // Step 4: Verification
        current_step_ = CalibrationStep::VERIFICATION;
        updateProgress(0.8f);
        
        if (!verifyCalibration()) {
            result.error_message = "Calibration verification failed";
            storeResult(result);
            return;
        }
        
        // Step 5: Complete
        current_step_ = CalibrationStep::COMPLETE;
        updateProgress(1.0f);
        
        result.success = true;
        result.end_time = std::chrono::system_clock::now();
        
        spdlog::info("Full calibration completed successfully");
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during calibration: " + std::string(e.what());
        spdlog::error("Full calibration failed: {}", e.what());
    }
    
    storeResult(result);
    is_calibrating_.store(false);
    current_step_ = CalibrationStep::IDLE;
    
    if (completion_callback_) {
        completion_callback_(result.success, result.error_message);
    }
}

auto CalibrationSystem::performHomeCalibration() -> bool {
    spdlog::debug("Performing home calibration");
    
    try {
        // Move to home position
        if (!position_manager_->moveToPosition(calibration_config_.home_position)) {
            setError("Failed to move to home position");
            return false;
        }
        
        // Wait for movement to complete
        auto start_time = std::chrono::steady_clock::now();
        while (position_manager_->isMoving()) {
            if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(calibration_config_.timeout_ms)) {
                setError("Home calibration timeout");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Verify position
        auto current_position = position_manager_->getCurrentPosition();
        if (!current_position || *current_position != calibration_config_.home_position) {
            setError("Home position verification failed");
            return false;
        }
        
        spdlog::debug("Home calibration completed");
        return true;
        
    } catch (const std::exception& e) {
        setError("Exception during home calibration: " + std::string(e.what()));
        return false;
    }
}

auto CalibrationSystem::performPositionCalibration(int position) -> bool {
    spdlog::debug("Performing position calibration for position: {}", position);
    
    if (!isValidPosition(position)) {
        setError("Invalid position: " + std::to_string(position));
        return false;
    }
    
    try {
        for (int attempt = 0; attempt < calibration_config_.max_attempts; ++attempt) {
            // Move to position
            if (!position_manager_->moveToPosition(position)) {
                spdlog::warn("Move attempt {} failed for position {}", attempt + 1, position);
                continue;
            }
            
            // Wait for movement
            auto start_time = std::chrono::steady_clock::now();
            while (position_manager_->isMoving()) {
                if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(calibration_config_.timeout_ms)) {
                    spdlog::warn("Timeout on attempt {} for position {}", attempt + 1, position);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Verify position
            auto current_position = position_manager_->getCurrentPosition();
            if (current_position && *current_position == position) {
                spdlog::debug("Position {} calibration completed on attempt {}", position, attempt + 1);
                return true;
            }
        }
        
        setError("Position calibration failed after " + std::to_string(calibration_config_.max_attempts) + " attempts");
        return false;
        
    } catch (const std::exception& e) {
        setError("Exception during position calibration: " + std::string(e.what()));
        return false;
    }
}

auto CalibrationSystem::initializeCalibration() -> bool {
    spdlog::debug("Initializing calibration");
    
    if (!hardware_ || !hardware_->isConnected()) {
        setError("Hardware not connected");
        return false;
    }
    
    if (!position_manager_) {
        setError("Position manager not available");
        return false;
    }
    
    return true;
}

auto CalibrationSystem::verifyCalibration() -> bool {
    spdlog::debug("Verifying calibration");
    
    // Perform a quick verification by moving through all positions
    int filter_count = position_manager_->getFilterCount();
    
    for (int position = 0; position < filter_count; ++position) {
        if (!position_manager_->moveToPosition(position)) {
            setError("Verification failed at position " + std::to_string(position));
            return false;
        }
        
        // Wait briefly
        auto start_time = std::chrono::steady_clock::now();
        while (position_manager_->isMoving()) {
            if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(calibration_config_.timeout_ms)) {
                setError("Verification timeout at position " + std::to_string(position));
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        auto current_position = position_manager_->getCurrentPosition();
        if (!current_position || *current_position != position) {
            setError("Verification position mismatch at position " + std::to_string(position));
            return false;
        }
    }
    
    spdlog::debug("Calibration verification completed");
    return true;
}

auto CalibrationSystem::isValidPosition(int position) -> bool {
    return position >= 0 && position < position_manager_->getFilterCount();
}

auto CalibrationSystem::updateProgress(float progress) -> void {
    calibration_progress_.store(progress);
    
    if (progress_callback_) {
        try {
            progress_callback_(progress, stepToString(current_step_));
        } catch (const std::exception& e) {
            spdlog::error("Exception in progress callback: {}", e.what());
        }
    }
}

auto CalibrationSystem::storeResult(const CalibrationResult& result) -> void {
    std::lock_guard<std::mutex> lock(results_mutex_);
    calibration_results_.push_back(result);
    
    // Keep only last 10 results
    if (calibration_results_.size() > 10) {
        calibration_results_.erase(calibration_results_.begin());
    }
}

auto CalibrationSystem::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("CalibrationSystem error: {}", error);
}

auto CalibrationSystem::validateConfig(const CalibrationConfig& config) -> bool {
    if (config.max_attempts <= 0) {
        spdlog::error("Invalid max_attempts: {}", config.max_attempts);
        return false;
    }
    
    if (config.timeout_ms <= 0) {
        spdlog::error("Invalid timeout_ms: {}", config.timeout_ms);
        return false;
    }
    
    if (config.position_tolerance < 0.0) {
        spdlog::error("Invalid position_tolerance: {}", config.position_tolerance);
        return false;
    }
    
    return true;
}

auto CalibrationSystem::stepToString(CalibrationStep step) -> std::string {
    switch (step) {
        case CalibrationStep::IDLE: return "Idle";
        case CalibrationStep::INITIALIZE: return "Initialize";
        case CalibrationStep::HOME_CALIBRATION: return "Home Calibration";
        case CalibrationStep::POSITION_CALIBRATION: return "Position Calibration";
        case CalibrationStep::VERIFICATION: return "Verification";
        case CalibrationStep::COMPLETE: return "Complete";
        default: return "Unknown";
    }
}

} // namespace lithium::device::ascom::filterwheel::components
