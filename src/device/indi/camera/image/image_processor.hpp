#ifndef LITHIUM_INDI_CAMERA_IMAGE_PROCESSOR_HPP
#define LITHIUM_INDI_CAMERA_IMAGE_PROCESSOR_HPP

#include "../component_base.hpp"
#include "../../../template/camera_frame.hpp"

#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace lithium::device::indi::camera {

/**
 * @brief Image processing and analysis component for INDI cameras
 * 
 * This component handles image format conversion, compression,
 * quality analysis, and image processing operations.
 */
class ImageProcessor : public ComponentBase {
public:
    explicit ImageProcessor(INDICameraCore* core);
    ~ImageProcessor() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Image format control
    auto setImageFormat(const std::string& format) -> bool;
    auto getImageFormat() const -> std::string;
    auto getSupportedImageFormats() const -> std::vector<std::string>;

    // Image compression
    auto enableImageCompression(bool enable) -> bool;
    auto isImageCompressionEnabled() const -> bool;

    // Image quality analysis
    auto getLastImageQuality() const -> std::map<std::string, double>;
    auto getFrameStatistics() const -> std::map<std::string, double>;

    // Image processing utilities
    auto getImageFormat(const std::string& extension) -> std::string;
    auto validateImageData(const void* data, size_t size) -> bool;
    auto processReceivedImage(const INDI::PropertyBlob& property) -> void;

private:
    // Image format settings
    std::string currentImageFormat_{"FITS"};
    std::atomic_bool imageCompressionEnabled_{false};
    std::vector<std::string> supportedImageFormats_;

    // Image quality metrics
    std::atomic<double> lastImageMean_{0.0};
    std::atomic<double> lastImageStdDev_{0.0};
    std::atomic<int> lastImageMin_{0};
    std::atomic<int> lastImageMax_{0};

    // Helper methods
    void setupImageFormats();
    void analyzeImageQuality(const uint16_t* data, size_t pixelCount);
    void updateImageStatistics(std::shared_ptr<AtomCameraFrame> frame);
    auto detectImageFormat(const void* data, size_t size) -> std::string;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_IMAGE_PROCESSOR_HPP
