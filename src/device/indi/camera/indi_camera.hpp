#ifndef LITHIUM_INDI_CAMERA_HPP
#define LITHIUM_INDI_CAMERA_HPP

#include "../template/camera.hpp"
#include "component_base.hpp"
#include "core/indi_camera_core.hpp"
#include "exposure/exposure_controller.hpp"
#include "video/video_controller.hpp"
#include "temperature/temperature_controller.hpp"
#include "hardware/hardware_controller.hpp"
#include "image/image_processor.hpp"
#include "sequence/sequence_manager.hpp"
#include "properties/property_handler.hpp"

#include <memory>
#include <string>

namespace lithium::device::indi::camera {

/**
 * @brief Component-based INDI camera implementation
 * 
 * This class aggregates all camera components to provide a unified
 * interface while maintaining modularity and separation of concerns.
 */
class INDICamera : public AtomCamera {
public:
    explicit INDICamera(std::string deviceName);
    ~INDICamera() override = default;

    // Basic device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout = 5000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // Exposure control
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string& path) -> bool override;

    // Exposure history and statistics
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // Video streaming
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string& format) -> bool override;
    auto getVideoFormats() -> std::vector<std::string> override;

    // Advanced video features
    auto startVideoRecording(const std::string& filename) -> bool override;
    auto stopVideoRecording() -> bool override;
    auto isVideoRecording() const -> bool override;
    auto setVideoExposure(double exposure) -> bool override;
    auto getVideoExposure() const -> double override;
    auto setVideoGain(int gain) -> bool override;
    auto getVideoGain() const -> int override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    auto isCoolerOn() const -> bool override;
    auto getTemperature() const -> std::optional<double> override;
    auto getTemperatureInfo() const -> ::TemperatureInfo override;
    auto getCoolingPower() const -> std::optional<double> override;
    auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // Color and Bayer
    auto isColor() const -> bool override;
    auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // Gain control
    auto setGain(int gain) -> bool override;
    auto getGain() -> std::optional<int> override;
    auto getGainRange() -> std::pair<int, int> override;

    auto setOffset(int offset) -> bool override;
    auto getOffset() -> std::optional<int> override;
    auto getOffsetRange() -> std::pair<int, int> override;

    auto setISO(int iso) -> bool override;
    auto getISO() -> std::optional<int> override;
    auto getISOList() -> std::vector<int> override;

    // Frame settings
    auto getResolution() -> std::optional<AtomCameraFrame::Resolution> override;
    auto setResolution(int x, int y, int width, int height) -> bool override;
    auto getMaxResolution() -> AtomCameraFrame::Resolution override;

    auto getBinning() -> std::optional<AtomCameraFrame::Binning> override;
    auto setBinning(int horizontal, int vertical) -> bool override;
    auto getMaxBinning() -> AtomCameraFrame::Binning override;

    auto setFrameType(FrameType type) -> bool override;
    auto getFrameType() -> FrameType override;
    auto setUploadMode(UploadMode mode) -> bool override;
    auto getUploadMode() -> UploadMode override;

    // Pixel information
    auto getPixelSize() -> double override;
    auto getPixelSizeX() -> double override;
    auto getPixelSizeY() -> double override;
    auto getBitDepth() -> int override;

    // Shutter control
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // Fan control
    auto hasFan() -> bool override;
    auto setFanSpeed(int speed) -> bool override;
    auto getFanSpeed() -> int override;

    // Image sequence capabilities
    auto startSequence(int count, double exposure, double interval) -> bool override;
    auto stopSequence() -> bool override;
    auto isSequenceRunning() const -> bool override;
    auto getSequenceProgress() const -> std::pair<int, int> override;

    // Advanced image processing
    auto setImageFormat(const std::string& format) -> bool override;
    auto getImageFormat() const -> std::string override;
    auto enableImageCompression(bool enable) -> bool override;
    auto isImageCompressionEnabled() const -> bool override;

    auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> override;

    // Enhanced AtomCamera methods
    auto getSupportedImageFormats() const -> std::vector<std::string> override;
    auto getFrameStatistics() const -> std::map<std::string, double> override;
    auto getTotalFramesReceived() const -> uint64_t override;
    auto getDroppedFrames() const -> uint64_t override;
    auto getAverageFrameRate() const -> double override;
    auto getLastImageQuality() const -> std::map<std::string, double> override;

    // Component access (for advanced usage)
    auto getCore() -> INDICameraCore* { return core_.get(); }
    auto getExposureController() -> ExposureController* { return exposureController_.get(); }
    auto getVideoController() -> VideoController* { return videoController_.get(); }
    auto getTemperatureController() -> TemperatureController* { return temperatureController_.get(); }
    auto getHardwareController() -> HardwareController* { return hardwareController_.get(); }
    auto getImageProcessor() -> ImageProcessor* { return imageProcessor_.get(); }
    auto getSequenceManager() -> SequenceManager* { return sequenceManager_.get(); }
    auto getPropertyHandler() -> PropertyHandler* { return propertyHandler_.get(); }

private:
    // Core components
    std::shared_ptr<INDICameraCore> core_;
    std::shared_ptr<ExposureController> exposureController_;
    std::shared_ptr<VideoController> videoController_;
    std::shared_ptr<TemperatureController> temperatureController_;
    std::shared_ptr<HardwareController> hardwareController_;
    std::shared_ptr<ImageProcessor> imageProcessor_;
    std::shared_ptr<SequenceManager> sequenceManager_;
    std::shared_ptr<PropertyHandler> propertyHandler_;

    // Helper methods
    void initializeComponents();
    void registerPropertyHandlers();
    void setupComponentCommunication();
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_HPP
