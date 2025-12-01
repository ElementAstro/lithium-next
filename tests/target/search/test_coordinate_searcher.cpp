// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CoordinateSearcher
 */

#include <gtest/gtest.h>
#include "target/search/coordinate_searcher.hpp"

using namespace lithium::target::search;

class CoordinateSearcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        searcher_ = std::make_unique<CoordinateSearcher>();
    }

    std::unique_ptr<CoordinateSearcher> searcher_;
};

TEST_F(CoordinateSearcherTest, AngularDistance) {
    // Same point should have zero distance
    double dist = searcher_->angularDistance(10.0, 41.0, 10.0, 41.0);
    EXPECT_NEAR(dist, 0.0, 0.001);
}

TEST_F(CoordinateSearcherTest, AngularDistanceKnownValue) {
    // Test with known angular separation
    double dist = searcher_->angularDistance(0.0, 0.0, 1.0, 0.0);
    EXPECT_NEAR(dist, 1.0, 0.01);
}

TEST_F(CoordinateSearcherTest, SearchWithinRadius) {
    std::vector<CoordinatePoint> points = {
        {10.0, 41.0, "M31"},
        {10.5, 41.5, "M32"},
        {100.0, -20.0, "FarObject"}
    };
    
    auto results = searcher_->searchWithinRadius(points, 10.0, 41.0, 5.0);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(CoordinateSearcherTest, NearestNeighbors) {
    std::vector<CoordinatePoint> points = {
        {10.0, 41.0, "M31"},
        {10.5, 41.5, "M32"},
        {11.0, 42.0, "M33"},
        {100.0, -20.0, "FarObject"}
    };
    
    auto results = searcher_->nearestNeighbors(points, 10.0, 41.0, 2);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(CoordinateSearcherTest, ValidateCoordinates) {
    EXPECT_TRUE(searcher_->validateCoordinates(180.0, 45.0));
    EXPECT_TRUE(searcher_->validateCoordinates(0.0, -90.0));
    EXPECT_TRUE(searcher_->validateCoordinates(359.99, 90.0));
    
    EXPECT_FALSE(searcher_->validateCoordinates(-1.0, 0.0));
    EXPECT_FALSE(searcher_->validateCoordinates(360.1, 0.0));
    EXPECT_FALSE(searcher_->validateCoordinates(0.0, -91.0));
    EXPECT_FALSE(searcher_->validateCoordinates(0.0, 91.0));
}

TEST_F(CoordinateSearcherTest, ConvertToCartesian) {
    auto [x, y, z] = searcher_->toCartesian(0.0, 0.0);
    EXPECT_NEAR(x, 1.0, 0.001);
    EXPECT_NEAR(y, 0.0, 0.001);
    EXPECT_NEAR(z, 0.0, 0.001);
}

TEST_F(CoordinateSearcherTest, ConvertFromCartesian) {
    auto [ra, dec] = searcher_->fromCartesian(1.0, 0.0, 0.0);
    EXPECT_NEAR(ra, 0.0, 0.001);
    EXPECT_NEAR(dec, 0.0, 0.001);
}

TEST_F(CoordinateSearcherTest, EmptyPointList) {
    std::vector<CoordinatePoint> empty;
    auto results = searcher_->searchWithinRadius(empty, 0.0, 0.0, 10.0);
    EXPECT_TRUE(results.empty());
}

TEST_F(CoordinateSearcherTest, ZeroRadius) {
    std::vector<CoordinatePoint> points = {
        {10.0, 41.0, "M31"}
    };
    
    auto results = searcher_->searchWithinRadius(points, 10.0, 41.0, 0.0);
    EXPECT_EQ(results.size(), 1); // Exact match
}
