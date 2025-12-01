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

#ifndef LITHIUM_TARGET_SERVICE_CELESTIAL_SERVICE_HPP
#define LITHIUM_TARGET_SERVICE_CELESTIAL_SERVICE_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"
#include "atom/type/json.hpp"

#include "../celestial_model.hpp"
#include "../io/io.hpp"
#include "../observability/visibility_calculator.hpp"
#include "../online/online.hpp"
#include "../recommendation/recommendation_engine.hpp"
#include "../repository/repository_interface.hpp"
#include "../search/search_engine.hpp"

namespace lithium::target::service {

using json = nlohmann::json;

/**
 * @brief Service configuration for celestial operations
 *
 * Defines optional components and database settings for the service.
 */
struct ServiceConfig {
    /// Path to celestial objects database
    std::string databasePath = "data/celestial.db";

    /// Maximum objects to keep in memory cache (0 = unlimited)
    size_t cacheSize = 1000;

    /// Enable recommendation engine for user preferences
    bool enableRecommendations = true;

    /// Enable spatial indexing for coordinate searches
    bool enableSpatialIndex = true;

    /// Enable observability calculations for visibility
    bool enableObservability = true;

    /// Observer location for observability calculations
    std::optional<observability::ObserverLocation> observerLocation;

    /// Observer timezone for time conversions
    std::string observerTimezone = "UTC";
};

/**
 * @brief Service statistics for monitoring
 *
 * Tracks usage metrics and performance data.
 */
struct ServiceStats {
    /// Total celestial objects in database
    size_t totalObjects = 0;

    /// Objects currently in cache
    size_t cachedObjects = 0;

    /// Total number of search operations
    size_t searchCount = 0;

    /// Total number of recommendation operations
    size_t recommendationCount = 0;

    /// Average search operation time
    std::chrono::milliseconds avgSearchTime{0};

    /// Average recommendation operation time
    std::chrono::milliseconds avgRecommendationTime{0};

    /// Whether the service is initialized
    bool initialized = false;

    /// Timestamp of last statistics update
    std::chrono::system_clock::time_point lastUpdate =
        std::chrono::system_clock::now();
};

/**
 * @brief Unified service facade for celestial object management
 *
 * Provides a comprehensive interface to all target module functionality including:
 * - Search operations (exact, fuzzy, coordinate-based, advanced)
 * - Object management (CRUD operations)
 * - Observability calculations (visibility, rise/set times)
 * - Recommendation engine (user preferences, ratings)
 * - Import/export operations (JSON, CSV)
 * - Performance monitoring and optimization
 *
 * Thread-safe implementation using PIMPL pattern for internal state management.
 *
 * Usage:
 * @code
 * ServiceConfig config;
 * config.databasePath = "celestial.db";
 * config.enableRecommendations = true;
 *
 * CelestialService service(config);
 * if (auto result = service.initialize()) {
 *     // Service ready
 *     auto results = service.search("M31");
 *     auto observable = service.getObservableNow(50);
 * } else {
 *     // Handle initialization error
 * }
 * @endcode
 */
class CelestialService {
public:
    /**
     * @brief Construct service with optional configuration
     *
     * @param config Service configuration (uses defaults if not provided)
     */
    explicit CelestialService(const ServiceConfig& config = ServiceConfig());

    /**
     * @brief Destructor (virtual for interface)
     */
    ~CelestialService();

    // ========================================================================
    // Non-copyable, movable
    // ========================================================================

    CelestialService(const CelestialService&) = delete;
    CelestialService& operator=(const CelestialService&) = delete;

    CelestialService(CelestialService&&) noexcept;
    CelestialService& operator=(CelestialService&&) noexcept;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Initialize service and all components
     *
     * Creates database connection, initializes search indexes,
     * optionally sets up recommendation engine, and loads configuration.
     *
     * @return Expected with void on success or error message on failure
     */
    [[nodiscard]] auto initialize()
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Check if service is initialized and ready
     *
     * @return true if all components are ready for operations
     */
    [[nodiscard]] bool isInitialized() const;

    // ========================================================================
    // Search Operations
    // ========================================================================

    /**
     * @brief General search with multiple strategies
     *
     * Performs exact, fuzzy, and alias matching based on query.
     *
     * @param query Search query string
     * @param limit Maximum number of results
     * @return Vector of search results sorted by relevance
     */
    [[nodiscard]] auto search(const std::string& query, int limit = 50)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Search by celestial coordinates
     *
     * Finds objects within specified radius from RA/Dec coordinates.
     *
     * @param ra Right ascension in degrees (0-360)
     * @param dec Declination in degrees (-90 to +90)
     * @param radius Search radius in degrees
     * @param limit Maximum number of results
     * @return Vector of nearby objects sorted by distance
     */
    [[nodiscard]] auto searchByCoordinates(
        double ra, double dec, double radius, int limit = 50)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Get autocomplete suggestions for prefix
     *
     * Returns object names and aliases starting with given prefix.
     *
     * @param prefix Name prefix to complete
     * @param limit Maximum number of suggestions
     * @return Vector of completion suggestions
     */
    [[nodiscard]] auto autocomplete(const std::string& prefix, int limit = 10)
        -> std::vector<std::string>;

    /**
     * @brief Advanced search with complex filter criteria
     *
     * Applies comprehensive filters for magnitude, size, type, etc.
     *
     * @param filter Search filter with constraints
     * @return Vector of matching celestial objects
     */
    [[nodiscard]] auto advancedSearch(
        const model::CelestialSearchFilter& filter)
        -> std::vector<model::CelestialObjectModel>;

    // ========================================================================
    // Single Object Operations
    // ========================================================================

    /**
     * @brief Get object by identifier (e.g., "M31", "NGC 224")
     *
     * @param identifier Object identifier string
     * @return Object if found, std::nullopt otherwise
     */
    [[nodiscard]] auto getObject(const std::string& identifier)
        -> std::optional<model::CelestialObjectModel>;

    /**
     * @brief Get object by database ID
     *
     * @param id Object database ID
     * @return Object if found, std::nullopt otherwise
     */
    [[nodiscard]] auto getObjectById(int64_t id)
        -> std::optional<model::CelestialObjectModel>;

    /**
     * @brief Add new celestial object
     *
     * @param obj Object to insert
     * @return Expected with ID of inserted object, or error message
     */
    auto addObject(const model::CelestialObjectModel& obj)
        -> atom::type::Expected<int64_t, std::string>;

    /**
     * @brief Update existing celestial object
     *
     * @param obj Object to update (must have valid id)
     * @return Expected with void on success, or error message
     */
    auto updateObject(const model::CelestialObjectModel& obj)
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Remove object by ID
     *
     * @param id Object ID to delete
     * @return true if deleted, false otherwise
     */
    auto removeObject(int64_t id) -> bool;

    // ========================================================================
    // Observability (if enabled)
    // ========================================================================

    /**
     * @brief Get observable objects at current time
     *
     * Returns objects that are currently above the horizon.
     *
     * @param limit Maximum number of results
     * @return Vector of observable objects with visibility windows
     */
    [[nodiscard]] auto getObservableNow(int limit = 50) -> std::vector<
        std::pair<model::CelestialObjectModel,
                  observability::ObservabilityWindow>>;

    /**
     * @brief Get observable objects within time window
     *
     * Returns objects observable within specified time range.
     *
     * @param start Start of observation window
     * @param end End of observation window
     * @param limit Maximum number of results
     * @return Vector of observable objects with visibility windows
     */
    [[nodiscard]] auto getObservableInWindow(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end, int limit = 100)
        -> std::vector<std::pair<model::CelestialObjectModel,
                                  observability::ObservabilityWindow>>;

    /**
     * @brief Calculate visibility window for object
     *
     * @param identifier Object identifier
     * @return Observability window if found and observability enabled
     */
    [[nodiscard]] auto calculateVisibility(const std::string& identifier)
        -> std::optional<observability::ObservabilityWindow>;

    /**
     * @brief Set observer location for observability calculations
     *
     * @param location Observer location with coordinates and elevation
     */
    void setObserverLocation(
        const observability::ObserverLocation& location);

    /**
     * @brief Set observer timezone
     *
     * @param timezone IANA timezone name (e.g., "America/New_York")
     */
    void setObserverTimezone(const std::string& timezone);

    // ========================================================================
    // Recommendations (if enabled)
    // ========================================================================

    /**
     * @brief Add explicit user rating for object
     *
     * @param userId User identifier
     * @param objectId Object identifier
     * @param rating Rating value (typically 0-5)
     */
    void addUserRating(const std::string& userId,
                       const std::string& objectId, double rating);

    /**
     * @brief Record implicit user feedback (view, interaction, etc.)
     *
     * @param userId User identifier
     * @param objectId Object identifier
     */
    void addImplicitFeedback(const std::string& userId,
                             const std::string& objectId);

    /**
     * @brief Get recommendations for user
     *
     * Returns recommended objects based on user's ratings and feedback.
     *
     * @param userId User identifier
     * @param topN Number of recommendations to return
     * @return Vector of (object, score) pairs
     */
    [[nodiscard]] auto getRecommendations(const std::string& userId,
                                          int topN = 10)
        -> std::vector<std::pair<model::CelestialObjectModel, double>>;

    /**
     * @brief Train recommendation model
     *
     * Should be called after adding ratings/feedback for model updates.
     */
    void trainRecommendationModel();

    // ========================================================================
    // Import/Export
    // ========================================================================

    /**
     * @brief Import objects from JSON file
     *
     * @param path Path to JSON file
     * @return Import result with statistics
     */
    [[nodiscard]] auto importFromJson(const std::string& path)
        -> io::ImportResult;

    /**
     * @brief Import objects from CSV file
     *
     * @param path Path to CSV file
     * @return Import result with statistics
     */
    [[nodiscard]] auto importFromCsv(const std::string& path)
        -> io::ImportResult;

    /**
     * @brief Export objects to JSON file
     *
     * @param path Destination file path
     * @param filter Optional filter to select objects to export
     * @return Expected with count of exported objects, or error message
     */
    [[nodiscard]] auto exportToJson(
        const std::string& path,
        const model::CelestialSearchFilter& filter = model::CelestialSearchFilter())
        -> atom::type::Expected<int, std::string>;

    /**
     * @brief Export objects to CSV file
     *
     * @param path Destination file path
     * @param filter Optional filter to select objects to export
     * @return Expected with count of exported objects, or error message
     */
    [[nodiscard]] auto exportToCsv(
        const std::string& path,
        const model::CelestialSearchFilter& filter = model::CelestialSearchFilter())
        -> atom::type::Expected<int, std::string>;

    // ========================================================================
    // Service Management
    // ========================================================================

    /**
     * @brief Rebuild all search indexes
     *
     * Clears and rebuilds indexes from current database state.
     * May block for extended periods on large datasets.
     */
    void rebuildIndexes();

    /**
     * @brief Clear all memory caches
     *
     * Frees cached objects but keeps database intact.
     */
    void clearCaches();

    /**
     * @brief Optimize service performance
     *
     * Performs database optimization, cache tuning, and index cleanup.
     */
    void optimize();

    /**
     * @brief Get service statistics
     *
     * @return ServiceStats structure with current metrics
     */
    [[nodiscard]] auto getStats() const -> ServiceStats;

    /**
     * @brief Get service statistics as JSON
     *
     * @return JSON representation of statistics
     */
    [[nodiscard]] auto getStatsJson() const -> std::string;

    // ========================================================================
    // Component Access (advanced usage)
    // ========================================================================

    /**
     * @brief Get underlying repository for advanced operations
     *
     * For power users who need direct repository access.
     *
     * @return Shared pointer to repository interface
     */
    [[nodiscard]] auto getRepository()
        -> std::shared_ptr<repository::ICelestialRepository>;

    /**
     * @brief Get search engine for advanced search operations
     *
     * @return Shared pointer to search engine
     */
    [[nodiscard]] auto getSearchEngine()
        -> std::shared_ptr<search::SearchEngine>;

    /**
     * @brief Get recommendation engine
     *
     * Returns nullptr if recommendations are disabled.
     *
     * @return Shared pointer to recommendation engine, or nullptr
     */
    [[nodiscard]] auto getRecommendationEngine()
        -> std::shared_ptr<recommendation::IRecommendationEngine>;

    /**
     * @brief Get visibility calculator
     *
     * Returns nullptr if observability is disabled.
     *
     * @return Shared pointer to visibility calculator, or nullptr
     */
    [[nodiscard]] auto getVisibilityCalculator()
        -> std::shared_ptr<observability::VisibilityCalculator>;

    // ========================================================================
    // Online Search Operations (requires enableOnlineSearch)
    // ========================================================================

    /**
     * @brief Enable online search functionality
     *
     * @param config Online search configuration
     */
    void enableOnlineSearch(const online::OnlineSearchConfig& config = {});

    /**
     * @brief Disable online search functionality
     */
    void disableOnlineSearch();

    /**
     * @brief Check if online search is enabled
     */
    [[nodiscard]] bool isOnlineSearchEnabled() const;

    /**
     * @brief Search online databases only
     *
     * @param query Search query string
     * @param limit Maximum number of results
     * @return Vector of search results from online sources
     */
    [[nodiscard]] auto searchOnline(const std::string& query, int limit = 50)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Hybrid search combining local and online results
     *
     * Searches local database first, then online if needed.
     * Results are merged and deduplicated.
     *
     * @param query Search query string
     * @param limit Maximum number of results
     * @return Vector of merged search results
     */
    [[nodiscard]] auto searchHybrid(const std::string& query, int limit = 50)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Search online databases by coordinates
     *
     * @param ra Right ascension in degrees
     * @param dec Declination in degrees
     * @param radiusDeg Search radius in degrees
     * @param limit Maximum number of results
     * @return Vector of search results
     */
    [[nodiscard]] auto searchOnlineByCoordinates(
        double ra, double dec, double radiusDeg, int limit = 100)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Get ephemeris for solar system object
     *
     * @param objectName Name of solar system object (e.g., "Mars", "Jupiter")
     * @param time Time for ephemeris calculation
     * @return Ephemeris point if found
     */
    [[nodiscard]] auto getOnlineEphemeris(
        const std::string& objectName,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
        -> std::optional<online::EphemerisPoint>;

    /**
     * @brief Import object from online database to local
     *
     * @param identifier Object identifier to import
     * @return ID of imported object, or error message
     */
    [[nodiscard]] auto importFromOnline(const std::string& identifier)
        -> atom::type::Expected<int64_t, std::string>;

    /**
     * @brief Get access to online search service
     *
     * @return Shared pointer to online search service, or nullptr if disabled
     */
    [[nodiscard]] auto getOnlineSearchService()
        -> std::shared_ptr<online::OnlineSearchService>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::service

#endif  // LITHIUM_TARGET_SERVICE_CELESTIAL_SERVICE_HPP
