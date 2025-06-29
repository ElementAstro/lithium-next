#ifndef LITHIUM_TASK_GUIDE_DEVICE_CONFIG_TASKS_HPP
#define LITHIUM_TASK_GUIDE_DEVICE_CONFIG_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Get device configuration task.
 * Gets configuration for connected devices.
 */
class GetDeviceConfigTask : public Task {
public:
    GetDeviceConfigTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getDeviceConfig(const json& params);
};

/**
 * @brief Set device configuration task.
 * Sets configuration for connected devices.
 */
class SetDeviceConfigTask : public Task {
public:
    SetDeviceConfigTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setDeviceConfig(const json& params);
};

/**
 * @brief Get mount position task.
 * Gets current mount position information.
 */
class GetMountPositionTask : public Task {
public:
    GetMountPositionTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getMountPosition(const json& params);
};

/**
 * @brief Comprehensive PHD2 health check task.
 * Performs a comprehensive health check of the PHD2 system.
 */
class PHD2HealthCheckTask : public Task {
public:
    PHD2HealthCheckTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performHealthCheck(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_DEVICE_CONFIG_TASKS_HPP
