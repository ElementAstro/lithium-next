#ifndef LITHIUM_INDI_CAMERA_EXPOSURE_CONTROLLER_HPP
#define LITHIUM_INDI_CAMERA_EXPOSURE_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera_frame.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace lithium::device::indi::camera {

/**
 * @brief Exposure control component for INDI cameras
 *
 * This component handles all exposure-related operations including
 * starting/stopping exposures, tracking progress, and managing
 * exposure statistics.
 */
class ExposureController : public ComponentBase {
public:
    explicit ExposureController(std::shared_ptr<INDICameraCore> core);
    ~ExposureController() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Exposure control
    auto startExposure(double duration) -> bool;
    auto abortExposure() -> bool;
    auto isExposing() const -> bool;
    auto getExposureProgress() const -> double;
    auto getExposureRemaining() const -> double;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame>;

    // Exposure statistics
    auto getLastExposureDuration() const -> double;
    auto getExposureCount() const -> uint32_t;
    auto resetExposureCount() -> bool;

    // Image saving
    auto saveImage(const std::string& path) -> bool;

private:
    // Exposure state
    std::atomic_bool isExposing_{false};
    std::atomic<double> currentExposureDuration_{0.0};
    std::chrono::system_clock::time_point exposureStartTime_;

    // Exposure statistics
    std::atomic<double> lastExposureDuration_{0.0};
    std::atomic<uint32_t> exposureCount_{0};

    // Property handlers
    void handleExposureProperty(INDI::Property property);
    void handleBlobProperty(INDI::Property property);

    // Helper methods
    void processReceivedImage(const INDI::PropertyBlob& property);
    auto validateImageData(const void* data, size_t size) -> bool;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_EXPOSURE_CONTROLLER_HPP
