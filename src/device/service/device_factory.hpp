/*
 * device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device factory for creating device instances from backend discovery

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_DEVICE_FACTORY_HPP
#define LITHIUM_DEVICE_SERVICE_DEVICE_FACTORY_HPP

#include "device_backend.hpp"
#include "device_types.hpp"

#include "device/template/device.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lithium::device {

/**
 * @brief Device creation function type
 */
using DeviceCreator = std::function<std::shared_ptr<AtomDriver>(
    const std::string& name, const DiscoveredDevice& info)>;

/**
 * @brief Device factory for creating device instances
 *
 * Creates appropriate AtomDriver instances based on discovered device info
 * and backend type. Supports registration of custom device creators.
 */
class DeviceFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static DeviceFactory& getInstance() {
        static DeviceFactory instance;
        return instance;
    }

    // Disable copy
    DeviceFactory(const DeviceFactory&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = delete;

    // ==================== Creator Registration ====================

    /**
     * @brief Register a device creator for a specific backend and type
     * @param backend Backend name (e.g., "INDI", "ASCOM")
     * @param deviceType Device type
     * @param creator Creator function
     */
    void registerCreator(const std::string& backend, DeviceType deviceType,
                         DeviceCreator creator) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(backend, deviceType);
        creators_[key] = std::move(creator);
    }

    /**
     * @brief Register a device creator by type string
     */
    void registerCreator(const std::string& backend, const std::string& typeStr,
                         DeviceCreator creator) {
        registerCreator(backend, stringToDeviceType(typeStr),
                        std::move(creator));
    }

    /**
     * @brief Unregister a device creator
     */
    void unregisterCreator(const std::string& backend, DeviceType deviceType) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(backend, deviceType);
        creators_.erase(key);
    }

    /**
     * @brief Check if a creator is registered
     */
    bool hasCreator(const std::string& backend, DeviceType deviceType) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(backend, deviceType);
        return creators_.find(key) != creators_.end();
    }

    // ==================== Device Creation ====================

    /**
     * @brief Create a device from discovered device info
     * @param info Discovered device information
     * @return Created device or nullptr if no creator found
     */
    std::shared_ptr<AtomDriver> createDevice(const DiscoveredDevice& info) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Determine device type
        DeviceType type = stringToDeviceType(info.deviceType);
        if (type == DeviceType::Unknown) {
            return nullptr;
        }

        // Find creator
        std::string key = makeKey(info.driverName, type);
        auto it = creators_.find(key);
        if (it == creators_.end()) {
            // Try generic creator for this type
            key = makeKey("*", type);
            it = creators_.find(key);
            if (it == creators_.end()) {
                return nullptr;
            }
        }

        // Create device
        return it->second(info.displayName, info);
    }

    /**
     * @brief Create a device with explicit type
     */
    std::shared_ptr<AtomDriver> createDevice(const std::string& backend,
                                             DeviceType type,
                                             const std::string& name,
                                             const DiscoveredDevice& info) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = makeKey(backend, type);
        auto it = creators_.find(key);
        if (it == creators_.end()) {
            key = makeKey("*", type);
            it = creators_.find(key);
            if (it == creators_.end()) {
                return nullptr;
            }
        }

        return it->second(name, info);
    }

    // ==================== Batch Creation ====================

    /**
     * @brief Create devices from a list of discovered devices
     * @param devices List of discovered devices
     * @return Map of device ID to created device
     */
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> createDevices(
        const std::vector<DiscoveredDevice>& devices) {
        std::unordered_map<std::string, std::shared_ptr<AtomDriver>> result;

        for (const auto& info : devices) {
            auto device = createDevice(info);
            if (device) {
                result[info.deviceId] = device;
            }
        }

        return result;
    }

    // ==================== Initialization ====================

    /**
     * @brief Initialize default creators for INDI and ASCOM devices
     */
    void initializeDefaultCreators();

    /**
     * @brief Clear all registered creators
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        creators_.clear();
    }

private:
    DeviceFactory() = default;

    std::string makeKey(const std::string& backend, DeviceType type) const {
        return backend + ":" + deviceTypeToString(type);
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, DeviceCreator> creators_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_DEVICE_FACTORY_HPP
