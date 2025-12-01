/*
 * test_indi_gps.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI GPS Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_gps.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIGPSTest : public ::testing::Test {
protected:
    void SetUp() override { gps_ = std::make_unique<INDIGPS>("TestGPS"); }

    void TearDown() override { gps_.reset(); }

    std::unique_ptr<INDIGPS> gps_;
};

// ==================== Construction Tests ====================

TEST_F(INDIGPSTest, ConstructorSetsName) {
    EXPECT_EQ(gps_->getName(), "TestGPS");
}

TEST_F(INDIGPSTest, GetDeviceTypeReturnsGPS) {
    EXPECT_EQ(gps_->getDeviceType(), "GPS");
}

TEST_F(INDIGPSTest, InitialStateIsIdle) {
    EXPECT_EQ(gps_->getGPSState(), GPSState::Idle);
}

TEST_F(INDIGPSTest, InitiallyNoFix) {
    EXPECT_EQ(gps_->getFixType(), GPSFixType::NoFix);
    EXPECT_FALSE(gps_->hasFix());
}

// ==================== Position Tests ====================

TEST_F(INDIGPSTest, GetLatitudeReturnsValue) {
    auto lat = gps_->getLatitude();
    EXPECT_TRUE(lat.has_value());
}

TEST_F(INDIGPSTest, GetLongitudeReturnsValue) {
    auto lon = gps_->getLongitude();
    EXPECT_TRUE(lon.has_value());
}

TEST_F(INDIGPSTest, GetElevationReturnsValue) {
    auto elev = gps_->getElevation();
    EXPECT_TRUE(elev.has_value());
}

// ==================== Time Tests ====================

TEST_F(INDIGPSTest, SyncSystemTimeFailsWhenDisconnected) {
    EXPECT_FALSE(gps_->syncSystemTime());
}

// ==================== Refresh Tests ====================

TEST_F(INDIGPSTest, RefreshFailsWhenDisconnected) {
    EXPECT_FALSE(gps_->refresh());
}

// ==================== Status Tests ====================

TEST_F(INDIGPSTest, GetStatusReturnsValidJson) {
    auto status = gps_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("gpsState"));
    EXPECT_TRUE(status.contains("fixType"));
    EXPECT_TRUE(status.contains("hasFix"));
    EXPECT_TRUE(status.contains("position"));
    EXPECT_TRUE(status.contains("time"));
    EXPECT_TRUE(status.contains("satellite"));

    EXPECT_EQ(status["type"], "GPS");
    EXPECT_FALSE(status["hasFix"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(GPSPositionTest, ToJsonProducesValidOutput) {
    GPSPosition pos;
    pos.latitude = 45.5;
    pos.longitude = -75.5;
    pos.elevation = 100.0;
    pos.accuracy = 2.5;

    auto j = pos.toJson();

    EXPECT_DOUBLE_EQ(j["latitude"].get<double>(), 45.5);
    EXPECT_DOUBLE_EQ(j["longitude"].get<double>(), -75.5);
    EXPECT_DOUBLE_EQ(j["elevation"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(j["accuracy"].get<double>(), 2.5);
}

TEST(GPSTimeTest, ToJsonProducesValidOutput) {
    GPSTime time;
    time.year = 2024;
    time.month = 12;
    time.day = 15;
    time.hour = 10;
    time.minute = 30;
    time.second = 45.5;
    time.utcOffset = 0.0;

    auto j = time.toJson();

    EXPECT_EQ(j["year"].get<int>(), 2024);
    EXPECT_EQ(j["month"].get<int>(), 12);
    EXPECT_EQ(j["day"].get<int>(), 15);
    EXPECT_EQ(j["hour"].get<int>(), 10);
    EXPECT_EQ(j["minute"].get<int>(), 30);
    EXPECT_DOUBLE_EQ(j["second"].get<double>(), 45.5);
}

TEST(GPSSatelliteInfoTest, ToJsonProducesValidOutput) {
    GPSSatelliteInfo info;
    info.satellitesInView = 12;
    info.satellitesUsed = 8;
    info.hdop = 1.2;
    info.vdop = 1.5;
    info.pdop = 1.8;

    auto j = info.toJson();

    EXPECT_EQ(j["satellitesInView"].get<int>(), 12);
    EXPECT_EQ(j["satellitesUsed"].get<int>(), 8);
    EXPECT_DOUBLE_EQ(j["hdop"].get<double>(), 1.2);
    EXPECT_DOUBLE_EQ(j["vdop"].get<double>(), 1.5);
    EXPECT_DOUBLE_EQ(j["pdop"].get<double>(), 1.8);
}
