/**
 * @file integration_utils.hpp
 * @brief Integration utilities for connecting task system components
 *
 * This file provides helper functions and utilities to integrate various
 * components of the task system that were previously isolated.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_INTEGRATION_UTILS_HPP
#define LITHIUM_TASK_INTEGRATION_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

// Forward declarations
class Task;
class ImagePatternParser;
struct ImageInfo;

/**
 * @namespace integration
 * @brief Utilities for integrating task system components
 */
namespace integration {

/**
 * @brief Image output path generation and parsing utilities
 */
class ImagePathHelper {
public:
    /**
     * @brief Generate output path for an imaging task
     * @param basePath Base directory for images
     * @param taskName Name of the task
     * @param params Task parameters (may contain filter, exposure, etc.)
     * @param sequence Sequence number
     * @return Generated file path
     */
    static std::filesystem::path generateOutputPath(
        const std::filesystem::path& basePath,
        const std::string& taskName,
        const json& params,
        int sequence = 0);

    /**
     * @brief Parse image filename to extract metadata
     * @param imagePath Path to the image file
     * @return Optional ImageInfo if parsing succeeds
     */
    static std::optional<ImageInfo> parseImagePath(
        const std::filesystem::path& imagePath);

    /**
     * @brief Get or create ImagePatternParser instance
     * @param pattern Optional pattern string to use
     * @return Reference to ImagePatternParser
     */
    static ImagePatternParser& getParser(
        const std::string& pattern = "");

    /**
     * @brief Set default pattern for image parsing
     * @param pattern Pattern string
     */
    static void setDefaultPattern(const std::string& pattern);

private:
    static std::string defaultPattern_;
};

/**
 * @brief Script execution utilities
 */
class ScriptHelper {
public:
    /**
     * @brief Execute a script with parameters from a task
     * @param scriptPath Path to script file
     * @param params Parameters to pass to script
     * @param timeout Timeout in milliseconds
     * @return Execution result as JSON
     */
    static json executeScript(
        const std::filesystem::path& scriptPath,
        const json& params,
        int timeout = 30000);

    /**
     * @brief Validate script parameters
     * @param params Parameters to validate
     * @param schema Parameter schema
     * @return True if valid
     */
    static bool validateScriptParams(
        const json& params,
        const json& schema);

    /**
     * @brief Convert task output to script input format
     * @param taskOutput Task execution result
     * @return Converted parameters for script
     */
    static json convertTaskOutputToScriptInput(const json& taskOutput);
};

/**
 * @brief Device control utilities
 */
class DeviceHelper {
public:
    /**
     * @brief Wait for device to be ready
     * @param deviceName Name of the device
     * @param timeout Timeout in milliseconds
     * @return True if device is ready
     */
    static bool waitForDevice(
        const std::string& deviceName,
        int timeout = 10000);

    /**
     * @brief Check if device is connected
     * @param deviceName Name of the device
     * @return True if connected
     */
    static bool isDeviceConnected(const std::string& deviceName);

    /**
     * @brief Get device property
     * @param deviceName Name of the device
     * @param propertyName Name of the property
     * @return Property value as JSON
     */
    static json getDeviceProperty(
        const std::string& deviceName,
        const std::string& propertyName);

    /**
     * @brief Set device property
     * @param deviceName Name of the device
     * @param propertyName Name of the property
     * @param value Property value
     * @return True if successful
     */
    static bool setDeviceProperty(
        const std::string& deviceName,
        const std::string& propertyName,
        const json& value);
};

/**
 * @brief Parameter validation utilities
 */
class ValidationHelper {
public:
    /**
     * @brief Validate numeric parameter range
     * @param value Value to validate
     * @param min Minimum value
     * @param max Maximum value
     * @return True if in range
     */
    static bool validateRange(double value, double min, double max);

    /**
     * @brief Validate required parameters exist
     * @param params Parameters object
     * @param required List of required parameter names
     * @return True if all required parameters exist
     */
    static bool validateRequiredParams(
        const json& params,
        const std::vector<std::string>& required);

    /**
     * @brief Validate parameter against schema
     * @param param Parameter value
     * @param schema JSON schema for parameter
     * @return True if valid
     */
    static bool validateAgainstSchema(
        const json& param,
        const json& schema);

    /**
     * @brief Get validation error message
     * @return Last validation error
     */
    static std::string getLastError();

private:
    static thread_local std::string lastError_;
};

/**
 * @brief Task chaining utilities
 */
class TaskChainHelper {
public:
    /**
     * @brief Create dependent task chain
     * @param tasks List of task configurations
     * @return Vector of task names in dependency order
     */
    static std::vector<std::string> createDependencyChain(
        const std::vector<json>& tasks);

    /**
     * @brief Resolve task dependencies
     * @param taskName Name of the task
     * @param allTasks Map of all tasks
     * @return Ordered list of dependencies
     */
    static std::vector<std::string> resolveDependencies(
        const std::string& taskName,
        const std::unordered_map<std::string, json>& allTasks);

    /**
     * @brief Check for circular dependencies
     * @param tasks Map of all tasks with dependencies
     * @return True if circular dependency detected
     */
    static bool hasCircularDependency(
        const std::unordered_map<std::string, json>& tasks);
};

/**
 * @brief Resource management utilities
 */
class ResourceHelper {
public:
    /**
     * @brief Check if sufficient disk space is available
     * @param path Path to check
     * @param requiredMB Required space in megabytes
     * @return True if sufficient space
     */
    static bool checkDiskSpace(
        const std::filesystem::path& path,
        size_t requiredMB);

    /**
     * @brief Check if sufficient memory is available
     * @param requiredMB Required memory in megabytes
     * @return True if sufficient memory
     */
    static bool checkMemory(size_t requiredMB);

    /**
     * @brief Estimate output file size for imaging task
     * @param width Image width
     * @param height Image height
     * @param bitDepth Bit depth
     * @param compression Whether compression is used
     * @return Estimated size in bytes
     */
    static size_t estimateImageSize(
        int width, int height, int bitDepth, bool compression = false);
};

}  // namespace integration
}  // namespace lithium::task

#endif  // LITHIUM_TASK_INTEGRATION_UTILS_HPP
