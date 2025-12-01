// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_CELESTIAL_REPOSITORY_HPP
#define LITHIUM_TARGET_CELESTIAL_REPOSITORY_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "celestial_model.hpp"
#include "database/database.hpp"

namespace lithium::target {

/**
 * @brief Search filter criteria for celestial objects
 */
struct CelestialSearchFilter {
    std::string namePattern;
    std::string type;
    std::string morphology;
    std::string constellation;
    double minMagnitude = -30.0;
    double maxMagnitude = 30.0;
    double minRA = 0.0;
    double maxRA = 360.0;
    double minDec = -90.0;
    double maxDec = 90.0;
    int limit = 100;
    int offset = 0;
    std::string orderBy = "identifier";
    bool ascending = true;
};

/**
 * @brief Import/Export options
 */
struct ImportExportOptions {
    bool includeAliases = true;
    bool includeRatings = false;
    bool includeHistory = false;
    std::string delimiter = ",";
    bool hasHeader = true;
};

/**
 * @brief Import result statistics
 */
struct ImportResult {
    int totalRecords = 0;
    int successCount = 0;
    int errorCount = 0;
    int duplicateCount = 0;
    std::vector<std::string> errors;
};

/**
 * @brief Repository for celestial object database operations
 *
 * Provides high-level database operations for celestial objects with
 * caching, batch operations, and advanced search capabilities.
 */
class CelestialRepository {
public:
    /**
     * @brief Construct repository with database connection
     * @param dbPath Path to SQLite database file
     */
    explicit CelestialRepository(const std::string& dbPath);

    /**
     * @brief Construct repository with existing database
     * @param db Shared pointer to database instance
     */
    explicit CelestialRepository(
        std::shared_ptr<database::core::Database> db);

    ~CelestialRepository();

    // Prevent copying
    CelestialRepository(const CelestialRepository&) = delete;
    CelestialRepository& operator=(const CelestialRepository&) = delete;

    // Allow moving
    CelestialRepository(CelestialRepository&&) noexcept;
    CelestialRepository& operator=(CelestialRepository&&) noexcept;

    /**
     * @brief Initialize database schema
     * @return true if successful
     */
    bool initializeSchema();

    // ==================== CRUD Operations ====================

    /**
     * @brief Insert a celestial object
     * @param obj Object to insert
     * @return ID of inserted object, or -1 on failure
     */
    int64_t insert(const CelestialObjectModel& obj);

    /**
     * @brief Update a celestial object
     * @param obj Object with updated values
     * @return true if successful
     */
    bool update(const CelestialObjectModel& obj);

    /**
     * @brief Delete a celestial object by ID
     * @param id Object ID
     * @return true if successful
     */
    bool remove(int64_t id);

    /**
     * @brief Find object by ID
     * @param id Object ID
     * @return Object if found
     */
    std::optional<CelestialObjectModel> findById(int64_t id);

    /**
     * @brief Find object by identifier (e.g., "M31", "NGC 224")
     * @param identifier Object identifier
     * @return Object if found
     */
    std::optional<CelestialObjectModel> findByIdentifier(
        const std::string& identifier);

    // ==================== Search Operations ====================

    /**
     * @brief Search objects by name pattern (supports wildcards)
     * @param pattern Search pattern (e.g., "M*", "%Andromeda%")
     * @param limit Maximum results
     * @return Matching objects
     */
    std::vector<CelestialObjectModel> searchByName(
        const std::string& pattern, int limit = 100);

    /**
     * @brief Fuzzy search by name using Levenshtein distance
     * @param name Name to search
     * @param tolerance Maximum edit distance
     * @param limit Maximum results
     * @return Matching objects with scores
     */
    std::vector<std::pair<CelestialObjectModel, int>> fuzzySearch(
        const std::string& name, int tolerance = 2, int limit = 50);

    /**
     * @brief Search with complex filter criteria
     * @param filter Search filter
     * @return Matching objects
     */
    std::vector<CelestialObjectModel> search(const CelestialSearchFilter& filter);

    /**
     * @brief Get autocomplete suggestions for prefix
     * @param prefix Name prefix
     * @param limit Maximum suggestions
     * @return List of matching names
     */
    std::vector<std::string> autocomplete(
        const std::string& prefix, int limit = 10);

    /**
     * @brief Search by celestial coordinates
     * @param ra Right ascension (degrees)
     * @param dec Declination (degrees)
     * @param radius Search radius (degrees)
     * @param limit Maximum results
     * @return Objects within radius
     */
    std::vector<CelestialObjectModel> searchByCoordinates(
        double ra, double dec, double radius, int limit = 50);

    /**
     * @brief Get objects by type
     * @param type Object type
     * @param limit Maximum results
     * @return Matching objects
     */
    std::vector<CelestialObjectModel> getByType(
        const std::string& type, int limit = 100);

    /**
     * @brief Get objects by magnitude range
     * @param minMag Minimum magnitude
     * @param maxMag Maximum magnitude
     * @param limit Maximum results
     * @return Matching objects
     */
    std::vector<CelestialObjectModel> getByMagnitudeRange(
        double minMag, double maxMag, int limit = 100);

    // ==================== Batch Operations ====================

    /**
     * @brief Batch insert objects
     * @param objects Objects to insert
     * @param chunkSize Transaction chunk size
     * @return Number of successfully inserted objects
     */
    int batchInsert(const std::vector<CelestialObjectModel>& objects,
                    size_t chunkSize = 100);

    /**
     * @brief Batch update objects
     * @param objects Objects to update
     * @param chunkSize Transaction chunk size
     * @return Number of successfully updated objects
     */
    int batchUpdate(const std::vector<CelestialObjectModel>& objects,
                    size_t chunkSize = 100);

    /**
     * @brief Upsert (insert or update) objects
     * @param objects Objects to upsert
     * @return Number of affected objects
     */
    int upsert(const std::vector<CelestialObjectModel>& objects);

    // ==================== Import/Export ====================

    /**
     * @brief Import objects from JSON file
     * @param filename Path to JSON file
     * @param options Import options
     * @return Import result statistics
     */
    ImportResult importFromJson(const std::string& filename,
                                const ImportExportOptions& options = {});

    /**
     * @brief Import objects from CSV file
     * @param filename Path to CSV file
     * @param options Import options
     * @return Import result statistics
     */
    ImportResult importFromCsv(const std::string& filename,
                               const ImportExportOptions& options = {});

    /**
     * @brief Export objects to JSON file
     * @param filename Output file path
     * @param filter Optional filter for export
     * @param options Export options
     * @return Number of exported objects
     */
    int exportToJson(const std::string& filename,
                     const CelestialSearchFilter& filter = {},
                     const ImportExportOptions& options = {});

    /**
     * @brief Export objects to CSV file
     * @param filename Output file path
     * @param filter Optional filter for export
     * @param options Export options
     * @return Number of exported objects
     */
    int exportToCsv(const std::string& filename,
                    const CelestialSearchFilter& filter = {},
                    const ImportExportOptions& options = {});

    // ==================== User Ratings ====================

    /**
     * @brief Add or update user rating
     * @param userId User identifier
     * @param objectId Object identifier
     * @param rating Rating value (0.0 - 5.0)
     * @return true if successful
     */
    bool addRating(const std::string& userId, const std::string& objectId,
                   double rating);

    /**
     * @brief Get user ratings
     * @param userId User identifier
     * @param limit Maximum results
     * @return List of ratings
     */
    std::vector<UserRatingModel> getUserRatings(
        const std::string& userId, int limit = 100);

    /**
     * @brief Get average rating for object
     * @param objectId Object identifier
     * @return Average rating, or nullopt if no ratings
     */
    std::optional<double> getAverageRating(const std::string& objectId);

    // ==================== Search History ====================

    /**
     * @brief Record search query
     * @param userId User identifier
     * @param query Search query
     * @param searchType Type of search
     * @param resultCount Number of results
     */
    void recordSearch(const std::string& userId, const std::string& query,
                      const std::string& searchType, int resultCount);

    /**
     * @brief Get user search history
     * @param userId User identifier
     * @param limit Maximum results
     * @return Search history entries
     */
    std::vector<SearchHistoryModel> getSearchHistory(
        const std::string& userId, int limit = 50);

    /**
     * @brief Get popular searches
     * @param limit Maximum results
     * @return Popular search queries with counts
     */
    std::vector<std::pair<std::string, int>> getPopularSearches(int limit = 20);

    // ==================== Statistics & Maintenance ====================

    /**
     * @brief Get total object count
     * @return Number of objects in database
     */
    int64_t count();

    /**
     * @brief Get count by type
     * @return Map of type to count
     */
    std::unordered_map<std::string, int64_t> countByType();

    /**
     * @brief Increment click count for object
     * @param identifier Object identifier
     * @return true if successful
     */
    bool incrementClickCount(const std::string& identifier);

    /**
     * @brief Get most popular objects
     * @param limit Maximum results
     * @return Objects sorted by click count
     */
    std::vector<CelestialObjectModel> getMostPopular(int limit = 20);

    /**
     * @brief Optimize database (VACUUM, ANALYZE)
     */
    void optimize();

    /**
     * @brief Create indexes for better search performance
     */
    void createIndexes();

    /**
     * @brief Clear all data
     * @param includeHistory Also clear history and ratings
     */
    void clearAll(bool includeHistory = false);

    /**
     * @brief Get database statistics
     * @return JSON string with statistics
     */
    std::string getStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target

#endif  // LITHIUM_TARGET_CELESTIAL_REPOSITORY_HPP
