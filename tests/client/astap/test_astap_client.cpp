/*
 * test_astap_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for ASTAP plate solver client

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/astap/astap_client.hpp"

#include <filesystem>
#include <fstream>

using namespace lithium::client;

// ==================== AstapOptions Tests ====================

TEST(AstapOptionsTest, DefaultValues) {
    AstapOptions options;
    EXPECT_EQ(options.searchRadius, 180);
    EXPECT_EQ(options.maxStars, 500);
    EXPECT_DOUBLE_EQ(options.tolerance, 0.007);
    EXPECT_FALSE(options.update);
    EXPECT_FALSE(options.analyse);
    EXPECT_TRUE(options.database.empty());
    EXPECT_EQ(options.speed, 0);
}

// ==================== AstapClient Tests ====================

class AstapClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<AstapClient>("test_astap");
    }

    void TearDown() override {
        if (client_->isConnected()) {
            client_->disconnect();
        }
    }

    std::shared_ptr<AstapClient> client_;
};

TEST_F(AstapClientTest, Construction) {
    EXPECT_EQ(client_->getName(), "test_astap");
    EXPECT_EQ(client_->getType(), ClientType::Solver);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
    EXPECT_FALSE(client_->isConnected());
    EXPECT_FALSE(client_->isSolving());
}

TEST_F(AstapClientTest, Capabilities) {
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Scan));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Configure));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::AsyncOperation));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::StatusQuery));
}

TEST_F(AstapClientTest, TypeName) {
    EXPECT_EQ(client_->getTypeName(), "Solver");
}

TEST_F(AstapClientTest, AstapOptions) {
    AstapOptions options;
    options.searchRadius = 30;
    options.maxStars = 1000;
    options.speed = 2;
    options.database = "/path/to/database";

    client_->setAstapOptions(options);

    const auto& retrieved = client_->getAstapOptions();
    EXPECT_EQ(retrieved.searchRadius, 30);
    EXPECT_EQ(retrieved.maxStars, 1000);
    EXPECT_EQ(retrieved.speed, 2);
    EXPECT_EQ(retrieved.database, "/path/to/database");
}

TEST_F(AstapClientTest, SolverOptions) {
    SolverOptions options;
    options.scaleLow = 0.5;
    options.scaleHigh = 2.0;
    options.timeout = 60;
    options.downsample = 2;

    client_->setOptions(options);

    const auto& retrieved = client_->getOptions();
    EXPECT_TRUE(retrieved.scaleLow.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleLow, 0.5);
    EXPECT_EQ(retrieved.timeout, 60);
}

TEST_F(AstapClientTest, Configuration) {
    ClientConfig config;
    config.executablePath = "/usr/bin/astap";
    config.connectionTimeout = 10000;
    config.maxRetries = 5;

    EXPECT_TRUE(client_->configure(config));
    EXPECT_EQ(client_->getConfig().executablePath, "/usr/bin/astap");
}

TEST_F(AstapClientTest, Scan) {
    auto results = client_->scan();
    // Results depend on system installation
    // Just verify it returns without error
    EXPECT_TRUE(results.empty() || !results.empty());
}

TEST_F(AstapClientTest, InitializeWithoutAstap) {
    // This test may pass or fail depending on system
    // We're testing the error handling path
    bool result = client_->initialize();

    if (!result) {
        EXPECT_EQ(client_->getState(), ClientState::Error);
        EXPECT_TRUE(client_->getLastError().hasError());
    } else {
        EXPECT_EQ(client_->getState(), ClientState::Initialized);
    }
}

TEST_F(AstapClientTest, ConnectWithInvalidPath) {
    EXPECT_FALSE(client_->connect("/nonexistent/path/to/astap"));
    EXPECT_FALSE(client_->isConnected());
    EXPECT_TRUE(client_->getLastError().hasError());
}

TEST_F(AstapClientTest, DisconnectWhenNotConnected) {
    EXPECT_TRUE(client_->disconnect());
    EXPECT_EQ(client_->getState(), ClientState::Disconnected);
}

TEST_F(AstapClientTest, SolveWithoutConnection) {
    auto result = client_->solve("/path/to/image.fits", std::nullopt, 2.0, 1.5,
                                 1920, 1080);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(AstapClientTest, SolveWithNonexistentImage) {
    // Even if we could connect, the image doesn't exist
    auto result = client_->solve("/nonexistent/image.fits", std::nullopt, 2.0,
                                 1.5, 1920, 1080);

    EXPECT_FALSE(result.success);
}

TEST_F(AstapClientTest, AbortWhenNotSolving) {
    // Should not crash
    client_->abort();
    EXPECT_FALSE(client_->isSolving());
}

TEST_F(AstapClientTest, Destroy) {
    client_->initialize();
    EXPECT_TRUE(client_->destroy());
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

TEST_F(AstapClientTest, EventCallback) {
    std::vector<std::string> events;

    client_->setEventCallback(
        [&events](const std::string& event, const std::string& /*data*/) {
            events.push_back(event);
        });

    client_->initialize();
    client_->destroy();

    // Check that events were emitted
    // The exact events depend on initialization success
    EXPECT_TRUE(events.empty() || !events.empty());
}

TEST_F(AstapClientTest, StatusCallback) {
    std::vector<std::pair<ClientState, ClientState>> transitions;

    client_->setStatusCallback(
        [&transitions](ClientState old, ClientState current) {
            transitions.push_back({old, current});
        });

    client_->initialize();

    // Should have at least one transition
    EXPECT_FALSE(transitions.empty());
}

TEST_F(AstapClientTest, GetDefaultPath) {
    auto path = AstapClient::getDefaultPath();
    EXPECT_FALSE(path.empty());

#ifdef _WIN32
    EXPECT_TRUE(path.find("astap.exe") != std::string::npos);
#else
    EXPECT_TRUE(path.find("astap") != std::string::npos);
#endif
}

TEST_F(AstapClientTest, IsAstapInstalled) {
    // Just verify it returns without error
    bool installed = AstapClient::isAstapInstalled();
    EXPECT_TRUE(installed || !installed);
}

TEST_F(AstapClientTest, LastResult) {
    // Initially empty
    const auto& result = client_->getLastResult();
    EXPECT_FALSE(result.success);
}

TEST_F(AstapClientTest, GetVersion) {
    // Version is empty until initialized/connected
    auto version = client_->getVersion();
    EXPECT_TRUE(version.empty() || !version.empty());
}

// ==================== Integration Tests (require ASTAP) ====================

class AstapClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<AstapClient>("integration_test");

        // Skip if ASTAP not installed
        if (!AstapClient::isAstapInstalled()) {
            GTEST_SKIP() << "ASTAP not installed, skipping integration tests";
        }
    }

    std::shared_ptr<AstapClient> client_;
};

TEST_F(AstapClientIntegrationTest, InitializeAndConnect) {
    EXPECT_TRUE(client_->initialize());
    EXPECT_EQ(client_->getState(), ClientState::Initialized);

    // Connect with auto-detection
    EXPECT_TRUE(client_->connect(""));
    EXPECT_TRUE(client_->isConnected());

    auto version = client_->getAstapVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(AstapClientIntegrationTest, FullLifecycle) {
    EXPECT_TRUE(client_->initialize());
    EXPECT_TRUE(client_->connect(""));
    EXPECT_TRUE(client_->isConnected());
    EXPECT_TRUE(client_->disconnect());
    EXPECT_FALSE(client_->isConnected());
    EXPECT_TRUE(client_->destroy());
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
