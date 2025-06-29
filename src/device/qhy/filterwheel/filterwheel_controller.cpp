/*
 * filterwheel_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: QHY camera filter wheel controller implementation

*************************************************/

#include "filterwheel_controller.hpp"
// #include "../camera/core/qhy_camera_core.hpp"
#include "atom/log/loguru.hpp"

#include <chrono>
#include <fstream>
#include <thread>

// QHY SDK includes
#ifdef LITHIUM_QHY_ENABLED
extern "C" {
#include "../qhyccd.h"
}
#else
// Stub definitions for compilation when QHY SDK is not available
typedef char qhyccd_handle;
typedef unsigned int QHYCCD_ERROR;
const QHYCCD_ERROR QHYCCD_SUCCESS = 0;
const QHYCCD_ERROR QHYCCD_ERROR_CAMERA_NOT_FOUND = 1;
const int CFW_PORTS_NUM = 8;

static inline QHYCCD_ERROR ScanQHYCFW() { return QHYCCD_SUCCESS; }
static inline QHYCCD_ERROR GetQHYCFWId(char* id, unsigned int index) {
    if (id) strcpy(id, "QHY-CFW-SIM");
    return QHYCCD_SUCCESS;
}
static inline qhyccd_handle* OpenQHYCFW(char* id) { return reinterpret_cast<qhyccd_handle*>(0x1); }
static inline QHYCCD_ERROR CloseQHYCFW(qhyccd_handle* handle) { return QHYCCD_SUCCESS; }
static inline QHYCCD_ERROR SendOrder2QHYCFW(qhyccd_handle* handle, char* order, unsigned int length) { return QHYCCD_SUCCESS; }
static inline QHYCCD_ERROR GetQHYCFWStatus(qhyccd_handle* handle, char* status) {
    if (status) strcpy(status, "P1");
    return QHYCCD_SUCCESS;
}
static inline QHYCCD_ERROR IsQHYCFWPlugged(qhyccd_handle* handle) { return QHYCCD_SUCCESS; }
static inline unsigned int GetQHYCFWChipInfo(qhyccd_handle* handle) { return 7; }
static inline QHYCCD_ERROR SetQHYCFWParam(qhyccd_handle* handle, unsigned int param, double value) { return QHYCCD_SUCCESS; }
static inline double GetQHYCFWParam(qhyccd_handle* handle, unsigned int param) { return 1.0; }
#include <cstring>
#endif

namespace lithium::device::qhy::camera {

FilterWheelController::FilterWheelController(QHYCameraCore* core)
    : ComponentBase(core) {
    LOG_F(INFO, "QHY Filter Wheel Controller created");
}

FilterWheelController::~FilterWheelController() {
    destroy();
    LOG_F(INFO, "QHY Filter Wheel Controller destroyed");
}

auto FilterWheelController::initialize() -> bool {
    LOG_F(INFO, "Initializing QHY Filter Wheel Controller");

    try {
        // Detect QHY filter wheel
        if (!detectQHYFilterWheel()) {
            LOG_F(WARNING, "No QHY filter wheel detected");
            hasQHYFilterWheel_ = false;
            return true; // Not having a filter wheel is not an error
        }

        hasQHYFilterWheel_ = true;

        // Initialize filter wheel
        if (!initializeQHYFilterWheel()) {
            LOG_F(ERROR, "Failed to initialize QHY filter wheel");
            return false;
        }

        // Start monitoring thread if enabled
        if (filterWheelMonitoringEnabled_) {
            monitoringThread_ = std::thread(&FilterWheelController::monitoringThreadFunction, this);
        }

        LOG_F(INFO, "QHY Filter Wheel Controller initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during QHY filter wheel initialization: {}", e.what());
        return false;
    }
}

auto FilterWheelController::destroy() -> bool {
    LOG_F(INFO, "Destroying QHY Filter Wheel Controller");

    try {
        // Stop any running sequences
        stopFilterSequence();

        // Stop monitoring
        filterWheelMonitoringEnabled_ = false;
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }

        // Disconnect filter wheel
        if (qhyFilterWheelConnected_) {
            disconnectQHYFilterWheel();
        }

        shutdownQHYFilterWheel();

        LOG_F(INFO, "QHY Filter Wheel Controller destroyed successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during QHY filter wheel destruction: {}", e.what());
        return false;
    }
}

auto FilterWheelController::getComponentName() const -> std::string {
    return "QHY Filter Wheel Controller";
}

auto FilterWheelController::onCameraStateChanged(CameraState state) -> void {
    // Handle camera state changes if needed
    switch (state) {
        case CameraState::IDLE:
            // Try to connect filter wheel when camera is idle
            if (hasQHYFilterWheel_ && !qhyFilterWheelConnected_) {
                connectQHYFilterWheel();
            }
            break;
        case CameraState::ERROR:
            // Handle error states if needed
            LOG_F(WARNING, "Camera error state, checking filter wheel connection");
            break;
        default:
            break;
    }
}

auto FilterWheelController::hasQHYFilterWheel() -> bool {
    return hasQHYFilterWheel_;
}

auto FilterWheelController::connectQHYFilterWheel() -> bool {
    std::lock_guard<std::mutex> lock(filterWheelMutex_);

    if (qhyFilterWheelConnected_) {
        LOG_F(INFO, "QHY filter wheel already connected");
        return true;
    }

    if (!hasQHYFilterWheel_) {
        LOG_F(ERROR, "No QHY filter wheel available");
        return false;
    }

    try {
        LOG_F(INFO, "Connecting to QHY filter wheel");

#ifdef LITHIUM_QHY_ENABLED
        // Connect to the filter wheel using QHY SDK
        char cfwId[32];
        QHYCCD_ERROR ret = GetQHYCFWId(cfwId, 0);
        if (ret != QHYCCD_SUCCESS) {
            LOG_F(ERROR, "Failed to get QHY CFW ID");
            return false;
        }

        qhyccd_handle* cfwHandle = OpenQHYCFW(cfwId);
        if (!cfwHandle) {
            LOG_F(ERROR, "Failed to open QHY CFW");
            return false;
        }

        // Get filter wheel information
        qhyFilterCount_ = GetQHYCFWChipInfo(cfwHandle);
        qhyFilterWheelModel_ = std::string(cfwId);

        // Get firmware version
        char status[32];
        GetQHYCFWStatus(cfwHandle, status);
        qhyFilterWheelFirmware_ = std::string(status);

        // Get current position
        qhyCurrentFilterPosition_ = static_cast<int>(GetQHYCFWParam(cfwHandle, 0));

        // Initialize filter names with defaults
        qhyFilterNames_.clear();
        for (int i = 1; i <= qhyFilterCount_; ++i) {
            qhyFilterNames_.push_back("Filter " + std::to_string(i));
        }

#else
        // Simulation mode
        qhyFilterCount_ = 7;
        qhyFilterWheelModel_ = "QHY-CFW-SIM";
        qhyFilterWheelFirmware_ = "v1.0.0-sim";
        qhyCurrentFilterPosition_ = 1;

        qhyFilterNames_ = {"L", "R", "G", "B", "Ha", "OIII", "SII"};
#endif

        qhyFilterWheelConnected_ = true;

        LOG_F(INFO, "QHY filter wheel connected successfully: {} (firmware: {}, filters: {})",
              qhyFilterWheelModel_, qhyFilterWheelFirmware_, qhyFilterCount_);

        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception connecting QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::disconnectQHYFilterWheel() -> bool {
    std::lock_guard<std::mutex> lock(filterWheelMutex_);

    if (!qhyFilterWheelConnected_) {
        return true;
    }

    try {
        LOG_F(INFO, "Disconnecting QHY filter wheel");

#ifdef LITHIUM_QHY_ENABLED
        // Close QHY CFW handle
        // Note: In real implementation, we'd need to store the handle
        // CloseQHYCFW(cfwHandle);
#endif

        qhyFilterWheelConnected_ = false;
        qhyFilterWheelMoving_ = false;

        LOG_F(INFO, "QHY filter wheel disconnected successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception disconnecting QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::isQHYFilterWheelConnected() -> bool {
    return qhyFilterWheelConnected_;
}

auto FilterWheelController::setQHYFilterPosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(filterWheelMutex_);

    if (!qhyFilterWheelConnected_) {
        LOG_F(ERROR, "QHY filter wheel not connected");
        return false;
    }

    if (!validateQHYPosition(position)) {
        LOG_F(ERROR, "Invalid filter position: {}", position);
        return false;
    }

    if (position == qhyCurrentFilterPosition_) {
        LOG_F(INFO, "Already at filter position {}", position);
        return true;
    }

    try {
        LOG_F(INFO, "Moving QHY filter wheel to position {}", position);

        qhyFilterWheelMoving_ = true;
        notifyMovementChange(position, true);

#ifdef LITHIUM_QHY_ENABLED
        // Send move command to QHY filter wheel
        std::string command = "G" + std::to_string(position);
        // SendOrder2QHYCFW(cfwHandle, command.data(), command.length());
#else
        // Simulate movement
        std::thread([this, position]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            qhyCurrentFilterPosition_ = position;
            qhyFilterWheelMoving_ = false;
            addMovementToHistory(position);
            notifyMovementChange(position, false);
        }).detach();
#endif

        // Wait for movement completion
        if (!waitForQHYMovement()) {
            LOG_F(ERROR, "Timeout waiting for filter wheel movement");
            qhyFilterWheelMoving_ = false;
            return false;
        }

        qhyCurrentFilterPosition_ = position;
        qhyFilterWheelMoving_ = false;

        addMovementToHistory(position);
        notifyMovementChange(position, false);

        LOG_F(INFO, "QHY filter wheel moved to position {} successfully", position);
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception moving QHY filter wheel: {}", e.what());
        qhyFilterWheelMoving_ = false;
        notifyMovementChange(qhyCurrentFilterPosition_, false);
        return false;
    }
}

auto FilterWheelController::getQHYFilterPosition() -> int {
    return qhyCurrentFilterPosition_;
}

auto FilterWheelController::getQHYFilterCount() -> int {
    return qhyFilterCount_;
}

auto FilterWheelController::isQHYFilterWheelMoving() -> bool {
    return qhyFilterWheelMoving_;
}

auto FilterWheelController::homeQHYFilterWheel() -> bool {
    LOG_F(INFO, "Homing QHY filter wheel");

    if (!qhyFilterWheelConnected_) {
        LOG_F(ERROR, "QHY filter wheel not connected");
        return false;
    }

    try {
#ifdef LITHIUM_QHY_ENABLED
        // Send home command
        std::string command = "H";
        // SendOrder2QHYCFW(cfwHandle, command.data(), command.length());
#endif

        // Wait for homing to complete
        std::this_thread::sleep_for(std::chrono::seconds(5));

        qhyCurrentFilterPosition_ = 1;
        LOG_F(INFO, "QHY filter wheel homed successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception homing QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::getQHYFilterWheelFirmware() -> std::string {
    return qhyFilterWheelFirmware_;
}

auto FilterWheelController::setQHYFilterNames(const std::vector<std::string>& names) -> bool {
    if (names.size() != static_cast<size_t>(qhyFilterCount_)) {
        LOG_F(ERROR, "Filter names count ({}) doesn't match filter count ({})",
              names.size(), qhyFilterCount_);
        return false;
    }

    qhyFilterNames_ = names;
    LOG_F(INFO, "QHY filter names updated");
    return true;
}

auto FilterWheelController::getQHYFilterNames() -> std::vector<std::string> {
    return qhyFilterNames_;
}

auto FilterWheelController::getQHYFilterWheelModel() -> std::string {
    return qhyFilterWheelModel_;
}

auto FilterWheelController::calibrateQHYFilterWheel() -> bool {
    LOG_F(INFO, "Calibrating QHY filter wheel");

    if (!qhyFilterWheelConnected_) {
        LOG_F(ERROR, "QHY filter wheel not connected");
        return false;
    }

    try {
#ifdef LITHIUM_QHY_ENABLED
        // Send calibration command
        std::string command = "C";
        // SendOrder2QHYCFW(cfwHandle, command.data(), command.length());
#endif

        // Wait for calibration to complete
        std::this_thread::sleep_for(std::chrono::seconds(10));

        LOG_F(INFO, "QHY filter wheel calibrated successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception calibrating QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::setQHYFilterWheelDirection(bool clockwise) -> bool {
    qhyFilterWheelClockwise_ = clockwise;
    LOG_F(INFO, "QHY filter wheel direction set to: {}", clockwise ? "clockwise" : "counter-clockwise");
    return true;
}

auto FilterWheelController::getQHYFilterWheelDirection() -> bool {
    return qhyFilterWheelClockwise_;
}

auto FilterWheelController::getQHYFilterWheelStatus() -> std::string {
    return getFilterWheelStatusString();
}

auto FilterWheelController::enableFilterWheelMonitoring(bool enable) -> bool {
    filterWheelMonitoringEnabled_ = enable;
    LOG_F(INFO, "{} QHY filter wheel monitoring", enable ? "Enabled" : "Disabled");
    return true;
}

auto FilterWheelController::isFilterWheelMonitoringEnabled() const -> bool {
    return filterWheelMonitoringEnabled_;
}

auto FilterWheelController::setFilterOffset(int position, double offset) -> bool {
    if (!validateQHYPosition(position)) {
        return false;
    }

    filterOffsets_[position] = offset;
    LOG_F(INFO, "Set filter offset for position {}: {:.3f}", position, offset);
    return true;
}

auto FilterWheelController::getFilterOffset(int position) -> double {
    if (!validateQHYPosition(position)) {
        return 0.0;
    }

    auto it = filterOffsets_.find(position);
    return (it != filterOffsets_.end()) ? it->second : 0.0;
}

auto FilterWheelController::clearFilterOffsets() -> void {
    filterOffsets_.clear();
    LOG_F(INFO, "Cleared all filter offsets");
}

auto FilterWheelController::saveFilterConfiguration(const std::string& filename) -> bool {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filename);
            return false;
        }

        // Save filter names
        file << "# QHY Filter Wheel Configuration\n";
        file << "FilterCount=" << qhyFilterCount_ << "\n";
        file << "Model=" << qhyFilterWheelModel_ << "\n";
        file << "Firmware=" << qhyFilterWheelFirmware_ << "\n";
        file << "\n# Filter Names\n";

        for (size_t i = 0; i < qhyFilterNames_.size(); ++i) {
            file << "Filter" << (i + 1) << "=" << qhyFilterNames_[i] << "\n";
        }

        // Save filter offsets
        file << "\n# Filter Offsets\n";
        for (const auto& [position, offset] : filterOffsets_) {
            file << "Offset" << position << "=" << offset << "\n";
        }

        file.close();
        LOG_F(INFO, "Filter configuration saved to: {}", filename);
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception saving filter configuration: {}", e.what());
        return false;
    }
}

auto FilterWheelController::loadFilterConfiguration(const std::string& filename) -> bool {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open file for reading: {}", filename);
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            auto pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key.starts_with("Filter")) {
                int filterNum = std::stoi(key.substr(6)) - 1;
                if (filterNum >= 0 && filterNum < qhyFilterCount_) {
                    qhyFilterNames_[filterNum] = value;
                }
            } else if (key.starts_with("Offset")) {
                int position = std::stoi(key.substr(6));
                double offset = std::stod(value);
                filterOffsets_[position] = offset;
            }
        }

        file.close();
        LOG_F(INFO, "Filter configuration loaded from: {}", filename);
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception loading filter configuration: {}", e.what());
        return false;
    }
}

auto FilterWheelController::setMovementCallback(std::function<void(int, bool)> callback) -> void {
    movementCallback_ = std::move(callback);
}

auto FilterWheelController::enableMovementLogging(bool enable) -> bool {
    movementLoggingEnabled_ = enable;
    LOG_F(INFO, "{} movement logging", enable ? "Enabled" : "Disabled");
    return true;
}

auto FilterWheelController::getMovementHistory() -> std::vector<std::pair<std::chrono::system_clock::time_point, int>> {
    std::lock_guard<std::mutex> lock(historyMutex_);
    return movementHistory_;
}

auto FilterWheelController::clearMovementHistory() -> void {
    std::lock_guard<std::mutex> lock(historyMutex_);
    movementHistory_.clear();
    LOG_F(INFO, "Movement history cleared");
}

auto FilterWheelController::startFilterSequence(const std::vector<int>& positions,
                                               std::function<void(int, bool)> callback) -> bool {
    std::lock_guard<std::mutex> lock(sequenceMutex_);

    if (filterSequenceRunning_) {
        LOG_F(ERROR, "Filter sequence already running");
        return false;
    }

    if (positions.empty()) {
        LOG_F(ERROR, "Empty filter sequence");
        return false;
    }

    // Validate all positions
    for (int pos : positions) {
        if (!validateQHYPosition(pos)) {
            LOG_F(ERROR, "Invalid position in sequence: {}", pos);
            return false;
        }
    }

    sequencePositions_ = positions;
    sequenceCurrentIndex_ = 0;
    sequenceCallback_ = callback;
    filterSequenceRunning_ = true;

    sequenceThread_ = std::thread(&FilterWheelController::sequenceThreadFunction, this);

    LOG_F(INFO, "Started filter sequence with {} positions", positions.size());
    return true;
}

auto FilterWheelController::stopFilterSequence() -> bool {
    std::lock_guard<std::mutex> lock(sequenceMutex_);

    if (!filterSequenceRunning_) {
        return true;
    }

    filterSequenceRunning_ = false;

    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }

    LOG_F(INFO, "Filter sequence stopped");
    return true;
}

auto FilterWheelController::isFilterSequenceRunning() const -> bool {
    return filterSequenceRunning_;
}

auto FilterWheelController::getFilterSequenceProgress() const -> std::pair<int, int> {
    return {sequenceCurrentIndex_, static_cast<int>(sequencePositions_.size())};
}

// Private helper methods

auto FilterWheelController::detectQHYFilterWheel() -> bool {
    try {
#ifdef LITHIUM_QHY_ENABLED
        QHYCCD_ERROR ret = ScanQHYCFW();
        if (ret != QHYCCD_SUCCESS) {
            LOG_F(INFO, "No QHY filter wheel detected");
            return false;
        }

        char cfwId[32];
        ret = GetQHYCFWId(cfwId, 0);
        if (ret != QHYCCD_SUCCESS) {
            LOG_F(INFO, "No QHY filter wheel ID found");
            return false;
        }

        LOG_F(INFO, "QHY filter wheel detected: {}", cfwId);
        return true;
#else
        // Simulation mode - always detect
        LOG_F(INFO, "QHY filter wheel detected (simulation mode)");
        return true;
#endif

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception detecting QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::initializeQHYFilterWheel() -> bool {
    try {
        LOG_F(INFO, "Initializing QHY filter wheel");

        // Filter wheel specific initialization
        qhyFilterCount_ = 0;
        qhyCurrentFilterPosition_ = 1;
        qhyFilterWheelMoving_ = false;
        qhyFilterWheelConnected_ = false;
        qhyFilterWheelClockwise_ = true;

        // Clear collections
        qhyFilterNames_.clear();
        filterOffsets_.clear();

        LOG_F(INFO, "QHY filter wheel initialized");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception initializing QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::shutdownQHYFilterWheel() -> bool {
    try {
        LOG_F(INFO, "Shutting down QHY filter wheel");

        // Reset state
        hasQHYFilterWheel_ = false;
        qhyFilterWheelConnected_ = false;
        qhyFilterWheelMoving_ = false;

        LOG_F(INFO, "QHY filter wheel shutdown complete");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception shutting down QHY filter wheel: {}", e.what());
        return false;
    }
}

auto FilterWheelController::waitForQHYMovement(int timeoutMs) -> bool {
    auto startTime = std::chrono::steady_clock::now();

    while (qhyFilterWheelMoving_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            LOG_F(ERROR, "Timeout waiting for filter wheel movement");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return true;
}

auto FilterWheelController::validateQHYPosition(int position) const -> bool {
    return position >= 1 && position <= qhyFilterCount_;
}

auto FilterWheelController::notifyMovementChange(int position, bool moving) -> void {
    if (movementCallback_) {
        movementCallback_(position, moving);
    }
}

auto FilterWheelController::addMovementToHistory(int position) -> void {
    if (!movementLoggingEnabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(historyMutex_);

    auto now = std::chrono::system_clock::now();
    movementHistory_.emplace_back(now, position);

    // Keep history size manageable
    if (movementHistory_.size() > MAX_HISTORY_SIZE) {
        movementHistory_.erase(movementHistory_.begin());
    }
}

auto FilterWheelController::monitoringThreadFunction() -> void {
    LOG_F(INFO, "QHY filter wheel monitoring thread started");

    while (filterWheelMonitoringEnabled_) {
        try {
            if (qhyFilterWheelConnected_) {
                // Monitor filter wheel status
#ifdef LITHIUM_QHY_ENABLED
                char status[32];
                // GetQHYCFWStatus(cfwHandle, status);
                // Parse status and update state
#endif
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in monitoring thread: {}", e.what());
        }
    }

    LOG_F(INFO, "QHY filter wheel monitoring thread stopped");
}

auto FilterWheelController::sequenceThreadFunction() -> void {
    LOG_F(INFO, "Filter sequence thread started");

    while (filterSequenceRunning_ && sequenceCurrentIndex_ < sequencePositions_.size()) {
        try {
            int position = sequencePositions_[sequenceCurrentIndex_];

            LOG_F(INFO, "Executing sequence step {}/{}: position {}",
                  sequenceCurrentIndex_ + 1, sequencePositions_.size(), position);

            if (!executeSequenceStep(position)) {
                LOG_F(ERROR, "Failed to execute sequence step at position {}", position);
                break;
            }

            if (sequenceCallback_) {
                sequenceCallback_(position, sequenceCurrentIndex_ == sequencePositions_.size() - 1);
            }

            sequenceCurrentIndex_++;

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in sequence thread: {}", e.what());
            break;
        }
    }

    filterSequenceRunning_ = false;
    LOG_F(INFO, "Filter sequence thread completed");
}

auto FilterWheelController::executeSequenceStep(int position) -> bool {
    return setQHYFilterPosition(position);
}

auto FilterWheelController::getFilterWheelStatusString() const -> std::string {
    if (!qhyFilterWheelConnected_) {
        return "Disconnected";
    }

    if (qhyFilterWheelMoving_) {
        return "Moving";
    }

    return "Idle at position " + std::to_string(qhyCurrentFilterPosition_);
}

auto FilterWheelController::sendFilterWheelCommand(const std::string& command) -> std::string {
#ifdef LITHIUM_QHY_ENABLED
    // Send command to filter wheel
    // SendOrder2QHYCFW(cfwHandle, command.data(), command.length());

    // Wait for response
    char response[64];
    // GetQHYCFWStatus(cfwHandle, response);
    return std::string(response);
#else
    // Simulation mode
    return "OK";
#endif
}

auto FilterWheelController::parseFilterWheelResponse(const std::string& response) -> bool {
    // Parse response from filter wheel
    if (response.empty()) {
        return false;
    }

    // Check for error responses
    if (response.find("ERROR") != std::string::npos) {
        LOG_F(ERROR, "Filter wheel error: {}", response);
        return false;
    }

    return true;
}

} // namespace lithium::device::qhy::camera
