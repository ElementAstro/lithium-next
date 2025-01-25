#ifndef TEST_ENGINE_HPP
#define TEST_ENGINE_HPP

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include "atom/type/json.hpp"
#include "target/engine.hpp"

namespace lithium::target::test {

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<SearchEngine>();

        // Create test data
        testStarObject = StarObject("Test Star", {"Alias1", "Alias2"}, 0);

        testCelestialObject = CelestialObject(
            "ID1", "NGC1", "M31", "ext1", "comp1", "class1", "rank1", "天体1",
            "galaxy", "duplicate1", "spiral", "仙女座", "Andromeda",
            "00h42m44.3s", 11.11, "+41°16'9\"", 41.27, 4.36, 4.16, 0.2, 13.0,
            190.0, 60.0, 35, "Detailed description", "Brief description");

        testStarObject.setCelestialObject(testCelestialObject);
    }

    void TearDown() override {
        engine->clearCache();
        engine.reset();

        // Cleanup test files
        std::filesystem::remove("test_model.bin");
        std::filesystem::remove("test_export.csv");
    }

    std::unique_ptr<SearchEngine> engine;
    StarObject testStarObject;
    CelestialObject testCelestialObject;
};

// CelestialObject Tests
TEST_F(SearchEngineTest, CelestialObjectJsonSerialization) {
    auto json = testCelestialObject.to_json();
    auto deserialized = CelestialObject::from_json(json);

    EXPECT_EQ(deserialized.ID, testCelestialObject.ID);
    EXPECT_EQ(deserialized.Identifier, testCelestialObject.Identifier);
    EXPECT_DOUBLE_EQ(deserialized.RADJ2000, testCelestialObject.RADJ2000);
    EXPECT_DOUBLE_EQ(deserialized.VisualMagnitudeV,
                     testCelestialObject.VisualMagnitudeV);
}

// StarObject Tests
TEST_F(SearchEngineTest, StarObjectBasicOperations) {
    EXPECT_EQ(testStarObject.getName(), "Test Star");
    EXPECT_EQ(testStarObject.getAliases().size(), 2);
    EXPECT_EQ(testStarObject.getClickCount(), 0);

    testStarObject.setClickCount(5);
    EXPECT_EQ(testStarObject.getClickCount(), 5);
}

// SearchEngine Core Tests
TEST_F(SearchEngineTest, AddAndSearchStarObject) {
    engine->addStarObject(testStarObject);

    auto results = engine->searchStarObject("Test Star");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].getName(), "Test Star");
}

TEST_F(SearchEngineTest, FuzzySearch) {
    engine->addStarObject(testStarObject);

    auto results = engine->fuzzySearchStarObject("Test Str", 2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].getName(), "Test Star");
}

TEST_F(SearchEngineTest, AutoComplete) {
    engine->addStarObject(testStarObject);

    auto suggestions = engine->autoCompleteStarObject("Test");
    EXPECT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0], "Test Star");
}

// Filter Search Tests
TEST_F(SearchEngineTest, FilterSearch) {
    engine->addStarObject(testStarObject);

    auto results = engine->filterSearch("galaxy", "spiral", 0.0, 5.0);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].getName(), "Test Star");
}

// Recommendation Engine Tests
TEST_F(SearchEngineTest, RecommendationBasics) {
    ASSERT_NO_THROW(engine->initializeRecommendationEngine("test_model.bin"));
    engine->addStarObject(testStarObject);
    engine->addUserRating("user1", "Test Star", 4.5);

    ASSERT_NO_THROW(engine->trainRecommendationEngine());

    auto recommendations = engine->recommendItems("user1", 5);
    EXPECT_FALSE(recommendations.empty());
}

// Data Loading Tests
TEST_F(SearchEngineTest, JsonLoading) {
    // Create test JSON files first
    nlohmann::json nameJson = nlohmann::json::array();
    nameJson.push_back({"Test Star", "Alias1,Alias2"});

    std::ofstream nameFile("test_names.json");
    nameFile << nameJson.dump();
    nameFile.close();

    EXPECT_TRUE(engine->loadFromNameJson("test_names.json"));
    std::filesystem::remove("test_names.json");
}

// Thread Safety Tests
TEST_F(SearchEngineTest, ThreadSafety) {
    constexpr int NUM_THREADS = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            StarObject star("Star" + std::to_string(i),
                            {"Alias" + std::to_string(i)});
            ASSERT_NO_THROW(engine->addStarObject(star));
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// Cache Control Tests
TEST_F(SearchEngineTest, CacheOperations) {
    engine->setCacheSize(200);
    engine->addStarObject(testStarObject);

    // First search should populate cache
    auto results1 = engine->searchStarObject("Test Star");

    // Second search should hit cache
    auto results2 = engine->searchStarObject("Test Star");

    EXPECT_EQ(results1.size(), results2.size());

    engine->clearCache();
    auto stats = engine->getCacheStats();
    EXPECT_FALSE(stats.empty());
}

// Error Handling Tests
TEST_F(SearchEngineTest, ErrorHandling) {
    EXPECT_THROW(engine->loadRecommendationModel("nonexistent.bin"),
                 std::runtime_error);
    EXPECT_THROW(engine->addUserRating("", "Test Star", 6.0),
                 std::invalid_argument);
}

// Hybrid Recommendations Test
TEST_F(SearchEngineTest, HybridRecommendations) {
    engine->addStarObject(testStarObject);
    engine->addUserRating("user1", "Test Star", 4.5);
    engine->trainRecommendationEngine();

    auto recommendations =
        engine->getHybridRecommendations("user1", 5, 0.3, 0.7);
    EXPECT_FALSE(recommendations.empty());
}

// Export/Import Tests
TEST_F(SearchEngineTest, DataExportImport) {
    engine->addStarObject(testStarObject);

    std::vector<std::string> fields = {"name", "aliases", "type"};
    EXPECT_TRUE(engine->exportToCSV("test_export.csv", fields));

    SearchEngine newEngine;
    EXPECT_TRUE(newEngine.loadFromCSV("test_export.csv", fields));
}

}  // namespace lithium::target::test

#endif  // TEST_ENGINE_HPP
