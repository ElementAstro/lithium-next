// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "celestial_model.hpp"

namespace lithium::target {

std::vector<std::shared_ptr<database::orm::ColumnBase>>
CelestialObjectModel::columns() {
    using namespace database::orm;
    return {
        std::make_shared<Column<int64_t, CelestialObjectModel>>(
            "id", &CelestialObjectModel::id,
            "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "identifier", &CelestialObjectModel::identifier, "TEXT NOT NULL"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "m_identifier", &CelestialObjectModel::mIdentifier, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "extension_name", &CelestialObjectModel::extensionName, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "component", &CelestialObjectModel::component, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "class_name", &CelestialObjectModel::className, "TEXT"),
        std::make_shared<Column<int, CelestialObjectModel>>(
            "amateur_rank", &CelestialObjectModel::amateurRank,
            "INTEGER DEFAULT 0"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "chinese_name", &CelestialObjectModel::chineseName, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "type", &CelestialObjectModel::type, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "duplicate_type", &CelestialObjectModel::duplicateType, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "morphology", &CelestialObjectModel::morphology, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "constellation_zh", &CelestialObjectModel::constellationZh, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "constellation_en", &CelestialObjectModel::constellationEn, "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "ra_j2000", &CelestialObjectModel::raJ2000, "TEXT"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "rad_j2000", &CelestialObjectModel::radJ2000, "REAL DEFAULT 0.0"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "dec_j2000", &CelestialObjectModel::decJ2000, "TEXT"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "dec_d_j2000", &CelestialObjectModel::decDJ2000,
            "REAL DEFAULT 0.0"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "visual_magnitude_v", &CelestialObjectModel::visualMagnitudeV,
            "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "photographic_magnitude_b",
            &CelestialObjectModel::photographicMagnitudeB, "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "b_minus_v", &CelestialObjectModel::bMinusV, "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "surface_brightness", &CelestialObjectModel::surfaceBrightness,
            "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "major_axis", &CelestialObjectModel::majorAxis, "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "minor_axis", &CelestialObjectModel::minorAxis, "REAL"),
        std::make_shared<Column<double, CelestialObjectModel>>(
            "position_angle", &CelestialObjectModel::positionAngle, "REAL"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "detailed_description", &CelestialObjectModel::detailedDescription,
            "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "brief_description", &CelestialObjectModel::briefDescription,
            "TEXT"),
        std::make_shared<Column<std::string, CelestialObjectModel>>(
            "aliases", &CelestialObjectModel::aliases, "TEXT"),
        std::make_shared<Column<int, CelestialObjectModel>>(
            "click_count", &CelestialObjectModel::clickCount,
            "INTEGER DEFAULT 0"),
    };
}

std::vector<std::shared_ptr<database::orm::ColumnBase>>
UserRatingModel::columns() {
    using namespace database::orm;
    return {
        std::make_shared<Column<int64_t, UserRatingModel>>(
            "id", &UserRatingModel::id, "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<std::string, UserRatingModel>>(
            "user_id", &UserRatingModel::userId, "TEXT NOT NULL"),
        std::make_shared<Column<std::string, UserRatingModel>>(
            "object_id", &UserRatingModel::objectId, "TEXT NOT NULL"),
        std::make_shared<Column<double, UserRatingModel>>(
            "rating", &UserRatingModel::rating, "REAL NOT NULL"),
        std::make_shared<Column<int64_t, UserRatingModel>>(
            "timestamp", &UserRatingModel::timestamp, "INTEGER NOT NULL"),
    };
}

std::vector<std::shared_ptr<database::orm::ColumnBase>>
SearchHistoryModel::columns() {
    using namespace database::orm;
    return {
        std::make_shared<Column<int64_t, SearchHistoryModel>>(
            "id", &SearchHistoryModel::id, "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<std::string, SearchHistoryModel>>(
            "user_id", &SearchHistoryModel::userId, "TEXT NOT NULL"),
        std::make_shared<Column<std::string, SearchHistoryModel>>(
            "query", &SearchHistoryModel::query, "TEXT NOT NULL"),
        std::make_shared<Column<std::string, SearchHistoryModel>>(
            "search_type", &SearchHistoryModel::searchType, "TEXT NOT NULL"),
        std::make_shared<Column<int64_t, SearchHistoryModel>>(
            "timestamp", &SearchHistoryModel::timestamp, "INTEGER NOT NULL"),
        std::make_shared<Column<int, SearchHistoryModel>>(
            "result_count", &SearchHistoryModel::resultCount,
            "INTEGER DEFAULT 0"),
    };
}

}  // namespace lithium::target
