/*
 * sequence_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Sequence Manager Component Implementation

*************************************************/

#include "sequence_manager.hpp"
#include "spdlog/spdlog.h"

#include <stdexcept>
#include <algorithm>
#include <thread>
#include <sstream>
#include <iomanip>

namespace lithium::device::asi::camera::components {

SequenceManager::SequenceManager(std::shared_ptr<ExposureManager> exposureManager,
                                std::shared_ptr<PropertyManager> propertyManager)
    : exposureManager_(exposureManager), propertyManager_(propertyManager) {
    spdlog::info( "Creating sequence manager");
}

SequenceManager::~SequenceManager() {
    spdlog::info( "Destroying sequence manager");
    stopSequence();
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
}

// =========================================================================
// Sequence Control
// =========================================================================

bool SequenceManager::startSequence(const SequenceSettings& settings) {
    spdlog::info( "Starting sequence: %s", settings.name.c_str());
    
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != SequenceState::IDLE && state_ != SequenceState::COMPLETE) {
        spdlog::error( "Cannot start sequence, current state: %s", getStateString().c_str());
        return false;
    }
    
    if (!validateSequence(settings)) {
        spdlog::error( "Sequence validation failed");
        return false;
    }
    
    currentSettings_ = settings;
    updateState(SequenceState::PREPARING);
    
    // Start sequence in background thread
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
    
    sequenceThread_ = std::thread(&SequenceManager::sequenceWorker, this);
    
    spdlog::info( "Sequence started successfully");
    return true;
}

bool SequenceManager::pauseSequence() {
    if (state_ != SequenceState::RUNNING) {
        return false;
    }
    
    spdlog::info( "Pausing sequence");
    pauseRequested_ = true;
    updateState(SequenceState::PAUSED);
    return true;
}

bool SequenceManager::resumeSequence() {
    if (state_ != SequenceState::PAUSED) {
        return false;
    }
    
    spdlog::info( "Resuming sequence");
    pauseRequested_ = false;
    updateState(SequenceState::RUNNING);
    stateCondition_.notify_all();
    return true;
}

bool SequenceManager::stopSequence() {
    if (state_ == SequenceState::IDLE || state_ == SequenceState::COMPLETE) {
        return true;
    }
    
    spdlog::info( "Stopping sequence");
    stopRequested_ = true;
    pauseRequested_ = false;
    updateState(SequenceState::STOPPING);
    stateCondition_.notify_all();
    
    return true;
}

bool SequenceManager::abortSequence() {
    if (state_ == SequenceState::IDLE || state_ == SequenceState::COMPLETE) {
        return true;
    }
    
    spdlog::info( "Aborting sequence");
    abortRequested_ = true;
    stopRequested_ = true;
    pauseRequested_ = false;
    updateState(SequenceState::ABORTED);
    stateCondition_.notify_all();
    
    return true;
}

// =========================================================================
// State and Progress
// =========================================================================

std::string SequenceManager::getStateString() const {
    switch (state_) {
        case SequenceState::IDLE: return "Idle";
        case SequenceState::PREPARING: return "Preparing";
        case SequenceState::RUNNING: return "Running";
        case SequenceState::PAUSED: return "Paused";
        case SequenceState::STOPPING: return "Stopping";
        case SequenceState::COMPLETE: return "Complete";
        case SequenceState::ABORTED: return "Aborted";
        case SequenceState::ERROR: return "Error";
        default: return "Unknown";
    }
}

auto SequenceManager::getProgress() const -> SequenceProgress {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(stateMutex_));
    return currentProgress_;
}

// =========================================================================
// Results Management
// =========================================================================

auto SequenceManager::getLastResult() const -> SequenceResult {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    if (results_.empty()) {
        return SequenceResult{};
    }
    return results_.back();
}

std::vector<SequenceManager::SequenceResult> SequenceManager::getAllResults() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return results_;
}

bool SequenceManager::hasResult() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return !results_.empty();
}

void SequenceManager::clearResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    results_.clear();
    spdlog::info( "Sequence results cleared");
}

// =========================================================================
// Sequence Templates
// =========================================================================

auto SequenceManager::createSimpleSequence(double exposure, int count, 
                                          std::chrono::seconds interval) -> SequenceSettings {
    SequenceSettings settings;
    settings.type = SequenceType::SIMPLE;
    settings.name = "Simple Sequence";
    settings.intervalDelay = interval;
    
    for (int i = 0; i < count; ++i) {
        ExposureStep step;
        step.duration = exposure;
        step.filename = "exposure_{step:03d}";
        settings.steps.push_back(step);
    }
    
    return settings;
}

auto SequenceManager::createBracketingSequence(double baseExposure, 
                                              const std::vector<double>& exposureMultipliers,
                                              int repeatCount) -> SequenceSettings {
    SequenceSettings settings;
    settings.type = SequenceType::BRACKETING;
    settings.name = "Bracketing Sequence";
    settings.repeatCount = repeatCount;
    
    for (double multiplier : exposureMultipliers) {
        ExposureStep step;
        step.duration = baseExposure * multiplier;
        step.filename = "bracket_{step:03d}_{duration:.2f}s";
        settings.steps.push_back(step);
    }
    
    return settings;
}

auto SequenceManager::createTimeLapseSequence(double exposure, int count, 
                                             std::chrono::seconds interval) -> SequenceSettings {
    SequenceSettings settings;
    settings.type = SequenceType::TIME_LAPSE;
    settings.name = "Time Lapse";
    settings.intervalDelay = interval;
    
    for (int i = 0; i < count; ++i) {
        ExposureStep step;
        step.duration = exposure;
        step.filename = "timelapse_{step:03d}_{timestamp}";
        settings.steps.push_back(step);
    }
    
    return settings;
}

auto SequenceManager::createCalibrationSequence(const std::string& frameType,
                                               double exposure, int count) -> SequenceSettings {
    SequenceSettings settings;
    settings.type = SequenceType::CALIBRATION;
    settings.name = frameType + " Calibration";
    
    for (int i = 0; i < count; ++i) {
        ExposureStep step;
        step.duration = exposure;
        step.isDark = (frameType == "dark" || frameType == "bias");
        step.filename = frameType + "_{step:03d}";
        settings.steps.push_back(step);
    }
    
    return settings;
}

// =========================================================================
// Sequence Validation
// =========================================================================

bool SequenceManager::validateSequence(const SequenceSettings& settings) const {
    if (settings.steps.empty()) {
        spdlog::error( "Sequence has no steps");
        return false;
    }
    
    if (settings.repeatCount <= 0) {
        spdlog::error( "Invalid repeat count: %d", settings.repeatCount);
        return false;
    }
    
    for (const auto& step : settings.steps) {
        if (!validateExposureStep(step)) {
            return false;
        }
    }
    
    return true;
}

std::chrono::seconds SequenceManager::estimateSequenceDuration(const SequenceSettings& settings) const {
    std::chrono::seconds total{0};
    
    for (const auto& step : settings.steps) {
        total += std::chrono::seconds(static_cast<int>(step.duration));
        total += settings.intervalDelay;
    }
    
    total *= settings.repeatCount;
    total += settings.sequenceDelay * (settings.repeatCount - 1);
    
    return total;
}

int SequenceManager::calculateTotalExposures(const SequenceSettings& settings) const {
    return static_cast<int>(settings.steps.size()) * settings.repeatCount;
}

// =========================================================================
// Callback Management
// =========================================================================

void SequenceManager::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    progressCallback_ = std::move(callback);
}

void SequenceManager::setStepCallback(StepCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stepCallback_ = std::move(callback);
}

void SequenceManager::setCompletionCallback(CompletionCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    completionCallback_ = std::move(callback);
}

void SequenceManager::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

// =========================================================================
// Sequence Management
// =========================================================================

std::vector<std::string> SequenceManager::getRunningSequences() const {
    std::vector<std::string> running;
    if (isRunning()) {
        running.push_back(currentSettings_.name);
    }
    return running;
}

bool SequenceManager::isSequenceRunning(const std::string& sequenceName) const {
    return isRunning() && currentSettings_.name == sequenceName;
}

// =========================================================================
// Preset Management (Placeholder implementations)
// =========================================================================

bool SequenceManager::saveSequencePreset(const std::string& name, const SequenceSettings& settings) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    sequencePresets_[name] = settings;
    spdlog::info( "Saved sequence preset: %s", name.c_str());
    return true;
}

bool SequenceManager::loadSequencePreset(const std::string& name, SequenceSettings& settings) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    auto it = sequencePresets_.find(name);
    if (it != sequencePresets_.end()) {
        settings = it->second;
        spdlog::info( "Loaded sequence preset: %s", name.c_str());
        return true;
    }
    spdlog::warn( "Sequence preset not found: %s", name.c_str());
    return false;
}

std::vector<std::string> SequenceManager::getAvailablePresets() const {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    std::vector<std::string> names;
    names.reserve(sequencePresets_.size());
    for (const auto& pair : sequencePresets_) {
        names.emplace_back(pair.first);
    }
    return names;
}

bool SequenceManager::deleteSequencePreset(const std::string& name) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    auto it = sequencePresets_.find(name);
    if (it != sequencePresets_.end()) {
        sequencePresets_.erase(it);
        spdlog::info( "Deleted sequence preset: %s", name.c_str());
        return true;
    }
    spdlog::warn( "Sequence preset not found for deletion: %s", name.c_str());
    return false;
}

// =========================================================================
// Private Helper Methods
// =========================================================================

void SequenceManager::sequenceWorker() {
    spdlog::info( "Sequence worker started");
    
    SequenceResult result;
    result.sequenceName = currentSettings_.name;
    result.startTime = std::chrono::steady_clock::now();
    
    try {
        updateState(SequenceState::RUNNING);
        result.success = executeSequence(currentSettings_, result);
        
        if (result.success && !stopRequested_ && !abortRequested_) {
            updateState(SequenceState::COMPLETE);
        } else if (abortRequested_) {
            updateState(SequenceState::ABORTED);
        } else {
            updateState(SequenceState::ERROR);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
        updateState(SequenceState::ERROR);
        spdlog::error( "Sequence worker exception: %s", e.what());
    }
    
    result.endTime = std::chrono::steady_clock::now();
    result.totalDuration = std::chrono::duration_cast<std::chrono::seconds>(
        result.endTime - result.startTime);
    
    // Store result
    {
        std::lock_guard<std::mutex> lock(resultsMutex_);
        results_.push_back(result);
    }
    
    notifyCompletion(result);
    
    // Reset flags
    stopRequested_ = false;
    abortRequested_ = false;
    pauseRequested_ = false;
    
    spdlog::info( "Sequence worker finished");
}

bool SequenceManager::executeSequence(const SequenceSettings& settings, SequenceResult& result) {
    // Placeholder implementation
    spdlog::info( "Executing sequence: %s", settings.name.c_str());
    
    // TODO: Implement actual sequence execution
    // For now, just simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return true;
}

void SequenceManager::updateState(SequenceState newState) {
    state_ = newState;
    spdlog::info( "Sequence state changed to: %s", getStateString().c_str());
}

bool SequenceManager::validateExposureStep(const ExposureStep& step) const {
    if (step.duration <= 0.0) {
        spdlog::error( "Invalid exposure duration: %.3f", step.duration);
        return false;
    }
    return true;
}

void SequenceManager::notifyProgress(const SequenceProgress& progress) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (progressCallback_) {
        progressCallback_(progress);
    }
}

void SequenceManager::notifyStepStart(int step, const ExposureStep& stepSettings) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stepCallback_) {
        stepCallback_(step, stepSettings);
    }
}

void SequenceManager::notifyCompletion(const SequenceResult& result) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (completionCallback_) {
        completionCallback_(result);
    }
}

void SequenceManager::notifyError(const std::string& error) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error);
    }
}

} // namespace lithium::device::asi::camera::components
