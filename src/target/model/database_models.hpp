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

#ifndef LITHIUM_TARGET_MODEL_DATABASE_MODELS_HPP
#define LITHIUM_TARGET_MODEL_DATABASE_MODELS_HPP

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "database/orm/column.hpp"
#include "database/orm/column_base.hpp"

namespace lithium::target::model {

/**
 * @brief Database model for celestial objects
 *
 * This model represents a celestial object stored in the SQLite database,
 * providing ORM mapping for efficient database operations. It mirrors the
 * in-memory CelestialObject structure with additional database-specific fields.
 */
struct CelestialObjectModel {
    /// Unique database identifier
    int64_t id = 0;

    /// Primary catalog identifier (e.g., "M31", "NGC 224")
    std::string identifier;

    /// Messier catalog identifier
    std::string mIdentifier;

    /// Extended name or alternate designation
    std::string extensionName;

    /// Component information
    std::string component;

    /// Classification name
    std::string className;

    /// Amateur observing difficulty rank (1-10)
    int amateurRank = 0;

    /// Chinese name of the object
    std::string chineseName;

    /// Object type (e.g., "Galaxy", "Nebula", "Star Cluster")
    std::string type;

    /// Type classification including duplicates
    std::string duplicateType;

    /// Morphological classification (e.g., "Spiral", "Elliptical")
    std::string morphology;

    /// Constellation name in Chinese
    std::string constellationZh;

    /// Constellation name in English
    std::string constellationEn;

    /// Right ascension (J2000) in string format (HH:MM:SS)
    std::string raJ2000;

    /// Right ascension (J2000) in decimal degrees (0-360)
    double radJ2000 = 0.0;

    /// Declination (J2000) in string format (DD:MM:SS)
    std::string decJ2000;

    /// Declination (J2000) in decimal degrees (-90 to +90)
    double decDJ2000 = 0.0;

    /// Visual magnitude (V band)
    double visualMagnitudeV = 0.0;

    /// Photographic magnitude (B band)
    double photographicMagnitudeB = 0.0;

    /// B-V color index
    double bMinusV = 0.0;

    /// Surface brightness (mag/arcmin²)
    double surfaceBrightness = 0.0;

    /// Major axis size (arcmin)
    double majorAxis = 0.0;

    /// Minor axis size (arcmin)
    double minorAxis = 0.0;

    /// Position angle (degrees)
    double positionAngle = 0.0;

    /// Detailed object description
    std::string detailedDescription;

    /// Brief object description
    std::string briefDescription;

    /// Comma-separated list of aliases
    std::string aliases;

    /// Click/view count for popularity tracking
    int clickCount = 0;

    /// Timestamp of last update
    int64_t lastUpdated = 0;

    /**
     * @brief Get the table name for ORM
     *
     * @return Table name string
     */
    static std::string tableName() { return "celestial_objects"; }

    /**
     * @brief Get column definitions for ORM
     *
     * @return Vector of column definitions
     */
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();

    /**
     * @brief Check if model has all required fields
     *
     * @return true if identifier, type, and coordinates are set
     */
    [[nodiscard]] auto isComplete() const -> bool {
        return !identifier.empty() && !type.empty() &&
               (radJ2000 > 0.0 || radJ2000 < 0.0) &&
               (decDJ2000 > -90.0 || decDJ2000 < 90.0);
    }

    /**
     * @brief Update timestamp to current time
     */
    void updateTimestamp() {
        lastUpdated = std::time(nullptr);
    }
};

/**
 * @brief Database model for user ratings
 *
 * Stores user-provided ratings for celestial objects to support
 * recommendation and popularity ranking features.
 */
struct UserRatingModel {
    /// Unique database identifier
    int64_t id = 0;

    /// User identifier (username or user ID)
    std::string userId;

    /// Object identifier (celestial object ID)
    std::string objectId;

    /// Rating value (typically 0.0 to 5.0)
    double rating = 0.0;

    /// Timestamp of when the rating was created/updated
    int64_t timestamp = 0;

    /**
     * @brief Get the table name for ORM
     *
     * @return Table name string
     */
    static std::string tableName() { return "user_ratings"; }

    /**
     * @brief Get column definitions for ORM
     *
     * @return Vector of column definitions
     */
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();

    /**
     * @brief Check if rating is valid
     *
     * @return true if rating is between 0 and 5
     */
    [[nodiscard]] auto isValid() const -> bool {
        return !userId.empty() && !objectId.empty() && rating >= 0.0 &&
               rating <= 5.0;
    }

    /**
     * @brief Update timestamp to current time
     */
    void updateTimestamp() {
        timestamp = std::time(nullptr);
    }
};

/**
 * @brief Database model for search history
 *
 * Tracks user search queries to support analytics, popular searches,
 * and recommendation features.
 */
struct SearchHistoryModel {
    /// Unique database identifier
    int64_t id = 0;

    /// User identifier who performed the search
    std::string userId;

    /// Search query string
    std::string query;

    /// Type of search (e.g., "name", "coordinates", "filter")
    std::string searchType;

    /// Timestamp of when the search was performed
    int64_t timestamp = 0;

    /// Number of results returned
    int resultCount = 0;

    /**
     * @brief Get the table name for ORM
     *
     * @return Table name string
     */
    static std::string tableName() { return "search_history"; }

    /**
     * @brief Get column definitions for ORM
     *
     * @return Vector of column definitions
     */
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();

    /**
     * @brief Check if record is valid
     *
     * @return true if required fields are set
     */
    [[nodiscard]] auto isValid() const -> bool {
        return !userId.empty() && !query.empty() && !searchType.empty();
    }

    /**
     * @brief Update timestamp to current time
     */
    void updateTimestamp() {
        timestamp = std::time(nullptr);
    }
};

/**
 * @brief Statistics about a celestial object in the database
 *
 * Aggregated information about object popularity, quality, and usage.
 */
struct CelestialObjectStatistics {
    /// Object identifier
    std::string identifier;

    /// Total number of times viewed/clicked
    int64_t totalViews = 0;

    /// Total number of user ratings
    int64_t totalRatings = 0;

    /// Average user rating (0.0 to 5.0)
    double averageRating = 0.0;

    /// Timestamp of last view
    int64_t lastViewedTime = 0;

    /// Number of times appeared in search results
    int64_t searchResultCount = 0;

    /**
     * @brief Check if statistics are meaningful
     *
     * @return true if object has been viewed at least once
     */
    [[nodiscard]] auto hasMeaningfulData() const -> bool {
        return totalViews > 0 || totalRatings > 0;
    }
};

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_DATABASE_MODELS_HPP
