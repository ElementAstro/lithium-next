/*
 * device_exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Unified device exception hierarchy

**************************************************/

#ifndef LITHIUM_DEVICE_COMMON_DEVICE_EXCEPTIONS_HPP
#define LITHIUM_DEVICE_COMMON_DEVICE_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

#include "device_error.hpp"

namespace lithium::device {

/**
 * @brief Base exception class for all device-related exceptions
 */
class DeviceException : public std::runtime_error {
public:
    explicit DeviceException(const std::string& message,
                             DeviceErrorCode code = DeviceErrorCode::Unknown)
        : std::runtime_error(message), error_(code, message) {}

    explicit DeviceException(const DeviceError& error)
        : std::runtime_error(error.toString()), error_(error) {}

    DeviceException(const std::string& message, const std::string& deviceName,
                    DeviceErrorCode code = DeviceErrorCode::Unknown)
        : std::runtime_error(message), error_(code, message, deviceName) {}

    [[nodiscard]] auto error() const noexcept -> const DeviceError& {
        return error_;
    }

    [[nodiscard]] auto code() const noexcept -> DeviceErrorCode {
        return error_.code;
    }

    [[nodiscard]] auto deviceName() const noexcept
        -> std::optional<std::string> {
        return error_.deviceName;
    }

protected:
    DeviceError error_;
};

/**
 * @brief Exception for device connection failures
 */
class DeviceConnectionException : public DeviceException {
public:
    explicit DeviceConnectionException(
        const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::ConnectionFailed)
        : DeviceException(message, code) {}

    DeviceConnectionException(
        const std::string& message, const std::string& deviceName,
        DeviceErrorCode code = DeviceErrorCode::ConnectionFailed)
        : DeviceException(message, deviceName, code) {}
};

/**
 * @brief Exception for connection timeout
 */
class DeviceConnectionTimeoutException : public DeviceConnectionException {
public:
    explicit DeviceConnectionTimeoutException(const std::string& deviceName,
                                              int timeoutMs = 0)
        : DeviceConnectionException(
              "Connection timeout" +
                  (timeoutMs > 0
                       ? " after " + std::to_string(timeoutMs) + "ms"
                       : ""),
              deviceName, DeviceErrorCode::ConnectionTimeout),
          timeoutMs_(timeoutMs) {}

    [[nodiscard]] auto timeoutMs() const noexcept -> int { return timeoutMs_; }

private:
    int timeoutMs_;
};

/**
 * @brief Exception for device operation failures
 */
class DeviceOperationException : public DeviceException {
public:
    explicit DeviceOperationException(
        const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::OperationFailed)
        : DeviceException(message, code) {}

    DeviceOperationException(
        const std::string& message, const std::string& deviceName,
        DeviceErrorCode code = DeviceErrorCode::OperationFailed)
        : DeviceException(message, deviceName, code) {}

    DeviceOperationException(const std::string& message,
                             const std::string& deviceName,
                             const std::string& operationName,
                             DeviceErrorCode code = DeviceErrorCode::OperationFailed)
        : DeviceException(message, deviceName, code),
          operationName_(operationName) {
        error_.operationName = operationName;
    }

    [[nodiscard]] auto operationName() const noexcept
        -> std::optional<std::string> {
        return operationName_;
    }

private:
    std::optional<std::string> operationName_;
};

/**
 * @brief Exception for operation timeout
 */
class DeviceTimeoutException : public DeviceOperationException {
public:
    explicit DeviceTimeoutException(const std::string& message,
                                    int timeoutMs = 0)
        : DeviceOperationException(
              message +
                  (timeoutMs > 0
                       ? " (timeout: " + std::to_string(timeoutMs) + "ms)"
                       : ""),
              DeviceErrorCode::OperationTimeout),
          timeoutMs_(timeoutMs) {}

    DeviceTimeoutException(const std::string& deviceName,
                           const std::string& operationName, int timeoutMs = 0)
        : DeviceOperationException(
              "Operation '" + operationName + "' timeout" +
                  (timeoutMs > 0
                       ? " after " + std::to_string(timeoutMs) + "ms"
                       : ""),
              deviceName, operationName, DeviceErrorCode::OperationTimeout),
          timeoutMs_(timeoutMs) {}

    [[nodiscard]] auto timeoutMs() const noexcept -> int { return timeoutMs_; }

private:
    int timeoutMs_;
};

/**
 * @brief Exception for device busy state
 */
class DeviceBusyException : public DeviceOperationException {
public:
    explicit DeviceBusyException(const std::string& deviceName)
        : DeviceOperationException("Device is busy", deviceName,
                                   DeviceErrorCode::DeviceBusy) {}

    DeviceBusyException(const std::string& deviceName,
                        const std::string& currentOperation)
        : DeviceOperationException(
              "Device is busy with operation: " + currentOperation, deviceName,
              DeviceErrorCode::DeviceBusy),
          currentOperation_(currentOperation) {}

    [[nodiscard]] auto currentOperation() const noexcept
        -> std::optional<std::string> {
        return currentOperation_;
    }

private:
    std::optional<std::string> currentOperation_;
};

/**
 * @brief Exception for device not found
 */
class DeviceNotFoundException : public DeviceException {
public:
    explicit DeviceNotFoundException(const std::string& deviceName)
        : DeviceException("Device not found: " + deviceName, deviceName,
                          DeviceErrorCode::NotFound) {}
};

/**
 * @brief Exception for device not connected
 */
class DeviceNotConnectedException : public DeviceException {
public:
    explicit DeviceNotConnectedException(const std::string& deviceName)
        : DeviceException("Device not connected: " + deviceName, deviceName,
                          DeviceErrorCode::NotConnected) {}
};

/**
 * @brief Exception for invalid device state
 */
class DeviceInvalidStateException : public DeviceException {
public:
    explicit DeviceInvalidStateException(const std::string& message)
        : DeviceException(message, DeviceErrorCode::InvalidState) {}

    DeviceInvalidStateException(const std::string& deviceName,
                                const std::string& expectedState,
                                const std::string& actualState)
        : DeviceException("Invalid device state: expected " + expectedState +
                              ", got " + actualState,
                          deviceName, DeviceErrorCode::InvalidState),
          expectedState_(expectedState),
          actualState_(actualState) {}

    [[nodiscard]] auto expectedState() const noexcept
        -> std::optional<std::string> {
        return expectedState_;
    }

    [[nodiscard]] auto actualState() const noexcept
        -> std::optional<std::string> {
        return actualState_;
    }

private:
    std::optional<std::string> expectedState_;
    std::optional<std::string> actualState_;
};

/**
 * @brief Exception for plugin-related errors
 */
class DevicePluginException : public DeviceException {
public:
    explicit DevicePluginException(
        const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::PluginLoadFailed)
        : DeviceException(message, code) {}

    DevicePluginException(const std::string& pluginName,
                          const std::string& message,
                          DeviceErrorCode code = DeviceErrorCode::PluginLoadFailed)
        : DeviceException(message, code), pluginName_(pluginName) {}

    [[nodiscard]] auto pluginName() const noexcept
        -> std::optional<std::string> {
        return pluginName_;
    }

private:
    std::optional<std::string> pluginName_;
};

/**
 * @brief Exception for plugin not found
 */
class PluginNotFoundException : public DevicePluginException {
public:
    explicit PluginNotFoundException(const std::string& pluginName)
        : DevicePluginException(pluginName, "Plugin not found: " + pluginName,
                                DeviceErrorCode::PluginNotFound) {}
};

/**
 * @brief Exception for plugin load failure
 */
class PluginLoadException : public DevicePluginException {
public:
    explicit PluginLoadException(const std::string& pluginName,
                                 const std::string& reason = "")
        : DevicePluginException(
              pluginName,
              "Failed to load plugin: " + pluginName +
                  (reason.empty() ? "" : " - " + reason),
              DeviceErrorCode::PluginLoadFailed) {}
};

/**
 * @brief Exception for plugin interface mismatch
 */
class PluginInterfaceException : public DevicePluginException {
public:
    explicit PluginInterfaceException(const std::string& pluginName,
                                      const std::string& expectedInterface = "")
        : DevicePluginException(pluginName,
                                "Plugin interface mismatch" +
                                    (expectedInterface.empty()
                                         ? ""
                                         : ": expected " + expectedInterface),
                                DeviceErrorCode::PluginInvalidInterface) {}
};

/**
 * @brief Exception for backend errors
 */
class BackendException : public DeviceException {
public:
    explicit BackendException(
        const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::BackendError)
        : DeviceException(message, code) {}

    BackendException(const std::string& backendName, const std::string& message,
                     DeviceErrorCode code = DeviceErrorCode::BackendError)
        : DeviceException("[" + backendName + "] " + message, code),
          backendName_(backendName) {}

    [[nodiscard]] auto backendName() const noexcept
        -> std::optional<std::string> {
        return backendName_;
    }

private:
    std::optional<std::string> backendName_;
};

/**
 * @brief Exception for backend not connected
 */
class BackendNotConnectedException : public BackendException {
public:
    explicit BackendNotConnectedException(const std::string& backendName)
        : BackendException(backendName, "Backend not connected",
                           DeviceErrorCode::BackendNotConnected) {}
};

/**
 * @brief Exception for communication errors
 */
class DeviceCommunicationException : public DeviceException {
public:
    explicit DeviceCommunicationException(
        const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::CommunicationError)
        : DeviceException(message, code) {}

    DeviceCommunicationException(
        const std::string& deviceName, const std::string& message,
        DeviceErrorCode code = DeviceErrorCode::CommunicationError)
        : DeviceException(message, deviceName, code) {}
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_COMMON_DEVICE_EXCEPTIONS_HPP
