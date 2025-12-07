// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "search_engine.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <mutex>
#include <shared_mutex>
#include <utility>

namespace lithium::target::search {

/**
 * @brief Implementation class using PIMPL pattern
 *
 * Handles all internal search logic and index management.
 * Provides thread-safe access to multiple index structures.
 */
class SearchEngine::Impl {
public:
    /**
     * @brief Construct implementation
     */
    explicit Impl(std::shared_ptr<CelestialRepository> repository,
                  std::shared_ptr<index::TrieIndex> trieIndex,
                  std::shared_ptr<index::SpatialIndex> spatialIndex,
                  std::shared_ptr<index::FuzzyMatcher> fuzzyMatcher)
        : repository_(std::move(repository)),
          trieIndex_(std::move(trieIndex)),
          spatialIndex_(std::move(spatialIndex)),
          fuzzyMatcher_(std::move(fuzzyMatcher)),
          isInitialized_(false) {
        spdlog::debug("SearchEngine::Impl constructed");
    }

    /**
     * @brief Destructor
     */
    ~Impl() { spdlog::debug("SearchEngine::Impl destroyed"); }

    /**
     * @brief Initialize indexes from repository
     */
    auto initialize() -> atom::type::Expected<void, std::string> {
        std::unique_lock lock(indexMutex_);

        if (isInitialized_) {
            return {};  // Already initialized
        }

        if (!repository_) {
            return atom::type::make_unexpected("Repository not available");
        }

        try {
            spdlog::info("Initializing SearchEngine indexes");

            // Create default indexes if not provided
            if (!trieIndex_) {
                trieIndex_ = std::make_shared<index::TrieIndex>();
            }
            if (!spatialIndex_) {
                spatialIndex_ = std::make_shared<index::SpatialIndex>();
            }
            if (!fuzzyMatcher_) {
                fuzzyMatcher_ = std::make_shared<index::FuzzyMatcher>();
            }

            // Load all celestial objects from repository
            auto objects = repository_->search(CelestialSearchFilter());

            spdlog::info("Loading {} objects into indexes", objects.size());

            // Build indexes
            std::vector<std::string> names;
            names.reserve(objects.size());

            for (const auto& obj : objects) {
                // Add to Trie
                if (!obj.identifier.empty()) {
                    names.push_back(obj.identifier);
                }
                if (!obj.mIdentifier.empty()) {
                    names.push_back(obj.mIdentifier);
                }
                if (!obj.chineseName.empty()) {
                    names.push_back(obj.chineseName);
                }

                // Add to Spatial index
                spatialIndex_->insert(obj.identifier, obj.radJ2000,
                                      obj.decDJ2000);

                // Add to Fuzzy matcher
                fuzzyMatcher_->addTerm(obj.identifier, obj.identifier);
                if (!obj.mIdentifier.empty()) {
                    fuzzyMatcher_->addTerm(obj.mIdentifier, obj.identifier);
                }
            }

            // Batch insert to Trie
            if (!names.empty()) {
                trieIndex_->insertBatch(names);
            }

            isInitialized_ = true;
            objectCount_ = objects.size();

            spdlog::info("SearchEngine initialized with {} objects",
                         objectCount_);

            return {};
        } catch (const std::exception& e) {
            std::string error =
                std::format("Failed to initialize SearchEngine: {}", e.what());
            spdlog::error(error);
            return atom::type::make_unexpected(error);
        }
    }

    /**
     * @brief Check initialization status
     */
    [[nodiscard]] bool isInitialized() const {
        std::shared_lock lock(indexMutex_);
        return isInitialized_;
    }

    /**
     * @brief Perform general search
     */
    [[nodiscard]] auto search(const std::string& query,
                              const SearchOptions& options)
        -> std::vector<CelestialObjectModel> {
        std::shared_lock lock(indexMutex_);

        if (!isInitialized_) {
            spdlog::warn(
                "SearchEngine not initialized, returning empty results");
            return {};
        }

        // Try exact search first
        auto results = exactSearchImpl(query);

        // Try fuzzy search if enabled and no exact match
        if (results.empty() && options.useFuzzy && options.fuzzyTolerance > 0) {
            results = fuzzySearchImpl(query, options.fuzzyTolerance);
        }

        // Limit results
        if (static_cast<int>(results.size()) > options.maxResults) {
            results.resize(options.maxResults);
        }

        spdlog::debug("Search for '{}' returned {} results", query,
                      results.size());

        return results;
    }

    /**
     * @brief Exact search implementation
     */
    [[nodiscard]] auto exactSearchImpl(const std::string& query)
        -> std::vector<CelestialObjectModel> {
        std::vector<CelestialObjectModel> results;

        if (!repository_) {
            return results;
        }

        // Try direct lookup
        auto obj = repository_->findByIdentifier(query);
        if (obj.has_value()) {
            results.push_back(obj.value());
        }

        return results;
    }

    /**
     * @brief Fuzzy search implementation
     */
    [[nodiscard]] auto fuzzySearchImpl(const std::string& query, int tolerance)
        -> std::vector<CelestialObjectModel> {
        std::vector<CelestialObjectModel> results;

        if (!fuzzyMatcher_ || !repository_) {
            return results;
        }

        auto matches = fuzzyMatcher_->match(query, tolerance, 50);

        for (const auto& [objectId, distance] : matches) {
            auto obj = repository_->findByIdentifier(objectId);
            if (obj.has_value()) {
                results.push_back(obj.value());
            }
        }

        return results;
    }

    /**
     * @brief Coordinate search implementation
     */
    [[nodiscard]] auto searchByCoordinatesImpl(double ra, double dec,
                                               double radius, int limit)
        -> std::vector<CelestialObjectModel> {
        std::vector<CelestialObjectModel> results;

        if (!spatialIndex_ || !repository_) {
            return results;
        }

        auto nearby = spatialIndex_->searchRadius(ra, dec, radius, limit);

        for (const auto& [objectId, distance] : nearby) {
            auto obj = repository_->findByIdentifier(objectId);
            if (obj.has_value()) {
                results.push_back(obj.value());
            }
        }

        return results;
    }

    /**
     * @brief Autocomplete implementation
     */
    [[nodiscard]] auto autocompleteImpl(const std::string& prefix, int limit)
        -> std::vector<std::string> {
        if (!trieIndex_) {
            return {};
        }

        return trieIndex_->autocomplete(prefix, limit);
    }

    /**
     * @brief Advanced search implementation
     */
    [[nodiscard]] auto advancedSearchImpl(const CelestialSearchFilter& filter)
        -> std::vector<CelestialObjectModel> {
        if (!repository_) {
            return {};
        }

        // Delegate to repository's search
        return repository_->search(filter);
    }

    /**
     * @brief Rebuild indexes
     */
    auto rebuildIndexes() -> atom::type::Expected<void, std::string> {
        std::unique_lock lock(indexMutex_);

        try {
            clearIndexesImpl();
            // Re-initialize without lock since lock is already held
            isInitialized_ = false;
            return initialize();
        } catch (const std::exception& e) {
            return atom::type::make_unexpected(e.what());
        }
    }

    /**
     * @brief Clear indexes (internal, requires lock)
     */
    void clearIndexesImpl() {
        if (trieIndex_) {
            trieIndex_->clear();
        }
        if (spatialIndex_) {
            spatialIndex_->clear();
        }
        if (fuzzyMatcher_) {
            fuzzyMatcher_->clear();
        }
        isInitialized_ = false;
        objectCount_ = 0;
    }

    /**
     * @brief Clear indexes
     */
    void clearIndexes() {
        std::unique_lock lock(indexMutex_);
        clearIndexesImpl();
        spdlog::info("SearchEngine indexes cleared");
    }

    /**
     * @brief Get statistics
     */
    [[nodiscard]] auto getStats() const -> std::string {
        std::shared_lock lock(indexMutex_);

        std::string stats = std::format(
            "SearchEngine Statistics:\n"
            "  Initialized: {}\n"
            "  Object Count: {}\n",
            isInitialized_ ? "Yes" : "No", objectCount_);

        if (trieIndex_) {
            stats += std::format("  Trie Index Size: {}\n", trieIndex_->size());
        }

        if (spatialIndex_) {
            stats += std::format("  Spatial Index Objects: {}\n",
                                 spatialIndex_->size());
        }

        if (fuzzyMatcher_) {
            stats += std::format("  Fuzzy Matcher Stats:\n{}",
                                 fuzzyMatcher_->getStats());
        }

        return stats;
    }

private:
    /// Repository for data access
    std::shared_ptr<CelestialRepository> repository_;

    /// Trie index for autocomplete
    std::shared_ptr<index::TrieIndex> trieIndex_;

    /// Spatial index for coordinate-based search
    std::shared_ptr<index::SpatialIndex> spatialIndex_;

    /// Fuzzy matcher for approximate string matching
    std::shared_ptr<index::FuzzyMatcher> fuzzyMatcher_;

    /// Initialization flag
    bool isInitialized_;

    /// Count of indexed objects
    size_t objectCount_ = 0;

    /// Mutable to allow lock in const methods
    mutable std::shared_mutex indexMutex_;
};

// ============================================================================
// SearchEngine Public Implementation
// ============================================================================

SearchEngine::SearchEngine(std::shared_ptr<CelestialRepository> repository,
                           std::shared_ptr<index::TrieIndex> trieIndex,
                           std::shared_ptr<index::SpatialIndex> spatialIndex,
                           std::shared_ptr<index::FuzzyMatcher> fuzzyMatcher)
    : pImpl_(std::make_unique<Impl>(std::move(repository), std::move(trieIndex),
                                    std::move(spatialIndex),
                                    std::move(fuzzyMatcher))) {
    spdlog::debug("SearchEngine created");
}

SearchEngine::~SearchEngine() { spdlog::debug("SearchEngine destroyed"); }

SearchEngine::SearchEngine(SearchEngine&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {
    spdlog::debug("SearchEngine moved");
}

SearchEngine& SearchEngine::operator=(SearchEngine&& other) noexcept {
    pImpl_ = std::move(other.pImpl_);
    spdlog::debug("SearchEngine move-assigned");
    return *this;
}

auto SearchEngine::initialize() -> atom::type::Expected<void, std::string> {
    return pImpl_->initialize();
}

bool SearchEngine::isInitialized() const { return pImpl_->isInitialized(); }

auto SearchEngine::search(const std::string& query,
                          const SearchOptions& options)
    -> std::vector<CelestialObjectModel> {
    return pImpl_->search(query, options);
}

auto SearchEngine::exactSearch(const std::string& query, int limit)
    -> std::vector<CelestialObjectModel> {
    auto results = pImpl_->exactSearchImpl(query);
    if (static_cast<int>(results.size()) > limit) {
        results.resize(limit);
    }
    return results;
}

auto SearchEngine::fuzzySearch(const std::string& query, int tolerance,
                               int limit) -> std::vector<CelestialObjectModel> {
    auto results = pImpl_->fuzzySearchImpl(query, tolerance);
    if (static_cast<int>(results.size()) > limit) {
        results.resize(limit);
    }
    return results;
}

auto SearchEngine::searchByCoordinates(double ra, double dec, double radius,
                                       int limit)
    -> std::vector<CelestialObjectModel> {
    return pImpl_->searchByCoordinatesImpl(ra, dec, radius, limit);
}

auto SearchEngine::autocomplete(const std::string& prefix, int limit)
    -> std::vector<std::string> {
    return pImpl_->autocompleteImpl(prefix, limit);
}

auto SearchEngine::advancedSearch(const CelestialSearchFilter& filter)
    -> std::vector<CelestialObjectModel> {
    return pImpl_->advancedSearchImpl(filter);
}

auto SearchEngine::rebuildIndexes() -> atom::type::Expected<void, std::string> {
    return pImpl_->rebuildIndexes();
}

void SearchEngine::clearIndexes() { pImpl_->clearIndexes(); }

auto SearchEngine::getStats() const -> std::string {
    return pImpl_->getStats();
}

}  // namespace lithium::target::search
