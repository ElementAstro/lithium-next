// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for RecommendationEngine
 */

#include <gtest/gtest.h>
#include "target/recommendation/recommendation_engine.hpp"

using namespace lithium::target::recommendation;

class RecommendationEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<RecommendationEngine>();
        
        engine_->addItem("M31", {"NGC224", "Andromeda"});
        engine_->addItem("M42", {"Orion Nebula"});
        engine_->addItem("M45", {"Pleiades"});
        
        engine_->addRating("user1", "M31", 5.0);
        engine_->addRating("user1", "M42", 4.0);
    }

    std::unique_ptr<RecommendationEngine> engine_;
};

TEST_F(RecommendationEngineTest, AddItem) {
    engine_->addItem("M33", {"Triangulum"});
    // Item should be added successfully
}

TEST_F(RecommendationEngineTest, AddRating) {
    engine_->addRating("user2", "M31", 4.5);
    // Rating should be added successfully
}

TEST_F(RecommendationEngineTest, AddItemFeature) {
    engine_->addItemFeature("M31", "type", 1.0);
    // Feature should be added
}

TEST_F(RecommendationEngineTest, RecommendItems) {
    auto recs = engine_->recommendItems("user1", 5);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(RecommendationEngineTest, PredictRating) {
    double prediction = engine_->predictRating("user1", "M45");
    EXPECT_GE(prediction, 0.0);
}

TEST_F(RecommendationEngineTest, Train) {
    EXPECT_NO_THROW(engine_->train());
}

TEST_F(RecommendationEngineTest, SaveAndLoadModel) {
    std::string modelPath = "test_model.json";
    engine_->saveModel(modelPath);
    
    auto newEngine = std::make_unique<RecommendationEngine>();
    newEngine->loadModel(modelPath);
    
    // Cleanup
    std::filesystem::remove(modelPath);
}

TEST_F(RecommendationEngineTest, Optimize) {
    EXPECT_NO_THROW(engine_->optimize());
}

TEST_F(RecommendationEngineTest, GetStats) {
    auto stats = engine_->getStats();
    EXPECT_FALSE(stats.empty());
}

TEST_F(RecommendationEngineTest, AddImplicitFeedback) {
    engine_->addImplicitFeedback("user1", "M45");
    // Implicit feedback should be recorded
}

TEST_F(RecommendationEngineTest, ExportImportCSV) {
    std::string csvPath = "test_ratings.csv";
    engine_->exportToCSV(csvPath);
    
    auto newEngine = std::make_unique<RecommendationEngine>();
    newEngine->importFromCSV(csvPath);
    
    // Cleanup
    std::filesystem::remove(csvPath);
}

TEST_F(RecommendationEngineTest, AddBatchRatings) {
    std::vector<std::tuple<std::string, std::string, double>> ratings = {
        {"user3", "M31", 4.0},
        {"user3", "M42", 5.0},
        {"user3", "M45", 3.0}
    };
    engine_->addRatings(ratings);
}
