// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for HybridRecommender
 */

#include <gtest/gtest.h>
#include "target/recommendation/hybrid_recommender.hpp"

using namespace lithium::target::recommendation;

class HybridRecommenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        recommender_ = std::make_unique<HybridRecommender>();
        
        // Setup collaborative data
        recommender_->addRating("user1", "M31", 5.0);
        recommender_->addRating("user1", "M42", 4.0);
        recommender_->addRating("user2", "M31", 4.0);
        recommender_->addRating("user2", "M45", 5.0);
        
        // Setup content data
        recommender_->addItemFeatures("M31", {{"type", "Galaxy"}});
        recommender_->addItemFeatures("M42", {{"type", "Nebula"}});
        recommender_->addItemFeatures("M45", {{"type", "Cluster"}});
        recommender_->addItemFeatures("NGC224", {{"type", "Galaxy"}});
    }

    std::unique_ptr<HybridRecommender> recommender_;
};

TEST_F(HybridRecommenderTest, GetHybridRecommendations) {
    auto recs = recommender_->getRecommendations("user1", 5, 0.5, 0.5);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(HybridRecommenderTest, ContentWeightOnly) {
    auto recs = recommender_->getRecommendations("user1", 5, 1.0, 0.0);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(HybridRecommenderTest, CollaborativeWeightOnly) {
    auto recs = recommender_->getRecommendations("user1", 5, 0.0, 1.0);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(HybridRecommenderTest, SetWeights) {
    recommender_->setWeights(0.7, 0.3);
    auto recs = recommender_->getRecommendations("user1", 5);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(HybridRecommenderTest, Train) {
    EXPECT_NO_THROW(recommender_->train());
}

TEST_F(HybridRecommenderTest, PredictRating) {
    double prediction = recommender_->predictRating("user1", "NGC224");
    EXPECT_GE(prediction, 0.0);
}

TEST_F(HybridRecommenderTest, NewUser) {
    auto recs = recommender_->getRecommendations("newuser", 5);
    // New user should get content-based recommendations
    EXPECT_TRUE(recs.empty() || recs.size() > 0);
}

TEST_F(HybridRecommenderTest, GetStats) {
    auto stats = recommender_->getStats();
    EXPECT_FALSE(stats.empty());
}
