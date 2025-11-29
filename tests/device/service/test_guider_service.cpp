/*
 * test_guider_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "device/service/guider_service.hpp"

using namespace lithium::device;
using namespace testing;
using json = nlohmann::json;

// ============================================================================
// GuiderService Construction Tests
// ============================================================================

class GuiderServiceConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GuiderServiceConstructionTest, DefaultConstruction) {
    GuiderService service;
    // Service should be created successfully
    SUCCEED();
}

// ============================================================================
// GuiderService Connection Tests
// ============================================================================

class GuiderServiceConnectionTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceConnectionTest, GetConnectionStatusNotConnected) {
    auto result = service->getConnectionStatus();

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result.contains("data"));
    EXPECT_FALSE(result["data"]["connected"].get<bool>());
}

TEST_F(GuiderServiceConnectionTest, DisconnectWhenNotConnected) {
    auto result = service->disconnect();

    EXPECT_EQ(result["status"], "success");
}

// ============================================================================
// GuiderService Status Tests
// ============================================================================

class GuiderServiceStatusTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceStatusTest, GetStatusNotConnected) {
    auto result = service->getStatus();

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result.contains("data"));
    EXPECT_FALSE(result["data"]["connected"].get<bool>());
    EXPECT_EQ(result["data"]["state"], "DISCONNECTED");
}

TEST_F(GuiderServiceStatusTest, GetStatsNotConnected) {
    auto result = service->getStats();

    EXPECT_EQ(result["status"], "error");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(GuiderServiceStatusTest, GetCurrentStarNotConnected) {
    auto result = service->getCurrentStar();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Guiding Control Tests
// ============================================================================

class GuiderServiceGuidingTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceGuidingTest, StartGuidingNotConnected) {
    auto result = service->startGuiding();

    EXPECT_EQ(result["status"], "error");
    EXPECT_TRUE(result.contains("error"));
    EXPECT_EQ(result["error"]["code"], "device_not_found");
}

TEST_F(GuiderServiceGuidingTest, StopGuidingNotConnected) {
    auto result = service->stopGuiding();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceGuidingTest, PauseNotConnected) {
    auto result = service->pause();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceGuidingTest, ResumeNotConnected) {
    auto result = service->resume();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceGuidingTest, DitherNotConnected) {
    auto result = service->dither();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceGuidingTest, LoopNotConnected) {
    auto result = service->loop();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceGuidingTest, StopCaptureNotConnected) {
    auto result = service->stopCapture();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Calibration Tests
// ============================================================================

class GuiderServiceCalibrationTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceCalibrationTest, IsCalibratedNotConnected) {
    auto result = service->isCalibrated();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceCalibrationTest, ClearCalibrationNotConnected) {
    auto result = service->clearCalibration();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceCalibrationTest, FlipCalibrationNotConnected) {
    auto result = service->flipCalibration();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceCalibrationTest, GetCalibrationDataNotConnected) {
    auto result = service->getCalibrationData();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Star Selection Tests
// ============================================================================

class GuiderServiceStarSelectionTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceStarSelectionTest, FindStarNotConnected) {
    auto result = service->findStar();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceStarSelectionTest, FindStarWithROINotConnected) {
    auto result = service->findStar(100, 100, 200, 200);

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceStarSelectionTest, SetLockPositionNotConnected) {
    auto result = service->setLockPosition(512.0, 384.0);

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceStarSelectionTest, GetLockPositionNotConnected) {
    auto result = service->getLockPosition();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Camera Control Tests
// ============================================================================

class GuiderServiceCameraTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceCameraTest, GetExposureNotConnected) {
    auto result = service->getExposure();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceCameraTest, SetExposureNotConnected) {
    auto result = service->setExposure(1000);

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceCameraTest, GetExposureDurationsNotConnected) {
    auto result = service->getExposureDurations();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Guide Pulse Tests
// ============================================================================

class GuiderServicePulseTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServicePulseTest, GuidePulseNotConnected) {
    auto result = service->guidePulse("N", 500);

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServicePulseTest, GuidePulseWithAONotConnected) {
    auto result = service->guidePulse("E", 300, true);

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Algorithm Settings Tests
// ============================================================================

class GuiderServiceAlgorithmTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceAlgorithmTest, GetDecGuideModeNotConnected) {
    auto result = service->getDecGuideMode();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceAlgorithmTest, SetDecGuideModeNotConnected) {
    auto result = service->setDecGuideMode("Auto");

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceAlgorithmTest, GetAlgoParamNotConnected) {
    auto result = service->getAlgoParam("ra", "aggression");

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceAlgorithmTest, SetAlgoParamNotConnected) {
    auto result = service->setAlgoParam("ra", "aggression", 0.7);

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Equipment Tests
// ============================================================================

class GuiderServiceEquipmentTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceEquipmentTest, IsEquipmentConnectedNotConnected) {
    auto result = service->isEquipmentConnected();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceEquipmentTest, ConnectEquipmentNotConnected) {
    auto result = service->connectEquipment();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceEquipmentTest, DisconnectEquipmentNotConnected) {
    auto result = service->disconnectEquipment();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceEquipmentTest, GetEquipmentInfoNotConnected) {
    auto result = service->getEquipmentInfo();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Profile Tests
// ============================================================================

class GuiderServiceProfileTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceProfileTest, GetProfilesNotConnected) {
    auto result = service->getProfiles();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceProfileTest, GetCurrentProfileNotConnected) {
    auto result = service->getCurrentProfile();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceProfileTest, SetProfileNotConnected) {
    auto result = service->setProfile(1);

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Settings Tests
// ============================================================================

class GuiderServiceSettingsTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceSettingsTest, UpdateSettingsNotConnected) {
    json settings;
    settings["exposure"] = 1000;
    settings["decGuideMode"] = "Auto";

    auto result = service->updateSettings(settings);

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Lock Shift Tests
// ============================================================================

class GuiderServiceLockShiftTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceLockShiftTest, IsLockShiftEnabledNotConnected) {
    auto result = service->isLockShiftEnabled();

    EXPECT_EQ(result["status"], "error");
}

TEST_F(GuiderServiceLockShiftTest, SetLockShiftEnabledNotConnected) {
    auto result = service->setLockShiftEnabled(true);

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// GuiderService Shutdown Tests
// ============================================================================

class GuiderServiceShutdownTest : public ::testing::Test {
protected:
    std::unique_ptr<GuiderService> service;

    void SetUp() override { service = std::make_unique<GuiderService>(); }

    void TearDown() override { service.reset(); }
};

TEST_F(GuiderServiceShutdownTest, ShutdownNotConnected) {
    auto result = service->shutdown();

    EXPECT_EQ(result["status"], "error");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
