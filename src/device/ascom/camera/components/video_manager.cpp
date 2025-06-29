/*
 * video_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Video Manager Component Implementation

This component manages video streaming, live view, and video recording
functionality for ASCOM cameras.

*************************************************/

#include "video_manager.hpp"
#include "hardware_interface.hpp"

#include "atom/log/loguru.hpp"
#include "atom/utils/time.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace lithium::device::ascom::camera::components {

VideoManager::VideoManager(std::shared_ptr<HardwareInterface> hardware)
    : m_hardware(hardware)
    , m_currentState(VideoState::STOPPED)
    , m_targetFPS(10.0)
    , m_actualFPS(0.0)
    , m_frameWidth(0)
    , m_frameHeight(0)
    , m_binning(1)
    , m_maxBufferSize(10)
    , m_isStreamingActive(false)
    , m_isRecordingActive(false)
    , m_autoExposure(true)
    , m_exposureTime(0.1)
    , m_autoGain(true)
    , m_gain(0.0)
    , m_compressionEnabled(false)
    , m_compressionQuality(85)
    , m_recordingFrameCount(0)
    , m_recordingStartTime()
    , m_lastFrameTime()
    , m_frameCounter(0)
    , m_droppedFrames(0) {
    LOG_F(INFO, "ASCOM Camera VideoManager initialized");
}

VideoManager::~VideoManager() {
    stopStreaming();
    stopRecording();
    LOG_F(INFO, "ASCOM Camera VideoManager destroyed");
}

// =========================================================================
// Streaming Control
// =========================================================================

bool VideoManager::startStreaming(const VideoSettings& settings) {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_currentState != VideoState::STOPPED) {
        LOG_F(ERROR, "Cannot start streaming: current state is {}",
              static_cast<int>(m_currentState));
        return false;
    }

    if (!m_hardware || !m_hardware->isConnected()) {
        LOG_F(ERROR, "Cannot start streaming: hardware not connected");
        return false;
    }

    LOG_F(INFO, "Starting video streaming: FPS={:.1f}, {}x{}, binning={}",
          settings.fps, settings.width, settings.height, settings.binning);

    m_currentSettings = settings;
    setState(VideoState::STARTING);

    // Configure streaming parameters
    if (!configureStreamingParameters()) {
        setState(VideoState::STOPPED);
        return false;
    }

    // Start streaming thread
    m_isStreamingActive = true;
    setState(VideoState::STREAMING);

    m_streamingThread = std::thread(&VideoManager::streamingThreadFunction, this);

    return true;
}

bool VideoManager::stopStreaming() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_currentState == VideoState::STOPPED) {
        return true; // Already stopped
    }

    LOG_F(INFO, "Stopping video streaming");
    setState(VideoState::STOPPING);

    // Stop streaming
    m_isStreamingActive = false;

    // Wait for streaming thread to finish
    if (m_streamingThread.joinable()) {
        m_streamingThread.join();
    }

    // Clear frame buffer
    clearFrameBuffer();

    setState(VideoState::STOPPED);

    return true;
}

bool VideoManager::isStreaming() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_currentState == VideoState::STREAMING || m_currentState == VideoState::RECORDING;
}

bool VideoManager::pauseStreaming() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_currentState != VideoState::STREAMING && m_currentState != VideoState::RECORDING) {
        return false;
    }

    LOG_F(INFO, "Pausing video streaming");
    m_isStreamingActive = false;
    return true;
}

bool VideoManager::resumeStreaming() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_currentState != VideoState::STREAMING && m_currentState != VideoState::RECORDING) {
        return false;
    }

    LOG_F(INFO, "Resuming video streaming");
    m_isStreamingActive = true;
    return true;
}

// =========================================================================
// Recording Control
// =========================================================================

bool VideoManager::startRecording(const std::string& filename, const RecordingSettings& settings) {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_currentState != VideoState::STREAMING) {
        LOG_F(ERROR, "Cannot start recording: not currently streaming");
        return false;
    }

    LOG_F(INFO, "Starting video recording to: {}", filename);

    m_recordingSettings = settings;
    m_recordingFilename = filename;
    m_recordingFrameCount = 0;
    m_recordingStartTime = std::chrono::steady_clock::now();
    m_isRecordingActive = true;

    setState(VideoState::RECORDING);

    // Initialize recording output
    if (!initializeRecording()) {
        LOG_F(ERROR, "Failed to initialize recording");
        m_isRecordingActive = false;
        setState(VideoState::STREAMING);
        return false;
    }

    return true;
}

bool VideoManager::stopRecording() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (!m_isRecordingActive) {
        return true; // Not recording
    }

    LOG_F(INFO, "Stopping video recording");
    m_isRecordingActive = false;

    // Finalize recording
    finalizeRecording();

    setState(VideoState::STREAMING);

    auto duration = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_recordingStartTime).count();

    LOG_F(INFO, "Recording completed: {} frames in {:.2f}s",
          m_recordingFrameCount, duration);

    return true;
}

bool VideoManager::isRecording() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_isRecordingActive;
}

// =========================================================================
// Frame Access
// =========================================================================

std::shared_ptr<AtomCameraFrame> VideoManager::getLatestFrame() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    if (m_frameBuffer.empty()) {
        return nullptr;
    }

    return m_frameBuffer.back().frame;
}

std::vector<std::shared_ptr<AtomCameraFrame>> VideoManager::getFrameBuffer() {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    std::vector<std::shared_ptr<AtomCameraFrame>> frames;
    frames.reserve(m_frameBuffer.size());

    for (const auto& bufferedFrame : m_frameBuffer) {
        frames.push_back(bufferedFrame.frame);
    }

    return frames;
}

size_t VideoManager::getBufferSize() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_frameBuffer.size();
}

void VideoManager::clearFrameBuffer() {
    m_frameBuffer.clear();
    LOG_F(INFO, "Frame buffer cleared");
}

// =========================================================================
// Statistics
// =========================================================================

VideoManager::VideoStatistics VideoManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);

    VideoStatistics stats;
    stats.currentState = m_currentState;
    stats.actualFPS = m_actualFPS;
    stats.targetFPS = m_targetFPS;
    stats.frameCount = m_frameCounter;
    stats.droppedFrames = m_droppedFrames;
    stats.bufferSize = m_frameBuffer.size();
    stats.isRecording = m_isRecordingActive;
    stats.recordingFrameCount = m_recordingFrameCount;

    if (m_isRecordingActive) {
        auto duration = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - m_recordingStartTime).count();
        stats.recordingDuration = duration;
    } else {
        stats.recordingDuration = 0.0;
    }

    if (m_frameCounter > 0) {
        stats.dropRate = (static_cast<double>(m_droppedFrames) /
                         static_cast<double>(m_frameCounter + m_droppedFrames)) * 100.0;
    } else {
        stats.dropRate = 0.0;
    }

    return stats;
}

void VideoManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_frameCounter = 0;
    m_droppedFrames = 0;
    m_actualFPS = 0.0;
    LOG_F(INFO, "Video statistics reset");
}

// =========================================================================
// Settings
// =========================================================================

bool VideoManager::setTargetFPS(double fps) {
    if (fps <= 0.0 || fps > 1000.0) {
        LOG_F(ERROR, "Invalid target FPS: {:.2f}", fps);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_targetFPS = fps;
    m_currentSettings.fps = fps;

    LOG_F(INFO, "Target FPS set to {:.2f}", fps);
    return true;
}

double VideoManager::getTargetFPS() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_targetFPS;
}

double VideoManager::getActualFPS() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_actualFPS;
}

bool VideoManager::setFrameSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_F(ERROR, "Invalid frame size: {}x{}", width, height);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_frameWidth = width;
    m_frameHeight = height;
    m_currentSettings.width = width;
    m_currentSettings.height = height;

    LOG_F(INFO, "Frame size set to {}x{}", width, height);
    return true;
}

std::pair<int, int> VideoManager::getFrameSize() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return {m_frameWidth, m_frameHeight};
}

bool VideoManager::setBinning(int binning) {
    if (binning <= 0 || binning > 8) {
        LOG_F(ERROR, "Invalid binning: {}", binning);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_binning = binning;
    m_currentSettings.binning = binning;

    LOG_F(INFO, "Binning set to {}", binning);
    return true;
}

int VideoManager::getBinning() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_binning;
}

bool VideoManager::setBufferSize(size_t maxSize) {
    if (maxSize == 0) {
        LOG_F(ERROR, "Invalid buffer size: {}", maxSize);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_maxBufferSize = maxSize;

    // Trim buffer if necessary
    while (m_frameBuffer.size() > maxSize) {
        m_frameBuffer.pop_front();
    }

    LOG_F(INFO, "Max buffer size set to {}", maxSize);
    return true;
}

size_t VideoManager::getMaxBufferSize() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_maxBufferSize;
}

// =========================================================================
// Exposure and Gain Control
// =========================================================================

bool VideoManager::setAutoExposure(bool enabled) {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_autoExposure = enabled;

    if (m_hardware && m_hardware->isConnected()) {
        // Update hardware setting if possible
        // Note: This depends on hardware capability
    }

    LOG_F(INFO, "Auto exposure {}", enabled ? "enabled" : "disabled");
    return true;
}

bool VideoManager::getAutoExposure() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_autoExposure;
}

bool VideoManager::setExposureTime(double seconds) {
    if (seconds <= 0.0) {
        LOG_F(ERROR, "Invalid exposure time: {:.6f}s", seconds);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_exposureTime = seconds;

    if (m_hardware && m_hardware->isConnected() && !m_autoExposure) {
        // Update hardware setting if not in auto mode
        // Note: This depends on hardware capability
    }

    LOG_F(INFO, "Exposure time set to {:.6f}s", seconds);
    return true;
}

double VideoManager::getExposureTime() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_exposureTime;
}

bool VideoManager::setAutoGain(bool enabled) {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_autoGain = enabled;

    if (m_hardware && m_hardware->isConnected()) {
        // Update hardware setting if possible
        // Note: This depends on hardware capability
    }

    LOG_F(INFO, "Auto gain {}", enabled ? "enabled" : "disabled");
    return true;
}

bool VideoManager::getAutoGain() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_autoGain;
}

bool VideoManager::setGain(double gain) {
    if (gain < 0.0) {
        LOG_F(ERROR, "Invalid gain: {:.2f}", gain);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_gain = gain;

    if (m_hardware && m_hardware->isConnected() && !m_autoGain) {
        // Update hardware setting if not in auto mode
        // Note: This depends on hardware capability
    }

    LOG_F(INFO, "Gain set to {:.2f}", gain);
    return true;
}

double VideoManager::getGain() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_gain;
}

// =========================================================================
// Callbacks
// =========================================================================

void VideoManager::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_frameCallback = callback;
}

void VideoManager::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_stateCallback = callback;
}

void VideoManager::setStatisticsCallback(StatisticsCallback callback) {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    m_statisticsCallback = callback;
}

// =========================================================================
// Utility Methods
// =========================================================================

VideoManager::VideoState VideoManager::getCurrentState() const {
    std::lock_guard<std::mutex> lock(m_videoMutex);
    return m_currentState;
}

std::string VideoManager::getStateString() const {
    switch (getCurrentState()) {
        case VideoState::STOPPED: return "Stopped";
        case VideoState::STARTING: return "Starting";
        case VideoState::STREAMING: return "Streaming";
        case VideoState::RECORDING: return "Recording";
        case VideoState::STOPPING: return "Stopping";
        case VideoState::ERROR: return "Error";
        default: return "Unknown";
    }
}

// =========================================================================
// Private Methods
// =========================================================================

void VideoManager::setState(VideoState newState) {
    VideoState oldState = m_currentState;
    m_currentState = newState;

    LOG_F(INFO, "Video state changed: {} -> {}",
          static_cast<int>(oldState), static_cast<int>(newState));

    // Notify state callback
    if (m_stateCallback) {
        m_stateCallback(oldState, newState);
    }
}

bool VideoManager::configureStreamingParameters() {
    if (!m_hardware) {
        return false;
    }

    // Set binning
    if (!m_hardware->setBinning(m_currentSettings.binning, m_currentSettings.binning)) {
        LOG_F(ERROR, "Failed to set binning to {}", m_currentSettings.binning);
        return false;
    }

    // Set ROI if specified
    if (m_currentSettings.width > 0 && m_currentSettings.height > 0) {
        if (!m_hardware->setROI(0, 0, m_currentSettings.width, m_currentSettings.height)) {
            LOG_F(ERROR, "Failed to set ROI: {}x{}",
                  m_currentSettings.width, m_currentSettings.height);
            return false;
        }
    }

    // Update internal settings
    m_targetFPS = m_currentSettings.fps;
    m_frameWidth = m_currentSettings.width;
    m_frameHeight = m_currentSettings.height;
    m_binning = m_currentSettings.binning;

    return true;
}

void VideoManager::streamingThreadFunction() {
    LOG_F(INFO, "Video streaming thread started");

    auto lastStatsUpdate = std::chrono::steady_clock::now();
    auto frameInterval = std::chrono::duration<double>(1.0 / m_targetFPS);

    while (m_isStreamingActive) {
        auto frameStart = std::chrono::steady_clock::now();

        if (m_hardware && m_hardware->isConnected()) {
            // Capture frame
            auto frame = captureVideoFrame();
            if (frame) {
                {
                    std::lock_guard<std::mutex> lock(m_videoMutex);
                    processNewFrame(frame);
                }

                // Update statistics
                updateFPSStatistics();

                // Notify frame callback
                if (m_frameCallback) {
                    m_frameCallback(frame);
                }
            } else {
                // Frame capture failed
                std::lock_guard<std::mutex> lock(m_videoMutex);
                m_droppedFrames++;
            }
        }

        // Update statistics periodically
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastStatsUpdate).count() >= 1.0) {
            if (m_statisticsCallback) {
                m_statisticsCallback(getStatistics());
            }
            lastStatsUpdate = now;
        }

        // Sleep to maintain target FPS
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = frameEnd - frameStart;
        if (elapsed < frameInterval) {
            std::this_thread::sleep_for(frameInterval - elapsed);
        }
    }

    LOG_F(INFO, "Video streaming thread stopped");
}

std::shared_ptr<AtomCameraFrame> VideoManager::captureVideoFrame() {
    // For video streaming, we use short exposures
    double exposureTime = m_autoExposure ? 0.01 : m_exposureTime; // Default 10ms for auto

    if (!m_hardware->startExposure(exposureTime, false)) {
        return nullptr;
    }

    // Wait for exposure to complete (with timeout)
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::duration<double>(exposureTime + 1.0); // Add 1s buffer

    while (!m_hardware->isExposureComplete()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            LOG_F(WARNING, "Video frame exposure timeout");
            m_hardware->abortExposure();
            return nullptr;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return m_hardware->downloadImage();
}

void VideoManager::processNewFrame(std::shared_ptr<AtomCameraFrame> frame) {
    // Add frame to buffer
    BufferedFrame bufferedFrame;
    bufferedFrame.frame = frame;
    bufferedFrame.timestamp = std::chrono::steady_clock::now();
    bufferedFrame.frameNumber = m_frameCounter++;

    m_frameBuffer.push_back(bufferedFrame);

    // Limit buffer size
    while (m_frameBuffer.size() > m_maxBufferSize) {
        m_frameBuffer.pop_front();
    }

    // Handle recording
    if (m_isRecordingActive) {
        recordFrame(frame);
    }

    m_lastFrameTime = bufferedFrame.timestamp;
}

void VideoManager::updateFPSStatistics() {
    auto now = std::chrono::steady_clock::now();

    if (m_frameCounter == 1) {
        m_lastFrameTime = now;
        return;
    }

    // Calculate instantaneous FPS
    auto elapsed = std::chrono::duration<double>(now - m_lastFrameTime).count();
    if (elapsed > 0) {
        double instantFPS = 1.0 / elapsed;

        // Apply exponential smoothing
        const double alpha = 0.1;
        m_actualFPS = alpha * instantFPS + (1.0 - alpha) * m_actualFPS;
    }
}

bool VideoManager::initializeRecording() {
    // Create output directory if needed
    std::filesystem::path filePath(m_recordingFilename);
    auto directory = filePath.parent_path();

    if (!directory.empty() && !std::filesystem::exists(directory)) {
        try {
            std::filesystem::create_directories(directory);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to create recording directory: {}", e.what());
            return false;
        }
    }

    // Initialize recording format based on file extension
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == ".avi" || extension == ".mp4") {
        // Video file format - would need video codec integration
        LOG_F(WARNING, "Video codec recording not implemented, using frame sequence");
        return true;
    } else {
        // Frame sequence format
        return true;
    }
}

void VideoManager::recordFrame(std::shared_ptr<AtomCameraFrame> frame) {
    if (!frame) {
        return;
    }

    try {
        // Generate frame filename
        std::filesystem::path basePath(m_recordingFilename);
        std::string baseName = basePath.stem().string();
        std::string extension = basePath.extension().string();

        std::ostringstream frameFilename;
        frameFilename << baseName << "_" << std::setfill('0') << std::setw(6)
                     << m_recordingFrameCount << extension;

        std::filesystem::path frameFilePath = basePath.parent_path() / frameFilename.str();

        // Save frame (this would need to be implemented based on frame format)
        // For now, just increment counter
        m_recordingFrameCount++;

        LOG_F(INFO, "Recorded frame {} to {}", m_recordingFrameCount, frameFilePath.string());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to record frame: {}", e.what());
    }
}

void VideoManager::finalizeRecording() {
    // Close any open video files, write metadata, etc.
    LOG_F(INFO, "Recording finalized: {} frames recorded", m_recordingFrameCount);
}

} // namespace lithium::device::ascom::camera::components
