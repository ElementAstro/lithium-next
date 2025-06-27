#ifndef LITHIUM_INDI_FILTERWHEEL_CORE_HPP
#define LITHIUM_INDI_FILTERWHEEL_CORE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <spdlog/spdlog.h>
#include <functional>
#include <chrono>
#include <atomic>
#include <string>
#include <memory>
#include <vector>

#include "device/template/filterwheel.hpp"

namespace lithium::device::indi::filterwheel {

/**
 * @brief Core state and functionality for INDI FilterWheel
 * 
 * This class encapsulates the essential state and INDI-specific functionality
 * that all filterwheel components need access to. It follows the same pattern
 * as INDIFocuserCore for consistency across the codebase.
 */
class INDIFilterWheelCore {
public:
    explicit INDIFilterWheelCore(std::string name);
    ~INDIFilterWheelCore() = default;

    // Non-copyable, non-movable due to atomic members
    INDIFilterWheelCore(const INDIFilterWheelCore& other) = delete;
    INDIFilterWheelCore& operator=(const INDIFilterWheelCore& other) = delete;
    INDIFilterWheelCore(INDIFilterWheelCore&& other) = delete;
    INDIFilterWheelCore& operator=(INDIFilterWheelCore&& other) = delete;

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
    
    // Current filter position
    int getCurrentSlot() const { return currentSlot_.load(); }
    void setCurrentSlot(int slot) { currentSlot_.store(slot); }
    
    // Filter wheel configuration
    int getMaxSlot() const { return maxSlot_; }
    void setMaxSlot(int maxSlot) { maxSlot_ = maxSlot; }
    
    int getMinSlot() const { return minSlot_; }
    void setMinSlot(int minSlot) { minSlot_ = minSlot; }
    
    // Filter names
    const std::vector<std::string>& getSlotNames() const { return slotNames_; }
    void setSlotNames(const std::vector<std::string>& names) { slotNames_ = names; }
    
    const std::string& getCurrentSlotName() const { return currentSlotName_; }
    void setCurrentSlotName(const std::string& name) { currentSlotName_ = name; }
    
    // Movement state
    bool isMoving() const { return isMoving_.load(); }
    void setMoving(bool moving) { isMoving_.store(moving); }
    
    // Debug and polling settings
    bool isDebugEnabled() const { return isDebug_.load(); }
    void setDebugEnabled(bool enabled) { isDebug_.store(enabled); }
    
    double getPollingPeriod() const { return currentPollingPeriod_.load(); }
    void setPollingPeriod(double period) { currentPollingPeriod_.store(period); }
    
    // Auto-search settings
    bool isAutoSearchEnabled() const { return deviceAutoSearch_.load(); }
    void setAutoSearchEnabled(bool enabled) { deviceAutoSearch_.store(enabled); }
    
    bool isPortScanEnabled() const { return devicePortScan_.load(); }
    void setPortScanEnabled(bool enabled) { devicePortScan_.store(enabled); }
    
    // Driver information
    const std::string& getDriverExec() const { return driverExec_; }
    void setDriverExec(const std::string& driverExec) { driverExec_ = driverExec; }
    
    const std::string& getDriverVersion() const { return driverVersion_; }
    void setDriverVersion(const std::string& version) { driverVersion_ = version; }
    
    const std::string& getDriverInterface() const { return driverInterface_; }
    void setDriverInterface(const std::string& interface) { driverInterface_ = interface; }

    // Event callbacks (following AtomFilterWheel template)
    using PositionCallback = std::function<void(int position, const std::string& filterName)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using TemperatureCallback = std::function<void(double temperature)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    void setPositionCallback(PositionCallback callback) { positionCallback_ = std::move(callback); }
    void setMoveCompleteCallback(MoveCompleteCallback callback) { moveCompleteCallback_ = std::move(callback); }
    void setTemperatureCallback(TemperatureCallback callback) { temperatureCallback_ = std::move(callback); }
    void setConnectionCallback(ConnectionCallback callback) { connectionCallback_ = std::move(callback); }

    // Notification methods for components to trigger callbacks
    void notifyPositionChange(int position, const std::string& filterName);
    void notifyMoveComplete(bool success, const std::string& message = "");
    void notifyTemperatureChange(double temperature);
    void notifyConnectionChange(bool connected);

private:
    // Basic identifiers
    std::string name_;
    std::string deviceName_;
    std::shared_ptr<spdlog::logger> logger_;
    
    // INDI connection
    INDI::BaseDevice device_;
    INDI::BaseClient* client_{nullptr};
    std::atomic_bool isConnected_{false};
    
    // Filter wheel state
    std::atomic_int currentSlot_{0};
    int maxSlot_{8};
    int minSlot_{1};
    std::string currentSlotName_;
    std::vector<std::string> slotNames_;
    std::atomic_bool isMoving_{false};
    
    // Device settings
    std::atomic_bool deviceAutoSearch_{false};
    std::atomic_bool devicePortScan_{false};
    std::atomic<double> currentPollingPeriod_{1000.0};
    std::atomic_bool isDebug_{false};
    
    // Driver information
    std::string driverExec_;
    std::string driverVersion_;
    std::string driverInterface_;
    
    // Event callbacks
    PositionCallback positionCallback_;
    MoveCompleteCallback moveCompleteCallback_;
    TemperatureCallback temperatureCallback_;
    ConnectionCallback connectionCallback_;
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_CORE_HPP
