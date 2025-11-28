#ifndef LITHIUM_SYSTEM_DEPENDENCY_MANAGER_HPP
#define LITHIUM_SYSTEM_DEPENDENCY_MANAGER_HPP

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dependency_exception.hpp"
#include "dependency_types.hpp"

namespace lithium::system {

/**
 * @class DependencyManager
 * @brief Manages software dependencies, installation, uninstallation, and
 * configuration.
 *
 * Provides asynchronous and synchronous interfaces for installing, removing,
 * and verifying dependencies, as well as exporting/importing configuration and
 * generating reports. Uses the PIMPL idiom for implementation hiding.
 */
class DependencyManager {
public:
    /**
     * @brief Construct a DependencyManager instance.
     * @param configPath Path to the package manager configuration file
     * (default: "package_managers.json").
     */
    explicit DependencyManager(
        const std::string& configPath = "package_managers.json");

    /**
     * @brief Destructor.
     */
    ~DependencyManager();

    DependencyManager(const DependencyManager&) = delete;
    DependencyManager& operator=(const DependencyManager&) = delete;

    /**
     * @brief Asynchronously install a dependency by name.
     * @param name The name of the dependency to install.
     * @return A future containing the result of the installation (dependency
     * name or error).
     */
    auto install(const std::string& name)
        -> std::future<DependencyResult<std::string>>;

    /**
     * @brief Asynchronously install a dependency with a specific version.
     * @param name The name of the dependency.
     * @param version The version to install.
     * @return A future containing the result of the installation (void or
     * error).
     */
    auto installWithVersion(const std::string& name, const std::string& version)
        -> std::future<DependencyResult<void>>;

    /**
     * @brief Asynchronously install multiple dependencies.
     * @param deps A vector of dependency names to install.
     * @return A vector of futures, each containing the result of an
     * installation.
     */
    auto installMultiple(const std::vector<std::string>& deps)
        -> std::vector<std::future<DependencyResult<void>>>;

    /**
     * @brief Check if a specific version of a dependency is compatible.
     * @param name The name of the dependency.
     * @param version The version to check.
     * @return Result indicating compatibility (true/false) or error.
     */
    auto checkVersionCompatibility(const std::string& name,
                                   const std::string& version)
        -> DependencyResult<bool>;

    /**
     * @brief Get a string representation of the current dependency graph.
     * @return A string describing the dependency graph.
     */
    auto getDependencyGraph() const -> std::string;

    /**
     * @brief Asynchronously verify the integrity of all dependencies.
     * @return A future containing the result (true if all are valid, or error).
     */
    auto verifyDependencies() -> std::future<DependencyResult<bool>>;

    /**
     * @brief Export the current dependency configuration as a string.
     * @return Result containing the configuration string or error.
     */
    auto exportConfig() const -> DependencyResult<std::string>;

    /**
     * @brief Import a dependency configuration from a string.
     * @param config The configuration string to import.
     * @return Result indicating success or error.
     */
    auto importConfig(const std::string& config) -> DependencyResult<void>;

    /**
     * @brief Check and install all required dependencies as needed.
     */
    void checkAndInstallDependencies();

    /**
     * @brief Set a custom install command for a specific dependency.
     * @param dep The dependency name.
     * @param command The custom install command.
     */
    void setCustomInstallCommand(const std::string& dep,
                                 const std::string& command);

    /**
     * @brief Generate a report of all managed dependencies.
     * @return A string containing the dependency report.
     */
    auto generateDependencyReport() const -> std::string;

    /**
     * @brief Uninstall a dependency by name.
     * @param dep The name of the dependency to uninstall.
     */
    void uninstallDependency(const std::string& dep);

    /**
     * @brief Get the current platform identifier.
     * @return A string representing the current platform.
     */
    auto getCurrentPlatform() const -> std::string;

    /**
     * @brief Asynchronously install a dependency.
     * @param dep The DependencyInfo object describing the dependency.
     */
    void installDependencyAsync(const DependencyInfo& dep);

    /**
     * @brief Cancel an ongoing installation for a dependency.
     * @param dep The name of the dependency to cancel installation for.
     */
    void cancelInstallation(const std::string& dep);

    /**
     * @brief Add a dependency to the manager.
     * @param dep The DependencyInfo object to add.
     */
    void addDependency(const DependencyInfo& dep);

    /**
     * @brief Remove a dependency by name.
     * @param depName The name of the dependency to remove.
     */
    void removeDependency(const std::string& depName);

    /**
     * @brief Search for dependencies by name.
     * @param depName The name or pattern to search for.
     * @return A vector of matching dependency names.
     */
    auto searchDependency(const std::string& depName)
        -> std::vector<std::string>;

    /**
     * @brief Load system package managers from configuration.
     */
    void loadSystemPackageManagers();

    /**
     * @brief Get information about all available package managers.
     * @return A vector of PackageManagerInfo objects.
     */
    auto getPackageManagers() const -> std::vector<PackageManagerInfo>;

    /**
     * @brief Check if a dependency is installed on the system.
     * @param depName The name of the dependency to check.
     * @return True if installed, false otherwise.
     */
    auto isDependencyInstalled(const std::string& depName) -> bool;

    /**
     * @brief Get the installed version of a dependency.
     * @param depName The name of the dependency.
     * @return Optional containing the version if found, nullopt otherwise.
     */
    auto getInstalledVersion(const std::string& depName) 
        -> std::optional<VersionInfo>;

    /**
     * @brief Refresh the installation cache by re-checking all dependencies.
     */
    void refreshCache();

private:
    /**
     * @brief Opaque implementation pointer (PIMPL idiom).
     */
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_DEPENDENCY_MANAGER_HPP
