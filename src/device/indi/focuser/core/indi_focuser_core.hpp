#ifndef LITHIUM_INDI_FOCUSER_CORE_HPP
#define LITHIUM_INDI_FOCUSER_CORE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <string>
#include <memory>

#include "device/template/focuser.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Core state and functionality for INDI Focuser
 *
 * This class encapsulates the essential state and INDI-specific functionality
 * that all focuser components need access to. It follows the same pattern
 * as INDICameraCore for consistency across the codebase.
 */
class INDIFocuserCore {
public:
    explicit INDIFocuserCore(std::string name);
    ~INDIFocuserCore() = default;

    // Non-copyable, non-movable due to atomic members
    INDIFocuserCore(const INDIFocuserCore& other) = delete;
    INDIFocuserCore& operator=(const INDIFocuserCore& other) = delete;
    INDIFocuserCore(INDIFocuserCore&& other) = delete;
    INDIFocuserCore& operator=(INDIFocuserCore&& other) = delete;

    // Basic accessors
    const std::string& getName() const { return name_; }
    std::shared_ptr<spdlog::logger> getLogger() const { return logger_; }

    // INDI device access
    INDI::BaseDevice& getDevice() { return device_; }
    const INDI::BaseDevice& getDevice() const { return device_; }
    void setDevice(const INDI::BaseDevice& device) { device_ = device; }

    // Client access for sending properties
    void setClient(INDI::BaseClient* client) { client_ = client; }
    INDI::BaseClient* getClient() const { return client_; }

    // Connection state
    bool isConnected() const { return isConnected_.load(); }
    void setConnected(bool connected) { isConnected_.store(connected); }

    // Device name management
    const std::string& getDeviceName() const { return deviceName_; }
    void setDeviceName(const std::string& deviceName) { deviceName_ = deviceName; }

    // Movement state
    bool isMoving() const { return isFocuserMoving_.load(); }
    void setMoving(bool moving) { isFocuserMoving_.store(moving); }

    // Position tracking
    int getCurrentPosition() const { return realAbsolutePosition_.load(); }
    void setCurrentPosition(int position) { realAbsolutePosition_.store(position); }

    int getRelativePosition() const { return realRelativePosition_.load(); }
    void setRelativePosition(int position) { realRelativePosition_.store(position); }

    // Limits
    int getMaxPosition() const { return maxPosition_; }
    void setMaxPosition(int maxPos) { maxPosition_ = maxPos; }

    int getMinPosition() const { return minPosition_; }
    void setMinPosition(int minPos) { minPosition_ = minPos; }

    // Speed control
    double getCurrentSpeed() const { return currentFocusSpeed_.load(); }
    void setCurrentSpeed(double speed) { currentFocusSpeed_.store(speed); }

    // Direction
    FocusDirection getDirection() const { return focusDirection_; }
    void setDirection(FocusDirection direction) { focusDirection_ = direction; }

    // Reverse setting
    bool isReversed() const { return isReverse_.load(); }
    void setReversed(bool reversed) { isReverse_.store(reversed); }

    // Temperature readings
    double getTemperature() const { return temperature_.load(); }
    void setTemperature(double temp) { temperature_.store(temp); }

    double getChipTemperature() const { return chipTemperature_.load(); }
    void setChipTemperature(double temp) { chipTemperature_.store(temp); }

    // Backlash compensation
    bool isBacklashEnabled() const { return backlashEnabled_.load(); }
    void setBacklashEnabled(bool enabled) { backlashEnabled_.store(enabled); }

    int getBacklashSteps() const { return backlashSteps_.load(); }
    void setBacklashSteps(int steps) { backlashSteps_.store(steps); }

private:
    // Basic identifiers
    std::string name_;
    std::string deviceName_;
    std::shared_ptr<spdlog::logger> logger_;

    // INDI connection
    INDI::BaseDevice device_;
    INDI::BaseClient* client_{nullptr};
    std::atomic_bool isConnected_{false};

    // Movement state
    std::atomic_bool isFocuserMoving_{false};
    FocusDirection focusDirection_{FocusDirection::IN};
    std::atomic<double> currentFocusSpeed_{1.0};
    std::atomic_bool isReverse_{false};

    // Position tracking
    std::atomic_int realRelativePosition_{0};
    std::atomic_int realAbsolutePosition_{0};
    int maxPosition_{100000};
    int minPosition_{0};

    // Backlash compensation
    std::atomic_bool backlashEnabled_{false};
    std::atomic_int backlashSteps_{0};

    // Temperature monitoring
    std::atomic<double> temperature_{0.0};
    std::atomic<double> chipTemperature_{0.0};
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_CORE_HPP
