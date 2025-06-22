#include "video_manager.hpp"
#include "hardware_interface.hpp"
#include <algorithm>

namespace lithium::device::asi::camera::components {

VideoManager::VideoManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {}

VideoManager::~VideoManager() {
    if (state_ == VideoState::STREAMING) {
        stopVideo();
    }
    cleanupResources();
}

bool VideoManager::startVideo(const VideoSettings& settings) {
    if (state_ != VideoState::IDLE) {
        return false;
    }

    if (!validateVideoSettings(settings)) {
        return false;
    }

    updateState(VideoState::STARTING);
    
    try {
        if (!configureVideoMode(settings)) {
            updateState(VideoState::ERROR);
            return false;
        }

        currentSettings_ = settings;
        maxBufferSize_ = static_cast<size_t>(settings.bufferSize);
        
        // Reset statistics
        resetStatistics();
        
        // Start worker threads
        stopRequested_ = false;
        captureThread_ = std::thread(&VideoManager::captureWorker, this);
        processingThread_ = std::thread(&VideoManager::processingWorker, this);
        statisticsThread_ = std::thread(&VideoManager::statisticsWorker, this);
        
        updateState(VideoState::STREAMING);
        return true;
        
    } catch (const std::exception&) {
        updateState(VideoState::ERROR);
        return false;
    }
}

bool VideoManager::stopVideo() {
    if (state_ != VideoState::STREAMING) {
        return false;
    }

    updateState(VideoState::STOPPING);
    
    // Signal threads to stop
    stopRequested_ = true;
    bufferCondition_.notify_all();
    
    // Wait for threads to finish
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
    if (statisticsThread_.joinable()) {
        statisticsThread_.join();
    }
    
    // Stop recording if active
    if (recording_) {
        stopRecording();
    }
    
    // Clear frame buffer
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        while (!frameBuffer_.empty()) {
            frameBuffer_.pop();
        }
    }
    
    updateState(VideoState::IDLE);
    return true;
}

std::string VideoManager::getStateString() const {
    switch (state_) {
        case VideoState::IDLE: return "IDLE";
        case VideoState::STARTING: return "STARTING";
        case VideoState::STREAMING: return "STREAMING";
        case VideoState::STOPPING: return "STOPPING";
        case VideoState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

VideoManager::VideoStatistics VideoManager::getStatistics() const {
    return statistics_;
}

void VideoManager::resetStatistics() {
    statistics_ = VideoStatistics{};
    statistics_.startTime = std::chrono::steady_clock::now();
}

std::shared_ptr<AtomCameraFrame> VideoManager::getLatestFrame() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (frameBuffer_.empty()) {
        return nullptr;
    }
    
    auto frame = frameBuffer_.front();
    frameBuffer_.pop();
    return frame;
}

bool VideoManager::hasFrameAvailable() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return !frameBuffer_.empty();
}

size_t VideoManager::getBufferSize() const {
    return maxBufferSize_;
}

size_t VideoManager::getBufferUsage() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return frameBuffer_.size();
}

VideoManager::VideoSettings VideoManager::getCurrentSettings() const {
    return currentSettings_;
}

bool VideoManager::updateSettings(const VideoSettings& settings) {
    if (state_ == VideoState::STREAMING) {
        return false; // Cannot update while streaming
    }
    return validateVideoSettings(settings);
}

bool VideoManager::updateExposure(int exposureUs) {
    if (state_ != VideoState::STREAMING) {
        return false;
    }
    
    currentSettings_.exposure = exposureUs;
    return true; // Would update hardware in real implementation
}

bool VideoManager::updateGain(int gain) {
    if (state_ != VideoState::STREAMING) {
        return false;
    }
    
    currentSettings_.gain = gain;
    return true; // Would update hardware in real implementation
}

bool VideoManager::updateFrameRate(double fps) {
    if (state_ != VideoState::STREAMING) {
        return false;
    }
    
    currentSettings_.fps = fps;
    return true; // Would update hardware in real implementation
}

bool VideoManager::startRecording(const std::string& filename, const std::string& codec) {
    if (recording_ || state_ != VideoState::STREAMING) {
        return false;
    }
    
    recordingFilename_ = filename;
    recordingCodec_ = codec;
    recordedFrames_ = 0;
    recording_ = true;
    
    return true;
}

bool VideoManager::stopRecording() {
    if (!recording_) {
        return false;
    }
    
    recording_ = false;
    recordingFilename_.clear();
    recordingCodec_.clear();
    
    return true;
}

std::string VideoManager::getRecordingFilename() const {
    return recordingFilename_;
}

void VideoManager::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

void VideoManager::setStatisticsCallback(StatisticsCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    statisticsCallback_ = std::move(callback);
}

void VideoManager::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

void VideoManager::setFrameBufferSize(size_t size) {
    maxBufferSize_ = std::max(size_t(1), size);
}

// Private implementation methods
void VideoManager::captureWorker() {
    while (!stopRequested_ && state_ == VideoState::STREAMING) {
        try {
            auto frame = captureFrame();
            if (frame) {
                processFrame(frame);
            }
        } catch (const std::exception& e) {
            notifyError(formatVideoError("Capture", e.what()));
        }
    }
}

void VideoManager::processingWorker() {
    while (!stopRequested_ && state_ == VideoState::STREAMING) {
        std::unique_lock<std::mutex> lock(bufferMutex_);
        bufferCondition_.wait(lock, [this] { 
            return !frameBuffer_.empty() || stopRequested_; 
        });
        
        if (stopRequested_) break;
        
        if (!frameBuffer_.empty()) {
            auto frame = frameBuffer_.front();
            frameBuffer_.pop();
            lock.unlock();
            
            notifyFrame(frame);
            
            if (recording_) {
                saveFrameToFile(frame);
                recordedFrames_++;
            }
        }
    }
}

void VideoManager::statisticsWorker() {
    while (!stopRequested_ && state_ == VideoState::STREAMING) {
        updateStatistics();
        notifyStatistics(statistics_);
        
        std::this_thread::sleep_for(statisticsInterval_);
    }
}

bool VideoManager::configureVideoMode(const VideoSettings& settings) {
    // Configure hardware for video mode - stub implementation
    return hardware_->isConnected();
}

std::shared_ptr<AtomCameraFrame> VideoManager::captureFrame() {
    // Stub implementation - would capture from hardware
    return nullptr;
}

void VideoManager::processFrame(std::shared_ptr<AtomCameraFrame> frame) {
    if (!frame) return;
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    if (frameBuffer_.size() >= maxBufferSize_) {
        if (dropFramesWhenFull_) {
            frameBuffer_.pop(); // Drop oldest frame
            statistics_.framesDropped++;
        } else {
            return; // Skip this frame
        }
    }
    
    frameBuffer_.push(frame);
    statistics_.framesReceived++;
    bufferCondition_.notify_one();
}

void VideoManager::updateStatistics() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - statistics_.startTime).count();
    
    if (elapsed > 0) {
        statistics_.actualFPS = static_cast<double>(statistics_.framesProcessed) * 1000.0 / elapsed;
    }
    
    statistics_.lastFrameTime = now;
}

void VideoManager::notifyFrame(std::shared_ptr<AtomCameraFrame> frame) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (frameCallback_) {
        frameCallback_(frame);
    }
    statistics_.framesProcessed++;
}

void VideoManager::notifyStatistics(const VideoStatistics& stats) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (statisticsCallback_) {
        statisticsCallback_(stats);
    }
}

void VideoManager::notifyError(const std::string& error) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error);
    }
}

void VideoManager::updateState(VideoState newState) {
    state_ = newState;
}

bool VideoManager::validateVideoSettings(const VideoSettings& settings) {
    return settings.width >= 0 && settings.height >= 0 && 
           settings.fps > 0 && settings.bufferSize > 0;
}

std::shared_ptr<AtomCameraFrame> VideoManager::createFrameFromBuffer(
    const unsigned char* buffer, const VideoSettings& settings) {
    // Stub implementation
    return nullptr;
}

size_t VideoManager::calculateFrameSize(const VideoSettings& settings) {
    // Calculate based on format and dimensions
    size_t pixelCount = static_cast<size_t>(settings.width * settings.height);
    
    if (settings.format == "RAW16") {
        return pixelCount * 2;
    } else if (settings.format == "RGB24") {
        return pixelCount * 3;
    }
    
    return pixelCount; // RAW8 or Y8
}

bool VideoManager::saveFrameToFile(std::shared_ptr<AtomCameraFrame> frame) {
    // Stub implementation for frame recording
    return frame != nullptr;
}

void VideoManager::cleanupResources() {
    // Clean up any remaining resources
}

std::string VideoManager::formatVideoError(const std::string& operation, 
                                         const std::string& error) {
    return operation + " error: " + error;
}

} // namespace lithium::device::asi::camera::components
