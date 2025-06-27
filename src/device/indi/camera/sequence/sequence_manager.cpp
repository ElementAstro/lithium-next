#include "sequence_manager.hpp"
#include "../core/indi_camera_core.hpp"
#include "../exposure/exposure_controller.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi::camera {

SequenceManager::SequenceManager(std::shared_ptr<INDICameraCore> core) 
    : ComponentBase(core) {
    spdlog::debug("Creating sequence manager");
}

SequenceManager::~SequenceManager() {
    if (isSequenceRunning()) {
        stopSequence();
    }
}

auto SequenceManager::initialize() -> bool {
    spdlog::debug("Initializing sequence manager");
    
    // Reset sequence state
    isSequenceRunning_.store(false);
    sequenceCount_.store(0);
    sequenceTotal_.store(0);
    sequenceExposure_.store(1.0);
    sequenceInterval_.store(0.0);
    stopSequenceFlag_.store(false);
    
    return true;
}

auto SequenceManager::destroy() -> bool {
    spdlog::debug("Destroying sequence manager");
    
    // Stop any running sequence
    if (isSequenceRunning()) {
        stopSequence();
    }
    
    // Wait for thread to finish
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
    
    return true;
}

auto SequenceManager::getComponentName() const -> std::string {
    return "SequenceManager";
}

auto SequenceManager::handleProperty(INDI::Property property) -> bool {
    // Sequence manager typically doesn't handle INDI properties directly
    // It coordinates with other components instead
    return false;
}

auto SequenceManager::startSequence(int count, double exposure, double interval) -> bool {
    if (isSequenceRunning()) {
        spdlog::warn("Sequence already running");
        return false;
    }
    
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }
    
    if (!exposureController_) {
        spdlog::error("Exposure controller not set");
        return false;
    }
    
    if (count <= 0 || exposure <= 0) {
        spdlog::error("Invalid sequence parameters: count={}, exposure={}", count, exposure);
        return false;
    }
    
    spdlog::info("Starting sequence: {} frames, {} second exposures, {} second intervals", 
                 count, exposure, interval);
    
    // Set sequence parameters
    sequenceTotal_.store(count);
    sequenceCount_.store(0);
    sequenceExposure_.store(exposure);
    sequenceInterval_.store(interval);
    isSequenceRunning_.store(true);
    stopSequenceFlag_.store(false);
    
    sequenceStartTime_ = std::chrono::system_clock::now();
    
    // Start sequence worker thread
    sequenceThread_ = std::thread(&SequenceManager::sequenceWorker, this);
    
    return true;
}

auto SequenceManager::stopSequence() -> bool {
    if (!isSequenceRunning()) {
        spdlog::warn("No sequence running");
        return false;
    }
    
    spdlog::info("Stopping sequence...");
    
    // Signal stop to worker thread
    stopSequenceFlag_.store(true);
    isSequenceRunning_.store(false);
    
    // Abort current exposure if in progress
    if (exposureController_ && exposureController_->isExposing()) {
        exposureController_->abortExposure();
    }
    
    // Wait for worker thread to finish
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
    
    // Call completion callback with failure status
    if (completeCallback_) {
        completeCallback_(false);
    }
    
    spdlog::info("Sequence stopped");
    return true;
}

auto SequenceManager::isSequenceRunning() const -> bool {
    return isSequenceRunning_.load();
}

auto SequenceManager::getSequenceProgress() const -> std::pair<int, int> {
    return {sequenceCount_.load(), sequenceTotal_.load()};
}

auto SequenceManager::setSequenceCallback(
    std::function<void(int, std::shared_ptr<AtomCameraFrame>)> callback) -> void {
    frameCallback_ = std::move(callback);
}

auto SequenceManager::setSequenceCompleteCallback(
    std::function<void(bool)> callback) -> void {
    completeCallback_ = std::move(callback);
}

auto SequenceManager::setExposureController(ExposureController* controller) -> void {
    exposureController_ = controller;
}

// Private methods
void SequenceManager::sequenceWorker() {
    spdlog::debug("Sequence worker thread started");
    
    int totalFrames = sequenceTotal_.load();
    double exposureTime = sequenceExposure_.load();
    double interval = sequenceInterval_.load();
    
    try {
        for (int i = 0; i < totalFrames && !stopSequenceFlag_.load(); ++i) {
            sequenceCount_.store(i + 1);
            
            spdlog::info("Capturing frame {}/{}", i + 1, totalFrames);
            
            // Execute sequence step
            if (!executeSequenceStep(i + 1)) {
                spdlog::error("Failed to capture frame {}", i + 1);
                break;
            }
            
            // Handle interval between frames (except for last frame)
            if (i < totalFrames - 1 && interval > 0 && !stopSequenceFlag_.load()) {
                spdlog::debug("Waiting {} seconds before next frame", interval);
                
                auto intervalMs = static_cast<int>(interval * 1000);
                auto sleepStart = std::chrono::steady_clock::now();
                
                // Sleep in small chunks to allow for early termination
                while (intervalMs > 0 && !stopSequenceFlag_.load()) {
                    int chunkMs = std::min(intervalMs, 100); // 100ms chunks
                    std::this_thread::sleep_for(std::chrono::milliseconds(chunkMs));
                    intervalMs -= chunkMs;
                }
            }
        }
        
        // Check if sequence completed successfully
        bool success = (sequenceCount_.load() >= totalFrames) && !stopSequenceFlag_.load();
        
        if (success) {
            spdlog::info("Sequence completed successfully: {}/{} frames", 
                         sequenceCount_.load(), totalFrames);
        } else {
            spdlog::warn("Sequence terminated early: {}/{} frames", 
                         sequenceCount_.load(), totalFrames);
        }
        
        // Call completion callback
        if (completeCallback_) {
            completeCallback_(success);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Sequence worker thread error: {}", e.what());
        if (completeCallback_) {
            completeCallback_(false);
        }
    }
    
    isSequenceRunning_.store(false);
    spdlog::debug("Sequence worker thread finished");
}

auto SequenceManager::executeSequenceStep(int currentFrame) -> bool {
    if (!exposureController_) {
        return false;
    }
    
    double exposureTime = sequenceExposure_.load();
    
    // Start exposure
    if (!exposureController_->startExposure(exposureTime)) {
        spdlog::error("Failed to start exposure for frame {}", currentFrame);
        return false;
    }
    
    // Wait for exposure to complete
    if (!waitForExposureComplete()) {
        spdlog::error("Exposure failed or was aborted for frame {}", currentFrame);
        return false;
    }
    
    // Get the captured frame
    auto frame = exposureController_->getExposureResult();
    if (!frame) {
        spdlog::error("No frame data received for frame {}", currentFrame);
        return false;
    }
    
    // Update capture timestamp
    lastSequenceCapture_ = std::chrono::system_clock::now();
    
    // Call frame callback if set
    if (frameCallback_) {
        frameCallback_(currentFrame, frame);
    }
    
    spdlog::info("Frame {} captured successfully", currentFrame);
    return true;
}

auto SequenceManager::waitForExposureComplete() -> bool {
    if (!exposureController_) {
        return false;
    }
    
    // Wait for exposure to start
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!exposureController_->isExposing() && 
           std::chrono::steady_clock::now() < timeout && 
           !stopSequenceFlag_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (!exposureController_->isExposing()) {
        spdlog::error("Exposure failed to start within timeout");
        return false;
    }
    
    // Wait for exposure to complete
    while (exposureController_->isExposing() && !stopSequenceFlag_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Check if we were stopped
    if (stopSequenceFlag_.load()) {
        return false;
    }
    
    // Give a short time for image download
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return true;
}

} // namespace lithium::device::indi::camera
