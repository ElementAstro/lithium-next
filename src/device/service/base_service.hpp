/*
 * base_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Base device service providing common functionality

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_BASE_HPP
#define LITHIUM_DEVICE_SERVICE_BASE_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"
#include "device/manager.hpp"

namespace lithium::device {

using json = nlohmann::json;

/**
 * @brief Error codes for device service operations
 */
struct ErrorCode {
    static constexpr const char* INTERNAL_ERROR = "internal_error";
    static constexpr const char* DEVICE_NOT_FOUND = "device_not_found";
    static constexpr const char* DEVICE_NOT_CONNECTED = "device_not_connected";
    static constexpr const char* DEVICE_BUSY = "device_busy";
    static constexpr const char* CONNECTION_FAILED = "connection_failed";
    static constexpr const char* INVALID_FIELD_VALUE = "invalid_field_value";
    static constexpr const char* FEATURE_NOT_SUPPORTED = "feature_not_supported";
    static constexpr const char* OPERATION_FAILED = "operation_failed";
    static constexpr const char* INVALID_COORDINATES = "invalid_coordinates";
    static constexpr const char* TIMEOUT = "timeout";
};

/**
 * @brief Base class for all device services providing common functionality
 *
 * This class provides:
 * - Unified JSON response construction
 * - Error handling utilities
 * - Device manager integration
 * - Message bus access for event publishing
 * - Logging helpers
 */
class BaseDeviceService {
public:
    explicit BaseDeviceService(std::string serviceName)
        : serviceName_(std::move(serviceName)) {
        initializeCommonDependencies();
    }

    virtual ~BaseDeviceService() = default;

    // Disable copy
    BaseDeviceService(const BaseDeviceService&) = delete;
    BaseDeviceService& operator=(const BaseDeviceService&) = delete;

    // Enable move
    BaseDeviceService(BaseDeviceService&&) = default;
    BaseDeviceService& operator=(BaseDeviceService&&) = default;

protected:
    /**
     * @brief Create a success response
     */
    static auto makeSuccessResponse() -> json {
        json response;
        response["status"] = "success";
        return response;
    }

    /**
     * @brief Create a success response with data
     */
    static auto makeSuccessResponse(const json& data) -> json {
        json response;
        response["status"] = "success";
        response["data"] = data;
        return response;
    }

    /**
     * @brief Create a success response with message
     */
    static auto makeSuccessResponse(const std::string& message) -> json {
        json response;
        response["status"] = "success";
        response["message"] = message;
        return response;
    }

    /**
     * @brief Create a success response with data and message
     */
    static auto makeSuccessResponse(const json& data,
                                    const std::string& message) -> json {
        json response;
        response["status"] = "success";
        response["data"] = data;
        response["message"] = message;
        return response;
    }

    /**
     * @brief Create an error response
     */
    static auto makeErrorResponse(const std::string& code,
                                  const std::string& message) -> json {
        json response;
        response["status"] = "error";
        response["error"] = {{"code", code}, {"message", message}};
        return response;
    }

    /**
     * @brief Create an error response from exception
     */
    static auto makeErrorResponse(const std::exception& e) -> json {
        return makeErrorResponse(ErrorCode::INTERNAL_ERROR, e.what());
    }

    /**
     * @brief Check if device is connected and return error response if not
     * @return Empty optional if connected, error response if not
     */
    template <typename DeviceT>
    auto checkDeviceConnected(const std::shared_ptr<DeviceT>& device,
                              const std::string& deviceType)
        -> std::optional<json> {
        if (!device) {
            return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND,
                                     deviceType + " not found");
        }
        if (!device->isConnected()) {
            return makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED,
                                     deviceType + " is not connected");
        }
        return std::nullopt;
    }

    /**
     * @brief Execute an operation with standard error handling
     */
    template <typename Func>
    auto executeWithErrorHandling(const std::string& operationName,
                                  Func&& operation) -> json {
        logOperationStart(operationName);
        try {
            auto result = std::forward<Func>(operation)();
            logOperationEnd(operationName);
            return result;
        } catch (const std::exception& e) {
            logOperationError(operationName, e.what());
            return makeErrorResponse(e);
        }
    }

    /**
     * @brief Get device from DeviceManager by type
     */
    template <typename DeviceT>
    auto getDevice(const std::string& deviceType)
        -> std::shared_ptr<DeviceT> {
        if (deviceManager_) {
            auto device = deviceManager_->getPrimaryDevice(deviceType);
            return std::dynamic_pointer_cast<DeviceT>(device);
        }
        return nullptr;
    }

    /**
     * @brief Get device by name from DeviceManager
     */
    template <typename DeviceT>
    auto getDeviceByName(const std::string& name) -> std::shared_ptr<DeviceT> {
        if (deviceManager_) {
            auto device = deviceManager_->getDeviceByName(name);
            return std::dynamic_pointer_cast<DeviceT>(device);
        }
        return nullptr;
    }

    /**
     * @brief Publish event to message bus
     */
    void publishEvent(const std::string& topic, const std::string& message) {
        if (messageBus_) {
            messageBus_->publish(topic, message);
        }
    }

    /**
     * @brief Publish device state change event
     */
    void publishDeviceStateChange(const std::string& deviceType,
                                  const std::string& deviceId,
                                  const std::string& state) {
        json event;
        event["deviceType"] = deviceType;
        event["deviceId"] = deviceId;
        event["state"] = state;
        event["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
        publishEvent("device.state", event.dump());
    }

    /**
     * @brief Log operation start
     */
    void logOperationStart(const std::string& operation) const {
        LOG_INFO( "%s::%s: Starting", serviceName_.c_str(), operation.c_str());
    }

    /**
     * @brief Log operation end
     */
    void logOperationEnd(const std::string& operation) const {
        LOG_INFO( "%s::%s: Completed", serviceName_.c_str(), operation.c_str());
    }

    /**
     * @brief Log operation error
     */
    void logOperationError(const std::string& operation,
                           const std::string& error) const {
        LOG_ERROR( "%s::%s: Error: %s", serviceName_.c_str(),
              operation.c_str(), error.c_str());
    }

    /**
     * @brief Log operation warning
     */
    void logOperationWarning(const std::string& operation,
                             const std::string& warning) const {
        LOG_WARN( "%s::%s: %s", serviceName_.c_str(), operation.c_str(),
              warning.c_str());
    }

    /**
     * @brief Get the service name
     */
    [[nodiscard]] auto getServiceName() const -> const std::string& {
        return serviceName_;
    }

    /**
     * @brief Get the device manager
     */
    [[nodiscard]] auto getDeviceManager() const
        -> std::shared_ptr<DeviceManager> {
        return deviceManager_;
    }

    /**
     * @brief Get the message bus
     */
    [[nodiscard]] auto getMessageBus() const
        -> std::shared_ptr<atom::async::MessageBus> {
        return messageBus_;
    }

private:
    void initializeCommonDependencies() {
        try {
            GET_OR_CREATE_PTR(deviceManager_, DeviceManager,
                              Constants::DEVICE_MANAGER)
        } catch (...) {
            LOG_WARN( "%s: DeviceManager not available",
                  serviceName_.c_str());
        }

        try {
            GET_OR_CREATE_PTR(messageBus_, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
        } catch (...) {
            LOG_WARN( "%s: MessageBus not available", serviceName_.c_str());
        }
    }

    std::string serviceName_;
    std::shared_ptr<DeviceManager> deviceManager_;
    std::shared_ptr<atom::async::MessageBus> messageBus_;
};

/**
 * @brief CRTP base for typed device services
 */
template <typename Derived, typename DeviceT>
class TypedDeviceService : public BaseDeviceService {
public:
    using DeviceType = DeviceT;
    using DevicePtr = std::shared_ptr<DeviceT>;

    explicit TypedDeviceService(std::string serviceName,
                                std::string deviceTypeName,
                                const char* primaryDeviceConstant)
        : BaseDeviceService(std::move(serviceName)),
          deviceTypeName_(std::move(deviceTypeName)),
          primaryDeviceConstant_(primaryDeviceConstant) {}

protected:
    /**
     * @brief Get the primary device of this type
     */
    auto getPrimaryDevice() -> DevicePtr {
        try {
            DevicePtr device;
            GET_OR_CREATE_PTR(device, DeviceT, primaryDeviceConstant_)
            return device;
        } catch (...) {
            logOperationWarning("getPrimaryDevice",
                                deviceTypeName_ + " not available");
            return nullptr;
        }
    }

    /**
     * @brief Execute operation requiring connected device
     */
    template <typename Func>
    auto withConnectedDevice(const std::string& deviceId,
                             const std::string& operationName,
                             Func&& operation) -> json {
        return executeWithErrorHandling(operationName, [&]() -> json {
            auto device = getPrimaryDevice();
            if (auto error = checkDeviceConnected(device, deviceTypeName_)) {
                return *error;
            }
            return std::forward<Func>(operation)(device);
        });
    }

    /**
     * @brief Execute operation that may work with disconnected device
     */
    template <typename Func>
    auto withDevice(const std::string& deviceId,
                    const std::string& operationName, Func&& operation) -> json {
        return executeWithErrorHandling(operationName, [&]() -> json {
            auto device = getPrimaryDevice();
            if (!device) {
                return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND,
                                         deviceTypeName_ + " not found");
            }
            return std::forward<Func>(operation)(device);
        });
    }

    [[nodiscard]] auto getDeviceTypeName() const -> const std::string& {
        return deviceTypeName_;
    }

private:
    std::string deviceTypeName_;
    const char* primaryDeviceConstant_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_BASE_HPP
