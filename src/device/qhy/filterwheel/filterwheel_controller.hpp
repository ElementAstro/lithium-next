/*
 * qhy_filterwheel_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: QHY camera filter wheel controller component

*************************************************/

#ifndef LITHIUM_QHY_CAMERA_FILTERWHEEL_CONTROLLER_HPP
#define LITHIUM_QHY_CAMERA_FILTERWHEEL_CONTROLLER_HPP

#include "../camera/component_base.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <map>

namespace lithium::device::qhy::camera {

/**
 * @brief Filter wheel controller for QHY cameras
 *
 * This component handles QHY CFW (Color Filter Wheel) operations
 * including position control, movement monitoring, and filter
 * management with comprehensive features.
 */
class FilterWheelController : public ComponentBase {
public:
    explicit FilterWheelController(QHYCameraCore* core);
    ~FilterWheelController() override;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto onCameraStateChanged(CameraState state) -> void override;

    // QHY CFW (Color Filter Wheel) control
    auto hasQHYFilterWheel() -> bool;
    auto connectQHYFilterWheel() -> bool;
    auto disconnectQHYFilterWheel() -> bool;
    auto isQHYFilterWheelConnected() -> bool;
    auto setQHYFilterPosition(int position) -> bool;
    auto getQHYFilterPosition() -> int;
    auto getQHYFilterCount() -> int;
    auto isQHYFilterWheelMoving() -> bool;
    auto homeQHYFilterWheel() -> bool;
    auto getQHYFilterWheelFirmware() -> std::string;
    auto setQHYFilterNames(const std::vector<std::string>& names) -> bool;
    auto getQHYFilterNames() -> std::vector<std::string>;
    auto getQHYFilterWheelModel() -> std::string;
    auto calibrateQHYFilterWheel() -> bool;

    // Advanced filter wheel features
    auto setQHYFilterWheelDirection(bool clockwise) -> bool;
    auto getQHYFilterWheelDirection() -> bool;
    auto getQHYFilterWheelStatus() -> std::string;
    auto enableFilterWheelMonitoring(bool enable) -> bool;
    auto isFilterWheelMonitoringEnabled() const -> bool;

    // Filter management
    auto setFilterOffset(int position, double offset) -> bool;
    auto getFilterOffset(int position) -> double;
    auto clearFilterOffsets() -> void;
    auto saveFilterConfiguration(const std::string& filename) -> bool;
    auto loadFilterConfiguration(const std::string& filename) -> bool;

    // Movement callbacks and monitoring
    auto setMovementCallback(std::function<void(int, bool)> callback) -> void;
    auto enableMovementLogging(bool enable) -> bool;
    auto getMovementHistory() -> std::vector<std::pair<std::chrono::system_clock::time_point, int>>;
    auto clearMovementHistory() -> void;

    // Filter sequence automation
    auto startFilterSequence(const std::vector<int>& positions,
                            std::function<void(int, bool)> callback = nullptr) -> bool;
    auto stopFilterSequence() -> bool;
    auto isFilterSequenceRunning() const -> bool;
    auto getFilterSequenceProgress() const -> std::pair<int, int>;

private:
    // QHY CFW (Color Filter Wheel) state
    bool hasQHYFilterWheel_{false};
    bool qhyFilterWheelConnected_{false};
    int qhyCurrentFilterPosition_{1};
    int qhyFilterCount_{0};
    bool qhyFilterWheelMoving_{false};
    std::string qhyFilterWheelFirmware_;
    std::string qhyFilterWheelModel_;
    std::vector<std::string> qhyFilterNames_;
    bool qhyFilterWheelClockwise_{true};

    // Filter offsets for focus compensation
    std::map<int, double> filterOffsets_;

    // Movement monitoring
    std::atomic_bool filterWheelMonitoringEnabled_{true};
    std::atomic_bool movementLoggingEnabled_{false};
    std::thread monitoringThread_;
    std::vector<std::pair<std::chrono::system_clock::time_point, int>> movementHistory_;
    static constexpr size_t MAX_HISTORY_SIZE = 500;

    // Filter sequence automation
    std::atomic_bool filterSequenceRunning_{false};
    std::thread sequenceThread_;
    std::vector<int> sequencePositions_;
    int sequenceCurrentIndex_{0};
    std::function<void(int, bool)> sequenceCallback_;

    // Callbacks and synchronization
    std::function<void(int, bool)> movementCallback_;
    mutable std::mutex filterWheelMutex_;
    mutable std::mutex historyMutex_;
    mutable std::mutex sequenceMutex_;

    // Private helper methods
    auto detectQHYFilterWheel() -> bool;
    auto initializeQHYFilterWheel() -> bool;
    auto shutdownQHYFilterWheel() -> bool;
    auto waitForQHYMovement(int timeoutMs = 30000) -> bool;
    auto validateQHYPosition(int position) const -> bool;
    auto notifyMovementChange(int position, bool moving) -> void;
    auto addMovementToHistory(int position) -> void;
    auto monitoringThreadFunction() -> void;
    auto sequenceThreadFunction() -> void;
    auto executeSequenceStep(int position) -> bool;
    auto getFilterWheelStatusString() const -> std::string;
    auto sendFilterWheelCommand(const std::string& command) -> std::string;
    auto parseFilterWheelResponse(const std::string& response) -> bool;
};

} // namespace lithium::device::qhy::camera

#endif // LITHIUM_QHY_CAMERA_FILTERWHEEL_CONTROLLER_HPP
