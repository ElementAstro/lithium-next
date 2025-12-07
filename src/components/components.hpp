/**
 * @file components.hpp
 * @brief Main aggregated header for Lithium Components library.
 *
 * This is the primary include file for the components library.
 * Include this file to get access to all component management functionality.
 *
 * @par Usage Example:
 * @code
 * #include "components/components.hpp"
 *
 * using namespace lithium;
 *
 * // Create a ModuleLoader instance
 * auto loader = ModuleLoader::createShared();
 *
 * // Load a module
 * auto result = loader->loadModule("path/to/module.so", "module_name");
 *
 * // Create a ComponentManager instance
 * auto manager = ComponentManager::createShared();
 *
 * // Load a component
 * manager->loadComponent(params);
 *
 * // Use DependencyGraph for dependency resolution
 * DependencyGraph graph;
 * graph.addNode("package_a", Version{1, 0, 0});
 * @endcode
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_HPP
#define LITHIUM_COMPONENTS_HPP

// ============================================================================
// Core Module
// ============================================================================
// Module loading, dependency management, version handling, file tracking
//
// Components:
// - DependencyGraph: Directed dependency graph with cycle detection
// - ModuleLoader: Dynamic module loading with async support
// - FileTracker: File state tracking with incremental scanning
// - Version: Semantic versioning (SemVer) support
// - ModuleInfo: Module information structure

#include "core/core.hpp"

// ============================================================================
// Manager Module
// ============================================================================
// Component lifecycle management
//
// Components:
// - ComponentManager: Component lifecycle manager (load, start, stop, pause,
// resume)
// - ComponentEvent: Component event enumeration
// - ComponentState: Component state enumeration
// - ComponentOptions: Component configuration options

#include "manager/manager_module.hpp"

// ============================================================================
// Debug Module
// ============================================================================
// Debugging and analysis utilities
//
// Components:
// - CoreDumpAnalyzer: Core dump file analysis
// - DynamicLibraryParser: Dynamic library dependency parsing
// - ElfParser: ELF file parsing (Linux only)

#include "debug/debug.hpp"

// ============================================================================
// System Module
// ============================================================================
// System dependency management
//
// Components:
// - DependencyManager: System dependency manager
// - PackageManagerRegistry: Package manager registry
// - PlatformDetector: Platform detection

#include "system/system.hpp"

namespace lithium {

// ============================================================================
// Library Version
// ============================================================================

/**
 * @brief Lithium Components library version.
 */
inline constexpr const char* COMPONENTS_VERSION = "1.1.0";

/**
 * @brief Get components library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getComponentsVersion() noexcept {
    return COMPONENTS_VERSION;
}

/**
 * @brief Get all module versions as a formatted string.
 * @return Formatted string with all module versions.
 */
[[nodiscard]] inline std::string getAllModuleVersions() {
    std::string result;
    result += "Components: ";
    result += COMPONENTS_VERSION;
    result += "\n  Core: ";
    result += getCoreModuleVersion();
    result += "\n  Manager: ";
    result += getManagerModuleVersion();
    result += "\n  Debug: ";
    result += addon::getDebugModuleVersion();
    result += "\n  System: ";
    result += system::getSystemModuleVersion();
    return result;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

// Core types (from core/core.hpp)
// - DependencyGraph, ModuleLoader, FileTracker, Version, ModuleInfo
// - VersionCompare, ModuleStatus, ModuleStatistics, DependencyNode,
// TrackerFileStats
// - ModuleLoaderPtr, DependencyGraphPtr, FileTrackerPtr, ModuleInfoPtr

// Manager types (from manager/manager_module.hpp)
// - ComponentManager, ComponentEvent, ComponentState, ComponentOptions
// - ComponentManagerPtr, ComponentManagerWeakPtr, ComponentEventCallback

// Debug types (in lithium::addon namespace, from debug/debug.hpp)
// - CoreDumpAnalyzer, DynamicLibraryParser, ElfParser
// - CoreDumpAnalyzerPtr, DynamicLibraryParserPtr, ElfParserPtr

// System types (in lithium::system namespace, from system/system.hpp)
// - DependencyManager, PackageManagerRegistry, PlatformDetector
// - DependencyException, DependencyErrorCode, DependencyError
// - VersionInfo, DependencyInfo, PackageManagerInfo
// - DependencyManagerPtr, PackageManagerRegistryPtr, PlatformDetectorPtr

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Create a complete component management system.
 * @return Tuple of (ModuleLoader, ComponentManager, DependencyGraph).
 */
[[nodiscard]] inline auto createComponentSystem() {
    return std::make_tuple(createModuleLoader(), createComponentManager(),
                           createDependencyGraph());
}

/**
 * @brief Check if all required modules are available.
 * @return True if all modules are available.
 */
[[nodiscard]] inline bool checkModulesAvailable() {
    return getCoreModuleVersion() != nullptr &&
           getManagerModuleVersion() != nullptr &&
           addon::getDebugModuleVersion() != nullptr &&
           system::getSystemModuleVersion() != nullptr;
}

}  // namespace lithium

#endif  // LITHIUM_COMPONENTS_HPP
