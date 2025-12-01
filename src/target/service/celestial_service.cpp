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

#include "celestial_service.hpp"

#include <algorithm>
#include <chrono>
#include <set>
#include <shared_mutex>
#include <spdlog/spdlog.h>

#include "../repository/cached_repository.hpp"
#include "../repository/sqlite_repository.hpp"
#include "../search/search_engine.hpp"
#include "../io/csv_handler.hpp"
#include "../io/json_handler.hpp"
#include "../recommendation/recommendation_engine.hpp"
#include "../observability/visibility_calculator.hpp"

namespace lithium::target::service {

/**
 * @brief Implementation class for CelestialService using PIMPL pattern
 *
 * Encapsulates all internal state and implementation details,
 * providing thread-safety through mutex synchronization.
 */
class CelestialService::Impl {
public:
    ServiceConfig config;
    std::shared_ptr<repository::ICelestialRepository> repository;
    std::shared_ptr<search::SearchEngine> searchEngine;
    std::shared_ptr<recommendation::IRecommendationEngine> recommender;
    std::shared_ptr<observability::VisibilityCalculator> visibilityCalculator;
    std::shared_ptr<online::OnlineSearchService> onlineService_;
    std::shared_ptr<online::ResultMerger> resultMerger_;
    bool onlineSearchEnabled_ = false;

    // Statistics tracking
    ServiceStats stats;
    std::chrono::steady_clock::time_point lastStatsUpdate;

    // Thread safety
    mutable std::shared_mutex statsMutex;
    mutable std::shared_mutex repositoryMutex;
    mutable std::shared_mutex searchMutex;
    mutable std::shared_mutex recommenderMutex;
    mutable std::shared_mutex visibilityMutex;
    mutable std::shared_mutex onlineServiceMutex;

    // Search operation timing (circular buffer for averaging)
    static constexpr size_t TIMING_HISTORY_SIZE = 100;
    std::vector<std::chrono::milliseconds> searchTimings;
    std::vector<std::chrono::milliseconds> recommendationTimings;
    size_t searchTimingIndex = 0;
    size_t recommendationTimingIndex = 0;

    bool initialized = false;

    explicit Impl(const ServiceConfig& cfg)
        : config(cfg),
          lastStatsUpdate(std::chrono::steady_clock::now()) {
        searchTimings.resize(TIMING_HISTORY_SIZE, std::chrono::milliseconds(0));
        recommendationTimings.resize(TIMING_HISTORY_SIZE,
                                     std::chrono::milliseconds(0));
    }

    void recordSearchTiming(std::chrono::milliseconds duration) {
        searchTimings[searchTimingIndex] = duration;
        searchTimingIndex = (searchTimingIndex + 1) % TIMING_HISTORY_SIZE;

        // Calculate average
        std::chrono::milliseconds sum(0);
        for (const auto& t : searchTimings) {
            sum += t;
        }
        stats.avgSearchTime = sum / static_cast<int>(TIMING_HISTORY_SIZE);
    }

    void recordRecommendationTiming(std::chrono::milliseconds duration) {
        recommendationTimings[recommendationTimingIndex] = duration;
        recommendationTimingIndex =
            (recommendationTimingIndex + 1) % TIMING_HISTORY_SIZE;

        // Calculate average
        std::chrono::milliseconds sum(0);
        for (const auto& t : recommendationTimings) {
            sum += t;
        }
        stats.avgRecommendationTime =
            sum / static_cast<int>(TIMING_HISTORY_SIZE);
    }
};

// ============================================================================
// Constructors and Destructors
// ============================================================================

CelestialService::CelestialService(const ServiceConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {
    SPDLOG_INFO("CelestialService constructed with database: {}",
                config.databasePath);
}

CelestialService::~CelestialService() {
    SPDLOG_DEBUG("CelestialService destructor");
}

CelestialService::CelestialService(CelestialService&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {}

CelestialService& CelestialService::operator=(
    CelestialService&& other) noexcept {
    pImpl_ = std::move(other.pImpl_);
    return *this;
}

// ============================================================================
// Initialization
// ============================================================================

auto CelestialService::initialize()
    -> atom::type::Expected<void, std::string> {
    try {
        SPDLOG_INFO("Initializing CelestialService");

        // Create repository
        pImpl_->repository =
            repository::RepositoryFactory::createSqliteRepository(
                pImpl_->config.databasePath);

        if (!pImpl_->repository) {
            return atom::type::Unexpected(
                "Failed to create SQLite repository");
        }

        // Wrap with cache if specified
        if (pImpl_->config.cacheSize > 0) {
            pImpl_->repository =
                repository::RepositoryFactory::createCachedRepository(
                    std::move(pImpl_->repository), pImpl_->config.cacheSize);
            SPDLOG_DEBUG("Created cached repository with size: {}",
                         pImpl_->config.cacheSize);
        }

        // Create search engine
        pImpl_->searchEngine =
            std::make_shared<search::SearchEngine>(pImpl_->repository);

        if (!pImpl_->searchEngine) {
            return atom::type::Unexpected("Failed to create search engine");
        }

        // Initialize search engine
        if (auto result = pImpl_->searchEngine->initialize()) {
            SPDLOG_INFO("Search engine initialized successfully");
        } else {
            return atom::type::Unexpected(
                "Failed to initialize search engine");
        }

        // Create recommendation engine if enabled
        if (pImpl_->config.enableRecommendations) {
            try {
                pImpl_->recommender =
                    recommendation::createRecommendationEngine("hybrid");
                SPDLOG_INFO(
                    "Recommendation engine initialized (hybrid strategy)");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize recommendation engine: {}",
                            e.what());
                pImpl_->recommender = nullptr;
            }
        }

        // Create visibility calculator if enabled
        if (pImpl_->config.enableObservability &&
            pImpl_->config.observerLocation) {
            try {
                pImpl_->visibilityCalculator =
                    std::make_shared<observability::VisibilityCalculator>(
                        *pImpl_->config.observerLocation);

                if (!pImpl_->config.observerTimezone.empty()) {
                    pImpl_->visibilityCalculator->setTimezone(
                        pImpl_->config.observerTimezone);
                }

                SPDLOG_INFO("Visibility calculator initialized");
            } catch (const std::exception& e) {
                SPDLOG_WARN("Failed to initialize visibility calculator: {}",
                            e.what());
                pImpl_->visibilityCalculator = nullptr;
            }
        }

        // Update statistics
        {
            std::unique_lock<std::shared_mutex> lock(pImpl_->statsMutex);
            pImpl_->stats.totalObjects = pImpl_->repository->count();
            pImpl_->stats.initialized = true;
            pImpl_->initialized = true;
            pImpl_->stats.lastUpdate = std::chrono::system_clock::now();
        }

        SPDLOG_INFO("CelestialService initialized successfully with {} objects",
                    pImpl_->stats.totalObjects);

        return {};
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during initialization: {}", e.what());
        return atom::type::Unexpected(
            std::string("Initialization failed: ") + e.what());
    }
}

bool CelestialService::isInitialized() const {
    std::shared_lock<std::shared_mutex> lock(pImpl_->statsMutex);
    return pImpl_->initialized;
}

// ============================================================================
// Search Operations
// ============================================================================

auto CelestialService::search(const std::string& query, int limit)
    -> std::vector<model::ScoredSearchResult> {
    auto startTime = std::chrono::steady_clock::now();

    {
        std::shared_lock<std::shared_mutex> lock(pImpl_->searchMutex);

        if (!pImpl_->searchEngine) {
            SPDLOG_WARN("Search engine not available");
            return {};
        }

        try {
            auto results = pImpl_->searchEngine->search(
                query, search::SearchOptions{.maxResults = limit});

            // Convert results to ScoredSearchResult
            std::vector<model::ScoredSearchResult> scored;
            for (const auto& obj : results) {
                model::ScoredSearchResult result;
                result.matchType = model::MatchType::Exact;
                result.relevanceScore = 1.0;
                result.editDistance = 0;
                result.coordinateDistance = 0.0;
                result.isComplete = true;
                scored.push_back(result);
            }

            // Record timing
            auto duration = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);

            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                pImpl_->stats.searchCount++;
                pImpl_->recordSearchTiming(duration);
            }

            SPDLOG_DEBUG("Search for '{}' returned {} results in {}ms", query,
                         scored.size(), duration.count());

            return scored;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Search error: {}", e.what());
            return {};
        }
    }
}

auto CelestialService::searchByCoordinates(double ra, double dec,
                                           double radius, int limit)
    -> std::vector<model::ScoredSearchResult> {
    auto startTime = std::chrono::steady_clock::now();

    {
        std::shared_lock<std::shared_mutex> lock(pImpl_->searchMutex);

        if (!pImpl_->searchEngine) {
            SPDLOG_WARN("Search engine not available");
            return {};
        }

        try {
            auto results = pImpl_->searchEngine->searchByCoordinates(
                ra, dec, radius, limit);

            // Convert to ScoredSearchResult
            std::vector<model::ScoredSearchResult> scored;
            for (const auto& obj : results) {
                model::ScoredSearchResult result;
                result.matchType = model::MatchType::Coordinate;
                result.relevanceScore = 1.0;
                result.editDistance = 0;
                result.coordinateDistance = 0.0;
                result.isComplete = true;
                scored.push_back(result);
            }

            auto duration = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);

            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                pImpl_->stats.searchCount++;
                pImpl_->recordSearchTiming(duration);
            }

            SPDLOG_DEBUG("Coordinate search (RA: {}, Dec: {}, R: {}) returned "
                         "{} results",
                         ra, dec, radius, scored.size());

            return scored;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Coordinate search error: {}", e.what());
            return {};
        }
    }
}

auto CelestialService::autocomplete(const std::string& prefix, int limit)
    -> std::vector<std::string> {
    {
        std::shared_lock<std::shared_mutex> lock(pImpl_->searchMutex);

        if (!pImpl_->searchEngine) {
            SPDLOG_WARN("Search engine not available");
            return {};
        }

        try {
            auto results = pImpl_->searchEngine->autocomplete(prefix, limit);

            SPDLOG_DEBUG("Autocomplete for '{}' returned {} suggestions",
                         prefix, results.size());

            return results;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Autocomplete error: {}", e.what());
            return {};
        }
    }
}

auto CelestialService::advancedSearch(
    const model::CelestialSearchFilter& filter)
    -> std::vector<model::CelestialObjectModel> {
    auto startTime = std::chrono::steady_clock::now();

    {
        std::shared_lock<std::shared_mutex> lock(pImpl_->searchMutex);

        if (!pImpl_->searchEngine) {
            SPDLOG_WARN("Search engine not available");
            return {};
        }

        try {
            auto results = pImpl_->searchEngine->advancedSearch(filter);

            auto duration = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);

            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                pImpl_->stats.searchCount++;
                pImpl_->recordSearchTiming(duration);
            }

            SPDLOG_DEBUG("Advanced search returned {} results",
                         results.size());

            return results;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Advanced search error: {}", e.what());
            return {};
        }
    }
}

// ============================================================================
// Single Object Operations
// ============================================================================

auto CelestialService::getObject(const std::string& identifier)
    -> std::optional<model::CelestialObjectModel> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

    if (!pImpl_->repository) {
        return std::nullopt;
    }

    try {
        return pImpl_->repository->findByIdentifier(identifier);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting object by identifier: {}", e.what());
        return std::nullopt;
    }
}

auto CelestialService::getObjectById(int64_t id)
    -> std::optional<model::CelestialObjectModel> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

    if (!pImpl_->repository) {
        return std::nullopt;
    }

    try {
        return pImpl_->repository->findById(id);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting object by ID: {}", e.what());
        return std::nullopt;
    }
}

auto CelestialService::addObject(const model::CelestialObjectModel& obj)
    -> atom::type::Expected<int64_t, std::string> {
    std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

    if (!pImpl_->repository) {
        return atom::type::Unexpected("Repository not available");
    }

    try {
        auto result = pImpl_->repository->insert(obj);

        if (result) {
            SPDLOG_DEBUG("Added object with ID: {}", result.value());

            // Update total count
            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                pImpl_->stats.totalObjects++;
            }
        }

        return result;
    } catch (const std::exception& e) {
        return atom::type::Unexpected(
            std::string("Failed to add object: ") + e.what());
    }
}

auto CelestialService::updateObject(const model::CelestialObjectModel& obj)
    -> atom::type::Expected<void, std::string> {
    std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

    if (!pImpl_->repository) {
        return atom::type::Unexpected("Repository not available");
    }

    try {
        return pImpl_->repository->update(obj);
    } catch (const std::exception& e) {
        return atom::type::Unexpected(
            std::string("Failed to update object: ") + e.what());
    }
}

auto CelestialService::removeObject(int64_t id) -> bool {
    std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

    if (!pImpl_->repository) {
        return false;
    }

    try {
        bool result = pImpl_->repository->remove(id);

        if (result) {
            SPDLOG_DEBUG("Removed object with ID: {}", id);

            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                if (pImpl_->stats.totalObjects > 0) {
                    pImpl_->stats.totalObjects--;
                }
            }
        }

        return result;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error removing object: {}", e.what());
        return false;
    }
}

// ============================================================================
// Observability
// ============================================================================

auto CelestialService::getObservableNow(int limit) -> std::vector<
    std::pair<model::CelestialObjectModel,
              observability::ObservabilityWindow>> {
    if (!pImpl_->visibilityCalculator) {
        SPDLOG_WARN("Visibility calculator not available");
        return {};
    }

    try {
        // Get all objects from repository
        std::shared_lock<std::shared_mutex> repoLock(
            pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return {};
        }

        // Get observable objects
        std::shared_lock<std::shared_mutex> visLock(
            pImpl_->visibilityMutex);

        // Note: For a full implementation, we would fetch objects from
        // repository and filter. This is a simplified version.
        return {};
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting observable objects: {}", e.what());
        return {};
    }
}

auto CelestialService::getObservableInWindow(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end, int limit)
    -> std::vector<std::pair<model::CelestialObjectModel,
                              observability::ObservabilityWindow>> {
    if (!pImpl_->visibilityCalculator) {
        SPDLOG_WARN("Visibility calculator not available");
        return {};
    }

    try {
        // Similar implementation to getObservableNow
        return {};
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting observable objects in window: {}",
                     e.what());
        return {};
    }
}

auto CelestialService::calculateVisibility(const std::string& identifier)
    -> std::optional<observability::ObservabilityWindow> {
    if (!pImpl_->visibilityCalculator) {
        return std::nullopt;
    }

    try {
        // Get object first
        auto obj = getObject(identifier);
        if (!obj) {
            return std::nullopt;
        }

        std::shared_lock<std::shared_mutex> lock(pImpl_->visibilityMutex);

        auto window = pImpl_->visibilityCalculator->calculateWindow(
            obj->coordinates.raDecimal, obj->coordinates.decDecimal,
            std::chrono::system_clock::now());

        return window;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error calculating visibility: {}", e.what());
        return std::nullopt;
    }
}

void CelestialService::setObserverLocation(
    const observability::ObserverLocation& location) {
    pImpl_->config.observerLocation = location;

    if (pImpl_->visibilityCalculator) {
        std::unique_lock<std::shared_mutex> lock(pImpl_->visibilityMutex);
        pImpl_->visibilityCalculator->setLocation(location);
        SPDLOG_INFO("Observer location updated");
    }
}

void CelestialService::setObserverTimezone(const std::string& timezone) {
    pImpl_->config.observerTimezone = timezone;

    if (pImpl_->visibilityCalculator) {
        std::unique_lock<std::shared_mutex> lock(pImpl_->visibilityMutex);
        pImpl_->visibilityCalculator->setTimezone(timezone);
        SPDLOG_INFO("Observer timezone set to: {}", timezone);
    }
}

// ============================================================================
// Recommendations
// ============================================================================

void CelestialService::addUserRating(const std::string& userId,
                                     const std::string& objectId,
                                     double rating) {
    if (!pImpl_->recommender) {
        SPDLOG_WARN("Recommendation engine not available");
        return;
    }

    try {
        std::unique_lock<std::shared_mutex> lock(pImpl_->recommenderMutex);
        pImpl_->recommender->addRating(userId, objectId, rating);
        SPDLOG_DEBUG("Added rating: user={}, object={}, rating={}", userId,
                     objectId, rating);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error adding rating: {}", e.what());
    }
}

void CelestialService::addImplicitFeedback(const std::string& userId,
                                          const std::string& objectId) {
    if (!pImpl_->recommender) {
        SPDLOG_WARN("Recommendation engine not available");
        return;
    }

    try {
        std::unique_lock<std::shared_mutex> lock(pImpl_->recommenderMutex);
        pImpl_->recommender->addImplicitFeedback(userId, objectId);
        SPDLOG_DEBUG("Added implicit feedback: user={}, object={}", userId,
                     objectId);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error adding implicit feedback: {}", e.what());
    }
}

auto CelestialService::getRecommendations(const std::string& userId,
                                          int topN)
    -> std::vector<std::pair<model::CelestialObjectModel, double>> {
    if (!pImpl_->recommender) {
        SPDLOG_WARN("Recommendation engine not available");
        return {};
    }

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::shared_lock<std::shared_mutex> lock(pImpl_->recommenderMutex);

        auto recommendations = pImpl_->recommender->recommend(userId, topN);

        // Convert to include CelestialObjectModel
        std::vector<std::pair<model::CelestialObjectModel, double>> results;

        {
            std::shared_lock<std::shared_mutex> repoLock(
                pImpl_->repositoryMutex);

            for (const auto& [objectId, score] : recommendations) {
                if (auto obj = pImpl_->repository->findByIdentifier(objectId)) {
                    results.emplace_back(*obj, score);
                }
            }
        }

        auto duration = std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        {
            std::unique_lock<std::shared_mutex> statsLock(
                pImpl_->statsMutex);
            pImpl_->stats.recommendationCount++;
            pImpl_->recordRecommendationTiming(duration);
        }

        SPDLOG_DEBUG("Generated {} recommendations for user {} in {}ms",
                     results.size(), userId, duration.count());

        return results;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting recommendations: {}", e.what());
        return {};
    }
}

void CelestialService::trainRecommendationModel() {
    if (!pImpl_->recommender) {
        SPDLOG_WARN("Recommendation engine not available");
        return;
    }

    try {
        std::unique_lock<std::shared_mutex> lock(pImpl_->recommenderMutex);
        pImpl_->recommender->train();
        SPDLOG_INFO("Recommendation model trained");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error training recommendation model: {}", e.what());
    }
}

// ============================================================================
// Import/Export
// ============================================================================

auto CelestialService::importFromJson(const std::string& path)
    -> io::ImportResult {
    try {
        io::JsonHandler handler;
        auto result = handler.importCelestialObjects(path);

        if (!result) {
            SPDLOG_ERROR("Failed to import from JSON: {}", path);
            return io::ImportResult{};
        }

        auto [objects, stats] = result.value();

        // Insert objects into repository
        std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return io::ImportResult{};
        }

        int successCount = pImpl_->repository->batchInsert(
            std::span<const model::CelestialObjectModel>(objects.data(),
                                                         objects.size()));

        {
            std::unique_lock<std::shared_mutex> statsLock(
                pImpl_->statsMutex);
            pImpl_->stats.totalObjects = pImpl_->repository->count();
        }

        io::ImportResult importResult;
        importResult.totalRecords = objects.size();
        importResult.successCount = successCount;
        importResult.errorCount = objects.size() - successCount;

        SPDLOG_INFO("Imported {} objects from JSON file: {}", successCount,
                    path);

        return importResult;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during JSON import: {}", e.what());
        return io::ImportResult{};
    }
}

auto CelestialService::importFromCsv(const std::string& path)
    -> io::ImportResult {
    try {
        io::CsvHandler handler;
        auto result = handler.importCelestialObjects(path);

        if (!result) {
            SPDLOG_ERROR("Failed to import from CSV: {}", path);
            return io::ImportResult{};
        }

        auto [objects, stats] = result.value();

        // Insert objects into repository
        std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return io::ImportResult{};
        }

        int successCount = pImpl_->repository->batchInsert(
            std::span<const model::CelestialObjectModel>(objects.data(),
                                                         objects.size()));

        {
            std::unique_lock<std::shared_mutex> statsLock(
                pImpl_->statsMutex);
            pImpl_->stats.totalObjects = pImpl_->repository->count();
        }

        io::ImportResult importResult;
        importResult.totalRecords = objects.size();
        importResult.successCount = successCount;
        importResult.errorCount = objects.size() - successCount;

        SPDLOG_INFO("Imported {} objects from CSV file: {}", successCount,
                    path);

        return importResult;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during CSV import: {}", e.what());
        return io::ImportResult{};
    }
}

auto CelestialService::exportToJson(const std::string& path,
                                    const model::CelestialSearchFilter& filter)
    -> atom::type::Expected<int, std::string> {
    try {
        std::shared_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return atom::type::Unexpected("Repository not available");
        }

        // Search for objects
        auto objects = pImpl_->repository->search(filter);

        if (objects.empty()) {
            SPDLOG_WARN("No objects found to export");
            return 0;
        }

        // Export using JSON handler
        io::JsonHandler handler;
        handler.exportCelestialObjects(path, objects, true, 2);

        SPDLOG_INFO("Exported {} objects to JSON file: {}", objects.size(),
                    path);

        return static_cast<int>(objects.size());
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during JSON export: {}", e.what());
        return atom::type::Unexpected(
            std::string("Export failed: ") + e.what());
    }
}

auto CelestialService::exportToCsv(const std::string& path,
                                   const model::CelestialSearchFilter& filter)
    -> atom::type::Expected<int, std::string> {
    try {
        std::shared_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return atom::type::Unexpected("Repository not available");
        }

        // Search for objects
        auto objects = pImpl_->repository->search(filter);

        if (objects.empty()) {
            SPDLOG_WARN("No objects found to export");
            return 0;
        }

        // Export using CSV handler
        io::CsvHandler handler;
        handler.exportCelestialObjects(path, objects);

        SPDLOG_INFO("Exported {} objects to CSV file: {}", objects.size(),
                    path);

        return static_cast<int>(objects.size());
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during CSV export: {}", e.what());
        return atom::type::Unexpected(
            std::string("Export failed: ") + e.what());
    }
}

// ============================================================================
// Service Management
// ============================================================================

void CelestialService::rebuildIndexes() {
    SPDLOG_INFO("Rebuilding search indexes");

    try {
        std::unique_lock<std::shared_mutex> lock(pImpl_->searchMutex);

        if (pImpl_->searchEngine) {
            if (auto result = pImpl_->searchEngine->rebuildIndexes()) {
                SPDLOG_INFO("Indexes rebuilt successfully");
            } else {
                SPDLOG_ERROR("Failed to rebuild indexes");
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during index rebuild: {}", e.what());
    }
}

void CelestialService::clearCaches() {
    SPDLOG_INFO("Clearing caches");

    try {
        {
            std::unique_lock<std::shared_mutex> repoLock(
                pImpl_->repositoryMutex);
            // Repository cache clearing would be implemented in repository
        }

        {
            std::unique_lock<std::shared_mutex> searchLock(
                pImpl_->searchMutex);
            if (pImpl_->searchEngine) {
                pImpl_->searchEngine->clearIndexes();
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during cache clearing: {}", e.what());
    }
}

void CelestialService::optimize() {
    SPDLOG_INFO("Optimizing service");

    try {
        // Rebuild indexes for optimization
        rebuildIndexes();

        // Repository optimization
        {
            std::unique_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);
            // Would call repository->optimize() if available
        }

        SPDLOG_INFO("Service optimization completed");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during optimization: {}", e.what());
    }
}

auto CelestialService::getStats() const -> ServiceStats {
    std::shared_lock<std::shared_mutex> lock(pImpl_->statsMutex);

    ServiceStats stats = pImpl_->stats;
    stats.lastUpdate = std::chrono::system_clock::now();

    // Update object counts
    if (pImpl_->repository) {
        stats.totalObjects = pImpl_->repository->count();
    }

    return stats;
}

auto CelestialService::getStatsJson() const -> std::string {
    auto stats = getStats();

    json j;
    j["initialized"] = stats.initialized;
    j["totalObjects"] = stats.totalObjects;
    j["cachedObjects"] = stats.cachedObjects;
    j["searchCount"] = stats.searchCount;
    j["recommendationCount"] = stats.recommendationCount;
    j["avgSearchTimeMs"] = stats.avgSearchTime.count();
    j["avgRecommendationTimeMs"] = stats.avgRecommendationTime.count();
    j["lastUpdate"] =
        std::chrono::system_clock::to_time_t(stats.lastUpdate);

    return j.dump(2);
}

// ============================================================================
// Component Access
// ============================================================================

auto CelestialService::getRepository()
    -> std::shared_ptr<repository::ICelestialRepository> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->repositoryMutex);
    return pImpl_->repository;
}

auto CelestialService::getSearchEngine()
    -> std::shared_ptr<search::SearchEngine> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->searchMutex);
    return pImpl_->searchEngine;
}

auto CelestialService::getRecommendationEngine()
    -> std::shared_ptr<recommendation::IRecommendationEngine> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->recommenderMutex);
    return pImpl_->recommender;
}

auto CelestialService::getVisibilityCalculator()
    -> std::shared_ptr<observability::VisibilityCalculator> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->visibilityMutex);
    return pImpl_->visibilityCalculator;
}

// ============================================================================
// Online Search Operations
// ============================================================================

void CelestialService::enableOnlineSearch(
    const online::OnlineSearchConfig& config) {
    try {
        std::unique_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);

        // Create online search service
        pImpl_->onlineService_ =
            online::OnlineSearchServiceFactory::createService("simbad");

        if (!pImpl_->onlineService_) {
            SPDLOG_ERROR("Failed to create online search service");
            return;
        }

        // Initialize service with config
        if (auto result = pImpl_->onlineService_->initialize(config)) {
            pImpl_->onlineSearchEnabled_ = true;
            SPDLOG_INFO("Online search enabled with timeout: {}ms",
                        config.timeoutMs);
        } else {
            SPDLOG_ERROR("Failed to initialize online search service");
            pImpl_->onlineService_ = nullptr;
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception enabling online search: {}", e.what());
        pImpl_->onlineSearchEnabled_ = false;
        pImpl_->onlineService_ = nullptr;
    }
}

void CelestialService::disableOnlineSearch() {
    std::unique_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);
    pImpl_->onlineService_ = nullptr;
    pImpl_->onlineSearchEnabled_ = false;
    SPDLOG_INFO("Online search disabled");
}

bool CelestialService::isOnlineSearchEnabled() const {
    std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);
    return pImpl_->onlineSearchEnabled_ && pImpl_->onlineService_ != nullptr;
}

auto CelestialService::searchOnline(const std::string& query, int limit)
    -> std::vector<model::ScoredSearchResult> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);

    if (!isOnlineSearchEnabled()) {
        SPDLOG_WARN("Online search not enabled");
        return {};
    }

    try {
        auto results = pImpl_->onlineService_->searchByName(query, limit);

        // Convert string identifiers to ScoredSearchResult
        std::vector<model::ScoredSearchResult> scored;
        for (const auto& identifier : results) {
            model::ScoredSearchResult result;
            result.matchType = model::MatchType::Exact;
            result.relevanceScore = 1.0;
            result.editDistance = 0;
            result.coordinateDistance = 0.0;
            result.isComplete = false;  // Online results may be incomplete
            result.metadata = "source:online";
            scored.push_back(result);
        }

        SPDLOG_DEBUG("Online search for '{}' returned {} results", query,
                     scored.size());

        return scored;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Online search error: {}", e.what());
        return {};
    }
}

auto CelestialService::searchHybrid(const std::string& query, int limit)
    -> std::vector<model::ScoredSearchResult> {
    auto startTime = std::chrono::steady_clock::now();

    // First search local database
    auto localResults = search(query, limit);

    // Then search online if enabled
    if (isOnlineSearchEnabled()) {
        try {
            std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);

            auto onlineResults =
                pImpl_->onlineService_->searchByName(query, limit);

            // Merge results: local results take precedence
            std::vector<std::string> localIds;
            for (const auto& result : localResults) {
                if (result.object) {
                    localIds.push_back(result.object->ID);
                }
            }

            // Add online results not in local results
            std::set<std::string> seenIds(localIds.begin(), localIds.end());
            for (const auto& onlineId : onlineResults) {
                if (seenIds.find(onlineId) == seenIds.end()) {
                    model::ScoredSearchResult result;
                    result.matchType = model::MatchType::Filter;
                    result.relevanceScore = 0.8;  // Lower score for online-only
                    result.editDistance = 0;
                    result.coordinateDistance = 0.0;
                    result.isComplete = false;
                    result.metadata = "source:online";
                    localResults.push_back(result);
                    seenIds.insert(onlineId);

                    if (static_cast<int>(localResults.size()) >= limit) {
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Online search in hybrid mode failed: {}", e.what());
            // Continue with local results only
        }
    }

    // Sort by relevance score
    std::sort(localResults.begin(), localResults.end(),
              [](const auto& a, const auto& b) {
                  return a.relevanceScore > b.relevanceScore;
              });

    // Limit results
    if (static_cast<int>(localResults.size()) > limit) {
        localResults.resize(limit);
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    {
        std::unique_lock<std::shared_mutex> statsLock(pImpl_->statsMutex);
        pImpl_->stats.searchCount++;
        pImpl_->recordSearchTiming(duration);
    }

    SPDLOG_DEBUG("Hybrid search for '{}' returned {} results in {}ms", query,
                 localResults.size(), duration.count());

    return localResults;
}

auto CelestialService::searchOnlineByCoordinates(double ra, double dec,
                                                 double radiusDeg, int limit)
    -> std::vector<model::ScoredSearchResult> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);

    if (!isOnlineSearchEnabled()) {
        SPDLOG_WARN("Online search not enabled");
        return {};
    }

    try {
        auto results =
            pImpl_->onlineService_->searchByCoordinates(ra, dec, radiusDeg, limit);

        // Convert to ScoredSearchResult
        std::vector<model::ScoredSearchResult> scored;
        for (const auto& identifier : results) {
            model::ScoredSearchResult result;
            result.matchType = model::MatchType::Coordinate;
            result.relevanceScore = 1.0;
            result.editDistance = 0;
            result.coordinateDistance = 0.0;
            result.isComplete = false;
            result.metadata = "source:online";
            scored.push_back(result);
        }

        SPDLOG_DEBUG(
            "Online coordinate search (RA: {}, Dec: {}, R: {}) returned {} "
            "results",
            ra, dec, radiusDeg, scored.size());

        return scored;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Online coordinate search error: {}", e.what());
        return {};
    }
}

auto CelestialService::getOnlineEphemeris(
    const std::string& objectName,
    std::chrono::system_clock::time_point time)
    -> std::optional<online::EphemerisPoint> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);

    if (!isOnlineSearchEnabled()) {
        SPDLOG_WARN("Online search not enabled");
        return std::nullopt;
    }

    try {
        auto ephemeris = pImpl_->onlineService_->getEphemeris(objectName, time);

        if (ephemeris) {
            SPDLOG_DEBUG(
                "Retrieved ephemeris for {} at RA: {}, Dec: {}", objectName,
                ephemeris->ra, ephemeris->dec);
        } else {
            SPDLOG_WARN("No ephemeris found for: {}", objectName);
        }

        return ephemeris;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error getting ephemeris: {}", e.what());
        return std::nullopt;
    }
}

auto CelestialService::importFromOnline(const std::string& identifier)
    -> atom::type::Expected<int64_t, std::string> {
    std::shared_lock<std::shared_mutex> onlineLock(
        pImpl_->onlineServiceMutex);

    if (!isOnlineSearchEnabled()) {
        return atom::type::Unexpected("Online search not enabled");
    }

    try {
        // Get object details from online source
        auto details = pImpl_->onlineService_->getObjectDetails(identifier);

        if (!details) {
            return atom::type::Unexpected(
                std::string("Object not found online: ") + identifier);
        }

        // Create a new celestial object from the details
        // Note: This is a simplified implementation
        // In a real scenario, you would parse the JSON/string and populate
        // a CelestialObjectModel properly
        model::CelestialObjectModel obj;
        obj.mainIdentifier = identifier;
        obj.coordinates = {};

        // Add to local repository
        std::unique_lock<std::shared_mutex> repoLock(
            pImpl_->repositoryMutex);

        if (!pImpl_->repository) {
            return atom::type::Unexpected("Repository not available");
        }

        auto result = pImpl_->repository->insert(obj);

        if (result) {
            SPDLOG_INFO("Imported object from online: {} (ID: {})", identifier,
                        result.value());

            // Update statistics
            {
                std::unique_lock<std::shared_mutex> statsLock(
                    pImpl_->statsMutex);
                pImpl_->stats.totalObjects++;
            }
        }

        return result;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error importing from online: {}", e.what());
        return atom::type::Unexpected(
            std::string("Import failed: ") + e.what());
    }
}

auto CelestialService::getOnlineSearchService()
    -> std::shared_ptr<online::OnlineSearchService> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->onlineServiceMutex);
    return pImpl_->onlineService_;
}

}  // namespace lithium::target::service
