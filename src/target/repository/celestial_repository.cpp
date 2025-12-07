// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "celestial_repository.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::target {

using json = nlohmann::json;

// ==================== Levenshtein Distance ====================
static int levenshteinDistance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size();
    const size_t n = s2.size();

    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (size_t i = 0; i <= m; ++i)
        dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j)
        dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost =
                (std::tolower(s1[i - 1]) == std::tolower(s2[j - 1])) ? 0 : 1;
            dp[i][j] = std::min(
                {dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
        }
    }
    return dp[m][n];
}

// ==================== Implementation ====================
class CelestialRepository::Impl {
public:
    std::shared_ptr<database::core::Database> db_;
    bool ownsDatabase_ = false;

    // Prepared statement cache
    std::unique_ptr<database::core::Statement> insertStmt_;
    std::unique_ptr<database::core::Statement> updateStmt_;
    std::unique_ptr<database::core::Statement> findByIdStmt_;
    std::unique_ptr<database::core::Statement> findByIdentifierStmt_;
    std::unique_ptr<database::core::Statement> searchByNameStmt_;
    std::unique_ptr<database::core::Statement> incrementClickStmt_;

    explicit Impl(const std::string& dbPath)
        : db_(std::make_shared<database::core::Database>(dbPath)),
          ownsDatabase_(true) {
        spdlog::info("CelestialRepository: Opening database at {}", dbPath);
    }

    explicit Impl(std::shared_ptr<database::core::Database> db)
        : db_(std::move(db)), ownsDatabase_(false) {
        spdlog::info("CelestialRepository: Using existing database connection");
    }

    ~Impl() = default;

    bool initializeSchema() {
        try {
            // Create celestial_objects table
            db_->execute(R"(
                CREATE TABLE IF NOT EXISTS celestial_objects (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    identifier TEXT NOT NULL UNIQUE,
                    m_identifier TEXT,
                    extension_name TEXT,
                    component TEXT,
                    class_name TEXT,
                    amateur_rank INTEGER DEFAULT 0,
                    chinese_name TEXT,
                    type TEXT,
                    duplicate_type TEXT,
                    morphology TEXT,
                    constellation_zh TEXT,
                    constellation_en TEXT,
                    ra_j2000 TEXT,
                    rad_j2000 REAL DEFAULT 0.0,
                    dec_j2000 TEXT,
                    dec_d_j2000 REAL DEFAULT 0.0,
                    visual_magnitude_v REAL,
                    photographic_magnitude_b REAL,
                    b_minus_v REAL,
                    surface_brightness REAL,
                    major_axis REAL,
                    minor_axis REAL,
                    position_angle REAL,
                    detailed_description TEXT,
                    brief_description TEXT,
                    aliases TEXT,
                    click_count INTEGER DEFAULT 0,
                    created_at INTEGER DEFAULT (strftime('%s', 'now')),
                    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
                )
            )");

            // Create user_ratings table
            db_->execute(R"(
                CREATE TABLE IF NOT EXISTS user_ratings (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id TEXT NOT NULL,
                    object_id TEXT NOT NULL,
                    rating REAL NOT NULL,
                    timestamp INTEGER NOT NULL,
                    UNIQUE(user_id, object_id)
                )
            )");

            // Create search_history table
            db_->execute(R"(
                CREATE TABLE IF NOT EXISTS search_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id TEXT NOT NULL,
                    query TEXT NOT NULL,
                    search_type TEXT NOT NULL,
                    timestamp INTEGER NOT NULL,
                    result_count INTEGER DEFAULT 0
                )
            )");

            // Create indexes
            createIndexes();

            spdlog::info(
                "CelestialRepository: Schema initialized successfully");
            return true;
        } catch (const std::exception& e) {
            spdlog::error(
                "CelestialRepository: Failed to initialize schema: {}",
                e.what());
            return false;
        }
    }

    void createIndexes() {
        const std::vector<std::string> indexSqls = {
            "CREATE INDEX IF NOT EXISTS idx_celestial_identifier ON "
            "celestial_objects(identifier)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_type ON "
            "celestial_objects(type)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_magnitude ON "
            "celestial_objects(visual_magnitude_v)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_constellation ON "
            "celestial_objects(constellation_en)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_coords ON "
            "celestial_objects(rad_j2000, dec_d_j2000)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_click ON "
            "celestial_objects(click_count DESC)",
            "CREATE INDEX IF NOT EXISTS idx_celestial_aliases ON "
            "celestial_objects(aliases)",
            "CREATE INDEX IF NOT EXISTS idx_ratings_user ON "
            "user_ratings(user_id)",
            "CREATE INDEX IF NOT EXISTS idx_ratings_object ON "
            "user_ratings(object_id)",
            "CREATE INDEX IF NOT EXISTS idx_history_user ON "
            "search_history(user_id)",
            "CREATE INDEX IF NOT EXISTS idx_history_query ON "
            "search_history(query)",
        };

        for (const auto& sql : indexSqls) {
            try {
                db_->execute(sql);
            } catch (const std::exception& e) {
                spdlog::warn("CelestialRepository: Index creation warning: {}",
                             e.what());
            }
        }
    }

    int64_t insert(const CelestialObjectModel& obj) {
        try {
            auto stmt = db_->prepare(R"(
                INSERT INTO celestial_objects (
                    identifier, m_identifier, extension_name, component, class_name,
                    amateur_rank, chinese_name, type, duplicate_type, morphology,
                    constellation_zh, constellation_en, ra_j2000, rad_j2000,
                    dec_j2000, dec_d_j2000, visual_magnitude_v, photographic_magnitude_b,
                    b_minus_v, surface_brightness, major_axis, minor_axis,
                    position_angle, detailed_description, brief_description, aliases, click_count
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");

            stmt->bind(1, obj.identifier)
                .bind(2, obj.mIdentifier)
                .bind(3, obj.extensionName)
                .bind(4, obj.component)
                .bind(5, obj.className)
                .bind(6, obj.amateurRank)
                .bind(7, obj.chineseName)
                .bind(8, obj.type)
                .bind(9, obj.duplicateType)
                .bind(10, obj.morphology)
                .bind(11, obj.constellationZh)
                .bind(12, obj.constellationEn)
                .bind(13, obj.raJ2000)
                .bind(14, obj.radJ2000)
                .bind(15, obj.decJ2000)
                .bind(16, obj.decDJ2000)
                .bind(17, obj.visualMagnitudeV)
                .bind(18, obj.photographicMagnitudeB)
                .bind(19, obj.bMinusV)
                .bind(20, obj.surfaceBrightness)
                .bind(21, obj.majorAxis)
                .bind(22, obj.minorAxis)
                .bind(23, obj.positionAngle)
                .bind(24, obj.detailedDescription)
                .bind(25, obj.briefDescription)
                .bind(26, obj.aliases)
                .bind(27, obj.clickCount);

            stmt->execute();

            // Get last insert ID
            auto idStmt = db_->prepare("SELECT last_insert_rowid()");
            if (idStmt->step()) {
                return idStmt->getInt64(0);
            }
            return -1;
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Insert failed: {}", e.what());
            return -1;
        }
    }

    bool update(const CelestialObjectModel& obj) {
        try {
            auto stmt = db_->prepare(R"(
                UPDATE celestial_objects SET
                    m_identifier = ?, extension_name = ?, component = ?, class_name = ?,
                    amateur_rank = ?, chinese_name = ?, type = ?, duplicate_type = ?,
                    morphology = ?, constellation_zh = ?, constellation_en = ?,
                    ra_j2000 = ?, rad_j2000 = ?, dec_j2000 = ?, dec_d_j2000 = ?,
                    visual_magnitude_v = ?, photographic_magnitude_b = ?, b_minus_v = ?,
                    surface_brightness = ?, major_axis = ?, minor_axis = ?,
                    position_angle = ?, detailed_description = ?, brief_description = ?,
                    aliases = ?, click_count = ?, updated_at = strftime('%s', 'now')
                WHERE id = ?
            )");

            stmt->bind(1, obj.mIdentifier)
                .bind(2, obj.extensionName)
                .bind(3, obj.component)
                .bind(4, obj.className)
                .bind(5, obj.amateurRank)
                .bind(6, obj.chineseName)
                .bind(7, obj.type)
                .bind(8, obj.duplicateType)
                .bind(9, obj.morphology)
                .bind(10, obj.constellationZh)
                .bind(11, obj.constellationEn)
                .bind(12, obj.raJ2000)
                .bind(13, obj.radJ2000)
                .bind(14, obj.decJ2000)
                .bind(15, obj.decDJ2000)
                .bind(16, obj.visualMagnitudeV)
                .bind(17, obj.photographicMagnitudeB)
                .bind(18, obj.bMinusV)
                .bind(19, obj.surfaceBrightness)
                .bind(20, obj.majorAxis)
                .bind(21, obj.minorAxis)
                .bind(22, obj.positionAngle)
                .bind(23, obj.detailedDescription)
                .bind(24, obj.briefDescription)
                .bind(25, obj.aliases)
                .bind(26, obj.clickCount)
                .bind(27, obj.id);

            return stmt->execute();
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Update failed: {}", e.what());
            return false;
        }
    }

    bool remove(int64_t id) {
        try {
            auto stmt =
                db_->prepare("DELETE FROM celestial_objects WHERE id = ?");
            stmt->bind(1, id);
            return stmt->execute();
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Remove failed: {}", e.what());
            return false;
        }
    }

    CelestialObjectModel modelFromStatement(database::core::Statement& stmt) {
        CelestialObjectModel obj;
        obj.id = stmt.getInt64(0);
        obj.identifier = stmt.getText(1);
        obj.mIdentifier = stmt.getText(2);
        obj.extensionName = stmt.getText(3);
        obj.component = stmt.getText(4);
        obj.className = stmt.getText(5);
        obj.amateurRank = stmt.getInt(6);
        obj.chineseName = stmt.getText(7);
        obj.type = stmt.getText(8);
        obj.duplicateType = stmt.getText(9);
        obj.morphology = stmt.getText(10);
        obj.constellationZh = stmt.getText(11);
        obj.constellationEn = stmt.getText(12);
        obj.raJ2000 = stmt.getText(13);
        obj.radJ2000 = stmt.getDouble(14);
        obj.decJ2000 = stmt.getText(15);
        obj.decDJ2000 = stmt.getDouble(16);
        obj.visualMagnitudeV = stmt.getDouble(17);
        obj.photographicMagnitudeB = stmt.getDouble(18);
        obj.bMinusV = stmt.getDouble(19);
        obj.surfaceBrightness = stmt.getDouble(20);
        obj.majorAxis = stmt.getDouble(21);
        obj.minorAxis = stmt.getDouble(22);
        obj.positionAngle = stmt.getDouble(23);
        obj.detailedDescription = stmt.getText(24);
        obj.briefDescription = stmt.getText(25);
        obj.aliases = stmt.getText(26);
        obj.clickCount = stmt.getInt(27);
        return obj;
    }

    std::optional<CelestialObjectModel> findById(int64_t id) {
        try {
            auto stmt =
                db_->prepare("SELECT * FROM celestial_objects WHERE id = ?");
            stmt->bind(1, id);
            if (stmt->step()) {
                return modelFromStatement(*stmt);
            }
            return std::nullopt;
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: FindById failed: {}", e.what());
            return std::nullopt;
        }
    }

    std::optional<CelestialObjectModel> findByIdentifier(
        const std::string& identifier) {
        try {
            auto stmt = db_->prepare(
                "SELECT * FROM celestial_objects WHERE identifier = ? OR "
                "aliases LIKE ?");
            stmt->bind(1, identifier);
            stmt->bind(2, "%" + identifier + "%");
            if (stmt->step()) {
                return modelFromStatement(*stmt);
            }
            return std::nullopt;
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: FindByIdentifier failed: {}",
                          e.what());
            return std::nullopt;
        }
    }

    std::vector<CelestialObjectModel> searchByName(const std::string& pattern,
                                                   int limit) {
        std::vector<CelestialObjectModel> results;
        try {
            std::string searchPattern = pattern;
            // Convert * to % for SQL LIKE
            std::replace(searchPattern.begin(), searchPattern.end(), '*', '%');
            if (searchPattern.find('%') == std::string::npos) {
                searchPattern = "%" + searchPattern + "%";
            }

            auto stmt = db_->prepare(R"(
                SELECT * FROM celestial_objects 
                WHERE identifier LIKE ? OR chinese_name LIKE ? OR aliases LIKE ?
                ORDER BY click_count DESC
                LIMIT ?
            )");
            stmt->bind(1, searchPattern);
            stmt->bind(2, searchPattern);
            stmt->bind(3, searchPattern);
            stmt->bind(4, limit);

            while (stmt->step()) {
                results.push_back(modelFromStatement(*stmt));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: SearchByName failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<std::pair<CelestialObjectModel, int>> fuzzySearch(
        const std::string& name, int tolerance, int limit) {
        std::vector<std::pair<CelestialObjectModel, int>> results;
        try {
            // Get all objects and compute Levenshtein distance
            auto stmt = db_->prepare("SELECT * FROM celestial_objects");

            while (stmt->step()) {
                auto obj = modelFromStatement(*stmt);
                int dist = levenshteinDistance(name, obj.identifier);

                // Also check aliases
                if (dist > tolerance && !obj.aliases.empty()) {
                    std::istringstream iss(obj.aliases);
                    std::string alias;
                    while (std::getline(iss, alias, ',')) {
                        // Trim whitespace
                        alias.erase(0, alias.find_first_not_of(" \t"));
                        alias.erase(alias.find_last_not_of(" \t") + 1);
                        int aliasDist = levenshteinDistance(name, alias);
                        dist = std::min(dist, aliasDist);
                    }
                }

                if (dist <= tolerance) {
                    results.emplace_back(obj, dist);
                }
            }

            // Sort by distance
            std::sort(results.begin(), results.end(),
                      [](const auto& a, const auto& b) {
                          return a.second < b.second;
                      });

            // Limit results
            if (static_cast<int>(results.size()) > limit) {
                results.resize(limit);
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: FuzzySearch failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<CelestialObjectModel> search(
        const CelestialSearchFilter& filter) {
        std::vector<CelestialObjectModel> results;
        try {
            std::ostringstream sql;
            sql << "SELECT * FROM celestial_objects WHERE 1=1";

            if (!filter.namePattern.empty()) {
                sql << " AND (identifier LIKE '%" << filter.namePattern << "%'"
                    << " OR chinese_name LIKE '%" << filter.namePattern << "%'"
                    << " OR aliases LIKE '%" << filter.namePattern << "%')";
            }
            if (!filter.type.empty()) {
                sql << " AND type = '" << filter.type << "'";
            }
            if (!filter.morphology.empty()) {
                sql << " AND morphology = '" << filter.morphology << "'";
            }
            if (!filter.constellation.empty()) {
                sql << " AND (constellation_en = '" << filter.constellation
                    << "'"
                    << " OR constellation_zh = '" << filter.constellation
                    << "')";
            }
            if (filter.minMagnitude > -30.0) {
                sql << " AND visual_magnitude_v >= " << filter.minMagnitude;
            }
            if (filter.maxMagnitude < 30.0) {
                sql << " AND visual_magnitude_v <= " << filter.maxMagnitude;
            }
            if (filter.minRA > 0.0) {
                sql << " AND rad_j2000 >= " << filter.minRA;
            }
            if (filter.maxRA < 360.0) {
                sql << " AND rad_j2000 <= " << filter.maxRA;
            }
            if (filter.minDec > -90.0) {
                sql << " AND dec_d_j2000 >= " << filter.minDec;
            }
            if (filter.maxDec < 90.0) {
                sql << " AND dec_d_j2000 <= " << filter.maxDec;
            }

            sql << " ORDER BY " << filter.orderBy;
            sql << (filter.ascending ? " ASC" : " DESC");
            sql << " LIMIT " << filter.limit;
            sql << " OFFSET " << filter.offset;

            auto stmt = db_->prepare(sql.str());
            while (stmt->step()) {
                results.push_back(modelFromStatement(*stmt));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Search failed: {}", e.what());
        }
        return results;
    }

    std::vector<std::string> autocomplete(const std::string& prefix,
                                          int limit) {
        std::vector<std::string> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT DISTINCT identifier FROM celestial_objects 
                WHERE identifier LIKE ? 
                ORDER BY click_count DESC, identifier ASC
                LIMIT ?
            )");
            stmt->bind(1, prefix + "%");
            stmt->bind(2, limit);

            while (stmt->step()) {
                results.push_back(stmt->getText(0));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Autocomplete failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<CelestialObjectModel> searchByCoordinates(double ra, double dec,
                                                          double radius,
                                                          int limit) {
        std::vector<CelestialObjectModel> results;
        try {
            // Simple box search first, then filter by actual distance
            double raMin = ra - radius;
            double raMax = ra + radius;
            double decMin = dec - radius;
            double decMax = dec + radius;

            auto stmt = db_->prepare(R"(
                SELECT * FROM celestial_objects 
                WHERE rad_j2000 BETWEEN ? AND ?
                  AND dec_d_j2000 BETWEEN ? AND ?
                LIMIT ?
            )");
            stmt->bind(1, raMin);
            stmt->bind(2, raMax);
            stmt->bind(3, decMin);
            stmt->bind(4, decMax);
            stmt->bind(5, limit * 2);  // Get more for filtering

            while (stmt->step()) {
                auto obj = modelFromStatement(*stmt);
                // Calculate angular distance
                double dRa = (obj.radJ2000 - ra) * std::cos(dec * M_PI / 180.0);
                double dDec = obj.decDJ2000 - dec;
                double dist = std::sqrt(dRa * dRa + dDec * dDec);

                if (dist <= radius) {
                    results.push_back(obj);
                    if (static_cast<int>(results.size()) >= limit)
                        break;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: SearchByCoordinates failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<CelestialObjectModel> getByType(const std::string& type,
                                                int limit) {
        std::vector<CelestialObjectModel> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT * FROM celestial_objects 
                WHERE type = ?
                ORDER BY click_count DESC
                LIMIT ?
            )");
            stmt->bind(1, type);
            stmt->bind(2, limit);

            while (stmt->step()) {
                results.push_back(modelFromStatement(*stmt));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetByType failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<CelestialObjectModel> getByMagnitudeRange(double minMag,
                                                          double maxMag,
                                                          int limit) {
        std::vector<CelestialObjectModel> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT * FROM celestial_objects 
                WHERE visual_magnitude_v BETWEEN ? AND ?
                ORDER BY visual_magnitude_v ASC
                LIMIT ?
            )");
            stmt->bind(1, minMag);
            stmt->bind(2, maxMag);
            stmt->bind(3, limit);

            while (stmt->step()) {
                results.push_back(modelFromStatement(*stmt));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetByMagnitudeRange failed: {}",
                          e.what());
        }
        return results;
    }

    int batchInsert(const std::vector<CelestialObjectModel>& objects,
                    size_t chunkSize) {
        int successCount = 0;
        try {
            for (size_t i = 0; i < objects.size(); i += chunkSize) {
                auto transaction = db_->beginTransaction();
                try {
                    size_t end = std::min(i + chunkSize, objects.size());
                    for (size_t j = i; j < end; ++j) {
                        if (insert(objects[j]) >= 0) {
                            ++successCount;
                        }
                    }
                    transaction->commit();
                } catch (...) {
                    transaction->rollback();
                    throw;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: BatchInsert failed: {}",
                          e.what());
        }
        return successCount;
    }

    int batchUpdate(const std::vector<CelestialObjectModel>& objects,
                    size_t chunkSize) {
        int successCount = 0;
        try {
            for (size_t i = 0; i < objects.size(); i += chunkSize) {
                auto transaction = db_->beginTransaction();
                try {
                    size_t end = std::min(i + chunkSize, objects.size());
                    for (size_t j = i; j < end; ++j) {
                        if (update(objects[j])) {
                            ++successCount;
                        }
                    }
                    transaction->commit();
                } catch (...) {
                    transaction->rollback();
                    throw;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: BatchUpdate failed: {}",
                          e.what());
        }
        return successCount;
    }

    int upsert(const std::vector<CelestialObjectModel>& objects) {
        int affectedCount = 0;
        try {
            auto transaction = db_->beginTransaction();
            try {
                for (const auto& obj : objects) {
                    auto existing = findByIdentifier(obj.identifier);
                    if (existing) {
                        CelestialObjectModel updated = obj;
                        updated.id = existing->id;
                        if (update(updated))
                            ++affectedCount;
                    } else {
                        if (insert(obj) >= 0)
                            ++affectedCount;
                    }
                }
                transaction->commit();
            } catch (...) {
                transaction->rollback();
                throw;
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Upsert failed: {}", e.what());
        }
        return affectedCount;
    }

    ImportResult importFromJson(const std::string& filename,
                                const ImportExportOptions& options) {
        ImportResult result;
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                result.errors.push_back("Failed to open file: " + filename);
                return result;
            }

            json data;
            file >> data;

            auto transaction = db_->beginTransaction();
            try {
                for (const auto& item : data) {
                    ++result.totalRecords;
                    try {
                        CelestialObjectModel obj;
                        obj.identifier = item.value(
                            "Identifier", item.value("identifier", ""));
                        obj.mIdentifier = item.value(
                            "MIdentifier", item.value("m_identifier", ""));
                        obj.extensionName = item.value(
                            "ExtensionName", item.value("extension_name", ""));
                        obj.component = item.value("Component",
                                                   item.value("component", ""));
                        obj.className = item.value(
                            "ClassName", item.value("class_name", ""));
                        obj.amateurRank = item.value(
                            "AmateurRank", item.value("amateur_rank", 0));
                        obj.chineseName = item.value(
                            "ChineseName", item.value("chinese_name", ""));
                        obj.type = item.value("Type", item.value("type", ""));
                        obj.duplicateType = item.value(
                            "DuplicateType", item.value("duplicate_type", ""));
                        obj.morphology = item.value(
                            "Morphology", item.value("morphology", ""));
                        obj.constellationZh =
                            item.value("ConstellationZh",
                                       item.value("constellation_zh", ""));
                        obj.constellationEn =
                            item.value("ConstellationEn",
                                       item.value("constellation_en", ""));
                        obj.raJ2000 =
                            item.value("RAJ2000", item.value("ra_j2000", ""));
                        obj.radJ2000 = item.value("RADJ2000",
                                                  item.value("rad_j2000", 0.0));
                        obj.decJ2000 =
                            item.value("DecJ2000", item.value("dec_j2000", ""));
                        obj.decDJ2000 = item.value(
                            "DecDJ2000", item.value("dec_d_j2000", 0.0));
                        obj.visualMagnitudeV =
                            item.value("VisualMagnitudeV",
                                       item.value("visual_magnitude_v", 0.0));
                        obj.photographicMagnitudeB = item.value(
                            "PhotographicMagnitudeB",
                            item.value("photographic_magnitude_b", 0.0));
                        obj.bMinusV =
                            item.value("BMinusV", item.value("b_minus_v", 0.0));
                        obj.surfaceBrightness =
                            item.value("SurfaceBrightness",
                                       item.value("surface_brightness", 0.0));
                        obj.majorAxis = item.value(
                            "MajorAxis", item.value("major_axis", 0.0));
                        obj.minorAxis = item.value(
                            "MinorAxis", item.value("minor_axis", 0.0));
                        obj.positionAngle = item.value(
                            "PositionAngle", item.value("position_angle", 0.0));
                        obj.detailedDescription =
                            item.value("DetailedDescription",
                                       item.value("detailed_description", ""));
                        obj.briefDescription =
                            item.value("BriefDescription",
                                       item.value("brief_description", ""));

                        if (options.includeAliases &&
                            item.contains("aliases")) {
                            if (item["aliases"].is_array()) {
                                std::ostringstream oss;
                                for (size_t i = 0; i < item["aliases"].size();
                                     ++i) {
                                    if (i > 0)
                                        oss << ",";
                                    oss << item["aliases"][i]
                                               .get<std::string>();
                                }
                                obj.aliases = oss.str();
                            } else {
                                obj.aliases =
                                    item["aliases"].get<std::string>();
                            }
                        }

                        if (obj.identifier.empty()) {
                            result.errors.push_back(
                                "Record " +
                                std::to_string(result.totalRecords) +
                                ": Missing identifier");
                            ++result.errorCount;
                            continue;
                        }

                        // Check for duplicate
                        auto existing = findByIdentifier(obj.identifier);
                        if (existing) {
                            obj.id = existing->id;
                            if (update(obj)) {
                                ++result.duplicateCount;
                                ++result.successCount;
                            } else {
                                ++result.errorCount;
                            }
                        } else {
                            if (insert(obj) >= 0) {
                                ++result.successCount;
                            } else {
                                ++result.errorCount;
                            }
                        }
                    } catch (const std::exception& e) {
                        result.errors.push_back(
                            "Record " + std::to_string(result.totalRecords) +
                            ": " + e.what());
                        ++result.errorCount;
                    }
                }
                transaction->commit();
            } catch (...) {
                transaction->rollback();
                throw;
            }

            spdlog::info(
                "CelestialRepository: Imported {} objects from JSON ({} "
                "success, {} errors, {} duplicates)",
                result.totalRecords, result.successCount, result.errorCount,
                result.duplicateCount);
        } catch (const std::exception& e) {
            result.errors.push_back("Import failed: " + std::string(e.what()));
            spdlog::error("CelestialRepository: ImportFromJson failed: {}",
                          e.what());
        }
        return result;
    }

    ImportResult importFromCsv(const std::string& filename,
                               const ImportExportOptions& options) {
        ImportResult result;
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                result.errors.push_back("Failed to open file: " + filename);
                return result;
            }

            std::string line;
            std::vector<std::string> headers;

            // Read header
            if (options.hasHeader && std::getline(file, line)) {
                std::istringstream iss(line);
                std::string field;
                while (std::getline(iss, field, options.delimiter[0])) {
                    // Trim quotes and whitespace
                    field.erase(0, field.find_first_not_of(" \t\""));
                    field.erase(field.find_last_not_of(" \t\"") + 1);
                    headers.push_back(field);
                }
            }

            auto transaction = db_->beginTransaction();
            try {
                while (std::getline(file, line)) {
                    ++result.totalRecords;
                    try {
                        std::vector<std::string> values;
                        std::istringstream iss(line);
                        std::string field;
                        while (std::getline(iss, field, options.delimiter[0])) {
                            field.erase(0, field.find_first_not_of(" \t\""));
                            field.erase(field.find_last_not_of(" \t\"") + 1);
                            values.push_back(field);
                        }

                        CelestialObjectModel obj;
                        for (size_t i = 0;
                             i < headers.size() && i < values.size(); ++i) {
                            const auto& h = headers[i];
                            const auto& v = values[i];

                            if (h == "identifier" || h == "Identifier")
                                obj.identifier = v;
                            else if (h == "type" || h == "Type")
                                obj.type = v;
                            else if (h == "morphology" || h == "Morphology")
                                obj.morphology = v;
                            else if (h == "chinese_name" || h == "ChineseName")
                                obj.chineseName = v;
                            else if (h == "constellation_en" ||
                                     h == "ConstellationEn")
                                obj.constellationEn = v;
                            else if (h == "ra_j2000" || h == "RAJ2000")
                                obj.raJ2000 = v;
                            else if (h == "dec_j2000" || h == "DecJ2000")
                                obj.decJ2000 = v;
                            else if (h == "rad_j2000" || h == "RADJ2000")
                                obj.radJ2000 = std::stod(v);
                            else if (h == "dec_d_j2000" || h == "DecDJ2000")
                                obj.decDJ2000 = std::stod(v);
                            else if (h == "visual_magnitude_v" ||
                                     h == "VisualMagnitudeV")
                                obj.visualMagnitudeV = std::stod(v);
                            else if (h == "aliases")
                                obj.aliases = v;
                        }

                        if (obj.identifier.empty()) {
                            ++result.errorCount;
                            continue;
                        }

                        auto existing = findByIdentifier(obj.identifier);
                        if (existing) {
                            obj.id = existing->id;
                            if (update(obj)) {
                                ++result.duplicateCount;
                                ++result.successCount;
                            } else {
                                ++result.errorCount;
                            }
                        } else {
                            if (insert(obj) >= 0) {
                                ++result.successCount;
                            } else {
                                ++result.errorCount;
                            }
                        }
                    } catch (const std::exception& e) {
                        result.errors.push_back(
                            "Line " + std::to_string(result.totalRecords) +
                            ": " + e.what());
                        ++result.errorCount;
                    }
                }
                transaction->commit();
            } catch (...) {
                transaction->rollback();
                throw;
            }

            spdlog::info("CelestialRepository: Imported {} records from CSV",
                         result.successCount);
        } catch (const std::exception& e) {
            result.errors.push_back("Import failed: " + std::string(e.what()));
            spdlog::error("CelestialRepository: ImportFromCsv failed: {}",
                          e.what());
        }
        return result;
    }

    int exportToJson(const std::string& filename,
                     const CelestialSearchFilter& filter,
                     const ImportExportOptions& options) {
        try {
            auto objects = search(filter);
            json data = json::array();

            for (const auto& obj : objects) {
                json item;
                item["identifier"] = obj.identifier;
                item["m_identifier"] = obj.mIdentifier;
                item["extension_name"] = obj.extensionName;
                item["component"] = obj.component;
                item["class_name"] = obj.className;
                item["amateur_rank"] = obj.amateurRank;
                item["chinese_name"] = obj.chineseName;
                item["type"] = obj.type;
                item["duplicate_type"] = obj.duplicateType;
                item["morphology"] = obj.morphology;
                item["constellation_zh"] = obj.constellationZh;
                item["constellation_en"] = obj.constellationEn;
                item["ra_j2000"] = obj.raJ2000;
                item["rad_j2000"] = obj.radJ2000;
                item["dec_j2000"] = obj.decJ2000;
                item["dec_d_j2000"] = obj.decDJ2000;
                item["visual_magnitude_v"] = obj.visualMagnitudeV;
                item["photographic_magnitude_b"] = obj.photographicMagnitudeB;
                item["b_minus_v"] = obj.bMinusV;
                item["surface_brightness"] = obj.surfaceBrightness;
                item["major_axis"] = obj.majorAxis;
                item["minor_axis"] = obj.minorAxis;
                item["position_angle"] = obj.positionAngle;
                item["detailed_description"] = obj.detailedDescription;
                item["brief_description"] = obj.briefDescription;
                item["click_count"] = obj.clickCount;

                if (options.includeAliases && !obj.aliases.empty()) {
                    json aliases = json::array();
                    std::istringstream iss(obj.aliases);
                    std::string alias;
                    while (std::getline(iss, alias, ',')) {
                        alias.erase(0, alias.find_first_not_of(" \t"));
                        alias.erase(alias.find_last_not_of(" \t") + 1);
                        aliases.push_back(alias);
                    }
                    item["aliases"] = aliases;
                }

                data.push_back(item);
            }

            std::ofstream file(filename);
            file << data.dump(2);

            spdlog::info("CelestialRepository: Exported {} objects to JSON",
                         objects.size());
            return static_cast<int>(objects.size());
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: ExportToJson failed: {}",
                          e.what());
            return 0;
        }
    }

    int exportToCsv(const std::string& filename,
                    const CelestialSearchFilter& filter,
                    const ImportExportOptions& options) {
        try {
            auto objects = search(filter);
            std::ofstream file(filename);

            // Write header
            file << "identifier" << options.delimiter << "type"
                 << options.delimiter << "morphology" << options.delimiter
                 << "chinese_name" << options.delimiter << "constellation_en"
                 << options.delimiter << "ra_j2000" << options.delimiter
                 << "dec_j2000" << options.delimiter << "rad_j2000"
                 << options.delimiter << "dec_d_j2000" << options.delimiter
                 << "visual_magnitude_v" << options.delimiter << "click_count";
            if (options.includeAliases) {
                file << options.delimiter << "aliases";
            }
            file << "\n";

            for (const auto& obj : objects) {
                file << obj.identifier << options.delimiter << obj.type
                     << options.delimiter << obj.morphology << options.delimiter
                     << obj.chineseName << options.delimiter
                     << obj.constellationEn << options.delimiter << obj.raJ2000
                     << options.delimiter << obj.decJ2000 << options.delimiter
                     << obj.radJ2000 << options.delimiter << obj.decDJ2000
                     << options.delimiter << obj.visualMagnitudeV
                     << options.delimiter << obj.clickCount;
                if (options.includeAliases) {
                    file << options.delimiter << "\"" << obj.aliases << "\"";
                }
                file << "\n";
            }

            spdlog::info("CelestialRepository: Exported {} objects to CSV",
                         objects.size());
            return static_cast<int>(objects.size());
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: ExportToCsv failed: {}",
                          e.what());
            return 0;
        }
    }

    bool addRating(const std::string& userId, const std::string& objectId,
                   double rating) {
        try {
            auto timestamp =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

            auto stmt = db_->prepare(R"(
                INSERT OR REPLACE INTO user_ratings (user_id, object_id, rating, timestamp)
                VALUES (?, ?, ?, ?)
            )");
            stmt->bind(1, userId);
            stmt->bind(2, objectId);
            stmt->bind(3, rating);
            stmt->bind(4, timestamp);
            return stmt->execute();
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: AddRating failed: {}",
                          e.what());
            return false;
        }
    }

    std::vector<UserRatingModel> getUserRatings(const std::string& userId,
                                                int limit) {
        std::vector<UserRatingModel> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT id, user_id, object_id, rating, timestamp 
                FROM user_ratings WHERE user_id = ?
                ORDER BY timestamp DESC LIMIT ?
            )");
            stmt->bind(1, userId);
            stmt->bind(2, limit);

            while (stmt->step()) {
                UserRatingModel rating;
                rating.id = stmt->getInt64(0);
                rating.userId = stmt->getText(1);
                rating.objectId = stmt->getText(2);
                rating.rating = stmt->getDouble(3);
                rating.timestamp = stmt->getInt64(4);
                results.push_back(rating);
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetUserRatings failed: {}",
                          e.what());
        }
        return results;
    }

    std::optional<double> getAverageRating(const std::string& objectId) {
        try {
            auto stmt = db_->prepare(
                "SELECT AVG(rating) FROM user_ratings WHERE object_id = ?");
            stmt->bind(1, objectId);
            if (stmt->step() && !stmt->isNull(0)) {
                return stmt->getDouble(0);
            }
            return std::nullopt;
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetAverageRating failed: {}",
                          e.what());
            return std::nullopt;
        }
    }

    void recordSearch(const std::string& userId, const std::string& query,
                      const std::string& searchType, int resultCount) {
        try {
            auto timestamp =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

            auto stmt = db_->prepare(R"(
                INSERT INTO search_history (user_id, query, search_type, timestamp, result_count)
                VALUES (?, ?, ?, ?, ?)
            )");
            stmt->bind(1, userId);
            stmt->bind(2, query);
            stmt->bind(3, searchType);
            stmt->bind(4, timestamp);
            stmt->bind(5, resultCount);
            stmt->execute();
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: RecordSearch failed: {}",
                          e.what());
        }
    }

    std::vector<SearchHistoryModel> getSearchHistory(const std::string& userId,
                                                     int limit) {
        std::vector<SearchHistoryModel> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT id, user_id, query, search_type, timestamp, result_count
                FROM search_history WHERE user_id = ?
                ORDER BY timestamp DESC LIMIT ?
            )");
            stmt->bind(1, userId);
            stmt->bind(2, limit);

            while (stmt->step()) {
                SearchHistoryModel entry;
                entry.id = stmt->getInt64(0);
                entry.userId = stmt->getText(1);
                entry.query = stmt->getText(2);
                entry.searchType = stmt->getText(3);
                entry.timestamp = stmt->getInt64(4);
                entry.resultCount = stmt->getInt(5);
                results.push_back(entry);
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetSearchHistory failed: {}",
                          e.what());
        }
        return results;
    }

    std::vector<std::pair<std::string, int>> getPopularSearches(int limit) {
        std::vector<std::pair<std::string, int>> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT query, COUNT(*) as cnt FROM search_history
                GROUP BY query ORDER BY cnt DESC LIMIT ?
            )");
            stmt->bind(1, limit);

            while (stmt->step()) {
                results.emplace_back(stmt->getText(0), stmt->getInt(1));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetPopularSearches failed: {}",
                          e.what());
        }
        return results;
    }

    int64_t count() {
        try {
            auto stmt = db_->prepare("SELECT COUNT(*) FROM celestial_objects");
            if (stmt->step()) {
                return stmt->getInt64(0);
            }
            return 0;
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Count failed: {}", e.what());
            return 0;
        }
    }

    std::unordered_map<std::string, int64_t> countByType() {
        std::unordered_map<std::string, int64_t> results;
        try {
            auto stmt = db_->prepare(
                "SELECT type, COUNT(*) FROM celestial_objects GROUP BY type");
            while (stmt->step()) {
                results[stmt->getText(0)] = stmt->getInt64(1);
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: CountByType failed: {}",
                          e.what());
        }
        return results;
    }

    bool incrementClickCount(const std::string& identifier) {
        try {
            auto stmt = db_->prepare(R"(
                UPDATE celestial_objects 
                SET click_count = click_count + 1, updated_at = strftime('%s', 'now')
                WHERE identifier = ?
            )");
            stmt->bind(1, identifier);
            return stmt->execute();
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: IncrementClickCount failed: {}",
                          e.what());
            return false;
        }
    }

    std::vector<CelestialObjectModel> getMostPopular(int limit) {
        std::vector<CelestialObjectModel> results;
        try {
            auto stmt = db_->prepare(R"(
                SELECT * FROM celestial_objects 
                ORDER BY click_count DESC LIMIT ?
            )");
            stmt->bind(1, limit);

            while (stmt->step()) {
                results.push_back(modelFromStatement(*stmt));
            }
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: GetMostPopular failed: {}",
                          e.what());
        }
        return results;
    }

    void optimize() {
        try {
            db_->execute("VACUUM");
            db_->execute("ANALYZE");
            spdlog::info("CelestialRepository: Database optimized");
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: Optimize failed: {}", e.what());
        }
    }

    void clearAll(bool includeHistory) {
        try {
            db_->execute("DELETE FROM celestial_objects");
            if (includeHistory) {
                db_->execute("DELETE FROM user_ratings");
                db_->execute("DELETE FROM search_history");
            }
            spdlog::info("CelestialRepository: Data cleared");
        } catch (const std::exception& e) {
            spdlog::error("CelestialRepository: ClearAll failed: {}", e.what());
        }
    }

    std::string getStatistics() {
        json stats;
        try {
            stats["total_objects"] = count();
            stats["objects_by_type"] = countByType();

            auto ratingStmt = db_->prepare("SELECT COUNT(*) FROM user_ratings");
            if (ratingStmt->step()) {
                stats["total_ratings"] = ratingStmt->getInt64(0);
            }

            auto historyStmt =
                db_->prepare("SELECT COUNT(*) FROM search_history");
            if (historyStmt->step()) {
                stats["total_searches"] = historyStmt->getInt64(0);
            }

            stats["popular_searches"] = json::array();
            for (const auto& [query, cnt] : getPopularSearches(5)) {
                stats["popular_searches"].push_back(
                    {{"query", query}, {"count", cnt}});
            }
        } catch (const std::exception& e) {
            stats["error"] = e.what();
        }
        return stats.dump(2);
    }
};

// ==================== Public Interface ====================

CelestialRepository::CelestialRepository(const std::string& dbPath)
    : pImpl_(std::make_unique<Impl>(dbPath)) {}

CelestialRepository::CelestialRepository(
    std::shared_ptr<database::core::Database> db)
    : pImpl_(std::make_unique<Impl>(std::move(db))) {}

CelestialRepository::~CelestialRepository() = default;

CelestialRepository::CelestialRepository(CelestialRepository&&) noexcept =
    default;
CelestialRepository& CelestialRepository::operator=(
    CelestialRepository&&) noexcept = default;

bool CelestialRepository::initializeSchema() {
    return pImpl_->initializeSchema();
}

int64_t CelestialRepository::insert(const CelestialObjectModel& obj) {
    return pImpl_->insert(obj);
}
bool CelestialRepository::update(const CelestialObjectModel& obj) {
    return pImpl_->update(obj);
}
bool CelestialRepository::remove(int64_t id) { return pImpl_->remove(id); }
std::optional<CelestialObjectModel> CelestialRepository::findById(int64_t id) {
    return pImpl_->findById(id);
}
std::optional<CelestialObjectModel> CelestialRepository::findByIdentifier(
    const std::string& identifier) {
    return pImpl_->findByIdentifier(identifier);
}

std::vector<CelestialObjectModel> CelestialRepository::searchByName(
    const std::string& pattern, int limit) {
    return pImpl_->searchByName(pattern, limit);
}
std::vector<std::pair<CelestialObjectModel, int>>
CelestialRepository::fuzzySearch(const std::string& name, int tolerance,
                                 int limit) {
    return pImpl_->fuzzySearch(name, tolerance, limit);
}
std::vector<CelestialObjectModel> CelestialRepository::search(
    const CelestialSearchFilter& filter) {
    return pImpl_->search(filter);
}
std::vector<std::string> CelestialRepository::autocomplete(
    const std::string& prefix, int limit) {
    return pImpl_->autocomplete(prefix, limit);
}
std::vector<CelestialObjectModel> CelestialRepository::searchByCoordinates(
    double ra, double dec, double radius, int limit) {
    return pImpl_->searchByCoordinates(ra, dec, radius, limit);
}
std::vector<CelestialObjectModel> CelestialRepository::getByType(
    const std::string& type, int limit) {
    return pImpl_->getByType(type, limit);
}
std::vector<CelestialObjectModel> CelestialRepository::getByMagnitudeRange(
    double minMag, double maxMag, int limit) {
    return pImpl_->getByMagnitudeRange(minMag, maxMag, limit);
}

int CelestialRepository::batchInsert(
    const std::vector<CelestialObjectModel>& objects, size_t chunkSize) {
    return pImpl_->batchInsert(objects, chunkSize);
}
int CelestialRepository::batchUpdate(
    const std::vector<CelestialObjectModel>& objects, size_t chunkSize) {
    return pImpl_->batchUpdate(objects, chunkSize);
}
int CelestialRepository::upsert(
    const std::vector<CelestialObjectModel>& objects) {
    return pImpl_->upsert(objects);
}

ImportResult CelestialRepository::importFromJson(
    const std::string& filename, const ImportExportOptions& options) {
    return pImpl_->importFromJson(filename, options);
}
ImportResult CelestialRepository::importFromCsv(
    const std::string& filename, const ImportExportOptions& options) {
    return pImpl_->importFromCsv(filename, options);
}
int CelestialRepository::exportToJson(const std::string& filename,
                                      const CelestialSearchFilter& filter,
                                      const ImportExportOptions& options) {
    return pImpl_->exportToJson(filename, filter, options);
}
int CelestialRepository::exportToCsv(const std::string& filename,
                                     const CelestialSearchFilter& filter,
                                     const ImportExportOptions& options) {
    return pImpl_->exportToCsv(filename, filter, options);
}

bool CelestialRepository::addRating(const std::string& userId,
                                    const std::string& objectId,
                                    double rating) {
    return pImpl_->addRating(userId, objectId, rating);
}
std::vector<UserRatingModel> CelestialRepository::getUserRatings(
    const std::string& userId, int limit) {
    return pImpl_->getUserRatings(userId, limit);
}
std::optional<double> CelestialRepository::getAverageRating(
    const std::string& objectId) {
    return pImpl_->getAverageRating(objectId);
}

void CelestialRepository::recordSearch(const std::string& userId,
                                       const std::string& query,
                                       const std::string& searchType,
                                       int resultCount) {
    pImpl_->recordSearch(userId, query, searchType, resultCount);
}
std::vector<SearchHistoryModel> CelestialRepository::getSearchHistory(
    const std::string& userId, int limit) {
    return pImpl_->getSearchHistory(userId, limit);
}
std::vector<std::pair<std::string, int>>
CelestialRepository::getPopularSearches(int limit) {
    return pImpl_->getPopularSearches(limit);
}

int64_t CelestialRepository::count() { return pImpl_->count(); }
std::unordered_map<std::string, int64_t> CelestialRepository::countByType() {
    return pImpl_->countByType();
}
bool CelestialRepository::incrementClickCount(const std::string& identifier) {
    return pImpl_->incrementClickCount(identifier);
}
std::vector<CelestialObjectModel> CelestialRepository::getMostPopular(
    int limit) {
    return pImpl_->getMostPopular(limit);
}

void CelestialRepository::optimize() { pImpl_->optimize(); }
void CelestialRepository::createIndexes() { pImpl_->createIndexes(); }
void CelestialRepository::clearAll(bool includeHistory) {
    pImpl_->clearAll(includeHistory);
}
std::string CelestialRepository::getStatistics() {
    return pImpl_->getStatistics();
}

}  // namespace lithium::target
