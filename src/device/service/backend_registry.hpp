/*
 * backend_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device backend registry for managing multiple backends

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_BACKEND_REGISTRY_HPP
#define LITHIUM_DEVICE_SERVICE_BACKEND_REGISTRY_HPP

#include "device_backend.hpp"
#include "indi_backend.hpp"
#include "ascom_backend.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::device {

/**
 * @brief Device backend registry
 *
 * Manages registration and access to device backends (INDI, ASCOM, etc.)
 * Provides unified interface for device discovery across all backends.
 */
class BackendRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static BackendRegistry& getInstance() {
        static BackendRegistry instance;
        return instance;
    }

    // Disable copy
    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;

    // ==================== Backend Registration ====================

    /**
     * @brief Register a backend
     * @param backend Backend instance
     */
    void registerBackend(std::shared_ptr<DeviceBackend> backend) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (backend) {
            backends_[backend->getBackendName()] = backend;
        }
    }

    /**
     * @brief Register a backend factory
     * @param factory Backend factory
     */
    void registerFactory(std::shared_ptr<DeviceBackendFactory> factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (factory) {
            factories_[factory->getBackendName()] = factory;
        }
    }

    /**
     * @brief Unregister a backend
     * @param name Backend name
     */
    void unregisterBackend(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        backends_.erase(name);
    }

    /**
     * @brief Get backend by name
     * @param name Backend name
     * @return Backend instance or nullptr
     */
    std::shared_ptr<DeviceBackend> getBackend(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = backends_.find(name);
        if (it != backends_.end()) {
            return it->second;
        }

        // Try to create from factory
        auto factoryIt = factories_.find(name);
        if (factoryIt != factories_.end()) {
            auto backend = factoryIt->second->createBackend();
            backends_[name] = backend;
            return backend;
        }

        return nullptr;
    }

    /**
     * @brief Get or create INDI backend
     * @return INDI backend instance
     */
    std::shared_ptr<INDIBackend> getINDIBackend() {
        auto backend = getBackend("INDI");
        if (!backend) {
            auto indiBackend = std::make_shared<INDIBackend>();
            registerBackend(indiBackend);
            return indiBackend;
        }
        return std::dynamic_pointer_cast<INDIBackend>(backend);
    }

    /**
     * @brief Get or create ASCOM backend
     * @return ASCOM backend instance
     */
    std::shared_ptr<ASCOMBackend> getASCOMBackend() {
        auto backend = getBackend("ASCOM");
        if (!backend) {
            auto ascomBackend = std::make_shared<ASCOMBackend>();
            registerBackend(ascomBackend);
            return ascomBackend;
        }
        return std::dynamic_pointer_cast<ASCOMBackend>(backend);
    }

    /**
     * @brief Get all registered backends
     * @return Map of backend name to instance
     */
    std::unordered_map<std::string, std::shared_ptr<DeviceBackend>> getAllBackends() {
        std::lock_guard<std::mutex> lock(mutex_);
        return backends_;
    }

    /**
     * @brief Get list of registered backend names
     * @return Vector of backend names
     */
    std::vector<std::string> getBackendNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(backends_.size());
        for (const auto& [name, _] : backends_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Check if backend is registered
     * @param name Backend name
     * @return true if registered
     */
    bool hasBackend(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return backends_.find(name) != backends_.end() ||
               factories_.find(name) != factories_.end();
    }

    // ==================== Unified Device Discovery ====================

    /**
     * @brief Discover devices from all backends
     * @param timeout Discovery timeout in ms
     * @return Vector of discovered devices
     */
    std::vector<DiscoveredDevice> discoverAllDevices(int timeout = 5000) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DiscoveredDevice> allDevices;

        for (auto& [name, backend] : backends_) {
            if (backend && backend->isServerConnected()) {
                auto devices = backend->discoverDevices(timeout);
                allDevices.insert(allDevices.end(), devices.begin(), devices.end());
            }
        }

        return allDevices;
    }

    /**
     * @brief Discover devices from specific backend
     * @param backendName Backend name
     * @param timeout Discovery timeout in ms
     * @return Vector of discovered devices
     */
    std::vector<DiscoveredDevice> discoverDevices(const std::string& backendName,
                                                   int timeout = 5000) {
        auto backend = getBackend(backendName);
        if (backend && backend->isServerConnected()) {
            return backend->discoverDevices(timeout);
        }
        return {};
    }

    /**
     * @brief Refresh devices from all backends
     * @return Total number of devices found
     */
    int refreshAllDevices() {
        std::lock_guard<std::mutex> lock(mutex_);
        int total = 0;

        for (auto& [name, backend] : backends_) {
            if (backend && backend->isServerConnected()) {
                total += backend->refreshDevices();
            }
        }

        return total;
    }

    /**
     * @brief Get all devices from all backends
     * @return Vector of all devices
     */
    std::vector<DiscoveredDevice> getAllDevices() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DiscoveredDevice> allDevices;

        for (auto& [name, backend] : backends_) {
            if (backend) {
                auto devices = backend->getDevices();
                allDevices.insert(allDevices.end(), devices.begin(), devices.end());
            }
        }

        return allDevices;
    }

    // ==================== Backend Status ====================

    /**
     * @brief Get status of all backends
     * @return JSON with backend status
     */
    json getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        json status = json::object();

        for (const auto& [name, backend] : backends_) {
            if (backend) {
                status[name] = backend->getServerStatus();
            }
        }

        return status;
    }

    /**
     * @brief Connect all backends to their servers
     * @param configs Map of backend name to config
     * @return Number of successfully connected backends
     */
    int connectAllBackends(const std::unordered_map<std::string, BackendConfig>& configs) {
        std::lock_guard<std::mutex> lock(mutex_);
        int connected = 0;

        for (auto& [name, backend] : backends_) {
            auto configIt = configs.find(name);
            if (configIt != configs.end() && backend) {
                if (backend->connectServer(configIt->second)) {
                    connected++;
                }
            }
        }

        return connected;
    }

    /**
     * @brief Disconnect all backends
     */
    void disconnectAllBackends() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [name, backend] : backends_) {
            if (backend && backend->isServerConnected()) {
                backend->disconnectServer();
            }
        }
    }

    // ==================== Event System ====================

    /**
     * @brief Register event callback for all backends
     * @param callback Event callback
     */
    void registerGlobalEventCallback(BackendEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        globalCallback_ = callback;

        for (auto& [name, backend] : backends_) {
            if (backend) {
                backend->registerEventCallback(callback);
            }
        }
    }

    /**
     * @brief Unregister global event callback
     */
    void unregisterGlobalEventCallback() {
        std::lock_guard<std::mutex> lock(mutex_);
        globalCallback_ = nullptr;

        for (auto& [name, backend] : backends_) {
            if (backend) {
                backend->unregisterEventCallback();
            }
        }
    }

    // ==================== Initialization ====================

    /**
     * @brief Initialize default backends (INDI, ASCOM)
     */
    void initializeDefaultBackends() {
        registerFactory(std::make_shared<INDIBackendFactory>());
        registerFactory(std::make_shared<ASCOMBackendFactory>());
    }

    /**
     * @brief Clear all backends
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        disconnectAllBackends();
        backends_.clear();
    }

private:
    BackendRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<DeviceBackend>> backends_;
    std::unordered_map<std::string, std::shared_ptr<DeviceBackendFactory>> factories_;
    BackendEventCallback globalCallback_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_BACKEND_REGISTRY_HPP
