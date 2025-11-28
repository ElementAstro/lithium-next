#ifndef LITHIUM_DEVICE_MANAGER_HPP
#define LITHIUM_DEVICE_MANAGER_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "script/sheller.hpp"
#include "template/device.hpp"

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"

class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...)                               \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...)                              \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

namespace lithium {

using json = nlohmann::json;

/**
 * @brief Device metadata for enhanced device management
 */
struct DeviceMetadata {
    std::string deviceId;           // Unique device identifier
    std::string displayName;        // Human-readable name
    std::string driverName;         // Driver/backend name (e.g., "INDI", "ASCOM")
    std::string driverVersion;      // Driver version
    std::string connectionString;   // Connection parameters
    int priority = 0;               // Device priority (higher = preferred)
    bool autoConnect = false;       // Auto-connect on startup
    json customProperties;          // Additional device-specific properties

    auto toJson() const -> json {
        json j;
        j["deviceId"] = deviceId;
        j["displayName"] = displayName;
        j["driverName"] = driverName;
        j["driverVersion"] = driverVersion;
        j["connectionString"] = connectionString;
        j["priority"] = priority;
        j["autoConnect"] = autoConnect;
        j["customProperties"] = customProperties;
        return j;
    }

    static auto fromJson(const json& j) -> DeviceMetadata {
        DeviceMetadata meta;
        meta.deviceId = j.value("deviceId", "");
        meta.displayName = j.value("displayName", "");
        meta.driverName = j.value("driverName", "");
        meta.driverVersion = j.value("driverVersion", "");
        meta.connectionString = j.value("connectionString", "");
        meta.priority = j.value("priority", 0);
        meta.autoConnect = j.value("autoConnect", false);
        if (j.contains("customProperties")) {
            meta.customProperties = j["customProperties"];
        }
        return meta;
    }
};

/**
 * @brief Device state information
 */
struct DeviceState {
    bool isConnected = false;
    bool isInitialized = false;
    bool isBusy = false;
    std::string lastError;
    float healthScore = 1.0f;  // 0.0 to 1.0
    std::chrono::system_clock::time_point lastActivity;

    auto toJson() const -> json {
        json j;
        j["isConnected"] = isConnected;
        j["isInitialized"] = isInitialized;
        j["isBusy"] = isBusy;
        j["lastError"] = lastError;
        j["healthScore"] = healthScore;
        return j;
    }
};

/**
 * @brief Device event types
 */
enum class DeviceEventType {
    DEVICE_ADDED,
    DEVICE_REMOVED,
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    DEVICE_ERROR,
    DEVICE_STATE_CHANGED,
    PRIMARY_DEVICE_CHANGED
};

/**
 * @brief Device event callback type
 */
using DeviceEventCallback = std::function<void(
    DeviceEventType event, const std::string& deviceType,
    const std::string& deviceName, const json& data)>;

/**
 * @brief Enhanced device manager with INDI integration support
 */
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // 禁用拷贝
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    // 设备管理接口
    void addDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDevice(const std::string& type,
                      std::shared_ptr<AtomDriver> device);
    void removeDeviceByName(const std::string& name);
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
    getDevices() const;

    // 主设备管理
    void setPrimaryDevice(const std::string& type,
                          std::shared_ptr<AtomDriver> device);
    std::shared_ptr<AtomDriver> getPrimaryDevice(const std::string& type) const;

    // 设备操作接口
    void connectAllDevices();
    void disconnectAllDevices();
    void connectDevicesByType(const std::string& type);
    void disconnectDevicesByType(const std::string& type);
    void connectDeviceByName(const std::string& name);
    void disconnectDeviceByName(const std::string& name);

    // 查询接口
    std::shared_ptr<AtomDriver> getDeviceByName(const std::string& name) const;
    std::shared_ptr<AtomDriver> findDeviceByName(const std::string& name) const;
    std::vector<std::shared_ptr<AtomDriver>> findDevicesByType(
        const std::string& type) const;
    bool isDeviceConnected(const std::string& name) const;
    std::string getDeviceType(const std::string& name) const;

    // 设备生命周期管理
    bool initializeDevice(const std::string& name);
    bool destroyDevice(const std::string& name);
    std::vector<std::string> scanDevices(const std::string& type);

    // 新增方法
    bool isDeviceValid(const std::string& name) const;
    void setDeviceRetryStrategy(const std::string& name,
                                const RetryStrategy& strategy);
    float getDeviceHealth(const std::string& name) const;
    void abortDeviceOperation(const std::string& name);
    void resetDevice(const std::string& name);

    // ========== Enhanced Device Management ==========

    /**
     * @brief Add device with metadata
     */
    void addDeviceWithMetadata(const std::string& type,
                               std::shared_ptr<AtomDriver> device,
                               const DeviceMetadata& metadata);

    /**
     * @brief Get device metadata
     */
    std::optional<DeviceMetadata> getDeviceMetadata(
        const std::string& name) const;

    /**
     * @brief Update device metadata
     */
    void updateDeviceMetadata(const std::string& name,
                              const DeviceMetadata& metadata);

    /**
     * @brief Get device state
     */
    std::optional<DeviceState> getDeviceState(const std::string& name) const;

    /**
     * @brief Get all devices of a type with their states
     */
    std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>>
    getDevicesWithState(const std::string& type) const;

    /**
     * @brief Find devices by driver name
     */
    std::vector<std::shared_ptr<AtomDriver>> findDevicesByDriver(
        const std::string& driverName) const;

    /**
     * @brief Get device by ID
     */
    std::shared_ptr<AtomDriver> getDeviceById(
        const std::string& deviceId) const;

    /**
     * @brief Execute operation on device with automatic retry
     */
    template <typename Func>
    auto withDevice(const std::string& name, Func&& operation)
        -> decltype(operation(std::declval<std::shared_ptr<AtomDriver>>()));

    /**
     * @brief Execute operation on primary device of type
     */
    template <typename DeviceT, typename Func>
    auto withPrimaryDevice(const std::string& type, Func&& operation)
        -> decltype(operation(std::declval<std::shared_ptr<DeviceT>>()));

    // ========== Event System ==========

    /**
     * @brief Register event callback
     */
    void registerEventCallback(DeviceEventCallback callback);

    /**
     * @brief Unregister event callback
     */
    void unregisterEventCallback();

    // ========== Serialization ==========

    /**
     * @brief Export device configuration to JSON
     */
    json exportConfiguration() const;

    /**
     * @brief Import device configuration from JSON
     */
    void importConfiguration(const json& config);

    /**
     * @brief Get manager status as JSON
     */
    json getStatus() const;

    // ========== Device Discovery ==========

    /**
     * @brief Discover available devices (INDI/ASCOM)
     */
    std::vector<DeviceMetadata> discoverDevices(
        const std::string& backend = "INDI");

    /**
     * @brief Refresh device list from backend
     */
    void refreshDevices();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}  // namespace lithium

#endif  // LITHIUM_DEVICE_MANAGER_HPP
