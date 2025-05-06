/*
 * configor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-4

Description: Configor

**************************************************/

#ifndef LITHIUM_CONFIG_CONFIGOR_HPP
#define LITHIUM_CONFIG_CONFIGOR_HPP

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

class BadConfigException : public atom::error::Exception {
    using atom::error::Exception::Exception;
};

#define THROW_BAD_CONFIG_EXCEPTION(...)                                      \
    throw BadConfigException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                             __VA_ARGS__)

class InvalidConfigException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

#define THROW_INVALID_CONFIG_EXCEPTION(...)                      \
    throw InvalidConfigException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

namespace lithium {

// Forward declaration
class ConfigManagerImpl;

/**
 * @brief Concept for values that can be stored in a configuration
 */
template <typename T>
concept ConfigValue = requires(T value, json j) {
    { j = value } -> std::convertible_to<json>;
    { j.get<T>() } -> std::convertible_to<T>;
};

/**
 * @brief The ConfigManager class manages configuration data using JSON format.
 *
 * This class provides methods to manipulate configuration values, load from
 * files or directories, save to a file, and perform various operations like
 * merging configurations.
 */
class ConfigManager {
public:
    /**
     * @brief Default constructor.
     */
    ConfigManager();

    /**
     * @brief Destructor.
     */
    ~ConfigManager();

    // Delete copy operations to ensure proper resource management
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

    // Allow move operations
    ConfigManager(ConfigManager &&) noexcept;
    ConfigManager &operator=(ConfigManager &&) noexcept;

    /**
     * @brief Creates a shared pointer instance of ConfigManager.
     * @return std::shared_ptr<ConfigManager> Shared pointer to ConfigManager
     * instance.
     */
    [[nodiscard]] static auto createShared() -> std::shared_ptr<ConfigManager>;

    /**
     * @brief Creates a unique pointer instance of ConfigManager.
     * @return std::unique_ptr<ConfigManager> Unique pointer to ConfigManager
     * instance.
     */
    [[nodiscard]] static auto createUnique() -> std::unique_ptr<ConfigManager>;

    /**
     * @brief Retrieves the value associated with the given key path.
     * @param key_path The path to the configuration value.
     * @return std::optional<json> The optional JSON value if found.
     */
    [[nodiscard]] auto get(std::string_view key_path) const
        -> std::optional<json>;

    /**
     * @brief Retrieves a typed value from the configuration.
     * @tparam T The expected type of the value.
     * @param key_path The path to the configuration value.
     * @return std::optional<T> The typed value if found and convertible.
     */
    template <ConfigValue T>
    [[nodiscard]] auto get_as(std::string_view key_path) const
        -> std::optional<T> {
        auto value = get(key_path);
        if (!value.has_value()) {
            return std::nullopt;
        }

        try {
            return value->get<T>();
        } catch (const json::exception &e) {
            // Log error about type conversion
            return std::nullopt;
        }
    }

    /**
     * @brief Sets the value for the specified key path.
     * @param key_path The path to set the configuration value.
     * @param value The JSON value to set.
     * @return bool True if the value was successfully set, false otherwise.
     */
    auto set(std::string_view key_path, const json &value) -> bool;

    /**
     * @brief Sets the value for the specified key path.
     * @param key_path The path to set the configuration value.
     * @param value The JSON value to set.
     * @return bool True if the value was successfully set, false otherwise.
     */
    auto set(std::string_view key_path, json &&value) -> bool;

    /**
     * @brief Sets any compatible value for the specified key path.
     * @tparam T Type of the value to set (must be compatible with json).
     * @param key_path The path to set the configuration value.
     * @param value The value to set.
     * @return bool True if the value was successfully set, false otherwise.
     */
    template <ConfigValue T>
    auto set_value(std::string_view key_path, T &&value) -> bool {
        return set(key_path, json(std::forward<T>(value)));
    }

    /**
     * @brief Appends a value to an array at the specified key path.
     * @param key_path The path to the array.
     * @param value The JSON value to append.
     * @return bool True if the value was successfully appended, false
     * otherwise.
     */
    auto append(std::string_view key_path, const json &value) -> bool;

    /**
     * @brief Appends any compatible value to an array at the specified key
     * path.
     * @tparam T Type of the value to append (must be compatible with json).
     * @param key_path The path to the array.
     * @param value The value to append.
     * @return bool True if the value was successfully appended, false
     * otherwise.
     */
    template <ConfigValue T>
    auto append_value(std::string_view key_path, T &&value) -> bool {
        return append(key_path, json(std::forward<T>(value)));
    }

    /**
     * @brief Deletes the value associated with the given key path.
     * @param key_path The path to the configuration value to delete.
     * @return bool True if the value was successfully deleted, false otherwise.
     */
    auto remove(std::string_view key_path) -> bool;

    /**
     * @brief Checks if a value exists for the given key path.
     * @param key_path The path to check.
     * @return bool True if a value exists for the key path, false otherwise.
     */
    [[nodiscard]] auto has(std::string_view key_path) const -> bool;

    /**
     * @brief Retrieves all keys in the configuration.
     * @return std::vector<std::string> A vector of keys in the configuration.
     */
    [[nodiscard]] auto getKeys() const -> std::vector<std::string>;

    /**
     * @brief Lists all configuration files in specified directory.
     * @return std::vector<std::string> A vector of paths to configuration
     * files.
     */
    [[nodiscard]] auto listPaths() const -> std::vector<std::string>;

    /**
     * @brief Loads configuration data from a file.
     * @param path The path to the file containing configuration data.
     * @return bool True if the file was successfully loaded, false otherwise.
     */
    auto loadFromFile(const fs::path &path) -> bool;

    /**
     * @brief Loads configuration data from multiple files.
     * @param paths The paths to the files containing configuration data.
     * @return size_t The number of successfully loaded files.
     */
    auto loadFromFiles(std::span<const fs::path> paths) -> size_t;

    /**
     * @brief Loads configuration data from a directory.
     * @param dir_path The path to the directory containing configuration files.
     * @param recursive Flag indicating whether to recursively load from
     * subdirectories.
     * @return bool True if the directory was successfully loaded, false
     * otherwise.
     */
    auto loadFromDir(const fs::path &dir_path, bool recursive = false) -> bool;

    /**
     * @brief Saves the current configuration to a file.
     * @param file_path The path to save the configuration file.
     * @return bool True if the configuration was successfully saved, false
     * otherwise.
     */
    [[nodiscard]] auto save(const fs::path &file_path) const -> bool;

    /**
     * @brief Saves all configuration data to files in the specified directory.
     * @param dir_path The path to the directory to save configuration files.
     * @return bool True if all configuration data was successfully saved, false
     * otherwise.
     */
    [[nodiscard]] auto saveAll(const fs::path &dir_path) const -> bool;

    /**
     * @brief Cleans up the configuration by removing unused entries or
     * optimizing data.
     */
    void tidy();

    /**
     * @brief Clears all configuration data.
     */
    void clear();

    /**
     * @brief Merges the current configuration with the provided JSON data.
     * @param src The JSON object to merge into the current configuration.
     */
    void merge(const json &src);

    /**
     * @brief Registers a callback for configuration changes
     * @param callback Function to call when configuration changes
     * @return size_t ID of the registered callback
     */
    size_t onChanged(std::function<void(std::string_view path)> callback);

    /**
     * @brief Unregisters a configuration change callback
     * @param id ID of the callback to remove
     * @return bool True if callback was removed, false if not found
     */
    bool removeCallback(size_t id);

private:
    std::unique_ptr<ConfigManagerImpl>
        impl_;  ///< Implementation-specific pointer.

    void merge(const json &src, json &target);
};

}  // namespace lithium

#endif
