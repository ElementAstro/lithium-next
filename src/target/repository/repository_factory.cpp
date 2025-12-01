// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "repository_interface.hpp"

#include "cached_repository.hpp"
#include "memory_repository.hpp"
#include "sqlite_repository.hpp"

namespace lithium::target::repository {

auto RepositoryFactory::createSqliteRepository(const std::string& dbPath)
    -> std::unique_ptr<ICelestialRepository> {
    return std::make_unique<SqliteRepository>(dbPath);
}

auto RepositoryFactory::createMemoryRepository()
    -> std::unique_ptr<ICelestialRepository> {
    return std::make_unique<MemoryRepository>();
}

auto RepositoryFactory::createCachedRepository(
    std::unique_ptr<ICelestialRepository> inner, size_t cacheSize)
    -> std::unique_ptr<ICelestialRepository> {
    return std::make_unique<CachedRepository>(std::move(inner), cacheSize);
}

}  // namespace lithium::target::repository
