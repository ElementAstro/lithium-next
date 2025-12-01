/*
 * indi_device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Device Factory - Creates and manages INDI device instances

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_DEVICE_FACTORY_HPP
#define LITHIUM_CLIENT_INDI_DEVICE_FACTORY_HPP

#include "indi_camera.hpp"
#include "indi_dome.hpp"
#include "indi_filterwheel.hpp"
#include "indi_focuser.hpp"
#include "indi_gps.hpp"
#include "indi_rotator.hpp"
#include "indi_telescope.hpp"
#include "indi_weather.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace lithium::client::indi {

/**
 * @brief Device type enumeration
 */
enum class DeviceType { Camera, Focuser, FilterWheel, Telescope, Rotator, Dome, Weather, GPS, Unknown };

/**
 * @brief Convert device type to string
 */
[[nodiscard]] inline auto deviceTypeToString(DeviceType type) -> std::string {
    switch (type) {
        case DeviceType::Camera:
            return "Camera";
        case DeviceType::Focuser:
            return "Focuser";
        case DeviceType::FilterWheel:
            return "FilterWheel";
        case DeviceType::Telescope:
            return "Telescope";
        case DeviceType::Rotator:
            return "Rotator";
        case DeviceType::Dome:
            return "Dome";
        case DeviceType::Weather:
            return "Weather";
        case DeviceType::GPS:
            return "GPS";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert string to device type
 */
[[nodiscard]] inline auto deviceTypeFromString(const std::string& str)
    -> DeviceType {
    if (str == "Camera" || str == "CCD")
        return DeviceType::Camera;
    if (str == "Focuser")
        return DeviceType::Focuser;
    if (str == "FilterWheel" || str == "Filter Wheel")
        return DeviceType::FilterWheel;
    if (str == "Telescope" || str == "Mount")
        return DeviceType::Telescope;
    if (str == "Rotator")
        return DeviceType::Rotator;
    if (str == "Dome")
        return DeviceType::Dome;
    if (str == "Weather" || str == "Weather Station")
        return DeviceType::Weather;
    if (str == "GPS")
        return DeviceType::GPS;
    return DeviceType::Unknown;
}

/**
 * @brief INDI Device Factory
 *
 * Creates and manages INDI device instances. Supports:
 * - Factory pattern for device creation
 * - Device registration and lookup
 * - Custom device type registration
 */
class INDIDeviceFactory {
public:
    using DeviceCreator = std::function<std::shared_ptr<INDIDeviceBase>(
        const std::string& name)>;

    /**
     * @brief Get singleton instance
     * @return Factory instance
     */
    static auto getInstance() -> INDIDeviceFactory&;

    /**
     * @brief Create a device by type
     * @param type Device type
     * @param name Device name
     * @return Device instance, or nullptr if type unknown
     */
    auto createDevice(DeviceType type, const std::string& name)
        -> std::shared_ptr<INDIDeviceBase>;

    /**
     * @brief Create a device by type string
     * @param typeStr Device type string
     * @param name Device name
     * @return Device instance, or nullptr if type unknown
     */
    auto createDevice(const std::string& typeStr, const std::string& name)
        -> std::shared_ptr<INDIDeviceBase>;

    /**
     * @brief Create a camera device
     * @param name Device name
     * @return Camera instance
     */
    auto createCamera(const std::string& name) -> std::shared_ptr<INDICamera>;

    /**
     * @brief Create a focuser device
     * @param name Device name
     * @return Focuser instance
     */
    auto createFocuser(const std::string& name) -> std::shared_ptr<INDIFocuser>;

    /**
     * @brief Create a filterwheel device
     * @param name Device name
     * @return FilterWheel instance
     */
    auto createFilterWheel(const std::string& name)
        -> std::shared_ptr<INDIFilterWheel>;

    /**
     * @brief Create a telescope device
     * @param name Device name
     * @return Telescope instance
     */
    auto createTelescope(const std::string& name)
        -> std::shared_ptr<INDITelescope>;

    /**
     * @brief Create a rotator device
     * @param name Device name
     * @return Rotator instance
     */
    auto createRotator(const std::string& name)
        -> std::shared_ptr<INDIRotator>;

    /**
     * @brief Create a dome device
     * @param name Device name
     * @return Dome instance
     */
    auto createDome(const std::string& name)
        -> std::shared_ptr<INDIDome>;

    /**
     * @brief Create a weather device
     * @param name Device name
     * @return Weather instance
     */
    auto createWeather(const std::string& name)
        -> std::shared_ptr<INDIWeather>;

    /**
     * @brief Create a GPS device
     * @param name Device name
     * @return GPS instance
     */
    auto createGPS(const std::string& name)
        -> std::shared_ptr<INDIGPS>;

    /**
     * @brief Register a custom device creator
     * @param type Device type
     * @param creator Creator function
     */
    void registerCreator(DeviceType type, DeviceCreator creator);

    /**
     * @brief Register a custom device creator by type string
     * @param typeStr Device type string
     * @param creator Creator function
     */
    void registerCreator(const std::string& typeStr, DeviceCreator creator);

    /**
     * @brief Check if a device type is supported
     * @param type Device type
     * @return true if supported
     */
    [[nodiscard]] auto isSupported(DeviceType type) const -> bool;

    /**
     * @brief Get supported device types
     * @return Vector of supported types
     */
    [[nodiscard]] auto getSupportedTypes() const -> std::vector<DeviceType>;

private:
    INDIDeviceFactory();
    ~INDIDeviceFactory() = default;

    // Disable copy
    INDIDeviceFactory(const INDIDeviceFactory&) = delete;
    INDIDeviceFactory& operator=(const INDIDeviceFactory&) = delete;

    void registerDefaultCreators();

    std::unordered_map<DeviceType, DeviceCreator> creators_;
    std::unordered_map<std::string, DeviceType> typeMap_;
};

/**
 * @brief INDI Device Manager
 *
 * Manages a collection of INDI devices. Provides:
 * - Device lifecycle management
 * - Device lookup by name or type
 * - Batch operations
 */
class INDIDeviceManager {
public:
    /**
     * @brief Construct a new device manager
     */
    INDIDeviceManager() = default;

    /**
     * @brief Destructor - disconnects all devices
     */
    ~INDIDeviceManager();

    /**
     * @brief Add a device
     * @param device Device to add
     * @return true if added
     */
    auto addDevice(std::shared_ptr<INDIDeviceBase> device) -> bool;

    /**
     * @brief Remove a device by name
     * @param name Device name
     * @return true if removed
     */
    auto removeDevice(const std::string& name) -> bool;

    /**
     * @brief Get a device by name
     * @param name Device name
     * @return Device, or nullptr if not found
     */
    auto getDevice(const std::string& name) const
        -> std::shared_ptr<INDIDeviceBase>;

    /**
     * @brief Get a device by name and type
     * @tparam T Device type
     * @param name Device name
     * @return Device, or nullptr if not found or wrong type
     */
    template <typename T>
    auto getDevice(const std::string& name) const -> std::shared_ptr<T> {
        auto device = getDevice(name);
        return std::dynamic_pointer_cast<T>(device);
    }

    /**
     * @brief Get all devices
     * @return Vector of all devices
     */
    auto getDevices() const -> std::vector<std::shared_ptr<INDIDeviceBase>>;

    /**
     * @brief Get devices by type
     * @param type Device type
     * @return Vector of devices of the specified type
     */
    auto getDevicesByType(DeviceType type) const
        -> std::vector<std::shared_ptr<INDIDeviceBase>>;

    /**
     * @brief Get all cameras
     * @return Vector of cameras
     */
    auto getCameras() const -> std::vector<std::shared_ptr<INDICamera>>;

    /**
     * @brief Get all focusers
     * @return Vector of focusers
     */
    auto getFocusers() const -> std::vector<std::shared_ptr<INDIFocuser>>;

    /**
     * @brief Get all filterwheels
     * @return Vector of filterwheels
     */
    auto getFilterWheels() const
        -> std::vector<std::shared_ptr<INDIFilterWheel>>;

    /**
     * @brief Get all telescopes
     * @return Vector of telescopes
     */
    auto getTelescopes() const -> std::vector<std::shared_ptr<INDITelescope>>;

    /**
     * @brief Get all rotators
     * @return Vector of rotators
     */
    auto getRotators() const -> std::vector<std::shared_ptr<INDIRotator>>;

    /**
     * @brief Get all domes
     * @return Vector of domes
     */
    auto getDomes() const -> std::vector<std::shared_ptr<INDIDome>>;

    /**
     * @brief Get all weather stations
     * @return Vector of weather stations
     */
    auto getWeatherStations() const -> std::vector<std::shared_ptr<INDIWeather>>;

    /**
     * @brief Get all GPS devices
     * @return Vector of GPS devices
     */
    auto getGPSDevices() const -> std::vector<std::shared_ptr<INDIGPS>>;

    /**
     * @brief Check if a device exists
     * @param name Device name
     * @return true if exists
     */
    [[nodiscard]] auto hasDevice(const std::string& name) const -> bool;

    /**
     * @brief Get device count
     * @return Number of devices
     */
    [[nodiscard]] auto getDeviceCount() const -> size_t;

    /**
     * @brief Connect all devices
     * @return Number of successfully connected devices
     */
    auto connectAll() -> int;

    /**
     * @brief Disconnect all devices
     * @return Number of successfully disconnected devices
     */
    auto disconnectAll() -> int;

    /**
     * @brief Initialize all devices
     * @return Number of successfully initialized devices
     */
    auto initializeAll() -> int;

    /**
     * @brief Destroy all devices
     */
    void destroyAll();

    /**
     * @brief Clear all devices
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<INDIDeviceBase>> devices_;
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_DEVICE_FACTORY_HPP
