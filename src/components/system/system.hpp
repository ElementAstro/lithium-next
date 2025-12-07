/**
 * @file system.hpp
 * @brief Aggregated header for system dependency module.
 *
 * This is the primary include file for the system dependency module.
 * Include this file to get access to all system dependency management
 * functionality including dependency resolution, package management,
 * and platform detection.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_SYSTEM_HPP
#define LITHIUM_COMPONENTS_SYSTEM_HPP

#include <memory>

// ============================================================================
// System Dependency Components
// ============================================================================

#include "dependency_exception.hpp"
#include "dependency_manager.hpp"
#include "dependency_types.hpp"
#include "package_manager.hpp"
#include "platform_detector.hpp"

namespace lithium::system {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief System module version.
 */
inline constexpr const char* SYSTEM_MODULE_VERSION = "1.1.0";

/**
 * @brief Get system module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getSystemModuleVersion() noexcept {
    return SYSTEM_MODULE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to DependencyManager
using DependencyManagerPtr = std::shared_ptr<DependencyManager>;

/// Shared pointer to PackageManagerRegistry
using PackageManagerRegistryPtr = std::shared_ptr<PackageManagerRegistry>;

/// Shared pointer to PlatformDetector
using PlatformDetectorPtr = std::shared_ptr<PlatformDetector>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new DependencyManager instance.
 * @param configPath Optional path to package manager configuration file.
 * @return Shared pointer to the new DependencyManager.
 */
[[nodiscard]] inline DependencyManagerPtr createDependencyManager(
    const std::string& configPath = "package_managers.json") {
    return std::make_shared<DependencyManager>(configPath);
}

/**
 * @brief Create a new PlatformDetector instance.
 * @return Shared pointer to the new PlatformDetector.
 */
[[nodiscard]] inline PlatformDetectorPtr createPlatformDetector() {
    return std::make_shared<PlatformDetector>();
}

/**
 * @brief Create a new PackageManagerRegistry instance.
 * @param detector Reference to a PlatformDetector.
 * @return Shared pointer to the new PackageManagerRegistry.
 */
[[nodiscard]] inline PackageManagerRegistryPtr createPackageManagerRegistry(
    const PlatformDetector& detector) {
    return std::make_shared<PackageManagerRegistry>(detector);
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Get the current platform name.
 * @return Platform name string (e.g., "linux", "macos", "windows").
 */
[[nodiscard]] inline std::string getCurrentPlatform() {
    PlatformDetector detector;
    return detector.getNormalizedPlatform();
}

/**
 * @brief Get the default package manager for the current platform.
 * @return Default package manager name.
 */
[[nodiscard]] inline std::string getDefaultPackageManager() {
    PlatformDetector detector;
    return detector.getDefaultPackageManager();
}

/**
 * @brief Check if a dependency is installed on the system.
 * @param depName Name of the dependency to check.
 * @return True if installed, false otherwise.
 */
[[nodiscard]] inline bool isDependencyInstalled(const std::string& depName) {
    DependencyManager manager;
    return manager.isDependencyInstalled(depName);
}

/**
 * @brief Get the installed version of a dependency.
 * @param depName Name of the dependency.
 * @return Optional VersionInfo if found.
 */
[[nodiscard]] inline std::optional<VersionInfo> getInstalledVersion(
    const std::string& depName) {
    DependencyManager manager;
    return manager.getInstalledVersion(depName);
}

}  // namespace lithium::system

#endif  // LITHIUM_COMPONENTS_SYSTEM_HPP
