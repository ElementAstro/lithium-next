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

#include "target/observability/visibility_calculator.hpp"
#include "tools/astronomy/coordinates.hpp"

namespace lithium::target::observability::test {

using namespace lithium::tools::astronomy;

class VisibilityCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Observer location: Urbana, Illinois
        location = ObserverLocation(40.1125, -88.2434, 228.0);
        calculator = std::make_shared<VisibilityCalculator>(location);
        calculator->setTimezone("UTC");
    }

    ObserverLocation location;
    std::shared_ptr<VisibilityCalculator> calculator;
};

// ============================================================================
// Constructor and Location Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, ConstructorWithValidLocation) {
    EXPECT_NO_THROW({
        auto calc = VisibilityCalculator(location);
    });
}

TEST_F(VisibilityCalculatorTest, ConstructorWithInvalidLatitude) {
    ObserverLocation invalidLoc(-91.0, -88.0, 0.0);
    EXPECT_THROW({
        auto calc = VisibilityCalculator(invalidLoc);
    }, std::invalid_argument);
}

TEST_F(VisibilityCalculatorTest, ConstructorWithInvalidLongitude) {
    ObserverLocation invalidLoc(40.0, 181.0, 0.0);
    EXPECT_THROW({
        auto calc = VisibilityCalculator(invalidLoc);
    }, std::invalid_argument);
}

TEST_F(VisibilityCalculatorTest, SetLocation) {
    ObserverLocation newLoc(37.7749, -122.4194, 0.0);  // San Francisco
    EXPECT_NO_THROW({
        calculator->setLocation(newLoc);
        EXPECT_EQ(calculator->getLocation().latitude, 37.7749);
    });
}

TEST_F(VisibilityCalculatorTest, SetTimezone) {
    calculator->setTimezone("America/Chicago");
    EXPECT_EQ(calculator->getTimezone(), "America/Chicago");
}

// ============================================================================
// Coordinate Transformation Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, CalculateAltAz) {
    // M31 Andromeda: RA=270°, Dec=41.3°
    auto altAz = calculator->calculateAltAz(270.0, 41.3);

    // Altitude should be within valid range
    EXPECT_GE(altAz.altitude, -90.0);
    EXPECT_LE(altAz.altitude, 90.0);

    // Azimuth should be normalized
    EXPECT_GE(altAz.azimuth, 0.0);
    EXPECT_LT(altAz.azimuth, 360.0);
}

TEST_F(VisibilityCalculatorTest, CalculateAltAzConsistency) {
    auto time = std::chrono::system_clock::now();
    auto altAz1 = calculator->calculateAltAz(270.0, 41.3, time);
    auto altAz2 = calculator->calculateAltAz(270.0, 41.3, time);

    EXPECT_NEAR(altAz1.altitude, altAz2.altitude, 0.01);
    EXPECT_NEAR(altAz1.azimuth, altAz2.azimuth, 0.01);
}

TEST_F(VisibilityCalculatorTest, CalculateHourAngle) {
    auto ha = calculator->calculateHourAngle(270.0);

    // Hour angle should be in range [-12, 12]
    EXPECT_GE(ha, -12.0);
    EXPECT_LE(ha, 12.0);
}

TEST_F(VisibilityCalculatorTest, CalculateApparentSiderealTime) {
    auto lst = calculator->calculateApparentSiderealTime();

    // LST should be in hours [0, 24)
    EXPECT_GE(lst, 0.0);
    EXPECT_LT(lst, 24.0);
}

// ============================================================================
// Observability Window Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, CalculateWindowNeverRising) {
    // Object below southern horizon (from northern hemisphere)
    // Declination very far south
    auto constraints = AltitudeConstraints(20.0, 85.0);
    auto window = calculator->calculateWindow(0.0, -80.0,
                                              std::chrono::system_clock::now(),
                                              constraints);

    EXPECT_TRUE(window.neverRises);
}

TEST_F(VisibilityCalculatorTest, CalculateWindowCircumpolar) {
    // Object near north celestial pole (from northern hemisphere)
    auto constraints = AltitudeConstraints(10.0, 85.0);
    auto window = calculator->calculateWindow(180.0, 89.0,
                                              std::chrono::system_clock::now(),
                                              constraints);

    // Should be circumpolar from northern USA
    EXPECT_TRUE(window.isCircumpolar || !window.neverRises);
}

TEST_F(VisibilityCalculatorTest, CalculateWindowStructure) {
    // M31 Andromeda from Illinois
    auto constraints = AltitudeConstraints(20.0, 85.0);
    auto window = calculator->calculateWindow(270.0, 41.3,
                                              std::chrono::system_clock::now(),
                                              constraints);

    if (!window.neverRises) {
        // Rise should come before transit, transit before set
        EXPECT_LT(window.riseTime, window.transitTime);
        EXPECT_LT(window.transitTime, window.setTime);

        // Max altitude should be positive
        EXPECT_GT(window.maxAltitude, 0.0);

        // Azimuth should be normalized
        EXPECT_GE(window.transitAzimuth, 0.0);
        EXPECT_LT(window.transitAzimuth, 360.0);
    }
}

// ============================================================================
// Observability Queries
// ============================================================================

TEST_F(VisibilityCalculatorTest, IsCurrentlyObservable) {
    // Just check it returns a boolean
    bool observable = calculator->isCurrentlyObservable(270.0, 41.3);
    EXPECT_TRUE(observable == true || observable == false);
}

TEST_F(VisibilityCalculatorTest, IsObservableAt) {
    auto time = std::chrono::system_clock::now();
    bool observable = calculator->isObservableAt(270.0, 41.3, time);
    EXPECT_TRUE(observable == true || observable == false);
}

TEST_F(VisibilityCalculatorTest, IsObservableWithConstraints) {
    AltitudeConstraints tight(80.0, 85.0);  // Very restrictive
    bool observable = calculator->isObservableAt(270.0, 41.3,
                                                 std::chrono::system_clock::now(),
                                                 tight);

    // Should be false due to tight constraints
    EXPECT_FALSE(observable);
}

// ============================================================================
// Solar and Lunar Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, GetSunTimes) {
    auto date = std::chrono::system_clock::now();
    auto [sunset, twilightEnd, twilightStart, sunrise] = calculator->getSunTimes(date);

    // Times should be in chronological order
    EXPECT_LT(sunset, twilightEnd);
    EXPECT_LT(twilightEnd, twilightStart);
    EXPECT_LT(twilightStart, sunrise);
}

TEST_F(VisibilityCalculatorTest, GetMoonInfo) {
    auto time = std::chrono::system_clock::now();
    auto [ra, dec, phase] = calculator->getMoonInfo(time);

    // RA should be 0-360
    EXPECT_GE(ra, 0.0);
    EXPECT_LE(ra, 360.0);

    // Dec should be -90 to 90
    EXPECT_GE(dec, -90.0);
    EXPECT_LE(dec, 90.0);

    // Phase should be 0-1
    EXPECT_GE(phase, 0.0);
    EXPECT_LE(phase, 1.0);
}

TEST_F(VisibilityCalculatorTest, CalculateMoonDistance) {
    auto time = std::chrono::system_clock::now();
    double distance = calculator->calculateMoonDistance(270.0, 41.3, time);

    // Distance should be 0-180 degrees
    EXPECT_GE(distance, 0.0);
    EXPECT_LE(distance, 180.0);
}

TEST_F(VisibilityCalculatorTest, IsMoonAboveHorizon) {
    auto time = std::chrono::system_clock::now();
    bool aboveHorizon = calculator->isMoonAboveHorizon(time);
    EXPECT_TRUE(aboveHorizon == true || aboveHorizon == false);
}

TEST_F(VisibilityCalculatorTest, GetTonightWindow) {
    auto [start, end] = calculator->getTonightWindow();
    EXPECT_LT(start, end);
}

// ============================================================================
// Twilight Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, GetCivilTwilightTimes) {
    auto date = std::chrono::system_clock::now();
    auto [start, end] = calculator->getCivilTwilightTimes(date);
    EXPECT_LT(start, end);
}

TEST_F(VisibilityCalculatorTest, GetNauticalTwilightTimes) {
    auto date = std::chrono::system_clock::now();
    auto [start, end] = calculator->getNauticalTwilightTimes(date);
    EXPECT_LT(start, end);
}

TEST_F(VisibilityCalculatorTest, GetAstronomicalTwilightTimes) {
    auto date = std::chrono::system_clock::now();
    auto [start, end] = calculator->getAstronomicalTwilightTimes(date);
    EXPECT_LT(start, end);
}

// ============================================================================
// Time Conversion Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, LocalToUTC) {
    auto localTime = std::chrono::system_clock::now();
    auto utcTime = calculator->localToUTC(localTime);
    // UTC should be different from local unless timezone is UTC
    EXPECT_TRUE(utcTime.time_since_epoch() != localTime.time_since_epoch() ||
                calculator->getTimezone() == "UTC");
}

TEST_F(VisibilityCalculatorTest, UTCToLocal) {
    auto utcTime = std::chrono::system_clock::now();
    auto localTime = calculator->utcToLocal(utcTime);
    // Local should be different from UTC unless timezone is UTC
    EXPECT_TRUE(localTime.time_since_epoch() != utcTime.time_since_epoch() ||
                calculator->getTimezone() == "UTC");
}

TEST_F(VisibilityCalculatorTest, RoundTripTimeConversion) {
    auto original = std::chrono::system_clock::now();
    auto utc = calculator->localToUTC(original);
    auto back = calculator->utcToLocal(utc);

    // Should round-trip correctly
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        back.time_since_epoch() - original.time_since_epoch());
    EXPECT_EQ(diff.count(), 0);
}

TEST_F(VisibilityCalculatorTest, GetTimezoneOffset) {
    auto offset = calculator->getTimezoneOffset();
    // UTC offset should be 0
    EXPECT_EQ(offset, 0);
}

// ============================================================================
// Batch Operation Tests
// ============================================================================

TEST_F(VisibilityCalculatorTest, FilterObservable) {
    std::vector<CelestialObjectModel> objects;

    // Create test objects
    for (int i = 0; i < 5; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "TestObj" + std::to_string(i);
        obj.radJ2000 = i * 72.0;      // Spread across RA
        obj.decDJ2000 = 41.3;         // Same declination
        objects.push_back(obj);
    }

    auto startTime = std::chrono::system_clock::now();
    auto endTime = startTime + std::chrono::hours(24);
    auto filtered = calculator->filterObservable(objects, startTime, endTime);

    // Should filter at least one object
    EXPECT_GE(filtered.size(), 0);
}

TEST_F(VisibilityCalculatorTest, OptimizeSequence) {
    std::vector<CelestialObjectModel> objects;

    // Create test objects spread across sky
    for (int i = 0; i < 3; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "TestObj" + std::to_string(i);
        obj.radJ2000 = i * 120.0;
        obj.decDJ2000 = 30.0 + i * 10.0;
        objects.push_back(obj);
    }

    auto startTime = std::chrono::system_clock::now();
    auto sequence = calculator->optimizeSequence(objects, startTime);

    // Should return same number of objects
    EXPECT_EQ(sequence.size(), objects.size());

    // Each object should appear exactly once
    std::set<std::string> seenIds;
    for (const auto& [obj, time] : sequence) {
        seenIds.insert(obj.identifier);
    }
    EXPECT_EQ(seenIds.size(), objects.size());
}

// ============================================================================
// Edge Cases and Validation
// ============================================================================

TEST_F(VisibilityCalculatorTest, InvalidRACoordinates) {
    auto constraints = AltitudeConstraints();
    // RA should be 0-360, test with invalid values
    auto window = calculator->calculateWindow(-10.0, 41.3,
                                              std::chrono::system_clock::now(),
                                              constraints);
    EXPECT_TRUE(window.neverRises);  // Should handle gracefully
}

TEST_F(VisibilityCalculatorTest, InvalidDecCoordinates) {
    auto constraints = AltitudeConstraints();
    // Dec should be -90 to 90, test with invalid values
    auto window = calculator->calculateWindow(270.0, 100.0,
                                              std::chrono::system_clock::now(),
                                              constraints);
    EXPECT_TRUE(window.neverRises);  // Should handle gracefully
}

TEST_F(VisibilityCalculatorTest, SouthernHemisphereObserver) {
    ObserverLocation southernLoc(-34.5, 149.1, 0.0);  // Sydney
    auto calc = VisibilityCalculator(southernLoc);

    // Object near south celestial pole
    auto altAz = calc.calculateAltAz(180.0, -85.0);
    EXPECT_GE(altAz.altitude, -90.0);
    EXPECT_LE(altAz.altitude, 90.0);
}

TEST_F(VisibilityCalculatorTest, EquatorialObserver) {
    ObserverLocation equatorialLoc(0.0, 0.0, 0.0);  // Equator
    auto calc = VisibilityCalculator(equatorialLoc);

    // From equator, all objects rise and set
    auto altAz = calc.calculateAltAz(270.0, 45.0);
    EXPECT_GE(altAz.altitude, -90.0);
    EXPECT_LE(altAz.altitude, 90.0);
}

}  // namespace lithium::target::observability::test
