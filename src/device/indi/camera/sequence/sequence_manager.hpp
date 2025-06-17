#ifndef LITHIUM_INDI_CAMERA_SEQUENCE_MANAGER_HPP
#define LITHIUM_INDI_CAMERA_SEQUENCE_MANAGER_HPP

#include "../component_base.hpp"
#include "../../../template/camera_frame.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <utility>

namespace lithium::device::indi::camera {

// Forward declarations
class ExposureController;

/**
 * @brief Sequence management component for INDI cameras
 * 
 * This component handles automated image sequences including
 * multi-frame captures, timed sequences, and automated workflows.
 */
class SequenceManager : public ComponentBase {
public:
    explicit SequenceManager(INDICameraCore* core);
    ~SequenceManager() override;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Sequence control
    auto startSequence(int count, double exposure, double interval) -> bool;
    auto stopSequence() -> bool;
    auto isSequenceRunning() const -> bool;
    auto getSequenceProgress() const -> std::pair<int, int>; // current, total

    // Sequence configuration
    auto setSequenceCallback(std::function<void(int, std::shared_ptr<AtomCameraFrame>)> callback) -> void;
    auto setSequenceCompleteCallback(std::function<void(bool)> callback) -> void;

    // Set exposure controller reference
    auto setExposureController(ExposureController* controller) -> void;

private:
    // Sequence state
    std::atomic_bool isSequenceRunning_{false};
    std::atomic<int> sequenceCount_{0};
    std::atomic<int> sequenceTotal_{0};
    std::atomic<double> sequenceExposure_{1.0};
    std::atomic<double> sequenceInterval_{0.0};
    
    // Timing
    std::chrono::system_clock::time_point sequenceStartTime_;
    std::chrono::system_clock::time_point lastSequenceCapture_;
    
    // Worker thread
    std::thread sequenceThread_;
    std::atomic_bool stopSequenceFlag_{false};
    
    // Callbacks
    std::function<void(int, std::shared_ptr<AtomCameraFrame>)> frameCallback_;
    std::function<void(bool)> completeCallback_;
    
    // Component references
    ExposureController* exposureController_{nullptr};
    
    // Sequence execution
    void sequenceWorker();
    void handleSequenceCapture();
    auto waitForExposureComplete() -> bool;
    auto executeSequenceStep(int currentFrame) -> bool;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_SEQUENCE_MANAGER_HPP
