/*
 * config_discovery.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_CONFIG_DISCOVERY_HPP
#define LITHIUM_SCRIPT_ISOLATED_CONFIG_DISCOVERY_HPP

#include "types.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace lithium::isolated {

/**
 * @brief Configuration discovery and validation utilities
 */
class ConfigDiscovery {
public:
    /**
     * @brief Find the default Python executable
     * @return Path to Python executable or nullopt
     */
    [[nodiscard]] static std::optional<std::filesystem::path> findPythonExecutable();

    /**
     * @brief Find the default executor script
     * @return Path to executor script or nullopt
     */
    [[nodiscard]] static std::optional<std::filesystem::path> findExecutorScript();

    /**
     * @brief Validate the isolation configuration
     * @param config Configuration to validate
     * @return Success or error
     */
    [[nodiscard]] static Result<void> validateConfig(const IsolationConfig& config);

    /**
     * @brief Get the Python version string
     * @param pythonPath Path to Python executable
     * @return Version string or nullopt
     */
    [[nodiscard]] static std::optional<std::string> getPythonVersion(
        const std::filesystem::path& pythonPath);

    /**
     * @brief Check if a path is within allowed paths
     * @param path Path to check
     * @param allowedPaths List of allowed paths
     * @return True if path is allowed
     */
    [[nodiscard]] static bool isPathAllowed(
        const std::filesystem::path& path,
        const std::vector<std::filesystem::path>& allowedPaths);
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_CONFIG_DISCOVERY_HPP
