// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include <gtest/gtest.h>
#include <memory>

#include "target/online/merger/result_merger.hpp"
#include "target/celestial_model.hpp"

namespace lithium::target::online {

// Helper function to create test celestial objects
static auto createTestObject(const std::string& identifier, double ra, double dec,
                             const std::string& type = "",
                             const std::string& constellation = "") -> CelestialObjectModel {
    CelestialObjectModel obj;
    obj.identifier = identifier;
    obj.radJ2000 = ra;
    obj.decDJ2000 = dec;
    obj.type = type;
    obj.constellationEn = constellation;
    obj.visualMagnitudeV = 5.0;
    return obj;
}

class ResultMergerTest : public ::testing::Test {
protected:
    ResultMergerTest() : merger_(MergeConfig{}) {}

    ResultMerger merger_;
};

// Test 1: Basic merge with PreferLocal strategy
TEST_F(ResultMergerTest, MergePreferLocal) {
    std::vector<CelestialObjectModel> local = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M33", 23.4621, 30.6597),
    };

    std::vector<CelestialObjectModel> online = {
        createTestObject("M31", 10.6847, 41.2690),  // Duplicate
        createTestObject("M51", 202.4695, 47.1953),
    };

    MergeConfig config;
    config.strategy = MergeStrategy::PreferLocal;
    merger_.setConfig(config);

    auto result = merger_.merge(local, online);

    EXPECT_EQ(result.size(), 3);

    // Check stats
    auto stats = merger_.getLastMergeStats();
    EXPECT_EQ(stats.localCount, 2);
    EXPECT_EQ(stats.onlineCount, 2);
    EXPECT_EQ(stats.conflictsResolved, 1);
}

// Test 2: Coordinate matching
TEST_F(ResultMergerTest, CoordinateMatching) {
    std::vector<CelestialObjectModel> local = {
        createTestObject("M31", 10.6847, 41.2690),
    };

    // Create online object with slightly different coordinates
    auto online_obj = createTestObject("Andromeda", 10.6850, 41.2688);
    std::vector<CelestialObjectModel> online = {online_obj};

    // Configure for coordinate matching
    MergeConfig config;
    config.matchByCoordinates = true;
    config.coordinateMatchRadius = 0.01;  // Larger radius
    merger_.setConfig(config);

    EXPECT_TRUE(merger_.isDuplicate(local[0], online[0]));
}

// Test 3: Identifier matching
TEST_F(ResultMergerTest, IdentifierMatching) {
    auto obj1 = createTestObject("M31", 10.6847, 41.2690);
    auto obj2 = createTestObject("m31", 0.0, 0.0);  // Different case

    MergeConfig config;
    config.matchByName = true;
    config.matchByCoordinates = false;
    merger_.setConfig(config);

    EXPECT_TRUE(merger_.isDuplicate(obj1, obj2));
}

// Test 4: Messier number matching
TEST_F(ResultMergerTest, MessierMatching) {
    auto obj1 = createTestObject("M31", 10.6847, 41.2690);
    obj1.mIdentifier = "M31";

    auto obj2 = createTestObject("Andromeda", 0.0, 0.0);
    obj2.mIdentifier = "M31";

    MergeConfig config;
    config.matchByName = true;
    config.matchByCoordinates = false;
    merger_.setConfig(config);

    EXPECT_TRUE(merger_.isDuplicate(obj1, obj2));
}

// Test 5: Field merging
TEST_F(ResultMergerTest, FieldMerging) {
    auto local = createTestObject("M31", 10.6847, 41.2690, "Galaxy", "Andromeda");
    local.visualMagnitudeV = 3.4;

    auto online = createTestObject("M31", 10.6847, 41.2690);
    online.type = "Spiral Galaxy";
    online.briefDescription = "Great Andromeda Galaxy";
    online.photographicMagnitudeB = 4.2;

    MergeConfig config;
    config.strategy = MergeStrategy::PreferLocal;
    config.matchByCoordinates = false;
    config.matchByName = true;
    merger_.setConfig(config);

    auto merged = merger_.mergeObjects(local, online);

    // Local fields should be preserved
    EXPECT_EQ(merged.type, "Galaxy");
    EXPECT_EQ(merged.constellationEn, "Andromeda");
    EXPECT_EQ(merged.visualMagnitudeV, 3.4);

    // Online fields should fill gaps
    EXPECT_EQ(merged.briefDescription, "Great Andromeda Galaxy");
    EXPECT_EQ(merged.photographicMagnitudeB, 4.2);
}

// Test 6: PreferOnline strategy
TEST_F(ResultMergerTest, MergePreferOnline) {
    auto local = createTestObject("M31", 10.6847, 41.2690);
    local.type = "Galaxy";

    auto online = createTestObject("M31", 10.6847, 41.2690);
    online.type = "Spiral Galaxy";

    MergeConfig config;
    config.strategy = MergeStrategy::PreferOnline;
    config.matchByName = true;
    config.matchByCoordinates = false;
    merger_.setConfig(config);

    auto merged = merger_.mergeObjects(local, online);

    // Online strategy should prefer online fields
    EXPECT_EQ(merged.type, "Spiral Galaxy");
}

// Test 7: Union merge strategy
TEST_F(ResultMergerTest, UnionMergeStrategy) {
    std::vector<CelestialObjectModel> local = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M33", 23.4621, 30.6597),
    };

    std::vector<CelestialObjectModel> online = {
        createTestObject("M31", 10.6847, 41.2690),  // Duplicate
        createTestObject("M51", 202.4695, 47.1953),
    };

    MergeConfig config;
    config.strategy = MergeStrategy::Union;
    config.removeDuplicates = true;
    merger_.setConfig(config);

    auto result = merger_.merge(local, online);

    // Should have 3 unique objects (M31, M33, M51)
    EXPECT_EQ(result.size(), 3);
}

// Test 8: Max results limit
TEST_F(ResultMergerTest, MaxResultsLimit) {
    std::vector<CelestialObjectModel> local;
    std::vector<CelestialObjectModel> online;

    // Create 50 local and 50 online results
    for (int i = 0; i < 50; ++i) {
        local.push_back(createTestObject("Local_" + std::to_string(i),
                                         10.0 + i, 40.0));
        online.push_back(createTestObject("Online_" + std::to_string(i),
                                          20.0 + i, 50.0));
    }

    MergeConfig config;
    config.maxResults = 50;
    merger_.setConfig(config);

    auto result = merger_.merge(local, online);

    EXPECT_EQ(result.size(), 50);
}

// Test 9: No duplicates with coordinate matching disabled
TEST_F(ResultMergerTest, NoDuplicatesWithCoordinateDisabled) {
    std::vector<CelestialObjectModel> local = {
        createTestObject("M31", 10.6847, 41.2690),
    };

    std::vector<CelestialObjectModel> online = {
        createTestObject("Andromeda", 10.6847, 41.2690),
    };

    MergeConfig config;
    config.matchByName = false;
    config.matchByCoordinates = false;
    merger_.setConfig(config);

    auto result = merger_.merge(local, online);

    // Should not merge duplicates
    EXPECT_EQ(result.size(), 2);
}

// Test 10: Scored merge
TEST_F(ResultMergerTest, ScoredMerge) {
    std::vector<model::ScoredSearchResult> localScored = {
        model::ScoredSearchResult{},
        model::ScoredSearchResult{},
    };
    localScored[0].relevanceScore = 0.95;
    localScored[1].relevanceScore = 0.85;

    std::vector<CelestialObjectModel> online = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M51", 202.4695, 47.1953),
    };

    MergeConfig config;
    config.localScoreBonus = 0.05;
    config.onlineScoreBonus = 0.02;
    merger_.setConfig(config);

    auto result = merger_.mergeScored(localScored, online, 0.5);

    // Should have merged results with boosted scores
    EXPECT_LE(result[0].relevanceScore, 1.0);
    EXPECT_GE(result[0].relevanceScore, 0.95);
}

// Test 11: Multiple provider merge
TEST_F(ResultMergerTest, MultipleProviderMerge) {
    OnlineQueryResult result1;
    result1.objects = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M33", 23.4621, 30.6597),
    };
    result1.provider = "SIMBAD";

    OnlineQueryResult result2;
    result2.objects = {
        createTestObject("M31", 10.6847, 41.2690),  // Duplicate
        createTestObject("M51", 202.4695, 47.1953),
    };
    result2.provider = "VizieR";

    std::vector<OnlineQueryResult> results = {result1, result2};

    MergeConfig config;
    config.removeDuplicates = true;
    merger_.setConfig(config);

    auto merged = merger_.mergeMultiple(results);

    // Should have 3 unique objects
    EXPECT_EQ(merged.size(), 3);
}

// Test 12: MostComplete strategy
TEST_F(ResultMergerTest, MostCompleteStrategy) {
    auto obj1 = createTestObject("M31", 10.6847, 41.2690);
    obj1.type = "Galaxy";
    // Only 2 fields set

    auto obj2 = createTestObject("M31", 10.6847, 41.2690);
    obj2.type = "Spiral Galaxy";
    obj2.constellationEn = "Andromeda";
    obj2.morphology = "Sb";
    // 4 fields set

    MergeConfig config;
    config.strategy = MergeStrategy::MostComplete;
    config.matchByName = true;
    merger_.setConfig(config);

    auto merged = merger_.mergeObjects(obj1, obj2);

    // Most complete strategy should prefer obj2
    EXPECT_EQ(merged.type, "Spiral Galaxy");
    EXPECT_EQ(merged.constellationEn, "Andromeda");
}

// Test 13: Statistics tracking
TEST_F(ResultMergerTest, StatisticsTracking) {
    std::vector<CelestialObjectModel> local = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M33", 23.4621, 30.6597),
    };

    std::vector<CelestialObjectModel> online = {
        createTestObject("M31", 10.6847, 41.2690),
        createTestObject("M51", 202.4695, 47.1953),
    };

    MergeConfig config;
    config.matchByName = true;
    merger_.setConfig(config);

    merger_.merge(local, online);

    auto stats = merger_.getLastMergeStats();

    EXPECT_EQ(stats.localCount, 2);
    EXPECT_EQ(stats.onlineCount, 2);
    EXPECT_EQ(stats.conflictsResolved, 1);
    EXPECT_GT(stats.mergedCount, 0);
}

// Test 14: Empty result handling
TEST_F(ResultMergerTest, EmptyResultHandling) {
    std::vector<CelestialObjectModel> local;
    std::vector<CelestialObjectModel> online = {
        createTestObject("M31", 10.6847, 41.2690),
    };

    auto result = merger_.merge(local, online);

    EXPECT_EQ(result.size(), 1);
}

// Test 15: Configuration persistence
TEST_F(ResultMergerTest, ConfigurationPersistence) {
    MergeConfig config;
    config.strategy = MergeStrategy::PreferOnline;
    config.maxResults = 200;
    config.coordinateMatchRadius = 0.05;

    merger_.setConfig(config);

    auto retrievedConfig = merger_.getConfig();

    EXPECT_EQ(retrievedConfig.strategy, MergeStrategy::PreferOnline);
    EXPECT_EQ(retrievedConfig.maxResults, 200);
    EXPECT_EQ(retrievedConfig.coordinateMatchRadius, 0.05);
}

}  // namespace lithium::target::online

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
