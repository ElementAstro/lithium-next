/*
 * ascom_device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Device Factory - Creates and manages ASCOM device instances

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_DEVICE_FACTORY_HPP
#define LITHIUM_CLIENT_ASCOM_DEVICE_FACTORY_HPP

#include "ascom_camera.hpp"
#include "ascom_dome.hpp"
#include "ascom_filterwheel.hpp"
#include "ascom_focuser.hpp"
#include "ascom_observingconditions.hpp"
#include "ascom_rotator.hpp"
#include "ascom_telescope.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::client::ascom {

/**
 * @brief Device creator function type
 */
using DeviceCreator =
    std::function<std::shared_ptr<ASCOMDeviceBase>(const std::string&, int)>;

/**
 * @brief ASCOM Device Factory
 *
 * Creates and manages ASCOM device instances. Supports:
 * - Factory pattern for device creation
 * - Device registration and lookup
 * - Custom device type registration
 */
class ASCOMDeviceFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static ASCOMDeviceFactory& getInstance();

    // Disable copy
    ASCOMDeviceFactory(const ASCOMDeviceFactory&) = delete;
    ASCOMDeviceFactory& operator=(const ASCOMDeviceFactory&) = delete;

    // ==================== Device Creation ====================

    /**
     * @brief Create a device by type
     * @param type Device type
     * @param name Device name
     * @param deviceNumber Device number
     * @return Device instance or nullptr
     */
    auto createDevice(ASCOMDeviceType type, const std::string& name,
                      int deviceNumber = 0) -> std::shared_ptr<ASCOMDeviceBase>;

    /**
     * @brief Create a device by type string
     * @param typeStr Device type string
     * @param name Device name
     * @param deviceNumber Device number
     * @return Device instance or nullptr
     */
    auto createDevice(const std::string& typeStr, const std::string& name,
                      int deviceNumber = 0) -> std::shared_ptr<ASCOMDeviceBase>;

    // ==================== Typed Creation ====================

    auto createCamera(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMCamera>;

    auto createFocuser(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMFocuser>;

    auto createFilterWheel(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMFilterWheel>;

    auto createTelescope(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMTelescope>;

    auto createRotator(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMRotator>;

    auto createDome(const std::string& name, int deviceNumber = 0)
        -> std::shared_ptr<ASCOMDome>;

    auto createObservingConditions(const std::string& name,
                                   int deviceNumber = 0)
        -> std::shared_ptr<ASCOMObservingConditions>;

    // ==================== Custom Registration ====================

    /**
     * @brief Register a custom device creator
     * @param type Device type
     * @param creator Creator function
     */
    void registerCreator(ASCOMDeviceType type, DeviceCreator creator);

    /**
     * @brief Check if device type is supported
     * @param type Device type
     * @return true if supported
     */
    [[nodiscard]] auto isSupported(ASCOMDeviceType type) const -> bool;

    /**
     * @brief Get supported device types
     * @return Vector of supported types
     */
    [[nodiscard]] auto getSupportedTypes() const
        -> std::vector<ASCOMDeviceType>;

private:
    ASCOMDeviceFactory();
    ~ASCOMDeviceFactory() = default;

    void registerDefaultCreators();

    std::unordered_map<ASCOMDeviceType, DeviceCreator> creators_;
    mutable std::mutex mutex_;
};

/**
 * @brief ASCOM Device Manager
 *
 * Manages a collection of ASCOM devices with lifecycle management.
 */
class ASCOMDeviceManager {
public:
    ASCOMDeviceManager() = default;
    ~ASCOMDeviceManager();

    // ==================== Device Management ====================

    /**
     * @brief Add a device
     * @param device Device to add
     * @return true if added
     */
    auto addDevice(std::shared_ptr<ASCOMDeviceBase> device) -> bool;

    /**
     * @brief Remove a device by name
     * @param name Device name
     * @return true if removed
     */
    auto removeDevice(const std::string& name) -> bool;

    /**
     * @brief Get device by name
     * @param name Device name
     * @return Device or nullptr
     */
    auto getDevice(const std::string& name) -> std::shared_ptr<ASCOMDeviceBase>;

    /**
     * @brief Get all devices
     * @return Vector of devices
     */
    auto getAllDevices() const -> std::vector<std::shared_ptr<ASCOMDeviceBase>>;

    /**
     * @brief Get devices by type
     * @param type Device type
     * @return Vector of devices
     */
    auto getDevicesByType(ASCOMDeviceType type) const
        -> std::vector<std::shared_ptr<ASCOMDeviceBase>>;

    // ==================== Typed Getters ====================

    auto getCameras() const -> std::vector<std::shared_ptr<ASCOMCamera>>;
    auto getFocusers() const -> std::vector<std::shared_ptr<ASCOMFocuser>>;
    auto getFilterWheels() const
        -> std::vector<std::shared_ptr<ASCOMFilterWheel>>;
    auto getTelescopes() const -> std::vector<std::shared_ptr<ASCOMTelescope>>;
    auto getRotators() const -> std::vector<std::shared_ptr<ASCOMRotator>>;
    auto getDomes() const -> std::vector<std::shared_ptr<ASCOMDome>>;
    auto getObservingConditions() const
        -> std::vector<std::shared_ptr<ASCOMObservingConditions>>;

    // ==================== Lifecycle ====================

    /**
     * @brief Connect all devices
     * @param client Alpaca client to use
     * @return Number of devices connected
     */
    auto connectAll(std::shared_ptr<AlpacaClient> client) -> int;

    /**
     * @brief Disconnect all devices
     * @return Number of devices disconnected
     */
    auto disconnectAll() -> int;

    /**
     * @brief Check if device exists
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
     * @brief Clear all devices
     */
    void clear();

private:
    std::unordered_map<std::string, std::shared_ptr<ASCOMDeviceBase>> devices_;
    mutable std::mutex mutex_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_DEVICE_FACTORY_HPP
