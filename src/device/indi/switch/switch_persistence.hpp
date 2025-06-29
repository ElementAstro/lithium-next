/*
 * switch_persistence.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Persistence - State Persistence Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_PERSISTENCE_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_PERSISTENCE_HPP

#include <mutex>
#include <string>

// Forward declarations
class INDISwitchClient;

/**
 * @class SwitchPersistence
 * @brief Switch state persistence component for INDI devices.
 *
 * This class provides mechanisms to save, load, and reset switch states and
 * configuration for INDI switch devices. It also supports auto-save
 * functionality and thread-safe operations.
 */
class SwitchPersistence {
public:
    /**
     * @brief Construct a SwitchPersistence manager for a given
     * INDISwitchClient.
     * @param client Pointer to the associated INDISwitchClient.
     */
    explicit SwitchPersistence(INDISwitchClient* client);
    /**
     * @brief Destructor (defaulted).
     */
    ~SwitchPersistence() = default;

    /**
     * @brief Save the current switch state to persistent storage.
     *
     * In a real implementation, this would write to a file or database.
     * @return True if the state was saved successfully, false otherwise.
     */
    auto saveState() -> bool;

    /**
     * @brief Load the switch state from persistent storage.
     *
     * In a real implementation, this would read from a file or database.
     * @return True if the state was loaded successfully, false otherwise.
     */
    auto loadState() -> bool;

    /**
     * @brief Reset all switch, power, safety, and statistics components to
     * default values.
     * @return True if reset was successful, false otherwise.
     */
    auto resetToDefaults() -> bool;

    /**
     * @brief Save the current configuration to a file.
     * @param filename The file path to save the configuration to.
     * @return True if the configuration was saved successfully, false
     * otherwise.
     */
    auto saveConfiguration(const std::string& filename) -> bool;

    /**
     * @brief Load configuration from a file.
     * @param filename The file path to load the configuration from.
     * @return True if the configuration was loaded successfully, false
     * otherwise.
     */
    auto loadConfiguration(const std::string& filename) -> bool;

    /**
     * @brief Enable or disable auto-save functionality.
     * @param enable True to enable auto-save, false to disable.
     * @return True if the operation succeeded.
     */
    auto enableAutoSave(bool enable) -> bool;

    /**
     * @brief Check if auto-save is currently enabled.
     * @return True if auto-save is enabled, false otherwise.
     */
    auto isAutoSaveEnabled() -> bool;

    /**
     * @brief Set the interval for auto-save operations.
     * @param intervalSeconds The interval in seconds between auto-saves.
     */
    void setAutoSaveInterval(uint32_t intervalSeconds);

private:
    /**
     * @brief Pointer to the associated INDISwitchClient.
     */
    INDISwitchClient* client_;
    /**
     * @brief Mutex for thread-safe access to persistence state.
     */
    mutable std::mutex persistence_mutex_;

    /**
     * @brief Indicates if auto-save is enabled.
     */
    bool auto_save_enabled_{false};
    /**
     * @brief Interval in seconds for auto-save operations.
     */
    uint32_t auto_save_interval_{300};  // 5 minutes default

    /**
     * @brief Get the default configuration file path.
     * @return The default configuration file path as a string.
     */
    auto getDefaultConfigPath() -> std::string;
    /**
     * @brief Create a backup of the specified configuration file.
     * @param filename The file to back up.
     * @return True if the backup was created successfully, false otherwise.
     */
    auto createBackup(const std::string& filename) -> bool;
    /**
     * @brief Validate the specified configuration file.
     * @param filename The file to validate.
     * @return True if the file is valid, false otherwise.
     */
    auto validateConfigFile(const std::string& filename) -> bool;
};

#endif  // LITHIUM_DEVICE_INDI_SWITCH_PERSISTENCE_HPP
