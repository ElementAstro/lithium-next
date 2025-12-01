// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CollaborativeFilter
 */

#include <gtest/gtest.h>
#include "target/recommendation/collaborative_filter.hpp"

using namespace lithium::target::recommendation;

class CollaborativeFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        filter_ = std::make_unique<CollaborativeFilter>();
        
        // Add some test ratings
        filter_->addRating("user1", "M31", 5.0);
        filter_->addRating("user1", "M42", 4.0);
        filter_->addRating("user1", "M45", 3.0);
        
        filter_->addRating("user2", "M31", 4.0);
        filter_->addRating("user2", "M42", 5.0);
        filter_->addRating("user2", "NGC224", 4.0);
        
        filter_->addRating("user3", "M31", 5.0);
        filter_->addRating("user3", "M45", 4.0);
    }

    std::unique_ptr<CollaborativeFilter> filter_;
};

TEST_F(CollaborativeFilterTest, AddRating) {
    filter_->addRating("user4", "M33", 4.5);
    auto ratings = filter_->getUserRatings("user4");
    EXPECT_EQ(ratings.size(), 1);
}

TEST_F(CollaborativeFilterTest, GetUserRatings) {
    auto ratings = filter_->getUserRatings("user1");
    EXPECT_EQ(ratings.size(), 3);
}

TEST_F(CollaborativeFilterTest, GetItemRatings) {
    auto ratings = filter_->getItemRatings("M31");
    EXPECT_EQ(ratings.size(), 3);
}

TEST_F(CollaborativeFilterTest, PredictRating) {
    double prediction = filter_->predictRating("user1", "NGC224");
    EXPECT_GE(prediction, 0.0);
    EXPECT_LE(prediction, 5.0);
}

TEST_F(CollaborativeFilterTest, GetRecommendations) {
    auto recs = filter_->getRecommendations("user3", 5);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(CollaborativeFilterTest, UserSimilarity) {
    double sim = filter_->userSimilarity("user1", "user2");
    EXPECT_GE(sim, -1.0);
    EXPECT_LE(sim, 1.0);
}

TEST_F(CollaborativeFilterTest, ItemSimilarity) {
    double sim = filter_->itemSimilarity("M31", "M42");
    EXPECT_GE(sim, -1.0);
    EXPECT_LE(sim, 1.0);
}

TEST_F(CollaborativeFilterTest, Train) {
    EXPECT_NO_THROW(filter_->train());
}

TEST_F(CollaborativeFilterTest, Clear) {
    filter_->clear();
    auto ratings = filter_->getUserRatings("user1");
    EXPECT_TRUE(ratings.empty());
}

TEST_F(CollaborativeFilterTest, EmptyUser) {
    auto recs = filter_->getRecommendations("nonexistent", 5);
    EXPECT_TRUE(recs.empty());
}
