#ifndef LITHIUM_TASK_CAMERA_DEVICE_COORDINATION_TASKS_HPP
#define LITHIUM_TASK_CAMERA_DEVICE_COORDINATION_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Multi-device scanning and connection task.
 * Scans for and connects to all available devices.
 */
class DeviceScanConnectTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateScanParameters(const json& params);
    static void handleConnectionError(Task& task, const std::exception& e);
};

/**
 * @brief Device health monitoring task.
 * Monitors health status of all connected devices.
 */
class DeviceHealthMonitorTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateHealthParameters(const json& params);
};

/**
 * @brief Automated filter sequence task.
 * Manages filter wheel and exposures for multi-filter imaging.
 */
class AutoFilterSequenceTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFilterSequenceParameters(const json& params);
};

/**
 * @brief Focus-filter optimization task.
 * Measures and stores focus offsets for different filters.
 */
class FocusFilterOptimizationTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusFilterParameters(const json& params);
};

/**
 * @brief Intelligent auto-focus task.
 * Advanced autofocus with temperature compensation and filter offsets.
 */
class IntelligentAutoFocusTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateIntelligentFocusParameters(const json& params);
};

/**
 * @brief Coordinated shutdown task.
 * Safely shuts down all devices in proper sequence.
 */
class CoordinatedShutdownTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

/**
 * @brief Environment monitoring task.
 * Monitors environmental conditions and adjusts device settings.
 */
class EnvironmentMonitorTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateEnvironmentParameters(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_DEVICE_COORDINATION_TASKS_HPP
