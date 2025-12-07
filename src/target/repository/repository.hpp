// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file repository.hpp
 * @brief Aggregated header for target repository module.
 *
 * This is the primary include file for the target repository components.
 * Include this file to get access to all repository implementations
 * including SQLite, memory, and cached repositories.
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#pragma once

// ============================================================================
// Repository Components
// ============================================================================

#include "cached_repository.hpp"
#include "celestial_repository.hpp"
#include "memory_repository.hpp"
#include "repository_interface.hpp"
#include "sqlite_repository.hpp"

namespace lithium::target::repository {

/**
 * @brief Repository module version.
 */
inline constexpr const char* REPOSITORY_MODULE_VERSION = "1.0.0";

/**
 * @brief Get repository module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getRepositoryModuleVersion() noexcept {
    return REPOSITORY_MODULE_VERSION;
}

// ============================================================================
// Repository Implementations
// ============================================================================
// - RepositoryFactory: Create repository instances
// - ICelestialRepository: Interface for all implementations
// - SqliteRepository: Persistent SQLite storage
// - MemoryRepository: In-memory volatile storage
// - CachedRepository: LRU caching decorator

}  // namespace lithium::target::repository
