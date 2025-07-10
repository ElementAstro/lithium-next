#include "motion_controller.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace lithium::device::ascom::telescope::components {

MotionController::MotionController(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware),
      state_(MotionState::IDLE),
      monitorRunning_(false),
      currentSlewRateIndex_(1),
      northMoving_(false),
      southMoving_(false),
      eastMoving_(false),
      westMoving_(false) {
    
    auto logger = spdlog::get("telescope_motion");
    if (logger) {
        logger->info("ASCOM Telescope MotionController initialized");
    }
    
    // Initialize default slew rates
    initializeSlewRates();
}

MotionController::~MotionController() {
    stopMonitoring();
    
    auto logger = spdlog::get("telescope_motion");
    if (logger) {
        logger->info("ASCOM Telescope MotionController destroyed");
    }
}

// =========================================================================
// Initialization and State Management
// =========================================================================

bool MotionController::initialize() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    try {
        if (logger) logger->info("Initializing motion controller");
        
        setState(MotionState::IDLE);
        initializeSlewRates();
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to initialize motion controller: " + std::string(e.what()));
        if (logger) logger->error("Failed to initialize motion controller: {}", e.what());
        return false;
    }
}

bool MotionController::shutdown() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    try {
        if (logger) logger->info("Shutting down motion controller");
        
        stopMonitoring();
        stopAllMovement();
        setState(MotionState::IDLE);
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to shutdown motion controller: " + std::string(e.what()));
        if (logger) logger->error("Failed to shutdown motion controller: {}", e.what());
        return false;
    }
}

MotionState MotionController::getState() const {
    return state_.load();
}

bool MotionController::isMoving() const {
    MotionState currentState = state_.load();
    return currentState != MotionState::IDLE;
}

// =========================================================================
// Slew Operations
// =========================================================================

bool MotionController::slewToRADEC(double ra, double dec, bool async) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    // Basic coordinate validation
    if (ra < 0.0 || ra >= 24.0) {
        setLastError("Invalid RA coordinate");
        if (logger) logger->error("Invalid RA coordinate: {:.6f}", ra);
        return false;
    }
    
    if (dec < -90.0 || dec > 90.0) {
        setLastError("Invalid DEC coordinate");
        if (logger) logger->error("Invalid DEC coordinate: {:.6f}", dec);
        return false;
    }
    
    try {
        if (logger) logger->info("Starting slew to RA: {:.6f}h, DEC: {:.6f}° (async: {})", ra, dec, async);
        
        setState(MotionState::SLEWING);
        slewStartTime_ = std::chrono::steady_clock::now();
        
        // Implementation would command hardware to slew
        // For now, just simulate successful slew start
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to start slew: " + std::string(e.what()));
        if (logger) logger->error("Failed to start slew: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

bool MotionController::slewToAZALT(double az, double alt, bool async) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    // Basic coordinate validation
    if (az < 0.0 || az >= 360.0) {
        setLastError("Invalid AZ coordinate");
        if (logger) logger->error("Invalid AZ coordinate: {:.6f}", az);
        return false;
    }
    
    if (alt < -90.0 || alt > 90.0) {
        setLastError("Invalid ALT coordinate");
        if (logger) logger->error("Invalid ALT coordinate: {:.6f}", alt);
        return false;
    }
    
    try {
        if (logger) logger->info("Starting slew to AZ: {:.6f}°, ALT: {:.6f}° (async: {})", az, alt, async);
        
        setState(MotionState::SLEWING);
        slewStartTime_ = std::chrono::steady_clock::now();
        
        // Implementation would command hardware to slew
        // For now, just simulate successful slew start
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to start slew: " + std::string(e.what()));
        if (logger) logger->error("Failed to start slew: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

bool MotionController::isSlewing() const {
    return state_.load() == MotionState::SLEWING;
}

std::optional<double> MotionController::getSlewProgress() const {
    if (!isSlewing()) {
        return std::nullopt;
    }
    
    // For a real implementation, this would calculate actual progress
    // based on current and target positions
    return 0.5; // Placeholder
}

std::optional<double> MotionController::getSlewTimeRemaining() const {
    if (!isSlewing()) {
        return std::nullopt;
    }
    
    // For a real implementation, this would calculate remaining time
    // based on distance and slew rate
    return 10.0; // Placeholder: 10 seconds
}

bool MotionController::abortSlew() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    try {
        if (logger) logger->info("Aborting slew operation");
        
        setState(MotionState::ABORTING);
        
        // Implementation would command hardware to abort slew
        // For now, just simulate successful abort
        
        setState(MotionState::IDLE);
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to abort slew: " + std::string(e.what()));
        if (logger) logger->error("Failed to abort slew: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

// =========================================================================
// Directional Movement
// =========================================================================

bool MotionController::startDirectionalMove(const std::string& direction, double rate) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    if (!validateDirection(direction)) {
        setLastError("Invalid direction: " + direction);
        if (logger) logger->error("Invalid direction: {}", direction);
        return false;
    }
    
    if (rate <= 0.0) {
        setLastError("Invalid movement rate");
        if (logger) logger->error("Invalid movement rate: {:.6f}", rate);
        return false;
    }
    
    try {
        if (logger) logger->info("Starting {} movement at rate {:.6f}", direction, rate);
        
        // Set movement flags
        if (direction == "N") {
            northMoving_ = true;
            setState(MotionState::MOVING_NORTH);
        } else if (direction == "S") {
            southMoving_ = true;
            setState(MotionState::MOVING_SOUTH);
        } else if (direction == "E") {
            eastMoving_ = true;
            setState(MotionState::MOVING_EAST);
        } else if (direction == "W") {
            westMoving_ = true;
            setState(MotionState::MOVING_WEST);
        }
        
        // Implementation would command hardware to start movement
        // For now, just simulate successful start
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to start directional movement: " + std::string(e.what()));
        if (logger) logger->error("Failed to start directional movement: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

bool MotionController::stopDirectionalMove(const std::string& direction) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    if (!validateDirection(direction)) {
        setLastError("Invalid direction: " + direction);
        if (logger) logger->error("Invalid direction: {}", direction);
        return false;
    }
    
    try {
        if (logger) logger->info("Stopping {} movement", direction);
        
        // Clear movement flags
        if (direction == "N") {
            northMoving_ = false;
        } else if (direction == "S") {
            southMoving_ = false;
        } else if (direction == "E") {
            eastMoving_ = false;
        } else if (direction == "W") {
            westMoving_ = false;
        }
        
        // Update state based on remaining movements
        updateMotionState();
        
        // Implementation would command hardware to stop movement
        // For now, just simulate successful stop
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to stop directional movement: " + std::string(e.what()));
        if (logger) logger->error("Failed to stop directional movement: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

bool MotionController::stopAllMovement() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        if (logger) logger->error("Hardware interface not available");
        return false;
    }
    
    try {
        if (logger) logger->info("Stopping all movement");
        
        // Clear all movement flags
        northMoving_ = false;
        southMoving_ = false;
        eastMoving_ = false;
        westMoving_ = false;
        
        setState(MotionState::IDLE);
        
        // Implementation would command hardware to stop all movement
        // For now, just simulate successful stop
        
        clearError();
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Failed to stop all movement: " + std::string(e.what()));
        if (logger) logger->error("Failed to stop all movement: {}", e.what());
        setState(MotionState::ERROR);
        return false;
    }
}

bool MotionController::emergencyStop() {
    auto logger = spdlog::get("telescope_motion");
    
    if (logger) logger->warn("Emergency stop initiated");
    
    // Emergency stop should not fail - clear all flags immediately
    northMoving_ = false;
    southMoving_ = false;
    eastMoving_ = false;
    westMoving_ = false;
    
    setState(MotionState::IDLE);
    
    // Implementation would command immediate hardware stop
    // For now, just simulate successful emergency stop
    
    return true;
}

// =========================================================================
// Slew Rate Management
// =========================================================================

std::optional<double> MotionController::getCurrentSlewRate() const {
    std::lock_guard<std::mutex> lock(slewRateMutex_);
    
    int index = currentSlewRateIndex_.load();
    if (index >= 0 && index < static_cast<int>(availableSlewRates_.size())) {
        return availableSlewRates_[index];
    }
    return std::nullopt;
}

bool MotionController::setSlewRate(double rate) {
    std::lock_guard<std::mutex> lock(slewRateMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    // Find closest available rate
    auto it = std::min_element(availableSlewRates_.begin(), availableSlewRates_.end(),
        [rate](double a, double b) {
            return std::abs(a - rate) < std::abs(b - rate);
        });
    
    if (it != availableSlewRates_.end()) {
        int index = std::distance(availableSlewRates_.begin(), it);
        currentSlewRateIndex_ = index;
        
        if (logger) logger->info("Slew rate set to {:.6f} (index {})", *it, index);
        return true;
    }
    
    if (logger) logger->error("Failed to set slew rate: {:.6f}", rate);
    return false;
}

std::vector<double> MotionController::getAvailableSlewRates() const {
    std::lock_guard<std::mutex> lock(slewRateMutex_);
    return availableSlewRates_;
}

bool MotionController::setSlewRateIndex(int index) {
    std::lock_guard<std::mutex> lock(slewRateMutex_);
    
    auto logger = spdlog::get("telescope_motion");
    
    if (index >= 0 && index < static_cast<int>(availableSlewRates_.size())) {
        currentSlewRateIndex_ = index;
        
        if (logger) logger->info("Slew rate index set to {} (rate: {:.6f})", 
                                 index, availableSlewRates_[index]);
        return true;
    }
    
    if (logger) logger->error("Invalid slew rate index: {}", index);
    return false;
}

std::optional<int> MotionController::getCurrentSlewRateIndex() const {
    int index = currentSlewRateIndex_.load();
    if (index >= 0 && index < static_cast<int>(availableSlewRates_.size())) {
        return index;
    }
    return std::nullopt;
}

// =========================================================================
// Motion Monitoring
// =========================================================================

bool MotionController::startMonitoring() {
    if (monitorRunning_.load()) {
        return true; // Already running
    }
    
    auto logger = spdlog::get("telescope_motion");
    
    try {
        monitorRunning_ = true;
        monitorThread_ = std::make_unique<std::thread>(&MotionController::monitoringLoop, this);
        
        if (logger) logger->info("Motion monitoring started");
        return true;
        
    } catch (const std::exception& e) {
        monitorRunning_ = false;
        if (logger) logger->error("Failed to start motion monitoring: {}", e.what());
        return false;
    }
}

bool MotionController::stopMonitoring() {
    if (!monitorRunning_.load()) {
        return true; // Already stopped
    }
    
    auto logger = spdlog::get("telescope_motion");
    
    monitorRunning_ = false;
    
    if (monitorThread_ && monitorThread_->joinable()) {
        monitorThread_->join();
        monitorThread_.reset();
    }
    
    if (logger) logger->info("Motion monitoring stopped");
    return true;
}

bool MotionController::isMonitoring() const {
    return monitorRunning_.load();
}

void MotionController::setMotionUpdateCallback(std::function<void(MotionState)> callback) {
    motionUpdateCallback_ = std::move(callback);
}

// =========================================================================
// Status and Information
// =========================================================================

std::string MotionController::getMotionStatus() const {
    MotionState currentState = state_.load();
    
    switch (currentState) {
        case MotionState::IDLE: return "Idle";
        case MotionState::SLEWING: return "Slewing";
        case MotionState::TRACKING: return "Tracking";
        case MotionState::MOVING_NORTH: return "Moving North";
        case MotionState::MOVING_SOUTH: return "Moving South";
        case MotionState::MOVING_EAST: return "Moving East";
        case MotionState::MOVING_WEST: return "Moving West";
        case MotionState::ABORTING: return "Aborting";
        case MotionState::ERROR: return "Error";
        default: return "Unknown";
    }
}

std::string MotionController::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void MotionController::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

// =========================================================================
// Private Methods
// =========================================================================

void MotionController::setState(MotionState newState) {
    MotionState oldState = state_.exchange(newState);
    
    if (oldState != newState && motionUpdateCallback_) {
        motionUpdateCallback_(newState);
    }
}

void MotionController::setLastError(const std::string& error) const {
    lastError_ = error;
}

void MotionController::monitoringLoop() {
    auto logger = spdlog::get("telescope_motion");
    
    while (monitorRunning_.load()) {
        try {
            updateMotionState();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            if (logger) logger->error("Error in monitoring loop: {}", e.what());
        }
    }
}

void MotionController::updateMotionState() {
    MotionState currentState = determineCurrentState();
    setState(currentState);
}

bool MotionController::validateDirection(const std::string& direction) const {
    return direction == "N" || direction == "S" || direction == "E" || direction == "W";
}

bool MotionController::initializeSlewRates() {
    std::lock_guard<std::mutex> lock(slewRateMutex_);
    
    // Initialize default slew rates (degrees per second)
    availableSlewRates_ = {0.5, 1.0, 2.0, 5.0, 10.0, 20.0};
    currentSlewRateIndex_ = 1; // Default to 1.0 degrees/second
    
    return true;
}

MotionState MotionController::determineCurrentState() const {
    if (northMoving_.load()) return MotionState::MOVING_NORTH;
    if (southMoving_.load()) return MotionState::MOVING_SOUTH;
    if (eastMoving_.load()) return MotionState::MOVING_EAST;
    if (westMoving_.load()) return MotionState::MOVING_WEST;
    
    // If no directional movement, check other states
    MotionState currentState = state_.load();
    if (currentState == MotionState::SLEWING || 
        currentState == MotionState::TRACKING ||
        currentState == MotionState::ABORTING ||
        currentState == MotionState::ERROR) {
        return currentState;
    }
    
    return MotionState::IDLE;
}

} // namespace lithium::device::ascom::telescope::components
