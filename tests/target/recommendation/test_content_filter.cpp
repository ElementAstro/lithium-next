// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for ContentFilter
 */

#include <gtest/gtest.h>
#include "target/recommendation/content_filter.hpp"

using namespace lithium::target::recommendation;

class ContentFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        filter_ = std::make_unique<ContentFilter>();

        // Add items with features
        filter_->addItem("M31",
                         {{"type", "Galaxy"}, {"constellation", "Andromeda"}});
        filter_->addItem("M42",
                         {{"type", "Nebula"}, {"constellation", "Orion"}});
        filter_->addItem("M45",
                         {{"type", "Cluster"}, {"constellation", "Taurus"}});
        filter_->addItem("NGC224",
                         {{"type", "Galaxy"}, {"constellation", "Andromeda"}});
    }

    std::unique_ptr<ContentFilter> filter_;
};

TEST_F(ContentFilterTest, AddItem) {
    filter_->addItem("M33", {{"type", "Galaxy"}});
    auto features = filter_->getItemFeatures("M33");
    EXPECT_FALSE(features.empty());
}

TEST_F(ContentFilterTest, GetItemFeatures) {
    auto features = filter_->getItemFeatures("M31");
    EXPECT_EQ(features["type"], "Galaxy");
}

TEST_F(ContentFilterTest, ItemSimilarity) {
    double sim = filter_->similarity("M31", "NGC224");
    EXPECT_GT(sim, 0.5);  // Same type and constellation
}

TEST_F(ContentFilterTest, DifferentTypesSimilarity) {
    double sim = filter_->similarity("M31", "M42");
    EXPECT_LT(sim, 0.5);  // Different type
}

TEST_F(ContentFilterTest, GetSimilarItems) {
    auto similar = filter_->getSimilarItems("M31", 3);
    EXPECT_GE(similar.size(), 1);
    // NGC224 should be most similar
    EXPECT_EQ(similar[0].first, "NGC224");
}

TEST_F(ContentFilterTest, GetRecommendationsForUser) {
    // User likes galaxies
    std::vector<std::string> userHistory = {"M31"};
    auto recs = filter_->getRecommendations(userHistory, 5);
    EXPECT_GE(recs.size(), 1);
}

TEST_F(ContentFilterTest, UpdateItemFeatures) {
    filter_->updateItem("M31", {{"type", "Spiral Galaxy"}});
    auto features = filter_->getItemFeatures("M31");
    EXPECT_EQ(features["type"], "Spiral Galaxy");
}

TEST_F(ContentFilterTest, RemoveItem) {
    filter_->removeItem("M45");
    auto features = filter_->getItemFeatures("M45");
    EXPECT_TRUE(features.empty());
}

TEST_F(ContentFilterTest, EmptyHistory) {
    std::vector<std::string> empty;
    auto recs = filter_->getRecommendations(empty, 5);
    EXPECT_TRUE(recs.empty());
}

TEST_F(ContentFilterTest, NonexistentItem) {
    auto features = filter_->getItemFeatures("NONEXISTENT");
    EXPECT_TRUE(features.empty());
}
