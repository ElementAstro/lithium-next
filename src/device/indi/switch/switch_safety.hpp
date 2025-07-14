/*
 * switch_safety.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Safety - Safety Management Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_SAFETY_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_SAFETY_HPP

#include <atomic>
#include <functional>
#include <mutex>

// Forward declarations
class INDISwitchClient;

/**
 * @class SwitchSafety
 * @brief Switch safety management component for INDI devices.
 *
 * This class provides safety management features for INDI switch devices,
 * including emergency stop, safety mode, and safety checks. It allows
 * registration of emergency callbacks and ensures thread-safe operations using
 * mutexes and atomics.
 */
class SwitchSafety {
public:
    /**
     * @brief Construct a SwitchSafety manager for a given INDISwitchClient.
     * @param client Pointer to the associated INDISwitchClient.
     */
    explicit SwitchSafety(INDISwitchClient* client);
    /**
     * @brief Destructor (defaulted).
     */
    ~SwitchSafety() = default;

    /**
     * @brief Enable or disable safety mode.
     *
     * When enabled, additional safety checks are performed before operations.
     * @param enable True to enable safety mode, false to disable.
     * @return True if the operation succeeded.
     */
    [[nodiscard]] auto enableSafetyMode(bool enable) noexcept -> bool;

    /**
     * @brief Check if safety mode is currently enabled.
     * @return True if safety mode is enabled, false otherwise.
     */
    [[nodiscard]] auto isSafetyModeEnabled() const noexcept -> bool;

    /**
     * @brief Activate the emergency stop.
     *
     * Immediately halts all operations and triggers safety shutdown.
     * @return True if the emergency stop was set.
     */
    [[nodiscard]] auto setEmergencyStop() noexcept -> bool;

    /**
     * @brief Clear the emergency stop state.
     *
     * Allows operations to resume if all other safety conditions are met.
     * @return True if the emergency stop was cleared.
     */
    [[nodiscard]] auto clearEmergencyStop() noexcept -> bool;

    /**
     * @brief Check if the emergency stop is currently active.
     * @return True if emergency stop is active, false otherwise.
     */
    [[nodiscard]] auto isEmergencyStopActive() const noexcept -> bool;

    /**
     * @brief Check if it is currently safe to operate the device.
     *
     * Considers emergency stop, safety mode, and power limits.
     * @return True if it is safe to operate, false otherwise.
     */
    [[nodiscard]] auto isSafeToOperate() const noexcept -> bool;

    /**
     * @brief Perform all configured safety checks.
     *
     * This method should be called to verify all safety conditions, such as
     * power limits.
     */
    void performSafetyChecks() noexcept;

    /**
     * @brief Emergency callback type.
     *
     * The callback receives a boolean indicating if emergency is active.
     */
    using EmergencyCallback = std::function<void(bool emergencyActive)>;

    /**
     * @brief Register an emergency callback.
     *
     * The callback will be invoked when the emergency stop state changes.
     * @param callback The callback function to register (rvalue reference).
     */
    void setEmergencyCallback(EmergencyCallback&& callback) noexcept;

private:
    /**
     * @brief Pointer to the associated INDISwitchClient.
     */
    INDISwitchClient* client_;
    /**
     * @brief Mutex for thread-safe safety state access.
     */
    mutable std::mutex safety_mutex_;

    /**
     * @brief Indicates if safety mode is enabled.
     */
    std::atomic<bool> safety_mode_enabled_{false};
    /**
     * @brief Indicates if emergency stop is active.
     */
    std::atomic<bool> emergency_stop_active_{false};
    /**
     * @brief Registered emergency callback function.
     */
    EmergencyCallback emergency_callback_{};

    /**
     * @brief Notify the registered callback of an emergency event.
     * @param active True if emergency is active, false otherwise.
     */
    void notifyEmergencyEvent(bool active);
    /**
     * @brief Execute safety shutdown procedures (turn off switches, cancel
     * timers, etc).
     */
    void executeSafetyShutdown();
};

#endif  // LITHIUM_DEVICE_INDI_SWITCH_SAFETY_HPP
