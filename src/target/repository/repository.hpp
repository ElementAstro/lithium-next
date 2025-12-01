// ===========================================================================
// Lithium-Target Repository Module Header
// ===========================================================================
// This project is licensed under the terms of the GPL3 license.
//
// Module: Target Repository
// Description: Public interface for target repository subsystem
// ===========================================================================

#pragma once

// Include all repository-related headers
#include "cached_repository.hpp"
#include "celestial_repository.hpp"
#include "memory_repository.hpp"
#include "repository_interface.hpp"
#include "sqlite_repository.hpp"

// Aggregate namespace for repository module
namespace lithium::target::repository {
// Repository implementations:
// - RepositoryFactory: Create repository instances
// - ICelestialRepository: Interface for all implementations
// - SqliteRepository: Persistent SQLite storage
// - MemoryRepository: In-memory volatile storage
// - CachedRepository: LRU caching decorator
}  // namespace lithium::target::repository

