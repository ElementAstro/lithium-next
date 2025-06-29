/*
 * controller_stub.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Simple stub implementation for ASI Filter Wheel Controller

*************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace lithium::device::asi::filterwheel {

// Simple stub class that provides the minimum interface needed
class ASIFilterwheelController {
public:
    ASIFilterwheelController() = default;
    ~ASIFilterwheelController() = default;

    // Basic operations
    bool initialize(const ::std::string& device_path = "") { return true; }
    bool shutdown() { return true; }
    bool isInitialized() const { return true; }

    // Position control
    bool moveToPosition(int position) { return true; }
    int getCurrentPosition() const { return 1; }
    bool isMoving() const { return false; }
    bool stopMovement() { return true; }
    int getSlotCount() const { return 7; }

    // Filter management
    bool setFilterName(int slot, const ::std::string& name) { return true; }
    ::std::string getFilterName(int slot) const { return "Filter " + ::std::to_string(slot); }
    ::std::vector<::std::string> getFilterNames() const {
        return {"Filter 1", "Filter 2", "Filter 3", "Filter 4", "Filter 5", "Filter 6", "Filter 7"};
    }
    bool setFocusOffset(int slot, double offset) { return true; }
    double getFocusOffset(int slot) const { return 0.0; }

    // Calibration
    bool performCalibration() { return true; }
    bool performSelfTest() { return true; }

    // Configuration
    bool saveConfiguration(const ::std::string& filepath = "") { return true; }
    bool loadConfiguration(const ::std::string& filepath = "") { return true; }

    // Sequence operations
    bool createSequence(const ::std::string& name, const ::std::vector<int>& positions, int dwell_time_ms) { return true; }
    bool startSequence(const ::std::string& name) { return true; }
    bool stopSequence() { return true; }
    bool isSequenceRunning() const { return false; }
    double getSequenceProgress() const { return 0.0; }

    // Callbacks
    using PositionCallback = ::std::function<void(int, int)>;
    using SequenceCallback = ::std::function<void(const ::std::string&, int, int)>;

    void setPositionCallback(PositionCallback callback) {}
    void setSequenceCallback(SequenceCallback callback) {}

    // Device info
    ::std::string getDeviceInfo() const { return "ASI EFW Stub"; }
    ::std::string getLastError() const { return ""; }

    // Component access (stub)
    ::std::shared_ptr<void> getHardwareInterface() const { return nullptr; }
};

}  // namespace lithium::device::asi::filterwheel
