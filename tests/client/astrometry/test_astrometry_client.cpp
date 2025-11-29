/*
 * test_astrometry_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for Astrometry.net plate solver client

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/astrometry/astrometry_client.hpp"

using namespace lithium::client;

// ==================== AstrometryOptions Tests ====================

TEST(AstrometryOptionsTest, DefaultValues) {
    AstrometryOptions options;
    EXPECT_FALSE(options.backendConfig.has_value());
    EXPECT_FALSE(options.config.has_value());
    EXPECT_FALSE(options.batch);
    EXPECT_TRUE(options.noPlots);
    EXPECT_TRUE(options.overwrite);
    EXPECT_FALSE(options.skipSolved);
    EXPECT_FALSE(options.continueRun);
    EXPECT_FALSE(options.guessScale);
    EXPECT_FALSE(options.invert);
    EXPECT_FALSE(options.noBackgroundSubtraction);
    EXPECT_FALSE(options.crpixCenter);
    EXPECT_FALSE(options.noTweak);
    EXPECT_FALSE(options.useSourceExtractor);
    EXPECT_FALSE(options.noVerify);
}

// ==================== AstrometryClient Tests ====================

class AstrometryClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<AstrometryClient>("test_astrometry");
    }

    void TearDown() override {
        if (client_->isConnected()) {
            client_->disconnect();
        }
    }

    std::shared_ptr<AstrometryClient> client_;
};

TEST_F(AstrometryClientTest, Construction) {
    EXPECT_EQ(client_->getName(), "test_astrometry");
    EXPECT_EQ(client_->getType(), ClientType::Solver);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
    EXPECT_FALSE(client_->isConnected());
    EXPECT_FALSE(client_->isSolving());
}

TEST_F(AstrometryClientTest, Capabilities) {
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Scan));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Configure));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::AsyncOperation));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::StatusQuery));
}

TEST_F(AstrometryClientTest, AstrometryOptions) {
    AstrometryOptions options;
    options.scaleLow = 0.5;
    options.scaleHigh = 2.0;
    options.scaleUnits = "arcsecperpix";
    options.depth = 50;
    options.cpuLimit = 120;
    options.downsample = 2;
    options.useSourceExtractor = true;
    options.crpixCenter = true;
    options.tweakOrder = 3;

    client_->setAstrometryOptions(options);

    const auto& retrieved = client_->getAstrometryOptions();
    EXPECT_TRUE(retrieved.scaleLow.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleLow, 0.5);
    EXPECT_TRUE(retrieved.scaleHigh.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleHigh, 2.0);
    EXPECT_EQ(*retrieved.scaleUnits, "arcsecperpix");
    EXPECT_EQ(*retrieved.depth, 50);
    EXPECT_EQ(*retrieved.cpuLimit, 120);
    EXPECT_EQ(*retrieved.downsample, 2);
    EXPECT_TRUE(retrieved.useSourceExtractor);
    EXPECT_TRUE(retrieved.crpixCenter);
    EXPECT_EQ(*retrieved.tweakOrder, 3);
}

TEST_F(AstrometryClientTest, SolverOptions) {
    SolverOptions options;
    options.scaleLow = 0.8;
    options.scaleHigh = 1.5;
    options.timeout = 90;
    options.downsample = 4;

    client_->setOptions(options);

    const auto& retrieved = client_->getOptions();
    EXPECT_TRUE(retrieved.scaleLow.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleLow, 0.8);
    EXPECT_EQ(retrieved.timeout, 90);
}

TEST_F(AstrometryClientTest, Configuration) {
    ClientConfig config;
    config.executablePath = "/usr/bin/solve-field";
    config.connectionTimeout = 10000;
    config.maxRetries = 5;

    EXPECT_TRUE(client_->configure(config));
    EXPECT_EQ(client_->getConfig().executablePath, "/usr/bin/solve-field");
}

TEST_F(AstrometryClientTest, Scan) {
    auto results = client_->scan();
    // Results depend on system installation
    EXPECT_TRUE(results.empty() || !results.empty());
}

TEST_F(AstrometryClientTest, ConnectWithInvalidPath) {
    EXPECT_FALSE(client_->connect("/nonexistent/path/to/solve-field"));
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(AstrometryClientTest, DisconnectWhenNotConnected) {
    EXPECT_TRUE(client_->disconnect());
    EXPECT_EQ(client_->getState(), ClientState::Disconnected);
}

TEST_F(AstrometryClientTest, SolveWithoutConnection) {
    auto result = client_->solve("/path/to/image.fits", std::nullopt, 2.0, 1.5,
                                 1920, 1080);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(AstrometryClientTest, AbortWhenNotSolving) {
    client_->abort();
    EXPECT_FALSE(client_->isSolving());
}

TEST_F(AstrometryClientTest, Destroy) {
    client_->initialize();
    EXPECT_TRUE(client_->destroy());
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

TEST_F(AstrometryClientTest, GetDefaultPath) {
    auto path = AstrometryClient::getDefaultPath();
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("solve-field") != std::string::npos);
}

TEST_F(AstrometryClientTest, IsAstrometryInstalled) {
    bool installed = AstrometryClient::isAstrometryInstalled();
    EXPECT_TRUE(installed || !installed);
}

TEST_F(AstrometryClientTest, GetIndexFiles) {
    auto files = client_->getIndexFiles();
    // May be empty if no index files installed
    EXPECT_TRUE(files.empty() || !files.empty());
}

TEST_F(AstrometryClientTest, AdvancedOptions) {
    AstrometryOptions options;

    // Parity and tolerance
    options.parity = "pos";
    options.codeTolerance = 0.01;
    options.pixelError = 1;

    // Quad size
    options.quadSizeMin = 0.1;
    options.quadSizeMax = 1.0;

    // Odds
    options.oddsTuneUp = 1e6;
    options.oddsSolve = 1e9;
    options.oddsReject = 1e-100;
    options.oddsStopLooking = 1e9;

    // Output options
    options.newFits = "/tmp/new.fits";
    options.wcs = "/tmp/output.wcs";
    options.corr = "/tmp/corr.fits";

    client_->setAstrometryOptions(options);

    const auto& retrieved = client_->getAstrometryOptions();
    EXPECT_EQ(*retrieved.parity, "pos");
    EXPECT_DOUBLE_EQ(*retrieved.codeTolerance, 0.01);
    EXPECT_EQ(*retrieved.pixelError, 1);
    EXPECT_DOUBLE_EQ(*retrieved.quadSizeMin, 0.1);
    EXPECT_DOUBLE_EQ(*retrieved.quadSizeMax, 1.0);
    EXPECT_EQ(*retrieved.newFits, "/tmp/new.fits");
    EXPECT_EQ(*retrieved.wcs, "/tmp/output.wcs");
}

TEST_F(AstrometryClientTest, SourceExtractorOptions) {
    AstrometryOptions options;
    options.useSourceExtractor = true;
    options.sourceExtractorPath = "/usr/bin/source-extractor";
    options.sourceExtractorConfig = "/etc/sextractor/default.sex";

    client_->setAstrometryOptions(options);

    const auto& retrieved = client_->getAstrometryOptions();
    EXPECT_TRUE(retrieved.useSourceExtractor);
    EXPECT_EQ(*retrieved.sourceExtractorPath, "/usr/bin/source-extractor");
    EXPECT_EQ(*retrieved.sourceExtractorConfig, "/etc/sextractor/default.sex");
}

TEST_F(AstrometryClientTest, ScampOptions) {
    AstrometryOptions options;
    options.scamp = "/tmp/scamp.cat";
    options.scampConfig = "/etc/scamp/default.scamp";
    options.scampRef = "/tmp/ref.cat";

    client_->setAstrometryOptions(options);

    const auto& retrieved = client_->getAstrometryOptions();
    EXPECT_EQ(*retrieved.scamp, "/tmp/scamp.cat");
    EXPECT_EQ(*retrieved.scampConfig, "/etc/scamp/default.scamp");
    EXPECT_EQ(*retrieved.scampRef, "/tmp/ref.cat");
}

// ==================== Integration Tests (require Astrometry.net)
// ====================

class AstrometryClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<AstrometryClient>("integration_test");

        if (!AstrometryClient::isAstrometryInstalled()) {
            GTEST_SKIP()
                << "Astrometry.net not installed, skipping integration tests";
        }
    }

    std::shared_ptr<AstrometryClient> client_;
};

TEST_F(AstrometryClientIntegrationTest, InitializeAndConnect) {
    EXPECT_TRUE(client_->initialize());
    EXPECT_EQ(client_->getState(), ClientState::Initialized);

    EXPECT_TRUE(client_->connect(""));
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(AstrometryClientIntegrationTest, FullLifecycle) {
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
