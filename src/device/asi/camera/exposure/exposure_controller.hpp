/*
 * asi_exposure_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera exposure controller component

*************************************************/

#ifndef LITHIUM_ASI_CAMERA_EXPOSURE_CONTROLLER_HPP
#define LITHIUM_ASI_CAMERA_EXPOSURE_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera_frame.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <mutex>

namespace lithium::device::asi::camera {

/**
 * @brief Exposure control component for ASI cameras
 * 
 * This component handles all exposure-related operations including
 * starting/stopping exposures, tracking progress, and managing
 * exposure statistics using the ASI SDK.
 */
class ExposureController : public ComponentBase {
public:
    explicit ExposureController(ASICameraCore* core);
    ~ExposureController() override;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto onCameraStateChanged(CameraState state) -> void override;

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

    // ASI-specific exposure settings
    auto setExposureMode(int mode) -> bool;
    auto getExposureMode() -> int;
    auto enableAutoExposure(bool enable) -> bool;
    auto isAutoExposureEnabled() const -> bool;
    auto setAutoExposureTarget(int target) -> bool;
    auto getAutoExposureTarget() -> int;

private:
    // Exposure state
    std::atomic_bool isExposing_{false};
    std::atomic_bool exposureAbortRequested_{false};
    std::chrono::system_clock::time_point exposureStartTime_;
    double currentExposureDuration_{0.0};
    std::thread exposureThread_;
    mutable std::mutex exposureMutex_;

    // Exposure statistics
    uint32_t exposureCount_{0};
    double lastExposureDuration_{0.0};
    std::chrono::system_clock::time_point lastExposureTime_;

    // Current frame
    std::shared_ptr<AtomCameraFrame> lastFrameResult_;
    mutable std::mutex frameMutex_;

    // ASI-specific settings
    int exposureMode_{0};
    bool autoExposureEnabled_{false};
    int autoExposureTarget_{50};

    // Private helper methods
    auto exposureThreadFunction() -> void;
    auto captureFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto setupExposureParameters(double duration) -> bool;
    auto downloadImageData() -> std::unique_ptr<uint8_t[]>;
    auto createFrameFromData(std::unique_ptr<uint8_t[]> data, size_t size) -> std::shared_ptr<AtomCameraFrame>;
    auto isValidExposureTime(double duration) const -> bool;
    auto updateExposureStatistics() -> void;
};

} // namespace lithium::device::asi::camera

#endif // LITHIUM_ASI_CAMERA_EXPOSURE_CONTROLLER_HPP
