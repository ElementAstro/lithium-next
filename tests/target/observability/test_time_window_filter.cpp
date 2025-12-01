// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>

#include "target/observability/time_window_filter.hpp"
#include "target/observability/visibility_calculator.hpp"
#include "tools/astronomy/coordinates.hpp"

namespace lithium::target::observability::test {

using namespace lithium::tools::astronomy;

class TimeWindowFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Observer location: Urbana, Illinois
        location = ObserverLocation(40.1125, -88.2434, 228.0);
        calculator = std::make_shared<VisibilityCalculator>(location);
        filter = std::make_unique<TimeWindowFilter>(calculator);

        // Create test objects
        for (int i = 0; i < 5; ++i) {
            CelestialObjectModel obj;
            obj.identifier = "TestObject" + std::to_string(i);
            obj.radJ2000 = i * 72.0;
            obj.decDJ2000 = 30.0 + i * 5.0;
            obj.visualMagnitudeV = 5.0 + i;
            obj.type = (i % 2 == 0) ? "Galaxy" : "Star";
            testObjects.push_back(obj);
        }
    }

    ObserverLocation location;
    std::shared_ptr<VisibilityCalculator> calculator;
    std::unique_ptr<TimeWindowFilter> filter;
    std::vector<CelestialObjectModel> testObjects;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(TimeWindowFilterTest, ConstructorWithValidCalculator) {
    EXPECT_NO_THROW({
        auto f = TimeWindowFilter(calculator);
    });
}

TEST_F(TimeWindowFilterTest, ConstructorWithNullCalculator) {
    EXPECT_THROW({
        auto f = TimeWindowFilter(nullptr);
    }, std::invalid_argument);
}

// ========================================================================
// Window Configuration Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, SetPresetTonight) {
    EXPECT_NO_THROW({
        filter->setPreset(TimeWindowFilter::Preset::Tonight);
    });
    EXPECT_EQ(filter->getCurrentPreset(), TimeWindowFilter::Preset::Tonight);
}

TEST_F(TimeWindowFilterTest, SetPresetThisWeek) {
    filter->setPreset(TimeWindowFilter::Preset::ThisWeek);
    EXPECT_EQ(filter->getCurrentPreset(), TimeWindowFilter::Preset::ThisWeek);
    auto [start, end] = filter->getTimeWindow();
    EXPECT_LT(start, end);
}

TEST_F(TimeWindowFilterTest, SetPresetThisMonth) {
    filter->setPreset(TimeWindowFilter::Preset::ThisMonth);
    EXPECT_EQ(filter->getCurrentPreset(), TimeWindowFilter::Preset::ThisMonth);
    auto [start, end] = filter->getTimeWindow();
    EXPECT_LT(start, end);
}

TEST_F(TimeWindowFilterTest, SetCustomWindow) {
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::hours(4);
    filter->setCustomWindow(start, end);

    auto [resultStart, resultEnd] = filter->getTimeWindow();
    EXPECT_EQ(resultStart, start);
    EXPECT_EQ(resultEnd, end);
    EXPECT_EQ(filter->getCurrentPreset(), TimeWindowFilter::Preset::Custom);
}

TEST_F(TimeWindowFilterTest, SetCustomWindowInvalidRange) {
    auto start = std::chrono::system_clock::now();
    auto end = start - std::chrono::hours(1);  // End before start
    EXPECT_THROW({
        filter->setCustomWindow(start, end);
    }, std::invalid_argument);
}

TEST_F(TimeWindowFilterTest, SetCustomWindowEqualTimes) {
    auto time = std::chrono::system_clock::now();
    EXPECT_THROW({
        filter->setCustomWindow(time, time);
    }, std::invalid_argument);
}

// ========================================================================
// Constraint Management Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, SetConstraints) {
    AltitudeConstraints constraints(30.0, 80.0);
    filter->setConstraints(constraints);

    auto retrievedConstraints = filter->getConstraints();
    EXPECT_NEAR(retrievedConstraints.minAltitude, 30.0, 0.01);
    EXPECT_NEAR(retrievedConstraints.maxAltitude, 80.0, 0.01);
}

TEST_F(TimeWindowFilterTest, ResetConstraints) {
    AltitudeConstraints customConstraints(50.0, 70.0);
    filter->setConstraints(customConstraints);
    filter->resetConstraints();

    auto defaults = filter->getConstraints();
    EXPECT_EQ(defaults.minAltitude, 20.0);  // Default min
    EXPECT_EQ(defaults.maxAltitude, 85.0);  // Default max
}

// ========================================================================
// Filtering Operations Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, Filter) {
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto filtered = filter->filter(testObjects);

    // Should return 0 or more objects
    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, FilterInRange) {
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::hours(24);
    auto filtered = filter->filterInRange(testObjects, start, end);

    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, FilterAtTime) {
    auto time = std::chrono::system_clock::now();
    auto filtered = filter->filterAtTime(testObjects, time);

    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, FilterByMinDuration) {
    auto filtered = filter->filterByMinDuration(testObjects,
                                                std::chrono::minutes(30));

    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, FilterByTransitAltitude) {
    auto filtered = filter->filterByTransitAltitude(testObjects, 30.0);

    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, FilterByMoonDistance) {
    auto filtered = filter->filterByMoonDistance(testObjects, 30.0);

    EXPECT_GE(filtered.size(), 0);
    EXPECT_LE(filtered.size(), testObjects.size());
}

// ========================================================================
// Sequence Optimization Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, OptimizeSequence) {
    auto startTime = std::chrono::system_clock::now();
    auto sequence = filter->optimizeSequence(testObjects, startTime);

    // Should return same number of objects
    EXPECT_EQ(sequence.size(), testObjects.size());

    // Each object should appear exactly once
    std::set<std::string> seenIds;
    for (const auto& [obj, time] : sequence) {
        seenIds.insert(obj.identifier);
        EXPECT_GE(time.time_since_epoch().count(), startTime.time_since_epoch().count());
    }
    EXPECT_EQ(seenIds.size(), testObjects.size());
}

TEST_F(TimeWindowFilterTest, GetOptimalStartTime) {
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto startTime = filter->getOptimalStartTime();

    auto now = std::chrono::system_clock::now();
    // Optimal start should be reasonably close to now (within 24 hours)
    auto diff = std::chrono::duration_cast<std::chrono::hours>(
        std::abs((startTime.time_since_epoch() - now.time_since_epoch()).count()));
    EXPECT_LT(std::abs(diff.count()), 24);
}

TEST_F(TimeWindowFilterTest, GetNightDurationSeconds) {
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto duration = filter->getNightDurationSeconds();

    // Night should be at least a few hours
    EXPECT_GT(duration, 3600);  // At least 1 hour

    // Night should be less than 24 hours
    EXPECT_LT(duration, 86400);  // Less than 24 hours
}

TEST_F(TimeWindowFilterTest, GetObjectDurationSeconds) {
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto duration = filter->getObjectDurationSeconds(270.0, 41.3);

    // Duration should be non-negative
    EXPECT_GE(duration, 0);

    // Duration should be less than night duration
    auto nightDuration = filter->getNightDurationSeconds();
    EXPECT_LE(duration, nightDuration);
}

// ========================================================================
// Statistics and Reporting Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, CountObservable) {
    size_t count = filter->countObservable(testObjects);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, testObjects.size());
}

TEST_F(TimeWindowFilterTest, GetStatistics) {
    auto stats = filter->getStatistics(testObjects);

    EXPECT_TRUE(stats.contains("total_objects"));
    EXPECT_TRUE(stats.contains("observable_now"));
    EXPECT_TRUE(stats.contains("night_duration_hours"));
    EXPECT_TRUE(stats.contains("window_type"));
    EXPECT_TRUE(stats.contains("constraints"));

    EXPECT_EQ(stats["total_objects"], testObjects.size());
}

TEST_F(TimeWindowFilterTest, GenerateObservingPlan) {
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto plan = filter->generateObservingPlan(testObjects);

    EXPECT_TRUE(plan.contains("start_time"));
    EXPECT_TRUE(plan.contains("end_time"));
    EXPECT_TRUE(plan.contains("night_duration_hours"));
    EXPECT_TRUE(plan.contains("observable_objects"));
    EXPECT_TRUE(plan.contains("observation_sequence"));
    EXPECT_TRUE(plan.contains("moon"));
    EXPECT_TRUE(plan.contains("sun"));

    // Check moon data
    auto moon = plan["moon"];
    EXPECT_TRUE(moon.contains("ra"));
    EXPECT_TRUE(moon.contains("dec"));
    EXPECT_TRUE(moon.contains("phase"));
    EXPECT_TRUE(moon.contains("above_horizon"));

    // Check observation sequence
    auto sequence = plan["observation_sequence"];
    EXPECT_TRUE(sequence.is_array());
    EXPECT_LE(sequence.size(), testObjects.size());
}

// ========================================================================
// Edge Cases and Stress Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, EmptyObjectList) {
    std::vector<CelestialObjectModel> emptyList;
    auto filtered = filter->filter(emptyList);
    EXPECT_EQ(filtered.size(), 0);
}

TEST_F(TimeWindowFilterTest, SingleObject) {
    std::vector<CelestialObjectModel> singleObj;
    singleObj.push_back(testObjects[0]);
    auto filtered = filter->filter(singleObj);

    EXPECT_LE(filtered.size(), 1);
}

TEST_F(TimeWindowFilterTest, LargeObjectList) {
    std::vector<CelestialObjectModel> largeList;
    for (int i = 0; i < 100; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "BigList" + std::to_string(i);
        obj.radJ2000 = (i * 3.6) % 360.0;
        obj.decDJ2000 = -60.0 + (i * 1.2) % 120.0;
        largeList.push_back(obj);
    }

    EXPECT_NO_THROW({
        auto filtered = filter->filter(largeList);
    });
}

TEST_F(TimeWindowFilterTest, MultiplePresetChanges) {
    EXPECT_NO_THROW({
        filter->setPreset(TimeWindowFilter::Preset::Tonight);
        filter->setPreset(TimeWindowFilter::Preset::ThisWeek);
        filter->setPreset(TimeWindowFilter::Preset::ThisMonth);

        auto start = std::chrono::system_clock::now();
        auto end = start + std::chrono::hours(12);
        filter->setCustomWindow(start, end);

        filter->setPreset(TimeWindowFilter::Preset::Tonight);
    });
}

TEST_F(TimeWindowFilterTest, RapidFilteringOperations) {
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            auto filtered1 = filter->filter(testObjects);
            auto filtered2 = filter->filterAtTime(testObjects,
                                                  std::chrono::system_clock::now());
            auto stats = filter->getStatistics(testObjects);
        }
    });
}

// ========================================================================
// Integration Tests
// ========================================================================

TEST_F(TimeWindowFilterTest, CompleteObservingWorkflow) {
    // Set location and time window
    filter->setPreset(TimeWindowFilter::Preset::Tonight);

    // Set constraints
    AltitudeConstraints constraints(25.0, 80.0);
    filter->setConstraints(constraints);

    // Filter objects
    auto observable = filter->filter(testObjects);

    // Optimize sequence
    auto sequence = filter->optimizeSequence(observable,
                                             std::chrono::system_clock::now());

    // Generate plan
    auto plan = filter->generateObservingPlan(observable);

    // Verify results
    EXPECT_GE(observable.size(), 0);
    EXPECT_EQ(sequence.size(), observable.size());
    EXPECT_TRUE(plan.contains("observation_sequence"));
}

TEST_F(TimeWindowFilterTest, DifferentTimezones) {
    // Test with different timezone settings
    calculator->setTimezone("America/New_York");
    filter->setPreset(TimeWindowFilter::Preset::Tonight);
    auto filtered1 = filter->filter(testObjects);

    calculator->setTimezone("UTC");
    auto filtered2 = filter->filter(testObjects);

    // Results should be similar (objects observable are same)
    EXPECT_LE(std::abs(static_cast<int>(filtered1.size()) -
                       static_cast<int>(filtered2.size())),
              1);
}

}  // namespace lithium::target::observability::test
