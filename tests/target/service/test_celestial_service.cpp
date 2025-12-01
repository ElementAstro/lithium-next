// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CelestialService
 */

#include <gtest/gtest.h>
#include <filesystem>
#include "target/service/celestial_service.hpp"

using namespace lithium::target::service;
namespace fs = std::filesystem;

class CelestialServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath_ = "test_service.db";
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
        
        ServiceConfig config;
        config.databasePath = testDbPath_;
        config.useDatabase = true;
        
        service_ = std::make_unique<CelestialService>(config);
        service_->initialize();
        
        // Add test data
        CelestialObjectModel obj;
        obj.identifier = "M31";
        obj.type = "Galaxy";
        obj.radJ2000 = 10.6847;
        obj.decDJ2000 = 41.2689;
        obj.visualMagnitudeV = 3.44;
        service_->addObject(obj);
    }

    void TearDown() override {
        service_.reset();
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
    }

    std::string testDbPath_;
    std::unique_ptr<CelestialService> service_;
};

TEST_F(CelestialServiceTest, Initialize) {
    EXPECT_TRUE(service_->isInitialized());
}

TEST_F(CelestialServiceTest, AddObject) {
    CelestialObjectModel obj;
    obj.identifier = "M42";
    obj.type = "Nebula";
    
    auto id = service_->addObject(obj);
    EXPECT_GT(id, 0);
}

TEST_F(CelestialServiceTest, GetObject) {
    auto obj = service_->getObject("M31");
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->identifier, "M31");
}

TEST_F(CelestialServiceTest, UpdateObject) {
    auto obj = service_->getObject("M31");
    ASSERT_TRUE(obj.has_value());
    
    obj->type = "Spiral Galaxy";
    EXPECT_TRUE(service_->updateObject(*obj));
    
    auto updated = service_->getObject("M31");
    EXPECT_EQ(updated->type, "Spiral Galaxy");
}

TEST_F(CelestialServiceTest, RemoveObject) {
    EXPECT_TRUE(service_->removeObject("M31"));
    EXPECT_FALSE(service_->getObject("M31").has_value());
}

TEST_F(CelestialServiceTest, Search) {
    auto results = service_->search("M31");
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, FuzzySearch) {
    auto results = service_->fuzzySearch("M30", 2);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, SearchByCoordinates) {
    auto results = service_->searchByCoordinates(10.0, 41.0, 5.0);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, AdvancedSearch) {
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    
    auto results = service_->advancedSearch(filter);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, Autocomplete) {
    auto suggestions = service_->autocomplete("M3");
    EXPECT_GE(suggestions.size(), 1);
}

TEST_F(CelestialServiceTest, GetByType) {
    auto results = service_->getByType("Galaxy");
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, GetByMagnitude) {
    auto results = service_->getByMagnitude(0.0, 5.0);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialServiceTest, AddRating) {
    service_->addRating("user1", "M31", 5.0);
    // Rating should be added
}

TEST_F(CelestialServiceTest, GetRecommendations) {
    service_->addRating("user1", "M31", 5.0);
    auto recs = service_->getRecommendations("user1", 5);
    // May or may not have recommendations depending on data
}

TEST_F(CelestialServiceTest, RecordClick) {
    service_->recordClick("M31");
    auto obj = service_->getObject("M31");
    EXPECT_GE(obj->clickCount, 1);
}

TEST_F(CelestialServiceTest, GetMostPopular) {
    service_->recordClick("M31");
    auto popular = service_->getMostPopular(10);
    EXPECT_GE(popular.size(), 1);
}

TEST_F(CelestialServiceTest, GetStatistics) {
    auto stats = service_->getStatistics();
    EXPECT_FALSE(stats.empty());
}

TEST_F(CelestialServiceTest, GetObjectCount) {
    EXPECT_GE(service_->getObjectCount(), 1);
}

TEST_F(CelestialServiceTest, GetCountByType) {
    auto counts = service_->getCountByType();
    EXPECT_GE(counts["Galaxy"], 1);
}

TEST_F(CelestialServiceTest, ClearCache) {
    EXPECT_NO_THROW(service_->clearCache());
}

TEST_F(CelestialServiceTest, Optimize) {
    EXPECT_NO_THROW(service_->optimize());
}
