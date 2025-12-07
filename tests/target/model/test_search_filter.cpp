// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for SearchFilter model
 */

#include <gtest/gtest.h>
#include "target/model/search_filter.hpp"

using namespace lithium::target::model;

class SearchFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        filter_.namePattern = "M*";
        filter_.type = "Galaxy";
        filter_.minMagnitude = 0.0;
        filter_.maxMagnitude = 10.0;
        filter_.limit = 50;
    }

    CelestialSearchFilter filter_;
};

TEST_F(SearchFilterTest, DefaultConstruction) {
    CelestialSearchFilter filter;
    EXPECT_TRUE(filter.namePattern.empty());
    EXPECT_TRUE(filter.type.empty());
    EXPECT_EQ(filter.limit, 100);
}

TEST_F(SearchFilterTest, NamePatternFilter) {
    EXPECT_EQ(filter_.namePattern, "M*");
}

TEST_F(SearchFilterTest, TypeFilter) { EXPECT_EQ(filter_.type, "Galaxy"); }

TEST_F(SearchFilterTest, MagnitudeRange) {
    EXPECT_DOUBLE_EQ(filter_.minMagnitude, 0.0);
    EXPECT_DOUBLE_EQ(filter_.maxMagnitude, 10.0);
    EXPECT_LT(filter_.minMagnitude, filter_.maxMagnitude);
}

TEST_F(SearchFilterTest, CoordinateRangeDefaults) {
    CelestialSearchFilter filter;
    EXPECT_DOUBLE_EQ(filter.minRA, 0.0);
    EXPECT_DOUBLE_EQ(filter.maxRA, 360.0);
    EXPECT_DOUBLE_EQ(filter.minDec, -90.0);
    EXPECT_DOUBLE_EQ(filter.maxDec, 90.0);
}

TEST_F(SearchFilterTest, LimitAndOffset) {
    EXPECT_EQ(filter_.limit, 50);
    filter_.offset = 10;
    EXPECT_EQ(filter_.offset, 10);
}

TEST_F(SearchFilterTest, OrderByField) {
    filter_.orderBy = "magnitude";
    filter_.ascending = false;
    EXPECT_EQ(filter_.orderBy, "magnitude");
    EXPECT_FALSE(filter_.ascending);
}

TEST_F(SearchFilterTest, ConstellationFilter) {
    filter_.constellation = "Andromeda";
    EXPECT_EQ(filter_.constellation, "Andromeda");
}

TEST_F(SearchFilterTest, MorphologyFilter) {
    filter_.morphology = "Sb";
    EXPECT_EQ(filter_.morphology, "Sb");
}

TEST_F(SearchFilterTest, ValidCoordinateRange) {
    filter_.minRA = 0.0;
    filter_.maxRA = 180.0;
    filter_.minDec = 0.0;
    filter_.maxDec = 45.0;

    EXPECT_GE(filter_.minRA, 0.0);
    EXPECT_LE(filter_.maxRA, 360.0);
    EXPECT_GE(filter_.minDec, -90.0);
    EXPECT_LE(filter_.maxDec, 90.0);
}
