#ifndef LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP
#define LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP

#include <optional>
#include <string>
#include <vector>

#include "dependency_types.hpp"
#include "platform_detector.hpp"

namespace lithium::system {

/**
 * @class PackageManagerRegistry
 * @brief Registry and manager for available package managers on the system.
 *
 * Handles loading, configuration, and querying of package managers based on the
 * current platform. Provides methods to retrieve package manager information,
 * search for dependencies, and manage installation operations.
 */
class PackageManagerRegistry {
public:
    /**
     * @brief Construct a PackageManagerRegistry with a platform detector.
     * @param detector Reference to a PlatformDetector for platform-specific
     * logic.
     */
    explicit PackageManagerRegistry(const PlatformDetector& detector);

    /**
     * @brief Load all system-supported package managers.
     *
     * Detects and registers package managers available on the current platform.
     */
    void loadSystemPackageManagers();

    /**
     * @brief Load package manager configuration from a file.
     * @param configPath Path to the configuration file.
     *
     * Reads and applies package manager definitions from the specified
     * configuration file.
     */
    void loadPackageManagerConfig(const std::string& configPath);

    /**
     * @brief Get a package manager by name.
     * @param name The name of the package manager.
     * @return Optional PackageManagerInfo if found, otherwise std::nullopt.
     */
    auto getPackageManager(const std::string& name) const
        -> std::optional<PackageManagerInfo>;

    /**
     * @brief Get a list of all registered package managers.
     * @return Vector of PackageManagerInfo objects.
     */
    auto getPackageManagers() const -> std::vector<PackageManagerInfo>;

    /**
     * @brief Search for dependencies by name across all package managers.
     * @param depName The name or pattern of the dependency to search for.
     * @return Vector of matching dependency names.
     */
    auto searchDependency(const std::string& depName)
        -> std::vector<std::string>;

    /**
     * @brief Cancel an ongoing installation for a dependency.
     * @param depName The name of the dependency whose installation should be
     * cancelled.
     */
    void cancelInstallation(const std::string& depName);

private:
    /**
     * @brief Configure and initialize package managers for the current
     * platform.
     */
    void configurePackageManagers();

    std::vector<PackageManagerInfo>
        packageManagers_;  ///< Registered package managers.
    const PlatformDetector&
        platformDetector_;  ///< Reference to the platform detector.

    /**
     * @brief Parse search output from a specific package manager.
     * @param packageManager Name of the package manager
     * @param output Raw output from the search command
     * @param searchTerm The term that was searched for
     * @return Vector of parsed package names
     */
    std::vector<std::string> parseSearchOutput(
        const std::string& packageManager, const std::string& output,
        const std::string& searchTerm) const;
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP
