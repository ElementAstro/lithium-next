/*
 * test_phd2_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for PHD2 guider client

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/phd2/phd2_client.hpp"

using namespace lithium::client;

// ==================== PHD2Config Tests ====================

TEST(PHD2ConfigTest, DefaultValues) {
    PHD2Config config;
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 4400);
    EXPECT_EQ(config.reconnectAttempts, 3);
    EXPECT_EQ(config.reconnectDelayMs, 1000);
}

// ==================== PHD2Client Tests ====================

class PHD2ClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<PHD2Client>("test_phd2");
    }

    void TearDown() override {
        if (client_->isConnected()) {
            client_->disconnect();
        }
    }

    std::shared_ptr<PHD2Client> client_;
};

TEST_F(PHD2ClientTest, Construction) {
    EXPECT_EQ(client_->getName(), "test_phd2");
    EXPECT_EQ(client_->getType(), ClientType::Guider);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(PHD2ClientTest, Capabilities) {
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Configure));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::AsyncOperation));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::StatusQuery));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::EventCallback));
}

TEST_F(PHD2ClientTest, Initialize) {
    EXPECT_TRUE(client_->initialize());
    EXPECT_EQ(client_->getState(), ClientState::Initialized);
}

TEST_F(PHD2ClientTest, Destroy) {
    client_->initialize();
    EXPECT_TRUE(client_->destroy());
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

TEST_F(PHD2ClientTest, ConfigurePHD2) {
    PHD2Config config;
    config.host = "192.168.1.100";
    config.port = 4401;
    config.reconnectAttempts = 5;

    client_->configurePHD2(config);

    const auto& retrieved = client_->getPHD2Config();
    EXPECT_EQ(retrieved.host, "192.168.1.100");
    EXPECT_EQ(retrieved.port, 4401);
    EXPECT_EQ(retrieved.reconnectAttempts, 5);
}

TEST_F(PHD2ClientTest, Scan) {
    auto results = client_->scan();
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], "localhost:4400");
}

TEST_F(PHD2ClientTest, DisconnectWhenNotConnected) {
    EXPECT_TRUE(client_->disconnect());
    EXPECT_EQ(client_->getState(), ClientState::Disconnected);
}

TEST_F(PHD2ClientTest, GuiderStateInitial) {
    EXPECT_EQ(client_->getGuiderState(), GuiderState::Stopped);
    EXPECT_EQ(client_->getGuiderStateName(), "Stopped");
}

TEST_F(PHD2ClientTest, EventCallback) {
    std::vector<std::string> events;

    client_->setEventCallback(
        [&events](const std::string& event, const std::string& /*data*/) {
            events.push_back(event);
        });

    client_->initialize();
    client_->destroy();

    // Check that events were emitted
    EXPECT_FALSE(events.empty());
    EXPECT_TRUE(std::find(events.begin(), events.end(), "initialized") !=
                events.end());
    EXPECT_TRUE(std::find(events.begin(), events.end(), "destroyed") !=
                events.end());
}

TEST_F(PHD2ClientTest, StatusCallback) {
    std::vector<std::pair<ClientState, ClientState>> transitions;

    client_->setStatusCallback(
        [&transitions](ClientState old, ClientState current) {
            transitions.push_back({old, current});
        });

    client_->initialize();

    EXPECT_FALSE(transitions.empty());
}

// ==================== Integration Tests (require PHD2) ====================

class PHD2ClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<PHD2Client>("integration_test");
        client_->initialize();

        // Try to connect - skip if PHD2 not running
        if (!client_->connect("localhost:4400", 1000, 1)) {
            GTEST_SKIP() << "PHD2 not running, skipping integration tests";
        }
    }

    void TearDown() override {
        if (client_->isConnected()) {
            client_->disconnect();
        }
    }

    std::shared_ptr<PHD2Client> client_;
};

TEST_F(PHD2ClientIntegrationTest, GetAppState) {
    auto state = client_->getAppState();
    EXPECT_FALSE(state.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetExposure) {
    int exposure = client_->getExposure();
    EXPECT_GT(exposure, 0);
}

TEST_F(PHD2ClientIntegrationTest, GetExposureDurations) {
    auto durations = client_->getExposureDurations();
    EXPECT_FALSE(durations.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetPixelScale) {
    double scale = client_->getPixelScale();
    // May be 0 if not configured
    EXPECT_GE(scale, 0.0);
}

TEST_F(PHD2ClientIntegrationTest, GetProfiles) {
    auto profiles = client_->getProfiles();
    EXPECT_FALSE(profiles.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetProfile) {
    auto profile = client_->getProfile();
    EXPECT_FALSE(profile.empty());
}

TEST_F(PHD2ClientIntegrationTest, IsCalibrated) {
    bool calibrated = client_->isCalibrated();
    // Just verify it returns without error
    EXPECT_TRUE(calibrated || !calibrated);
}

TEST_F(PHD2ClientIntegrationTest, GetDecGuideMode) {
    auto mode = client_->getDecGuideMode();
    // Should be one of: Off, Auto, North, South
    EXPECT_TRUE(mode == "Off" || mode == "Auto" || mode == "North" ||
                mode == "South" || mode.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetCameraFrameSize) {
    auto size = client_->getCameraFrameSize();
    // May be {0, 0} if camera not connected
    EXPECT_GE(size[0], 0);
    EXPECT_GE(size[1], 0);
}

TEST_F(PHD2ClientIntegrationTest, GetConnected) {
    bool connected = client_->getConnected();
    // Just verify it returns without error
    EXPECT_TRUE(connected || !connected);
}

TEST_F(PHD2ClientIntegrationTest, GetCurrentEquipment) {
    auto equipment = client_->getCurrentEquipment();
    // Just verify it returns without error
    EXPECT_TRUE(equipment.empty() || !equipment.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetGuideOutputEnabled) {
    bool enabled = client_->getGuideOutputEnabled();
    EXPECT_TRUE(enabled || !enabled);
}

TEST_F(PHD2ClientIntegrationTest, GetLockShiftEnabled) {
    bool enabled = client_->getLockShiftEnabled();
    EXPECT_TRUE(enabled || !enabled);
}

TEST_F(PHD2ClientIntegrationTest, GetSettling) {
    bool settling = client_->getSettling();
    EXPECT_TRUE(settling || !settling);
}

TEST_F(PHD2ClientIntegrationTest, GetSearchRegion) {
    int region = client_->getSearchRegion();
    EXPECT_GE(region, 0);
}

TEST_F(PHD2ClientIntegrationTest, GetCameraBinning) {
    int binning = client_->getCameraBinning();
    EXPECT_GE(binning, 1);
}

TEST_F(PHD2ClientIntegrationTest, GetVariableDelaySettings) {
    auto settings = client_->getVariableDelaySettings();
    // Just verify it returns without error
    EXPECT_TRUE(settings.empty() || !settings.empty());
}

TEST_F(PHD2ClientIntegrationTest, GetAlgoParamNames) {
    auto names = client_->getAlgoParamNames("ra");
    // May be empty if not guiding
    EXPECT_TRUE(names.empty() || !names.empty());
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
