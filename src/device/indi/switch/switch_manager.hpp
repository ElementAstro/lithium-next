/*
 * switch_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Manager - Core Switch Control Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_MANAGER_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_MANAGER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "device/template/switch.hpp"

// Forward declarations
class INDISwitchClient;

/**
 * @class SwitchManager
 * @brief Core switch management component for INDI devices.
 *
 * This class provides comprehensive management for switch devices, including
 * basic switch operations, group management, and synchronization with INDI
 * properties. It is thread-safe and designed for integration with
 * astrophotography control systems.
 */
class SwitchManager {
public:
    /**
     * @brief Construct a new SwitchManager object.
     * @param client Pointer to the associated INDISwitchClient.
     */
    explicit SwitchManager(INDISwitchClient* client);

    /**
     * @brief Destroy the SwitchManager object.
     */
    ~SwitchManager() = default;

    // Basic switch operations

    /**
     * @brief Add a new switch to the manager.
     * @param switchInfo Information about the switch to add.
     * @return true if the switch was added successfully, false otherwise.
     */
    [[nodiscard]] auto addSwitch(const SwitchInfo& switchInfo) -> bool;

    /**
     * @brief Remove a switch by its index.
     * @param index Index of the switch to remove.
     * @return true if the switch was removed, false otherwise.
     */
    [[nodiscard]] auto removeSwitch(uint32_t index) -> bool;

    /**
     * @brief Remove a switch by its name.
     * @param name Name of the switch to remove.
     * @return true if the switch was removed, false otherwise.
     */
    [[nodiscard]] auto removeSwitch(const std::string& name) -> bool;

    /**
     * @brief Get the total number of switches managed.
     * @return Number of switches.
     */
    [[nodiscard]] auto getSwitchCount() const noexcept -> uint32_t;

    /**
     * @brief Get information about a switch by index.
     * @param index Index of the switch.
     * @return Optional SwitchInfo if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getSwitchInfo(uint32_t index) const
        -> std::optional<SwitchInfo>;

    /**
     * @brief Get information about a switch by name.
     * @param name Name of the switch.
     * @return Optional SwitchInfo if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getSwitchInfo(const std::string& name) const
        -> std::optional<SwitchInfo>;

    /**
     * @brief Get the index of a switch by name.
     * @param name Name of the switch.
     * @return Optional index if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getSwitchIndex(const std::string& name) const
        -> std::optional<uint32_t>;

    /**
     * @brief Get information about all switches.
     * @return Vector of SwitchInfo for all switches.
     */
    [[nodiscard]] auto getAllSwitches() const -> std::vector<SwitchInfo>;

    // Switch state management

    /**
     * @brief Set the state of a switch by index.
     * @param index Index of the switch.
     * @param state Desired switch state.
     * @return true if the state was set, false otherwise.
     */
    [[nodiscard]] auto setSwitchState(uint32_t index, SwitchState state)
        -> bool;

    /**
     * @brief Set the state of a switch by name.
     * @param name Name of the switch.
     * @param state Desired switch state.
     * @return true if the state was set, false otherwise.
     */
    [[nodiscard]] auto setSwitchState(const std::string& name,
                                      SwitchState state) -> bool;

    /**
     * @brief Get the state of a switch by index.
     * @param index Index of the switch.
     * @return Optional SwitchState if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getSwitchState(uint32_t index) const
        -> std::optional<SwitchState>;

    /**
     * @brief Get the state of a switch by name.
     * @param name Name of the switch.
     * @return Optional SwitchState if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getSwitchState(const std::string& name) const
        -> std::optional<SwitchState>;

    /**
     * @brief Set the state of all switches.
     * @param state Desired state for all switches.
     * @return true if all switches were set, false otherwise.
     */
    [[nodiscard]] auto setAllSwitches(SwitchState state) -> bool;

    /**
     * @brief Toggle the state of a switch by index.
     * @param index Index of the switch.
     * @return true if toggled, false otherwise.
     */
    [[nodiscard]] auto toggleSwitch(uint32_t index) -> bool;

    /**
     * @brief Toggle the state of a switch by name.
     * @param name Name of the switch.
     * @return true if toggled, false otherwise.
     */
    [[nodiscard]] auto toggleSwitch(const std::string& name) -> bool;

    // Group management

    /**
     * @brief Add a new group of switches.
     * @param group SwitchGroup object describing the group.
     * @return true if the group was added, false otherwise.
     */
    [[nodiscard]] auto addGroup(const SwitchGroup& group) -> bool;

    /**
     * @brief Remove a group by name.
     * @param name Name of the group.
     * @return true if the group was removed, false otherwise.
     */
    [[nodiscard]] auto removeGroup(const std::string& name) -> bool;

    /**
     * @brief Get the total number of groups.
     * @return Number of groups.
     */
    [[nodiscard]] auto getGroupCount() const noexcept -> uint32_t;

    /**
     * @brief Get information about a group by name.
     * @param name Name of the group.
     * @return Optional SwitchGroup if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto getGroupInfo(const std::string& name) const
        -> std::optional<SwitchGroup>;

    /**
     * @brief Get information about all groups.
     * @return Vector of SwitchGroup for all groups.
     */
    [[nodiscard]] auto getAllGroups() const -> std::vector<SwitchGroup>;

    /**
     * @brief Add a switch to a group.
     * @param groupName Name of the group.
     * @param switchIndex Index of the switch to add.
     * @return true if added, false otherwise.
     */
    [[nodiscard]] auto addSwitchToGroup(const std::string& groupName,
                                        uint32_t switchIndex) -> bool;

    /**
     * @brief Remove a switch from a group.
     * @param groupName Name of the group.
     * @param switchIndex Index of the switch to remove.
     * @return true if removed, false otherwise.
     */
    [[nodiscard]] auto removeSwitchFromGroup(const std::string& groupName,
                                             uint32_t switchIndex) -> bool;

    /**
     * @brief Set the state of a switch within a group.
     * @param groupName Name of the group.
     * @param switchIndex Index of the switch.
     * @param state Desired state.
     * @return true if set, false otherwise.
     */
    [[nodiscard]] auto setGroupState(const std::string& groupName,
                                     uint32_t switchIndex, SwitchState state)
        -> bool;

    /**
     * @brief Set all switches in a group to off.
     * @param groupName Name of the group.
     * @return true if all were set to off, false otherwise.
     */
    [[nodiscard]] auto setGroupAllOff(const std::string& groupName) -> bool;

    /**
     * @brief Get the states of all switches in a group.
     * @param groupName Name of the group.
     * @return Vector of pairs (switch index, state).
     */
    [[nodiscard]] auto getGroupStates(const std::string& groupName) const
        -> std::vector<std::pair<uint32_t, SwitchState>>;

    // INDI property handling

    /**
     * @brief Handle an incoming INDI switch property.
     * @param property The INDI property to handle.
     */
    void handleSwitchProperty(const INDI::Property& property);

    /**
     * @brief Synchronize the internal state with the device.
     */
    void synchronizeWithDevice();

    /**
     * @brief Find the INDI property associated with a switch.
     * @param switchName Name of the switch.
     * @return Pointer to the INDI::PropertyViewSwitch if found, nullptr
     * otherwise.
     */
    [[nodiscard]] auto findSwitchProperty(const std::string& switchName)
        -> INDI::PropertyViewSwitch*;

    /**
     * @brief Update a switch's state from an INDI property.
     * @param property Pointer to the INDI property.
     */
    void updateSwitchFromProperty(INDI::PropertyViewSwitch* property);

    // Utility methods

    /**
     * @brief Check if a switch index is valid.
     * @param index Index to check.
     * @return true if valid, false otherwise.
     */
    [[nodiscard]] auto isValidSwitchIndex(uint32_t index) const noexcept
        -> bool;

    /**
     * @brief Notify listeners of a switch state change.
     * @param index Index of the switch.
     * @param state New state of the switch.
     */
    void notifySwitchStateChange(uint32_t index, SwitchState state);

    /**
     * @brief Notify listeners of a group switch state change.
     * @param groupName Name of the group.
     * @param switchIndex Index of the switch.
     * @param state New state of the switch.
     */
    void notifyGroupStateChange(const std::string& groupName,
                                uint32_t switchIndex, SwitchState state);

private:
    INDISwitchClient* client_;  ///< Pointer to the associated INDISwitchClient.
    mutable std::recursive_mutex state_mutex_;  ///< Mutex for thread safety.

    // Switch data
    std::vector<SwitchInfo> switches_;  ///< List of managed switches.
    std::unordered_map<std::string, uint32_t>
        switch_name_to_index_;  ///< Map from switch name to index.

    // Group data
    std::vector<SwitchGroup> groups_;  ///< List of switch groups.
    std::unordered_map<std::string, uint32_t>
        group_name_to_index_;  ///< Map from group name to index.

    // INDI utility methods

    /**
     * @brief Convert SwitchState to INDI ISState.
     * @param state SwitchState to convert.
     * @return Corresponding ISState.
     */
    [[nodiscard]] auto createINDIState(SwitchState state) const noexcept
        -> ISState;

    /**
     * @brief Convert INDI ISState to SwitchState.
     * @param state ISState to convert.
     * @return Corresponding SwitchState.
     */
    [[nodiscard]] auto parseINDIState(ISState state) const noexcept
        -> SwitchState;

    /**
     * @brief Setup property mappings between switches and INDI properties.
     */
    void setupPropertyMappings();
};

#endif  // LITHIUM_DEVICE_INDI_SWITCH_MANAGER_HPP
