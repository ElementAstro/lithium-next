#ifndef LITHIUM_TASK_COMPONENTS_DEVICE_TASK_HPP
#define LITHIUM_TASK_COMPONENTS_DEVICE_TASK_HPP

#include <chrono>
#include <shared_mutex>
#include <string>
#include "../core/task.hpp"
#include "device/manager.hpp"

namespace lithium::task {

/**
 * @enum DeviceOperation
 * @brief Enumerates the types of device operations supported by DeviceTask.
 */
enum class DeviceOperation {
    Connect,     ///< Connect to a device.
    Scan,        ///< Scan for available devices.
    Initialize,  ///< Initialize a device.
    Configure,   ///< Configure a device.
    Test         ///< Test a device.
};

/**
 * @struct DevicePriority
 * @brief Represents the priority configuration for a device.
 */
struct DevicePriority {
    int level;     ///< Priority level.
    bool preempt;  ///< Whether the device can preempt others.
    int timeout;   ///< Timeout for device operations in milliseconds.
};

/**
 * @struct DeviceStatus
 * @brief Represents the status and health information of a device.
 */
struct DeviceStatus {
    bool isConnected;    ///< Whether the device is currently connected.
    bool isInitialized;  ///< Whether the device has been initialized.
    float health;        ///< Health metric of the device (0.0 - 1.0).
    std::string state;   ///< Current state description.
    std::chrono::system_clock::time_point
        lastOperation;                ///< Timestamp of last operation.
    std::vector<std::string> errors;  ///< List of error messages.
};

/**
 * @class DeviceTask
 * @brief Task class for managing and operating devices.
 *
 * Provides methods for connecting, scanning, initializing, configuring,
 * testing, and monitoring devices. Supports priority, concurrency, and
 * retry strategies.
 */
class DeviceTask : public Task {
public:
    /**
     * @brief Construct a DeviceTask.
     * @param name The name of the task.
     * @param manager Reference to the DeviceManager.
     */
    DeviceTask(const std::string& name, DeviceManager& manager);

    /**
     * @brief Execute the device task with the given parameters.
     * @param params JSON object containing execution parameters.
     */
    void execute(const json& params) override;

    /**
     * @brief Connect to a device.
     * @param name The device name.
     * @param timeout Timeout in milliseconds (default: 5000).
     * @return True if connection is successful, false otherwise.
     */
    bool connectDevice(const std::string& name, int timeout = 5000);

    /**
     * @brief Scan for devices of a specific type.
     * @param type The device type to scan for.
     * @return Vector of found device names.
     */
    std::vector<std::string> scanDevices(const std::string& type);

    /**
     * @brief Initialize a device.
     * @param name The device name.
     * @return True if initialization is successful, false otherwise.
     */
    bool initializeDevice(const std::string& name);

    /**
     * @brief Set the priority configuration for a device.
     * @param name The device name.
     * @param priority The priority configuration.
     */
    void setPriority(const std::string& name, DevicePriority priority);

    /**
     * @brief Set the concurrency limit for device operations.
     * @param limit The maximum number of concurrent operations.
     */
    void setConcurrencyLimit(int limit);

    /**
     * @brief Set the retry strategy for a device.
     * @param name The device name.
     * @param strategy The retry strategy.
     */
    void setRetryStrategy(const std::string& name, RetryStrategy strategy);

    /**
     * @brief Get the status of a device.
     * @param name The device name.
     * @return DeviceStatus structure with current status.
     */
    DeviceStatus getDeviceStatus(const std::string& name) const;

    /**
     * @brief Get a list of currently connected devices.
     * @return Vector of connected device names.
     */
    std::vector<std::string> getConnectedDevices() const;

    /**
     * @brief Get error logs for a specific device.
     * @param name The device name.
     * @return Vector of error messages.
     */
    std::vector<std::string> getErrorLogs(const std::string& name) const;

    /**
     * @brief Disconnect a device.
     * @param name The device name.
     */
    void disconnectDevice(const std::string& name);

    /**
     * @brief Reset a device.
     * @param name The device name.
     */
    void resetDevice(const std::string& name);

    /**
     * @brief Abort the current operation on a device.
     * @param name The device name.
     */
    void abortOperation(const std::string& name);

private:
    DeviceManager* deviceManager_;  ///< Pointer to the device manager.

    std::unordered_map<std::string, DevicePriority>
        priorities_;  ///< Device priorities.
    std::unordered_map<std::string, DeviceStatus>
        deviceStatuses_;       ///< Device statuses.
    int concurrencyLimit_{4};  ///< Maximum number of concurrent operations.
    std::shared_mutex statusMutex_;  ///< Mutex for thread-safe status access.
    std::atomic<bool> shouldStop_{
        false};  ///< Flag to indicate if operations should stop.

    // Internal methods

    /**
     * @brief Set up default device configurations.
     */
    void setupDefaults();

    /**
     * @brief Validate the parameters for device operations.
     * @param params JSON object containing parameters.
     */
    void validateParameters(const json& params);

    /**
     * @brief Handle device errors and update status.
     * @param deviceName The device name.
     * @param error The error message.
     */
    void handleDeviceError(const std::string& deviceName,
                           const std::string& error);

    /**
     * @brief Monitor the status of a device.
     * @param deviceName The device name.
     */
    void monitorDevice(const std::string& deviceName);

    /**
     * @brief Update the status of a device.
     * @param name The device name.
     * @param status The new status.
     */
    void updateDeviceStatus(const std::string& name,
                            const DeviceStatus& status);

    // Helper methods

    /**
     * @brief Check the health of a device.
     * @param name The device name.
     * @return True if the device is healthy, false otherwise.
     */
    bool checkDeviceHealth(const std::string& name);

    /**
     * @brief Clean up resources for a device.
     * @param name The device name.
     */
    void cleanupDevice(const std::string& name);

    /**
     * @brief Validate a device operation.
     * @param op The device operation.
     * @param name The device name.
     * @return Validation result as a string.
     */
    std::string validateDeviceOperation(DeviceOperation op,
                                        const std::string& name);
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMPONENTS_DEVICE_TASK_HPP
