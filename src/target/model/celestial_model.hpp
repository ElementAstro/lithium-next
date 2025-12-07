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

#ifndef LITHIUM_TARGET_CELESTIAL_MODEL_HPP
#define LITHIUM_TARGET_CELESTIAL_MODEL_HPP

#include <memory>
#include <string>
#include <vector>

#include "database/orm/column.hpp"
#include "database/orm/column_base.hpp"

namespace lithium::target {

/**
 * @brief Database model for celestial objects
 *
 * This model represents a celestial object stored in the SQLite database,
 * providing ORM mapping for efficient database operations.
 */
struct CelestialObjectModel {
    int64_t id = 0;
    std::string identifier;
    std::string mIdentifier;
    std::string extensionName;
    std::string component;
    std::string className;
    int amateurRank = 0;
    std::string chineseName;
    std::string type;
    std::string duplicateType;
    std::string morphology;
    std::string constellationZh;
    std::string constellationEn;
    std::string raJ2000;
    double radJ2000 = 0.0;
    std::string decJ2000;
    double decDJ2000 = 0.0;
    double visualMagnitudeV = 0.0;
    double photographicMagnitudeB = 0.0;
    double bMinusV = 0.0;
    double surfaceBrightness = 0.0;
    double majorAxis = 0.0;
    double minorAxis = 0.0;
    double positionAngle = 0.0;
    std::string detailedDescription;
    std::string briefDescription;
    std::string aliases;  // Comma-separated aliases
    int clickCount = 0;

    /**
     * @brief Get the table name for ORM
     */
    static std::string tableName() { return "celestial_objects"; }

    /**
     * @brief Get column definitions for ORM
     */
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();
};

/**
 * @brief Database model for user ratings
 */
struct UserRatingModel {
    int64_t id = 0;
    std::string userId;
    std::string objectId;
    double rating = 0.0;
    int64_t timestamp = 0;

    static std::string tableName() { return "user_ratings"; }
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();
};

/**
 * @brief Database model for search history
 */
struct SearchHistoryModel {
    int64_t id = 0;
    std::string userId;
    std::string query;
    std::string searchType;
    int64_t timestamp = 0;
    int resultCount = 0;

    static std::string tableName() { return "search_history"; }
    static std::vector<std::shared_ptr<database::orm::ColumnBase>> columns();
};

}  // namespace lithium::target

#endif  // LITHIUM_TARGET_CELESTIAL_MODEL_HPP
