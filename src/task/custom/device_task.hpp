#ifndef LITHIUM_DEVICE_TASK_HPP
#define LITHIUM_DEVICE_TASK_HPP

#include <chrono>
#include <shared_mutex>
#include <string>
#include "device/manager.hpp"
#include "task.hpp"

namespace lithium::sequencer {

// 设备操作类型枚举
enum class DeviceOperation { Connect, Scan, Initialize, Configure, Test };

// 设备优先级配置
struct DevicePriority {
    int level;
    bool preempt;
    int timeout;
};

// 设备状态
struct DeviceStatus {
    bool isConnected;
    bool isInitialized;
    float health;
    std::string state;
    std::chrono::system_clock::time_point lastOperation;
    std::vector<std::string> errors;
};

class DeviceTask : public Task {
public:
    DeviceTask(const std::string& name, DeviceManager& manager);

    // 核心功能
    void execute(const json& params) override;

    // 设备操作方法
    bool connectDevice(const std::string& name, int timeout = 5000);
    std::vector<std::string> scanDevices(const std::string& type);
    bool initializeDevice(const std::string& name);

    // 配置方法
    void setPriority(const std::string& name, DevicePriority priority);
    void setConcurrencyLimit(int limit);
    void setRetryStrategy(const std::string& name, RetryStrategy strategy);

    // 监控方法
    DeviceStatus getDeviceStatus(const std::string& name) const;
    std::vector<std::string> getConnectedDevices() const;
    std::vector<std::string> getErrorLogs(const std::string& name) const;

    // 控制方法
    void disconnectDevice(const std::string& name);
    void resetDevice(const std::string& name);
    void abortOperation(const std::string& name);

private:
    DeviceManager* deviceManager_;

    std::unordered_map<std::string, DevicePriority> priorities_;
    std::unordered_map<std::string, DeviceStatus> deviceStatuses_;
    int concurrencyLimit_{4};
    std::shared_mutex statusMutex_;
    std::atomic<bool> shouldStop_{false};

    // 内部方法
    void setupDefaults();
    void validateParameters(const json& params);
    void handleDeviceError(const std::string& deviceName,
                           const std::string& error);
    void monitorDevice(const std::string& deviceName);
    void updateDeviceStatus(const std::string& name,
                            const DeviceStatus& status);

    // 辅助方法
    bool checkDeviceHealth(const std::string& name);
    void cleanupDevice(const std::string& name);
    std::string validateDeviceOperation(DeviceOperation op,
                                        const std::string& name);
};

}  // namespace lithium::sequencer

#endif  // LITHIUM_DEVICE_TASK_HPP
