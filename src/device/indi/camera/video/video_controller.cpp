#include "video_controller.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::indi::camera {

VideoController::VideoController(std::shared_ptr<INDICameraCore> core) 
    : ComponentBase(core) {
    spdlog::debug("Creating video controller");
    setupVideoFormats();
}

auto VideoController::initialize() -> bool {
    spdlog::debug("Initializing video controller");
    
    // Reset video state
    isVideoRunning_.store(false);
    isVideoRecording_.store(false);
    videoExposure_.store(0.033); // 30 FPS default
    videoGain_.store(0);
    
    // Reset statistics
    totalFramesReceived_.store(0);
    droppedFrames_.store(0);
    averageFrameRate_.store(0.0);
    
    return true;
}

auto VideoController::destroy() -> bool {
    spdlog::debug("Destroying video controller");
    
    // Stop video if running
    if (isVideoRunning()) {
        stopVideo();
    }
    
    // Stop recording if active
    if (isVideoRecording()) {
        stopVideoRecording();
    }
    
    return true;
}

auto VideoController::getComponentName() const -> std::string {
    return "VideoController";
}

auto VideoController::handleProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        return false;
    }
    
    std::string propertyName = property.getName();
    
    if (propertyName == "CCD_VIDEO_STREAM") {
        handleVideoStreamProperty(property);
        return true;
    } else if (propertyName == "CCD_VIDEO_FORMAT") {
        handleVideoFormatProperty(property);
        return true;
    }
    
    return false;
}

auto VideoController::startVideo() -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdVideo = device.getProperty("CCD_VIDEO_STREAM");
        if (!ccdVideo.isValid()) {
            spdlog::error("CCD_VIDEO_STREAM property not found");
            return false;
        }

        spdlog::info("Starting video stream...");
        ccdVideo[0].setState(ISS_ON);
        getCore()->sendNewProperty(ccdVideo);
        isVideoRunning_.store(true);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start video: {}", e.what());
        return false;
    }
}

auto VideoController::stopVideo() -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdVideo = device.getProperty("CCD_VIDEO_STREAM");
        if (!ccdVideo.isValid()) {
            spdlog::error("CCD_VIDEO_STREAM property not found");
            return false;
        }

        spdlog::info("Stopping video stream...");
        ccdVideo[0].setState(ISS_OFF);
        getCore()->sendNewProperty(ccdVideo);
        isVideoRunning_.store(false);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to stop video: {}", e.what());
        return false;
    }
}

auto VideoController::isVideoRunning() const -> bool {
    return isVideoRunning_.load();
}

auto VideoController::getVideoFrame() -> std::shared_ptr<AtomCameraFrame> {
    // Return current frame - in video mode this is continuously updated
    auto frame = getCore()->getCurrentFrame();
    if (frame) {
        updateFrameRate();
        totalFramesReceived_.fetch_add(1);
    }
    return frame;
}

auto VideoController::setVideoFormat(const std::string& format) -> bool {
    // Check if format is supported
    auto it = std::find(videoFormats_.begin(), videoFormats_.end(), format);
    if (it == videoFormats_.end()) {
        spdlog::error("Unsupported video format: {}", format);
        return false;
    }

    currentVideoFormat_ = format;
    spdlog::info("Video format set to: {}", format);
    
    // Here we could set INDI property if the driver supports it
    return true;
}

auto VideoController::getVideoFormats() -> std::vector<std::string> {
    return videoFormats_;
}

auto VideoController::startVideoRecording(const std::string& filename) -> bool {
    if (!isVideoRunning()) {
        spdlog::error("Video streaming not active");
        return false;
    }
    
    if (isVideoRecording()) {
        spdlog::warn("Video recording already active");
        return false;
    }
    
    videoRecordingFile_ = filename;
    isVideoRecording_.store(true);
    
    spdlog::info("Started video recording to: {}", filename);
    return true;
}

auto VideoController::stopVideoRecording() -> bool {
    if (!isVideoRecording()) {
        spdlog::warn("Video recording not active");
        return false;
    }
    
    isVideoRecording_.store(false);
    
    spdlog::info("Stopped video recording: {}", videoRecordingFile_);
    videoRecordingFile_.clear();
    
    return true;
}

auto VideoController::isVideoRecording() const -> bool {
    return isVideoRecording_.load();
}

auto VideoController::setVideoExposure(double exposure) -> bool {
    if (exposure <= 0) {
        spdlog::error("Invalid video exposure value: {}", exposure);
        return false;
    }
    
    videoExposure_.store(exposure);
    spdlog::info("Video exposure set to: {} seconds", exposure);
    
    // Here we could set INDI property if the driver supports it
    return true;
}

auto VideoController::getVideoExposure() const -> double {
    return videoExposure_.load();
}

auto VideoController::setVideoGain(int gain) -> bool {
    if (gain < 0) {
        spdlog::error("Invalid video gain value: {}", gain);
        return false;
    }
    
    videoGain_.store(gain);
    spdlog::info("Video gain set to: {}", gain);
    
    // Here we could set INDI property if the driver supports it
    return true;
}

auto VideoController::getVideoGain() const -> int {
    return videoGain_.load();
}

auto VideoController::getTotalFramesReceived() const -> uint64_t {
    return totalFramesReceived_.load();
}

auto VideoController::getDroppedFrames() const -> uint64_t {
    return droppedFrames_.load();
}

auto VideoController::getAverageFrameRate() const -> double {
    return averageFrameRate_.load();
}

// Private methods
void VideoController::handleVideoStreamProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch videoProperty = property;
    if (!videoProperty.isValid()) {
        return;
    }
    
    if (videoProperty[0].getState() == ISS_ON) {
        isVideoRunning_.store(true);
        spdlog::debug("Video stream started");
    } else {
        isVideoRunning_.store(false);
        spdlog::debug("Video stream stopped");
    }
}

void VideoController::handleVideoFormatProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch formatProperty = property;
    if (!formatProperty.isValid()) {
        return;
    }
    
    // Find which format is selected
    for (int i = 0; i < formatProperty.size(); i++) {
        if (formatProperty[i].getState() == ISS_ON) {
            std::string format = formatProperty[i].getName();
            if (std::find(videoFormats_.begin(), videoFormats_.end(), format) 
                != videoFormats_.end()) {
                currentVideoFormat_ = format;
                spdlog::debug("Video format changed to: {}", format);
            }
            break;
        }
    }
}

void VideoController::setupVideoFormats() {
    videoFormats_ = {"MJPEG", "RAW8", "RAW16", "H264"};
    currentVideoFormat_ = "MJPEG";
    spdlog::debug("Video formats initialized");
}

void VideoController::updateFrameRate() {
    auto now = std::chrono::system_clock::now();
    if (lastFrameTime_.time_since_epoch().count() > 0) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastFrameTime_).count();
        if (duration > 0) {
            double frameRate = 1000.0 / duration;
            // Simple moving average
            double current = averageFrameRate_.load();
            averageFrameRate_.store((current * 0.9) + (frameRate * 0.1));
        }
    }
    lastFrameTime_ = now;
}

void VideoController::recordVideoFrame(std::shared_ptr<AtomCameraFrame> frame) {
    if (!isVideoRecording() || !frame) {
        return;
    }
    
    // Here we would implement actual video recording to file
    // For now, just log that a frame was recorded
    spdlog::debug("Recording video frame: {} bytes", frame->size);
}

} // namespace lithium::device::indi::camera
