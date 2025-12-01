// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for SearchEngine (search module)
 */

#include <gtest/gtest.h>
#include "target/search/search_engine.hpp"

using namespace lithium::target::search;

class SearchEngineModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto repository = std::make_shared<CelestialRepository>(":memory:");
        repository->initializeSchema();
        
        // Add test data
        CelestialObjectModel obj;
        obj.identifier = "M31";
        obj.type = "Galaxy";
        obj.radJ2000 = 10.6847;
        obj.decDJ2000 = 41.2689;
        repository->insert(obj);
        
        engine_ = std::make_unique<SearchEngine>(repository);
        engine_->initialize();
    }

    std::unique_ptr<SearchEngine> engine_;
};

TEST_F(SearchEngineModuleTest, Initialize) {
    EXPECT_TRUE(engine_->isInitialized());
}

TEST_F(SearchEngineModuleTest, Search) {
    auto results = engine_->search("M31");
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineModuleTest, ExactSearch) {
    auto results = engine_->exactSearch("M31", 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineModuleTest, FuzzySearch) {
    auto results = engine_->fuzzySearch("M30", 2, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineModuleTest, SearchByCoordinates) {
    auto results = engine_->searchByCoordinates(10.0, 41.0, 5.0, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineModuleTest, Autocomplete) {
    auto suggestions = engine_->autocomplete("M3", 10);
    EXPECT_GE(suggestions.size(), 1);
}

TEST_F(SearchEngineModuleTest, AdvancedSearch) {
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    filter.limit = 10;
    
    auto results = engine_->advancedSearch(filter);
    EXPECT_GE(results.size(), 1);
}

TEST_F(SearchEngineModuleTest, RebuildIndexes) {
    auto result = engine_->rebuildIndexes();
    EXPECT_TRUE(result.has_value());
}

TEST_F(SearchEngineModuleTest, ClearIndexes) {
    engine_->clearIndexes();
    // Should be able to reinitialize
    auto result = engine_->initialize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(SearchEngineModuleTest, GetStats) {
    auto stats = engine_->getStats();
    EXPECT_FALSE(stats.empty());
}
