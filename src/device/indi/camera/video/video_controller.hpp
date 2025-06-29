#ifndef LITHIUM_INDI_CAMERA_VIDEO_CONTROLLER_HPP
#define LITHIUM_INDI_CAMERA_VIDEO_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera_frame.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace lithium::device::indi::camera {

/**
 * @brief Video streaming and recording controller for INDI cameras
 *
 * This component handles video streaming, recording, and related
 * video-specific camera operations.
 */
class VideoController : public ComponentBase {
public:
    explicit VideoController(std::shared_ptr<INDICameraCore> core);
    ~VideoController() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Video streaming
    auto startVideo() -> bool;
    auto stopVideo() -> bool;
    auto isVideoRunning() const -> bool;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto setVideoFormat(const std::string& format) -> bool;
    auto getVideoFormats() -> std::vector<std::string>;

    // Video recording
    auto startVideoRecording(const std::string& filename) -> bool;
    auto stopVideoRecording() -> bool;
    auto isVideoRecording() const -> bool;

    // Video parameters
    auto setVideoExposure(double exposure) -> bool;
    auto getVideoExposure() const -> double;
    auto setVideoGain(int gain) -> bool;
    auto getVideoGain() const -> int;

    // Video statistics
    auto getTotalFramesReceived() const -> uint64_t;
    auto getDroppedFrames() const -> uint64_t;
    auto getAverageFrameRate() const -> double;

private:
    // Video state
    std::atomic_bool isVideoRunning_{false};
    std::atomic_bool isVideoRecording_{false};
    std::atomic<double> videoExposure_{0.033}; // 30 FPS default
    std::atomic<int> videoGain_{0};

    // Video formats
    std::vector<std::string> videoFormats_;
    std::string currentVideoFormat_;
    std::string videoRecordingFile_;

    // Video statistics
    std::atomic<uint64_t> totalFramesReceived_{0};
    std::atomic<uint64_t> droppedFrames_{0};
    std::atomic<double> averageFrameRate_{0.0};
    std::chrono::system_clock::time_point lastFrameTime_;

    // Property handlers
    void handleVideoStreamProperty(INDI::Property property);
    void handleVideoFormatProperty(INDI::Property property);

    // Helper methods
    void setupVideoFormats();
    void updateFrameRate();
    void recordVideoFrame(std::shared_ptr<AtomCameraFrame> frame);
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_VIDEO_CONTROLLER_HPP
