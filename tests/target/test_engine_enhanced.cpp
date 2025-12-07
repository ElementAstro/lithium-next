// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Comprehensive test suite for SearchEngine (main engine with enhanced
 * features)
 */

#include <gtest/gtest.h>
#include <filesystem>
#include "target/engine.hpp"

using namespace lithium::target;
namespace fs = std::filesystem;

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<SearchEngine>();

        // Add test star objects
        StarObject star1("M31", {"NGC224", "Andromeda Galaxy"}, 100);
        StarObject star2("M42", {"Orion Nebula"}, 50);
        StarObject star3("M45", {"Pleiades", "Seven Sisters"}, 75);

        engine_->addStarObject(star1);
        engine_->addStarObject(star2);
        engine_->addStarObject(star3);
    }

    std::unique_ptr<SearchEngine> engine_;
};

// ==================== Basic Search Tests ====================

TEST_F(SearchEngineTest, Construction) { EXPECT_NE(engine_, nullptr); }

TEST_F(SearchEngineTest, AddStarObject) {
    StarObject star("M33", {"Triangulum Galaxy"}, 30);
    engine_->addStarObject(star);

    auto results = engine_->searchStarObject("M33");
    EXPECT_EQ(results.size(), 1);
}

TEST_F(SearchEngineTest, SearchByName) {
    auto results = engine_->searchStarObject("M31");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].getName(), "M31");
}

TEST_F(SearchEngineTest, SearchByAlias) {
    auto results = engine_->searchStarObject("NGC224");
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineTest, FuzzySearch) {
    auto results = engine_->fuzzySearchStarObject("M30", 2);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineTest, AutoComplete) {
    auto suggestions = engine_->autoCompleteStarObject("M");
    EXPECT_GE(suggestions.size(), 3);
}

TEST_F(SearchEngineTest, RankedResults) {
    auto results = engine_->searchStarObject("M31");
    auto ranked = SearchEngine::getRankedResults(results);
    EXPECT_EQ(ranked.size(), results.size());
}

// ==================== Filter Search Tests ====================

TEST_F(SearchEngineTest, FilterByType) {
    // Add celestial data
    CelestialObject celestial;
    celestial.Identifier = "M31";
    celestial.Type = "Galaxy";

    StarObject star("M31", {}, 0);
    star.setCelestialObject(celestial);
    engine_->addStarObject(star);

    auto results = engine_->filterSearch("Galaxy");
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineTest, FilterByMagnitude) {
    CelestialObject celestial;
    celestial.Identifier = "M31";
    celestial.VisualMagnitudeV = 3.44;

    StarObject star("M31", {}, 0);
    star.setCelestialObject(celestial);
    engine_->addStarObject(star);

    auto results = engine_->filterSearch("", "", 0.0, 5.0);
    EXPECT_GE(results.size(), 1);
}

// ==================== Recommendation Tests ====================

TEST_F(SearchEngineTest, AddUserRating) {
    engine_->addUserRating("user1", "M31", 5.0);
    // Should not throw
}

TEST_F(SearchEngineTest, RecommendItems) {
    engine_->addUserRating("user1", "M31", 5.0);
    engine_->addUserRating("user1", "M42", 4.0);

    auto recs = engine_->recommendItems("user1", 5);
    // May or may not have recommendations
}

TEST_F(SearchEngineTest, HybridRecommendations) {
    engine_->addUserRating("user1", "M31", 5.0);

    auto recs = engine_->getHybridRecommendations("user1", 5, 0.5, 0.5);
    // May or may not have recommendations
}

// ==================== Cache Tests ====================

TEST_F(SearchEngineTest, ClearCache) {
    engine_->searchStarObject("M31");  // Populate cache
    engine_->clearCache();
    // Should not throw
}

TEST_F(SearchEngineTest, SetCacheSize) {
    engine_->setCacheSize(200);
    // Should not throw
}

TEST_F(SearchEngineTest, GetCacheStats) {
    auto stats = engine_->getCacheStats();
    EXPECT_FALSE(stats.empty());
}

// ==================== Enhanced Database Integration Tests ====================

class SearchEngineEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath_ = "test_engine_enhanced.db";
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }

        EngineConfig config;
        config.databasePath = testDbPath_;
        config.useDatabase = true;
        config.syncOnStartup = false;

        engine_ = std::make_unique<SearchEngine>();
        engine_->initializeWithConfig(config);

        // Add test data to database
        CelestialObjectModel obj;
        obj.identifier = "M31";
        obj.type = "Galaxy";
        obj.radJ2000 = 10.6847;
        obj.decDJ2000 = 41.2689;
        obj.visualMagnitudeV = 3.44;
        engine_->upsertObject(obj);
    }

    void TearDown() override {
        engine_.reset();
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
    }

    std::string testDbPath_;
    std::unique_ptr<SearchEngine> engine_;
};

TEST_F(SearchEngineEnhancedTest, InitializeWithConfig) {
    EXPECT_NE(engine_->getRepository(), nullptr);
}

TEST_F(SearchEngineEnhancedTest, ScoredSearch) {
    auto results = engine_->scoredSearch("M31", 10);
    EXPECT_GE(results.size(), 1);
    EXPECT_EQ(results[0].object.identifier, "M31");
}

TEST_F(SearchEngineEnhancedTest, ScoredFuzzySearch) {
    auto results = engine_->scoredFuzzySearch("M30", 2, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, SearchByCoordinates) {
    auto results = engine_->searchByCoordinates(10.0, 41.0, 5.0, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, AdvancedSearch) {
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    filter.limit = 10;

    auto results = engine_->advancedSearch(filter);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetObjectModel) {
    auto obj = engine_->getObjectModel("M31");
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->identifier, "M31");
}

TEST_F(SearchEngineEnhancedTest, GetByType) {
    auto results = engine_->getByType("Galaxy", 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetByMagnitude) {
    auto results = engine_->getByMagnitude(0.0, 5.0, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, UpsertObject) {
    CelestialObjectModel obj;
    obj.identifier = "M42";
    obj.type = "Nebula";

    auto id = engine_->upsertObject(obj);
    EXPECT_GT(id, 0);
}

TEST_F(SearchEngineEnhancedTest, BatchUpsert) {
    std::vector<CelestialObjectModel> objects;
    for (int i = 0; i < 5; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "BATCH" + std::to_string(i);
        obj.type = "Star";
        objects.push_back(obj);
    }

    int count = engine_->batchUpsert(objects);
    EXPECT_EQ(count, 5);
}

TEST_F(SearchEngineEnhancedTest, RemoveObject) {
    EXPECT_TRUE(engine_->removeObject("M31"));
    EXPECT_FALSE(engine_->getObjectModel("M31").has_value());
}

TEST_F(SearchEngineEnhancedTest, RecordClick) {
    engine_->recordClick("M31");
    auto obj = engine_->getObjectModel("M31");
    EXPECT_GE(obj->clickCount, 1);
}

TEST_F(SearchEngineEnhancedTest, RecordSearch) {
    engine_->recordSearch("user1", "M31", "exact", 1);
    auto history = engine_->getSearchHistory("user1", 10);
    EXPECT_GE(history.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetPopularSearches) {
    engine_->recordSearch("user1", "M31", "exact", 1);
    engine_->recordSearch("user2", "M31", "exact", 1);

    auto popular = engine_->getPopularSearches(10);
    EXPECT_GE(popular.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetMostPopular) {
    engine_->recordClick("M31");
    engine_->recordClick("M31");

    auto popular = engine_->getMostPopular(10);
    EXPECT_GE(popular.size(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetObjectCount) {
    EXPECT_GE(engine_->getObjectCount(), 1);
}

TEST_F(SearchEngineEnhancedTest, GetCountByType) {
    auto counts = engine_->getCountByType();
    EXPECT_GE(counts["Galaxy"], 1);
}

TEST_F(SearchEngineEnhancedTest, GetStatistics) {
    auto stats = engine_->getStatistics();
    EXPECT_FALSE(stats.empty());
}

TEST_F(SearchEngineEnhancedTest, OptimizeDatabase) {
    EXPECT_NO_THROW(engine_->optimizeDatabase());
}

TEST_F(SearchEngineEnhancedTest, ClearAllData) {
    engine_->clearAllData(true);
    EXPECT_EQ(engine_->getObjectCount(), 0);
}

TEST_F(SearchEngineEnhancedTest, GetModelRecommendations) {
    engine_->addUserRating("user1", "M31", 5.0);
    auto recs = engine_->getModelRecommendations("user1", 5);
    // May or may not have recommendations
}

// ==================== Import/Export Tests ====================

TEST_F(SearchEngineEnhancedTest, ImportExportJson) {
    std::string jsonPath = "test_export.json";

    int exported = engine_->exportToJsonFromDb(jsonPath);
    EXPECT_GE(exported, 1);

    engine_->clearAllData(false);

    auto result = engine_->importFromJsonToDb(jsonPath);
    EXPECT_GE(result.successCount, 1);

    fs::remove(jsonPath);
}

TEST_F(SearchEngineEnhancedTest, ImportExportCsv) {
    std::string csvPath = "test_export.csv";

    int exported = engine_->exportToCsvFromDb(csvPath);
    EXPECT_GE(exported, 1);

    engine_->clearAllData(false);

    auto result = engine_->importFromCsvToDb(csvPath);
    EXPECT_GE(result.successCount, 1);

    fs::remove(csvPath);
}
