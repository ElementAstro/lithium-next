/*
 * device_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Device service registry for centralized service management

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_REGISTRY_HPP
#define LITHIUM_DEVICE_SERVICE_REGISTRY_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

namespace lithium::device {

using json = nlohmann::json;

/**
 * @brief Device type enumeration for service registration
 */
enum class DeviceType {
    CAMERA,
    MOUNT,
    FOCUSER,
    FILTERWHEEL,
    DOME,
    ROTATOR,
    GUIDER,
    WEATHER,
    SWITCH,
    UNKNOWN
};

/**
 * @brief Convert DeviceType to string
 */
inline auto deviceTypeToString(DeviceType type) -> std::string {
    switch (type) {
        case DeviceType::CAMERA:
            return "camera";
        case DeviceType::MOUNT:
            return "mount";
        case DeviceType::FOCUSER:
            return "focuser";
        case DeviceType::FILTERWHEEL:
            return "filterwheel";
        case DeviceType::DOME:
            return "dome";
        case DeviceType::ROTATOR:
            return "rotator";
        case DeviceType::GUIDER:
            return "guider";
        case DeviceType::WEATHER:
            return "weather";
        case DeviceType::SWITCH:
            return "switch";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert string to DeviceType
 */
inline auto stringToDeviceType(const std::string& str) -> DeviceType {
    static const std::unordered_map<std::string, DeviceType> typeMap = {
        {"camera", DeviceType::CAMERA},
        {"mount", DeviceType::MOUNT},
        {"telescope", DeviceType::MOUNT},
        {"focuser", DeviceType::FOCUSER},
        {"filterwheel", DeviceType::FILTERWHEEL},
        {"dome", DeviceType::DOME},
        {"rotator", DeviceType::ROTATOR},
        {"guider", DeviceType::GUIDER},
        {"weather", DeviceType::WEATHER},
        {"switch", DeviceType::SWITCH},
    };

    auto it = typeMap.find(str);
    return it != typeMap.end() ? it->second : DeviceType::UNKNOWN;
}

/**
 * @brief Service registration info
 */
struct ServiceInfo {
    std::string name;
    DeviceType type;
    std::string version;
    bool isInitialized = false;
    std::function<json()> healthCheck;
};

/**
 * @brief Centralized registry for device services
 *
 * Provides:
 * - Service registration and discovery
 * - Health monitoring
 * - Cross-service communication
 * - Lifecycle management
 */
class DeviceServiceRegistry {
public:
    static auto getInstance() -> DeviceServiceRegistry& {
        static DeviceServiceRegistry instance;
        return instance;
    }

    /**
     * @brief Register a device service
     */
    template <typename ServiceT>
    void registerService(DeviceType type, std::shared_ptr<ServiceT> service,
                         const std::string& version = "1.0.0") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto typeIndex = std::type_index(typeid(ServiceT));
        services_[typeIndex] = service;

        ServiceInfo info;
        info.name = deviceTypeToString(type) + "_service";
        info.type = type;
        info.version = version;
        info.isInitialized = true;

        serviceInfos_[type] = info;
        typeToIndex_[type] = typeIndex;

        LOG_INFO("DeviceServiceRegistry: Registered %s service v%s",
                 info.name.c_str(), version.c_str());
    }

    /**
     * @brief Get a registered service by type
     */
    template <typename ServiceT>
    auto getService() -> std::shared_ptr<ServiceT> {
        std::lock_guard<std::mutex> lock(mutex_);

        auto typeIndex = std::type_index(typeid(ServiceT));
        auto it = services_.find(typeIndex);
        if (it != services_.end()) {
            return std::static_pointer_cast<ServiceT>(it->second);
        }
        return nullptr;
    }

    /**
     * @brief Get service by device type
     */
    template <typename ServiceT>
    auto getServiceByType(DeviceType type) -> std::shared_ptr<ServiceT> {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = typeToIndex_.find(type);
        if (it != typeToIndex_.end()) {
            auto serviceIt = services_.find(it->second);
            if (serviceIt != services_.end()) {
                return std::static_pointer_cast<ServiceT>(serviceIt->second);
            }
        }
        return nullptr;
    }

    /**
     * @brief Check if a service is registered
     */
    auto hasService(DeviceType type) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return serviceInfos_.find(type) != serviceInfos_.end();
    }

    /**
     * @brief Get service info
     */
    auto getServiceInfo(DeviceType type) const -> std::optional<ServiceInfo> {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = serviceInfos_.find(type);
        if (it != serviceInfos_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Get all registered service types
     */
    auto getRegisteredTypes() const -> std::vector<DeviceType> {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DeviceType> types;
        types.reserve(serviceInfos_.size());
        for (const auto& [type, info] : serviceInfos_) {
            types.push_back(type);
        }
        return types;
    }

    /**
     * @brief Get registry status as JSON
     */
    auto getStatus() const -> json {
        std::lock_guard<std::mutex> lock(mutex_);

        json status;
        status["serviceCount"] = serviceInfos_.size();

        json services = json::array();
        for (const auto& [type, info] : serviceInfos_) {
            json serviceJson;
            serviceJson["name"] = info.name;
            serviceJson["type"] = deviceTypeToString(type);
            serviceJson["version"] = info.version;
            serviceJson["initialized"] = info.isInitialized;
            services.push_back(serviceJson);
        }
        status["services"] = services;

        return status;
    }

    /**
     * @brief Unregister a service
     */
    void unregisterService(DeviceType type) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = typeToIndex_.find(type);
        if (it != typeToIndex_.end()) {
            services_.erase(it->second);
            typeToIndex_.erase(it);
        }
        serviceInfos_.erase(type);

        LOG_INFO("DeviceServiceRegistry: Unregistered %s service",
                 deviceTypeToString(type).c_str());
    }

    /**
     * @brief Clear all registered services
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
        serviceInfos_.clear();
        typeToIndex_.clear();
        LOG_INFO("DeviceServiceRegistry: Cleared all services");
    }

private:
    DeviceServiceRegistry() = default;
    ~DeviceServiceRegistry() = default;

    DeviceServiceRegistry(const DeviceServiceRegistry&) = delete;
    DeviceServiceRegistry& operator=(const DeviceServiceRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
    std::unordered_map<DeviceType, ServiceInfo> serviceInfos_;
    std::unordered_map<DeviceType, std::type_index> typeToIndex_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_REGISTRY_HPP
