/*
 * test_indi_weather.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Weather Station Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_weather.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIWeatherTest : public ::testing::Test {
protected:
    void SetUp() override {
        weather_ = std::make_unique<INDIWeather>("TestWeather");
    }

    void TearDown() override { weather_.reset(); }

    std::unique_ptr<INDIWeather> weather_;
};

// ==================== Construction Tests ====================

TEST_F(INDIWeatherTest, ConstructorSetsName) {
    EXPECT_EQ(weather_->getName(), "TestWeather");
}

TEST_F(INDIWeatherTest, GetDeviceTypeReturnsWeather) {
    EXPECT_EQ(weather_->getDeviceType(), "Weather");
}

TEST_F(INDIWeatherTest, InitialStateIsIdle) {
    EXPECT_EQ(weather_->getWeatherState(), WeatherState::Idle);
}

// ==================== Weather Data Tests ====================

TEST_F(INDIWeatherTest, GetTemperatureReturnsValue) {
    auto temp = weather_->getTemperature();
    EXPECT_TRUE(temp.has_value());
}

TEST_F(INDIWeatherTest, GetHumidityReturnsValue) {
    auto humidity = weather_->getHumidity();
    EXPECT_TRUE(humidity.has_value());
}

TEST_F(INDIWeatherTest, GetPressureReturnsValue) {
    auto pressure = weather_->getPressure();
    EXPECT_TRUE(pressure.has_value());
}

TEST_F(INDIWeatherTest, GetWindSpeedReturnsValue) {
    auto wind = weather_->getWindSpeed();
    EXPECT_TRUE(wind.has_value());
}

TEST_F(INDIWeatherTest, GetWindDirectionReturnsValue) {
    auto dir = weather_->getWindDirection();
    EXPECT_TRUE(dir.has_value());
}

TEST_F(INDIWeatherTest, GetDewPointReturnsValue) {
    auto dew = weather_->getDewPoint();
    EXPECT_TRUE(dew.has_value());
}

TEST_F(INDIWeatherTest, GetSkyQualityReturnsValue) {
    auto sq = weather_->getSkyQuality();
    EXPECT_TRUE(sq.has_value());
}

TEST_F(INDIWeatherTest, IsRainingReturnsFalseInitially) {
    EXPECT_FALSE(weather_->isRaining());
}

TEST_F(INDIWeatherTest, IsSafeReturnsTrueInitially) {
    EXPECT_TRUE(weather_->isSafe());
}

// ==================== Location Tests ====================

TEST_F(INDIWeatherTest, SetLocationFailsWhenDisconnected) {
    EXPECT_FALSE(weather_->setLocation(45.0, -75.0, 100.0));
}

// ==================== Refresh Tests ====================

TEST_F(INDIWeatherTest, RefreshFailsWhenDisconnected) {
    EXPECT_FALSE(weather_->refresh());
}

TEST_F(INDIWeatherTest, SetRefreshPeriodFailsWhenDisconnected) {
    EXPECT_FALSE(weather_->setRefreshPeriod(120));
}

TEST_F(INDIWeatherTest, GetRefreshPeriodReturnsDefault) {
    EXPECT_EQ(weather_->getRefreshPeriod(), 60);
}

// ==================== Status Tests ====================

TEST_F(INDIWeatherTest, GetStatusReturnsValidJson) {
    auto status = weather_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("weatherState"));
    EXPECT_TRUE(status.contains("weather"));
    EXPECT_TRUE(status.contains("location"));
    EXPECT_TRUE(status.contains("refreshPeriod"));
    EXPECT_TRUE(status.contains("parameters"));

    EXPECT_EQ(status["type"], "Weather");
}

// ==================== Struct Tests ====================

TEST(WeatherParameterTest, ToJsonProducesValidOutput) {
    WeatherParameter param;
    param.name = "TEMPERATURE";
    param.label = "Temperature";
    param.value = 20.5;
    param.min = -40.0;
    param.max = 60.0;
    param.status = ParameterStatus::Ok;

    auto j = param.toJson();

    EXPECT_EQ(j["name"], "TEMPERATURE");
    EXPECT_EQ(j["label"], "Temperature");
    EXPECT_DOUBLE_EQ(j["value"].get<double>(), 20.5);
}

TEST(WeatherDataTest, ToJsonProducesValidOutput) {
    WeatherData data;
    data.temperature = 15.0;
    data.humidity = 65.0;
    data.pressure = 1013.25;
    data.windSpeed = 5.0;
    data.isRaining = false;
    data.isSafe = true;

    auto j = data.toJson();

    EXPECT_DOUBLE_EQ(j["temperature"].get<double>(), 15.0);
    EXPECT_DOUBLE_EQ(j["humidity"].get<double>(), 65.0);
    EXPECT_FALSE(j["isRaining"].get<bool>());
    EXPECT_TRUE(j["isSafe"].get<bool>());
}

TEST(LocationInfoTest, ToJsonProducesValidOutput) {
    LocationInfo loc;
    loc.latitude = 45.5;
    loc.longitude = -75.5;
    loc.elevation = 100.0;

    auto j = loc.toJson();

    EXPECT_DOUBLE_EQ(j["latitude"].get<double>(), 45.5);
    EXPECT_DOUBLE_EQ(j["longitude"].get<double>(), -75.5);
    EXPECT_DOUBLE_EQ(j["elevation"].get<double>(), 100.0);
}
