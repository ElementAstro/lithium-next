/*
 * test_guider_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for guider client base class

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/common/guider_client.hpp"

#include <chrono>
#include <thread>

using namespace lithium::client;

// ==================== Mock Guider Client ====================

class MockGuiderClient : public GuiderClient {
public:
    explicit MockGuiderClient(std::string name = "mock_guider")
        : GuiderClient(std::move(name)) {}

    bool initialize() override {
        setState(ClientState::Initialized);
        return true;
    }

    bool destroy() override {
        setState(ClientState::Uninitialized);
        return true;
    }

    bool connect(const std::string& /*target*/, int /*timeout*/,
                 int /*maxRetry*/) override {
        setState(ClientState::Connected);
        return true;
    }

    bool disconnect() override {
        setState(ClientState::Disconnected);
        return true;
    }

    bool isConnected() const override {
        return getState() == ClientState::Connected;
    }

    std::vector<std::string> scan() override { return {"localhost:4400"}; }

    std::future<bool> startGuiding(const SettleParams& settle,
                                   bool recalibrate) override {
        lastSettleParams_ = settle;
        lastRecalibrate_ = recalibrate;
        guiderState_.store(GuiderState::Guiding);

        return std::async(std::launch::async, [this]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(settleDelayMs_));
            return settleResult_;
        });
    }

    void stopGuiding() override { guiderState_.store(GuiderState::Stopped); }

    void pause(bool full) override {
        lastPauseFull_ = full;
        guiderState_.store(GuiderState::Paused);
    }

    void resume() override { guiderState_.store(GuiderState::Guiding); }

    std::future<bool> dither(const DitherParams& params) override {
        lastDitherParams_ = params;
        return std::async(std::launch::async, [this]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(ditherDelayMs_));
            return ditherResult_;
        });
    }

    void loop() override { guiderState_.store(GuiderState::Looping); }

    bool isCalibrated() const override { return calibrated_; }

    void clearCalibration() override { calibrated_ = false; }

    void flipCalibration() override { calibrationFlipped_ = true; }

    CalibrationData getCalibrationData() const override {
        return calibrationData_;
    }

    GuideStar findStar(const std::optional<std::array<int, 4>>& roi) override {
        lastRoi_ = roi;
        return foundStar_;
    }

    void setLockPosition(double x, double y, bool exact) override {
        lockPosition_ = {x, y};
        lastLockExact_ = exact;
    }

    std::optional<std::array<double, 2>> getLockPosition() const override {
        return lockPosition_;
    }

    int getExposure() const override { return exposure_; }

    void setExposure(int exposureMs) override { exposure_ = exposureMs; }

    std::vector<int> getExposureDurations() const override {
        return exposureDurations_;
    }

    GuiderState getGuiderState() const override { return guiderState_.load(); }

    GuideStats getGuideStats() const override { return guideStats_; }

    GuideStar getCurrentStar() const override { return currentStar_; }

    double getPixelScale() const override { return pixelScale_; }

    // Test helpers
    void setSettleResult(bool result) { settleResult_ = result; }
    void setSettleDelay(int ms) { settleDelayMs_ = ms; }
    void setDitherResult(bool result) { ditherResult_ = result; }
    void setDitherDelay(int ms) { ditherDelayMs_ = ms; }
    void setCalibrated(bool calibrated) { calibrated_ = calibrated; }
    void setCalibrationData(const CalibrationData& data) {
        calibrationData_ = data;
    }
    void setFoundStar(const GuideStar& star) { foundStar_ = star; }
    void setCurrentStar(const GuideStar& star) { currentStar_ = star; }
    void setGuideStats(const GuideStats& stats) { guideStats_ = stats; }
    void setPixelScale(double scale) { pixelScale_ = scale; }
    void setExposureDurations(const std::vector<int>& durations) {
        exposureDurations_ = durations;
    }

    SettleParams getLastSettleParams() const { return lastSettleParams_; }
    bool getLastRecalibrate() const { return lastRecalibrate_; }
    DitherParams getLastDitherParams() const { return lastDitherParams_; }
    bool getLastPauseFull() const { return lastPauseFull_; }
    std::optional<std::array<int, 4>> getLastRoi() const { return lastRoi_; }
    bool getLastLockExact() const { return lastLockExact_; }
    bool wasCalibrationFlipped() const { return calibrationFlipped_; }

private:
    bool settleResult_{true};
    int settleDelayMs_{10};
    bool ditherResult_{true};
    int ditherDelayMs_{10};
    bool calibrated_{false};
    bool calibrationFlipped_{false};
    CalibrationData calibrationData_;
    GuideStar foundStar_;
    GuideStar currentStar_;
    GuideStats guideStats_;
    double pixelScale_{1.0};
    int exposure_{1000};
    std::vector<int> exposureDurations_{100, 500, 1000, 2000, 5000};
    std::optional<std::array<double, 2>> lockPosition_;

    SettleParams lastSettleParams_;
    bool lastRecalibrate_{false};
    DitherParams lastDitherParams_;
    bool lastPauseFull_{false};
    std::optional<std::array<int, 4>> lastRoi_;
    bool lastLockExact_{true};
};

// ==================== GuiderState Tests ====================

TEST(GuiderStateTest, StateName) {
    MockGuiderClient guider;
    guider.initialize();
    guider.connect("localhost:4400");

    EXPECT_EQ(guider.getGuiderStateName(), "Stopped");

    guider.loop();
    EXPECT_EQ(guider.getGuiderStateName(), "Looping");

    guider.pause(false);
    EXPECT_EQ(guider.getGuiderStateName(), "Paused");
}

// ==================== SettleParams Tests ====================

TEST(SettleParamsTest, DefaultValues) {
    SettleParams params;
    EXPECT_DOUBLE_EQ(params.pixels, 1.5);
    EXPECT_DOUBLE_EQ(params.time, 10.0);
    EXPECT_DOUBLE_EQ(params.timeout, 60.0);
}

// ==================== DitherParams Tests ====================

TEST(DitherParamsTest, DefaultValues) {
    DitherParams params;
    EXPECT_DOUBLE_EQ(params.amount, 5.0);
    EXPECT_FALSE(params.raOnly);
}

// ==================== CalibrationData Tests ====================

TEST(CalibrationDataTest, DefaultValues) {
    CalibrationData data;
    EXPECT_FALSE(data.calibrated);
    EXPECT_DOUBLE_EQ(data.raRate, 0.0);
    EXPECT_DOUBLE_EQ(data.decRate, 0.0);
}

// ==================== GuideStar Tests ====================

TEST(GuideStarTest, DefaultValues) {
    GuideStar star;
    EXPECT_DOUBLE_EQ(star.x, 0.0);
    EXPECT_DOUBLE_EQ(star.y, 0.0);
    EXPECT_DOUBLE_EQ(star.snr, 0.0);
    EXPECT_FALSE(star.valid);
}

// ==================== GuideStats Tests ====================

TEST(GuideStatsTest, DefaultValues) {
    GuideStats stats;
    EXPECT_DOUBLE_EQ(stats.rmsRA, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsDec, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsTotal, 0.0);
    EXPECT_EQ(stats.sampleCount, 0);
}

// ==================== GuiderClient Tests ====================

class GuiderClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        guider_ = std::make_shared<MockGuiderClient>("test_guider");
        guider_->initialize();
        guider_->connect("localhost:4400");
    }

    std::shared_ptr<MockGuiderClient> guider_;
};

TEST_F(GuiderClientTest, Construction) {
    EXPECT_EQ(guider_->getName(), "test_guider");
    EXPECT_EQ(guider_->getType(), ClientType::Guider);
}

TEST_F(GuiderClientTest, Capabilities) {
    EXPECT_TRUE(guider_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(guider_->hasCapability(ClientCapability::Configure));
    EXPECT_TRUE(guider_->hasCapability(ClientCapability::AsyncOperation));
    EXPECT_TRUE(guider_->hasCapability(ClientCapability::StatusQuery));
    EXPECT_TRUE(guider_->hasCapability(ClientCapability::EventCallback));
}

TEST_F(GuiderClientTest, StartGuiding) {
    SettleParams settle{2.0, 15.0, 120.0};
    auto future = guider_->startGuiding(settle, true);

    EXPECT_EQ(guider_->getGuiderState(), GuiderState::Guiding);
    EXPECT_TRUE(guider_->isGuiding());

    auto result = future.get();
    EXPECT_TRUE(result);

    auto lastParams = guider_->getLastSettleParams();
    EXPECT_DOUBLE_EQ(lastParams.pixels, 2.0);
    EXPECT_DOUBLE_EQ(lastParams.time, 15.0);
    EXPECT_DOUBLE_EQ(lastParams.timeout, 120.0);
    EXPECT_TRUE(guider_->getLastRecalibrate());
}

TEST_F(GuiderClientTest, StopGuiding) {
    guider_->startGuiding({});
    guider_->stopGuiding();

    EXPECT_EQ(guider_->getGuiderState(), GuiderState::Stopped);
    EXPECT_FALSE(guider_->isGuiding());
}

TEST_F(GuiderClientTest, PauseResume) {
    guider_->startGuiding({});

    guider_->pause(true);
    EXPECT_TRUE(guider_->isPaused());
    EXPECT_TRUE(guider_->getLastPauseFull());

    guider_->resume();
    EXPECT_FALSE(guider_->isPaused());
    EXPECT_TRUE(guider_->isGuiding());
}

TEST_F(GuiderClientTest, Dither) {
    guider_->startGuiding({});

    DitherParams params;
    params.amount = 10.0;
    params.raOnly = true;
    params.settle.pixels = 1.0;

    auto future = guider_->dither(params);
    auto result = future.get();

    EXPECT_TRUE(result);
    auto lastParams = guider_->getLastDitherParams();
    EXPECT_DOUBLE_EQ(lastParams.amount, 10.0);
    EXPECT_TRUE(lastParams.raOnly);
}

TEST_F(GuiderClientTest, Loop) {
    guider_->loop();
    EXPECT_EQ(guider_->getGuiderState(), GuiderState::Looping);
}

TEST_F(GuiderClientTest, Calibration) {
    EXPECT_FALSE(guider_->isCalibrated());

    guider_->setCalibrated(true);
    EXPECT_TRUE(guider_->isCalibrated());

    guider_->clearCalibration();
    EXPECT_FALSE(guider_->isCalibrated());
}

TEST_F(GuiderClientTest, FlipCalibration) {
    guider_->setCalibrated(true);
    guider_->flipCalibration();
    EXPECT_TRUE(guider_->wasCalibrationFlipped());
}

TEST_F(GuiderClientTest, CalibrationData) {
    CalibrationData data;
    data.calibrated = true;
    data.raRate = 15.0;
    data.decRate = 14.5;
    data.raAngle = 90.0;
    data.decAngle = 0.0;

    guider_->setCalibrationData(data);
    auto retrieved = guider_->getCalibrationData();

    EXPECT_TRUE(retrieved.calibrated);
    EXPECT_DOUBLE_EQ(retrieved.raRate, 15.0);
    EXPECT_DOUBLE_EQ(retrieved.decRate, 14.5);
    EXPECT_DOUBLE_EQ(retrieved.raAngle, 90.0);
}

TEST_F(GuiderClientTest, FindStar) {
    GuideStar star;
    star.x = 512.5;
    star.y = 384.25;
    star.snr = 25.0;
    star.valid = true;

    guider_->setFoundStar(star);

    std::array<int, 4> roi = {100, 100, 200, 200};
    auto found = guider_->findStar(roi);

    EXPECT_TRUE(found.valid);
    EXPECT_DOUBLE_EQ(found.x, 512.5);
    EXPECT_DOUBLE_EQ(found.y, 384.25);
    EXPECT_DOUBLE_EQ(found.snr, 25.0);

    auto lastRoi = guider_->getLastRoi();
    EXPECT_TRUE(lastRoi.has_value());
    EXPECT_EQ((*lastRoi)[0], 100);
}

TEST_F(GuiderClientTest, LockPosition) {
    guider_->setLockPosition(256.0, 192.0, false);

    auto pos = guider_->getLockPosition();
    EXPECT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ((*pos)[0], 256.0);
    EXPECT_DOUBLE_EQ((*pos)[1], 192.0);
    EXPECT_FALSE(guider_->getLastLockExact());
}

TEST_F(GuiderClientTest, Exposure) {
    EXPECT_EQ(guider_->getExposure(), 1000);

    guider_->setExposure(2000);
    EXPECT_EQ(guider_->getExposure(), 2000);
}

TEST_F(GuiderClientTest, ExposureDurations) {
    auto durations = guider_->getExposureDurations();
    EXPECT_EQ(durations.size(), 5);
    EXPECT_EQ(durations[0], 100);
    EXPECT_EQ(durations[4], 5000);
}

TEST_F(GuiderClientTest, GuideStats) {
    GuideStats stats;
    stats.rmsRA = 0.5;
    stats.rmsDec = 0.4;
    stats.rmsTotal = 0.64;
    stats.peakRA = 1.2;
    stats.peakDec = 0.9;
    stats.sampleCount = 100;
    stats.snr = 20.0;

    guider_->setGuideStats(stats);
    auto retrieved = guider_->getGuideStats();

    EXPECT_DOUBLE_EQ(retrieved.rmsRA, 0.5);
    EXPECT_DOUBLE_EQ(retrieved.rmsDec, 0.4);
    EXPECT_DOUBLE_EQ(retrieved.rmsTotal, 0.64);
    EXPECT_EQ(retrieved.sampleCount, 100);
}

TEST_F(GuiderClientTest, CurrentStar) {
    GuideStar star;
    star.x = 500.0;
    star.y = 400.0;
    star.snr = 30.0;
    star.mass = 1000.0;
    star.valid = true;

    guider_->setCurrentStar(star);
    auto current = guider_->getCurrentStar();

    EXPECT_TRUE(current.valid);
    EXPECT_DOUBLE_EQ(current.x, 500.0);
    EXPECT_DOUBLE_EQ(current.y, 400.0);
    EXPECT_DOUBLE_EQ(current.snr, 30.0);
}

TEST_F(GuiderClientTest, PixelScale) {
    guider_->setPixelScale(1.5);
    EXPECT_DOUBLE_EQ(guider_->getPixelScale(), 1.5);
}

TEST_F(GuiderClientTest, SettleFails) {
    guider_->setSettleResult(false);

    auto future = guider_->startGuiding({});
    auto result = future.get();

    EXPECT_FALSE(result);
}

TEST_F(GuiderClientTest, DitherFails) {
    guider_->setDitherResult(false);

    auto future = guider_->dither({});
    auto result = future.get();

    EXPECT_FALSE(result);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
