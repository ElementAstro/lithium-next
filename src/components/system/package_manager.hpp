#ifndef LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP
#define LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "dependency_types.hpp"
#include "platform_detector.hpp"

namespace lithium::system {

/**
 * @class PackageManagerRegistry
 * @brief Thread-safe registry and manager for available package managers.
 *
 * Handles loading, configuration, and querying of package managers based on the
 * current platform. Provides methods to retrieve package manager information,
 * search for dependencies, and manage installation operations.
 *
 * All public methods are thread-safe.
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
     * Thread-safe.
     */
    void loadSystemPackageManagers();

    /**
     * @brief Load package manager configuration from a file.
     * @param configPath Path to the configuration file.
     *
     * Reads and applies package manager definitions from the specified
     * configuration file. Thread-safe.
     */
    void loadPackageManagerConfig(const std::string& configPath);

    /**
     * @brief Get a package manager by name.
     * @param name The name of the package manager.
     * @return Optional PackageManagerInfo if found, otherwise std::nullopt.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto getPackageManager(const std::string& name) const
        -> std::optional<PackageManagerInfo>;

    /**
     * @brief Get the default package manager for the current platform.
     * @return Optional PackageManagerInfo if available, otherwise std::nullopt.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto getDefaultPackageManager() const
        -> std::optional<PackageManagerInfo>;

    /**
     * @brief Get a list of all registered package managers.
     * @return Vector of PackageManagerInfo objects.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto getPackageManagers() const
        -> std::vector<PackageManagerInfo>;

    /**
     * @brief Get the number of registered package managers.
     * @return Number of package managers.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto getPackageManagerCount() const -> size_t;

    /**
     * @brief Check if a package manager is registered.
     * @param name The name of the package manager.
     * @return True if registered, false otherwise.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto hasPackageManager(const std::string& name) const -> bool;

    /**
     * @brief Search for dependencies by name across all package managers.
     * @param depName The name or pattern of the dependency to search for.
     * @return Vector of matching dependency names.
     *
     * Thread-safe.
     */
    [[nodiscard]] auto searchDependency(const std::string& depName)
        -> std::vector<std::string>;

    /**
     * @brief Cancel an ongoing installation for a dependency.
     * @param depName The name of the dependency whose installation should be
     * cancelled.
     *
     * Thread-safe.
     */
    void cancelInstallation(const std::string& depName);

    /**
     * @brief Register a custom package manager.
     * @param info The PackageManagerInfo to register.
     * @return True if registered successfully, false if already exists.
     *
     * Thread-safe.
     */
    auto registerPackageManager(const PackageManagerInfo& info) -> bool;

    /**
     * @brief Unregister a package manager by name.
     * @param name The name of the package manager to remove.
     * @return True if removed, false if not found.
     *
     * Thread-safe.
     */
    auto unregisterPackageManager(const std::string& name) -> bool;

    /**
     * @brief Clear all registered package managers.
     *
     * Thread-safe.
     */
    void clearPackageManagers();

    /**
     * @brief Check if a package manager command exists on the system.
     * @param command The command to check.
     * @return True if the command exists.
     */
    [[nodiscard]] static auto commandExists(const std::string& command) -> bool;

private:
    /**
     * @brief Configure and initialize package managers for the current
     * platform.
     */
    void configurePackageManagers();

    /**
     * @brief Parse search output from a specific package manager.
     * @param packageManager Name of the package manager
     * @param output Raw output from the search command
     * @param searchTerm The term that was searched for
     * @return Vector of parsed package names
     */
    [[nodiscard]] std::vector<std::string> parseSearchOutput(
        const std::string& packageManager, const std::string& output,
        const std::string& searchTerm) const;

    /**
     * @brief Execute a command and return its output.
     * @param command The command to execute.
     * @return The command output.
     */
    [[nodiscard]] static std::string executeCommand(const std::string& command);

    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access.
    std::vector<PackageManagerInfo>
        packageManagers_;  ///< Registered package managers.
    std::unordered_set<std::string>
        loadedConfigs_;  ///< Track loaded config files.
    const PlatformDetector&
        platformDetector_;  ///< Reference to the platform detector.
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_PACKAGE_MANAGER_HPP
