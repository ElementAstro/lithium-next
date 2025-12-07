/*
 * indigo_device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Device Factory - Creates INDIGO device instances

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_DEVICE_FACTORY_HPP
#define LITHIUM_CLIENT_INDIGO_DEVICE_FACTORY_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "indigo_client.hpp"
#include "indigo_device_base.hpp"

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"

namespace lithium::client::indigo {

using json = nlohmann::json;
using namespace lithium::device;

// Forward declarations
class INDIGOCamera;
class INDIGOFocuser;
class INDIGOFilterWheel;
class INDIGOTelescope;
class INDIGODome;
class INDIGORotator;
class INDIGOWeather;
class INDIGOGPS;

/**
 * @brief Device type enumeration for INDIGO devices
 */
enum class INDIGODeviceType {
    Unknown,
    Camera,
    Focuser,
    FilterWheel,
    Telescope,
    Dome,
    Rotator,
    Weather,
    GPS,
    Guider,
    AO,          // Adaptive Optics
    Dustcap,
    Lightbox,
    Detector,
    Spectrograph,
    Aux
};

/**
 * @brief Device creation function type
 */
using DeviceCreator = std::function<std::shared_ptr<INDIGODeviceBase>(
    const std::string& deviceName, std::shared_ptr<INDIGOClient> client)>;

/**
 * @brief INDIGO Device Factory
 *
 * Factory for creating INDIGO device instances. Supports:
 * - Standard device type creation (Camera, Focuser, etc.)
 * - Custom device type registration
 * - Device discovery and enumeration
 * - Connection pooling (shared INDIGO client)
 */
class INDIGODeviceFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> INDIGODeviceFactory&;

    /**
     * @brief Create a device by type
     * @param type Device type
     * @param deviceName INDIGO device name
     * @param client Optional shared client (creates new if null)
     * @return Device instance or error
     */
    auto createDevice(INDIGODeviceType type, const std::string& deviceName,
                      std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGODeviceBase>>;

    /**
     * @brief Create a device by type name string
     * @param typeName Device type name (e.g., "Camera", "Focuser")
     * @param deviceName INDIGO device name
     * @param client Optional shared client
     * @return Device instance or error
     */
    auto createDevice(const std::string& typeName, const std::string& deviceName,
                      std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGODeviceBase>>;

    /**
     * @brief Create a device from discovered device info
     * @param device Discovered device information
     * @param client Optional shared client
     * @return Device instance or error
     */
    auto createDevice(const DiscoveredDevice& device,
                      std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGODeviceBase>>;

    /**
     * @brief Create a camera device
     */
    auto createCamera(const std::string& deviceName,
                      std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOCamera>>;

    /**
     * @brief Create a focuser device
     */
    auto createFocuser(const std::string& deviceName,
                       std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOFocuser>>;

    /**
     * @brief Create a filterwheel device
     */
    auto createFilterWheel(const std::string& deviceName,
                           std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOFilterWheel>>;

    /**
     * @brief Create a telescope device
     */
    auto createTelescope(const std::string& deviceName,
                         std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOTelescope>>;

    /**
     * @brief Create a dome device
     */
    auto createDome(const std::string& deviceName,
                    std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGODome>>;

    /**
     * @brief Create a rotator device
     */
    auto createRotator(const std::string& deviceName,
                       std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGORotator>>;

    /**
     * @brief Create a weather station device
     */
    auto createWeather(const std::string& deviceName,
                       std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOWeather>>;

    /**
     * @brief Create a GPS device
     */
    auto createGPS(const std::string& deviceName,
                   std::shared_ptr<INDIGOClient> client = nullptr)
        -> DeviceResult<std::shared_ptr<INDIGOGPS>>;

    /**
     * @brief Register a custom device creator
     * @param typeName Type name for the device
     * @param creator Creator function
     */
    void registerCreator(const std::string& typeName, DeviceCreator creator);

    /**
     * @brief Unregister a device creator
     * @param typeName Type name
     */
    void unregisterCreator(const std::string& typeName);

    /**
     * @brief Check if a device type is supported
     * @param typeName Type name
     */
    [[nodiscard]] auto isTypeSupported(const std::string& typeName) const -> bool;

    /**
     * @brief Get list of supported device types
     */
    [[nodiscard]] auto getSupportedTypes() const -> std::vector<std::string>;

    /**
     * @brief Infer device type from device interface flags
     * @param interfaces Device interface bitmask
     * @return Inferred device type
     */
    [[nodiscard]] static auto inferDeviceType(DeviceInterface interfaces)
        -> INDIGODeviceType;

    /**
     * @brief Convert device type to string
     */
    [[nodiscard]] static auto deviceTypeToString(INDIGODeviceType type)
        -> std::string;

    /**
     * @brief Convert string to device type
     */
    [[nodiscard]] static auto deviceTypeFromString(const std::string& typeName)
        -> INDIGODeviceType;

    // ==================== Connection Management ====================

    /**
     * @brief Get or create a shared INDIGO client for connection pooling
     * @param host Server host
     * @param port Server port
     * @return Shared client instance
     */
    auto getOrCreateClient(const std::string& host = "localhost", int port = 7624)
        -> std::shared_ptr<INDIGOClient>;

    /**
     * @brief Release a client from the pool
     * @param host Server host
     * @param port Server port
     */
    void releaseClient(const std::string& host, int port);

    /**
     * @brief Clear all pooled clients
     */
    void clearClientPool();

private:
    INDIGODeviceFactory();
    ~INDIGODeviceFactory() = default;

    INDIGODeviceFactory(const INDIGODeviceFactory&) = delete;
    INDIGODeviceFactory& operator=(const INDIGODeviceFactory&) = delete;

    void registerDefaultCreators();

    std::unordered_map<std::string, DeviceCreator> creators_;
    std::unordered_map<std::string, std::shared_ptr<INDIGOClient>> clientPool_;
    mutable std::mutex mutex_;
};

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_DEVICE_FACTORY_HPP
