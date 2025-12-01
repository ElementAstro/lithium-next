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
        std::make_shared<Column<CelestialObjectModel, int64_t>>(
            "id", &CelestialObjectModel::id, "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "identifier", &CelestialObjectModel::identifier, "TEXT NOT NULL"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "m_identifier", &CelestialObjectModel::mIdentifier, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "extension_name", &CelestialObjectModel::extensionName, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "component", &CelestialObjectModel::component, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "class_name", &CelestialObjectModel::className, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, int>>(
            "amateur_rank", &CelestialObjectModel::amateurRank, "INTEGER DEFAULT 0"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "chinese_name", &CelestialObjectModel::chineseName, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "type", &CelestialObjectModel::type, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "duplicate_type", &CelestialObjectModel::duplicateType, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "morphology", &CelestialObjectModel::morphology, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "constellation_zh", &CelestialObjectModel::constellationZh, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "constellation_en", &CelestialObjectModel::constellationEn, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "ra_j2000", &CelestialObjectModel::raJ2000, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "rad_j2000", &CelestialObjectModel::radJ2000, "REAL DEFAULT 0.0"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "dec_j2000", &CelestialObjectModel::decJ2000, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "dec_d_j2000", &CelestialObjectModel::decDJ2000, "REAL DEFAULT 0.0"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "visual_magnitude_v", &CelestialObjectModel::visualMagnitudeV, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "photographic_magnitude_b", &CelestialObjectModel::photographicMagnitudeB, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "b_minus_v", &CelestialObjectModel::bMinusV, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "surface_brightness", &CelestialObjectModel::surfaceBrightness, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "major_axis", &CelestialObjectModel::majorAxis, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "minor_axis", &CelestialObjectModel::minorAxis, "REAL"),
        std::make_shared<Column<CelestialObjectModel, double>>(
            "position_angle", &CelestialObjectModel::positionAngle, "REAL"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "detailed_description", &CelestialObjectModel::detailedDescription, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "brief_description", &CelestialObjectModel::briefDescription, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, std::string>>(
            "aliases", &CelestialObjectModel::aliases, "TEXT"),
        std::make_shared<Column<CelestialObjectModel, int>>(
            "click_count", &CelestialObjectModel::clickCount, "INTEGER DEFAULT 0"),
    };
}

std::vector<std::shared_ptr<database::orm::ColumnBase>>
UserRatingModel::columns() {
    using namespace database::orm;
    return {
        std::make_shared<Column<UserRatingModel, int64_t>>(
            "id", &UserRatingModel::id, "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<UserRatingModel, std::string>>(
            "user_id", &UserRatingModel::userId, "TEXT NOT NULL"),
        std::make_shared<Column<UserRatingModel, std::string>>(
            "object_id", &UserRatingModel::objectId, "TEXT NOT NULL"),
        std::make_shared<Column<UserRatingModel, double>>(
            "rating", &UserRatingModel::rating, "REAL NOT NULL"),
        std::make_shared<Column<UserRatingModel, int64_t>>(
            "timestamp", &UserRatingModel::timestamp, "INTEGER NOT NULL"),
    };
}

std::vector<std::shared_ptr<database::orm::ColumnBase>>
SearchHistoryModel::columns() {
    using namespace database::orm;
    return {
        std::make_shared<Column<SearchHistoryModel, int64_t>>(
            "id", &SearchHistoryModel::id, "INTEGER PRIMARY KEY AUTOINCREMENT"),
        std::make_shared<Column<SearchHistoryModel, std::string>>(
            "user_id", &SearchHistoryModel::userId, "TEXT NOT NULL"),
        std::make_shared<Column<SearchHistoryModel, std::string>>(
            "query", &SearchHistoryModel::query, "TEXT NOT NULL"),
        std::make_shared<Column<SearchHistoryModel, std::string>>(
            "search_type", &SearchHistoryModel::searchType, "TEXT NOT NULL"),
        std::make_shared<Column<SearchHistoryModel, int64_t>>(
            "timestamp", &SearchHistoryModel::timestamp, "INTEGER NOT NULL"),
        std::make_shared<Column<SearchHistoryModel, int>>(
            "result_count", &SearchHistoryModel::resultCount, "INTEGER DEFAULT 0"),
    };
}

}  // namespace lithium::target
