// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for FilterEvaluator
 */

#include <gtest/gtest.h>
#include "target/search/filter_evaluator.hpp"

using namespace lithium::target::search;

class FilterEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<FilterEvaluator>();
        
        testObject_.identifier = "M31";
        testObject_.type = "Galaxy";
        testObject_.morphology = "Sb";
        testObject_.constellationEn = "Andromeda";
        testObject_.radJ2000 = 10.6847;
        testObject_.decDJ2000 = 41.2689;
        testObject_.visualMagnitudeV = 3.44;
    }

    std::unique_ptr<FilterEvaluator> evaluator_;
    CelestialObjectModel testObject_;
};

TEST_F(FilterEvaluatorTest, EmptyFilterMatchesAll) {
    CelestialSearchFilter filter;
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, TypeFilter) {
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.type = "Nebula";
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, MagnitudeRangeFilter) {
    CelestialSearchFilter filter;
    filter.minMagnitude = 0.0;
    filter.maxMagnitude = 5.0;
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.minMagnitude = 5.0;
    filter.maxMagnitude = 10.0;
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, CoordinateRangeFilter) {
    CelestialSearchFilter filter;
    filter.minRA = 0.0;
    filter.maxRA = 20.0;
    filter.minDec = 30.0;
    filter.maxDec = 50.0;
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.minRA = 100.0;
    filter.maxRA = 200.0;
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, ConstellationFilter) {
    CelestialSearchFilter filter;
    filter.constellation = "Andromeda";
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.constellation = "Orion";
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, MorphologyFilter) {
    CelestialSearchFilter filter;
    filter.morphology = "Sb";
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.morphology = "E0";
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, NamePatternFilter) {
    CelestialSearchFilter filter;
    filter.namePattern = "M*";
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.namePattern = "NGC*";
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, CombinedFilters) {
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    filter.minMagnitude = 0.0;
    filter.maxMagnitude = 5.0;
    filter.constellation = "Andromeda";
    EXPECT_TRUE(evaluator_->matches(testObject_, filter));
    
    filter.type = "Nebula"; // One mismatch
    EXPECT_FALSE(evaluator_->matches(testObject_, filter));
}

TEST_F(FilterEvaluatorTest, FilterMultipleObjects) {
    std::vector<CelestialObjectModel> objects;
    
    CelestialObjectModel obj1 = testObject_;
    obj1.identifier = "M31";
    obj1.type = "Galaxy";
    objects.push_back(obj1);
    
    CelestialObjectModel obj2;
    obj2.identifier = "M42";
    obj2.type = "Nebula";
    objects.push_back(obj2);
    
    CelestialSearchFilter filter;
    filter.type = "Galaxy";
    
    auto results = evaluator_->filter(objects, filter);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].identifier, "M31");
}

TEST_F(FilterEvaluatorTest, SortByMagnitude) {
    std::vector<CelestialObjectModel> objects;
    
    CelestialObjectModel obj1, obj2, obj3;
    obj1.visualMagnitudeV = 5.0;
    obj2.visualMagnitudeV = 3.0;
    obj3.visualMagnitudeV = 7.0;
    objects = {obj1, obj2, obj3};
    
    CelestialSearchFilter filter;
    filter.orderBy = "magnitude";
    filter.ascending = true;
    
    auto sorted = evaluator_->sort(objects, filter);
    EXPECT_DOUBLE_EQ(sorted[0].visualMagnitudeV, 3.0);
    EXPECT_DOUBLE_EQ(sorted[2].visualMagnitudeV, 7.0);
}
