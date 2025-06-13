#include "base.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include "atom/error/exception.hpp"

namespace lithium::task::focuser {

BaseFocuserTask::BaseFocuserTask(const std::string& name)
    : Task(name, [this](const json& params) { this->execute(params); }),
      limits_{0, 50000},
      lastTemperature_{20.0},
      isSetup_{false} {
    
    // Set up default task properties
    setPriority(6);
    setTimeout(std::chrono::seconds(300));
    setLogLevel(2);
    
    addHistoryEntry("BaseFocuserTask initialized");
}

std::optional<int> BaseFocuserTask::getCurrentPosition() const {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    try {
        // In a real implementation, this would interface with actual focuser hardware
        // For now, return a mock position
        return 25000;  // Mock current position
    } catch (const std::exception& e) {
        spdlog::error("Failed to get focuser position: {}", e.what());
        return std::nullopt;
    }
}

bool BaseFocuserTask::moveToPosition(int position, int timeout) {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    if (!isValidPosition(position)) {
        spdlog::error("Invalid focuser position: {}", position);
        logFocuserOperation("moveToPosition", false);
        return false;
    }
    
    try {
        addHistoryEntry("Moving to position: " + std::to_string(position));
        
        // In a real implementation, this would command the actual focuser
        spdlog::info("Moving focuser to position {}", position);
        
        // Simulate movement time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (!waitForMovementComplete(timeout)) {
            spdlog::error("Focuser movement timed out");
            logFocuserOperation("moveToPosition", false);
            return false;
        }
        
        logFocuserOperation("moveToPosition", true);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to move focuser to position {}: {}", position, e.what());
        logFocuserOperation("moveToPosition", false);
        return false;
    }
}

bool BaseFocuserTask::moveRelative(int steps, int timeout) {
    auto currentPos = getCurrentPosition();
    if (!currentPos) {
        spdlog::error("Cannot get current position for relative move");
        return false;
    }
    
    int targetPosition = *currentPos + steps;
    return moveToPosition(targetPosition, timeout);
}

bool BaseFocuserTask::isMoving() const {
    // In a real implementation, this would check actual focuser status
    return false;  // Mock: focuser is not moving
}

bool BaseFocuserTask::abortMovement() {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    try {
        spdlog::info("Aborting focuser movement");
        addHistoryEntry("Movement aborted");
        
        // In a real implementation, this would send abort command to focuser
        logFocuserOperation("abortMovement", true);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to abort focuser movement: {}", e.what());
        logFocuserOperation("abortMovement", false);
        return false;
    }
}

std::optional<double> BaseFocuserTask::getTemperature() const {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    try {
        // In a real implementation, this would read from actual temperature sensor
        return lastTemperature_;  // Mock temperature
    } catch (const std::exception& e) {
        spdlog::error("Failed to get temperature: {}", e.what());
        return std::nullopt;
    }
}

FocusMetrics BaseFocuserTask::analyzeFocusQuality(double exposureTime, int binning) {
    FocusMetrics metrics;
    
    try {
        addHistoryEntry("Analyzing focus quality");
        
        // In a real implementation, this would:
        // 1. Take an exposure with the camera
        // 2. Detect stars in the image
        // 3. Calculate HFR, FWHM, and other metrics
        
        // Mock focus analysis
        metrics.hfr = 2.5 + (rand() % 100) / 100.0;  // Random HFR between 2.5-3.5
        metrics.fwhm = metrics.hfr * 2.1;
        metrics.starCount = 15 + (rand() % 10);
        metrics.peakIntensity = 50000 + (rand() % 15000);
        metrics.backgroundLevel = 1000 + (rand() % 500);
        metrics.quality = assessFocusQuality(metrics);
        
        spdlog::info("Focus analysis: HFR={:.2f}, Stars={}, Quality={}", 
                     metrics.hfr, metrics.starCount, static_cast<int>(metrics.quality));
        
        return metrics;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to analyze focus quality: {}", e.what());
        
        // Return default poor metrics on error
        metrics.hfr = 10.0;
        metrics.fwhm = 20.0;
        metrics.starCount = 0;
        metrics.peakIntensity = 0;
        metrics.backgroundLevel = 1000;
        metrics.quality = FocusQuality::Bad;
        
        return metrics;
    }
}

int BaseFocuserTask::calculateTemperatureCompensation(double currentTemp, 
                                                    double referenceTemp, 
                                                    double compensationRate) {
    double tempDiff = currentTemp - referenceTemp;
    int compensation = static_cast<int>(tempDiff * compensationRate);
    
    spdlog::info("Temperature compensation: {:.1f}°C difference = {} steps", 
                 tempDiff, compensation);
    
    return compensation;
}

bool BaseFocuserTask::validateFocuserParams(const json& params) {
    std::vector<std::string> errors;
    
    if (params.contains("position")) {
        int position = params["position"].get<int>();
        if (!isValidPosition(position)) {
            errors.push_back("Position " + std::to_string(position) + " is out of range");
        }
    }
    
    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"].get<double>();
        if (exposure <= 0 || exposure > 300) {
            errors.push_back("Exposure time must be between 0 and 300 seconds");
        }
    }
    
    if (params.contains("timeout")) {
        int timeout = params["timeout"].get<int>();
        if (timeout <= 0 || timeout > 600) {
            errors.push_back("Timeout must be between 1 and 600 seconds");
        }
    }
    
    if (!errors.empty()) {
        for (const auto& error : errors) {
            spdlog::error("Parameter validation error: {}", error);
        }
        return false;
    }
    
    return true;
}

std::pair<int, int> BaseFocuserTask::getFocuserLimits() const {
    return limits_;
}

bool BaseFocuserTask::setupFocuser() {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    try {
        if (isSetup_) {
            return true;
        }
        
        addHistoryEntry("Setting up focuser");
        
        // In a real implementation, this would:
        // 1. Initialize focuser connection
        // 2. Read focuser capabilities and limits
        // 3. Set up temperature monitoring
        // 4. Verify focuser is responsive
        
        spdlog::info("Focuser setup completed");
        isSetup_ = true;
        logFocuserOperation("setupFocuser", true);
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to setup focuser: {}", e.what());
        logFocuserOperation("setupFocuser", false);
        return false;
    }
}

bool BaseFocuserTask::performBacklashCompensation(FocuserDirection direction, int backlashSteps) {
    std::lock_guard<std::mutex> lock(focuserMutex_);
    
    try {
        addHistoryEntry("Performing backlash compensation");
        
        auto currentPos = getCurrentPosition();
        if (!currentPos) {
            return false;
        }
        
        // Move past target to eliminate backlash
        int overshootPos = *currentPos + (direction == FocuserDirection::Out ? backlashSteps : -backlashSteps);
        
        if (!moveToPosition(overshootPos)) {
            return false;
        }
        
        // Move back to original position
        if (!moveToPosition(*currentPos)) {
            return false;
        }
        
        logFocuserOperation("performBacklashCompensation", true);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Backlash compensation failed: {}", e.what());
        logFocuserOperation("performBacklashCompensation", false);
        return false;
    }
}

bool BaseFocuserTask::waitForMovementComplete(int timeout) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (isMoving()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(timeout)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    return true;
}

bool BaseFocuserTask::isValidPosition(int position) const {
    return position >= limits_.first && position <= limits_.second;
}

void BaseFocuserTask::logFocuserOperation(const std::string& operation, bool success) {
    std::string status = success ? "SUCCESS" : "FAILED";
    addHistoryEntry(operation + ": " + status);
    
    if (success) {
        spdlog::debug("Focuser operation completed: {}", operation);
    } else {
        spdlog::warn("Focuser operation failed: {}", operation);
        setErrorType(TaskErrorType::DeviceError);
    }
}

FocusQuality BaseFocuserTask::assessFocusQuality(const FocusMetrics& metrics) {
    if (metrics.starCount < 3) {
        return FocusQuality::Bad;
    }
    
    if (metrics.hfr < 2.0) {
        return FocusQuality::Excellent;
    } else if (metrics.hfr < 3.0) {
        return FocusQuality::Good;
    } else if (metrics.hfr < 4.0) {
        return FocusQuality::Fair;
    } else if (metrics.hfr < 5.0) {
        return FocusQuality::Poor;
    } else {
        return FocusQuality::Bad;
    }
}

}  // namespace lithium::task::focuser
