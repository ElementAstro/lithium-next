/**
 * @file core.hpp
 * @brief Aggregated header for core component module.
 *
 * This is the primary include file for the core components of the
 * component management system. Include this file to get access to
 * all core functionality including module loading, dependency management,
 * version handling, and file tracking.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_CORE_HPP
#define LITHIUM_COMPONENTS_CORE_HPP

#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Core Module Components
// ============================================================================

#include "dependency.hpp"
#include "loader.hpp"
#include "module.hpp"
#include "tracker.hpp"
#include "version.hpp"

namespace lithium {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Core module version.
 */
inline constexpr const char* CORE_MODULE_VERSION = "1.1.0";

/**
 * @brief Get core module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getCoreModuleVersion() noexcept {
    return CORE_MODULE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Version comparison strategy
using VersionCompare = VersionCompareStrategy;

/// Module status enumeration
using ModuleStatus = ModuleInfo::Status;

/// Module statistics structure
using ModuleStatistics = ModuleInfo::Statistics;

/// Dependency graph node type
using DependencyNode = DependencyGraph::Node;

/// File tracker statistics structure
using TrackerFileStats = FileTracker::FileStats;

/// Shared pointer to ModuleLoader
using ModuleLoaderPtr = std::shared_ptr<ModuleLoader>;

/// Shared pointer to DependencyGraph
using DependencyGraphPtr = std::shared_ptr<DependencyGraph>;

/// Shared pointer to FileTracker
using FileTrackerPtr = std::shared_ptr<FileTracker>;

/// Shared pointer to ModuleInfo
using ModuleInfoPtr = std::shared_ptr<ModuleInfo>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new ModuleLoader instance.
 * @return Shared pointer to the new ModuleLoader.
 */
[[nodiscard]] inline ModuleLoaderPtr createModuleLoader() {
    return ModuleLoader::createShared();
}

/**
 * @brief Create a new ModuleLoader instance with a specified directory.
 * @param dirName The directory name where modules are located.
 * @return Shared pointer to the new ModuleLoader.
 */
[[nodiscard]] inline ModuleLoaderPtr createModuleLoader(
    const std::string& dirName) {
    return ModuleLoader::createShared(dirName);
}

/**
 * @brief Create a new DependencyGraph instance.
 * @return Shared pointer to the new DependencyGraph.
 */
[[nodiscard]] inline DependencyGraphPtr createDependencyGraph() {
    return std::make_shared<DependencyGraph>();
}

/**
 * @brief Create a new FileTracker instance.
 * @param directory The directory to track.
 * @param jsonFilePath The path to the JSON file for storing file information.
 * @param fileTypes The types of files to track.
 * @param recursive Whether to track files recursively.
 * @return Shared pointer to the new FileTracker.
 */
[[nodiscard]] inline FileTrackerPtr createFileTracker(
    const std::string& directory, const std::string& jsonFilePath,
    const std::vector<std::string>& fileTypes, bool recursive = false) {
    return std::make_shared<FileTracker>(directory, jsonFilePath, fileTypes,
                                         recursive);
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Parse a version string into a Version object.
 * @param versionStr The version string to parse.
 * @return Parsed Version object.
 */
[[nodiscard]] inline Version parseVersion(const std::string& versionStr) {
    return Version::parse(versionStr);
}

/**
 * @brief Check if two versions are compatible.
 * @param v1 First version.
 * @param v2 Second version.
 * @return True if versions are compatible.
 */
[[nodiscard]] inline bool areVersionsCompatible(const Version& v1,
                                                const Version& v2) {
    return v1.isCompatibleWith(v2);
}

/**
 * @brief Check if a version satisfies a version range.
 * @param version The version to check.
 * @param min Minimum version.
 * @param max Maximum version.
 * @return True if version is within range.
 */
[[nodiscard]] inline bool isVersionInRange(const Version& version,
                                           const Version& min,
                                           const Version& max) {
    return version.satisfiesRange(min, max);
}

/**
 * @brief Get the string representation of a module status.
 * @param status The module status.
 * @return String representation.
 */
[[nodiscard]] inline std::string moduleStatusToString(ModuleStatus status) {
    switch (status) {
        case ModuleStatus::UNLOADED:
            return "Unloaded";
        case ModuleStatus::LOADING:
            return "Loading";
        case ModuleStatus::LOADED:
            return "Loaded";
        case ModuleStatus::ERROR:
            return "Error";
    }
    return "Unknown";
}

/**
 * @brief Check if a module status indicates the module is usable.
 * @param status The module status.
 * @return True if the module is usable.
 */
[[nodiscard]] inline bool isModuleUsable(ModuleStatus status) {
    return status == ModuleStatus::LOADED;
}

/**
 * @brief Create a version range from minimum to maximum.
 * @param min Minimum version string.
 * @param max Maximum version string.
 * @return VersionRange object.
 */
[[nodiscard]] inline VersionRange createVersionRange(const std::string& min,
                                                     const std::string& max) {
    return VersionRange(Version::parse(min), Version::parse(max));
}

/**
 * @brief Create a version range starting from a minimum version.
 * @param min Minimum version string.
 * @return VersionRange object.
 */
[[nodiscard]] inline VersionRange createVersionRangeFrom(
    const std::string& min) {
    return VersionRange::from(Version::parse(min));
}

/**
 * @brief Create a version range up to a maximum version.
 * @param max Maximum version string.
 * @return VersionRange object.
 */
[[nodiscard]] inline VersionRange createVersionRangeUpTo(
    const std::string& max) {
    return VersionRange::upTo(Version::parse(max));
}

}  // namespace lithium

#endif  // LITHIUM_COMPONENTS_CORE_HPP
