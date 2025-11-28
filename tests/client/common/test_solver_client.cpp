/*
 * test_solver_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for solver client base class

*************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "client/common/solver_client.hpp"

#include <chrono>
#include <thread>

using namespace lithium::client;

// ==================== Mock Solver Client ====================

class MockSolverClient : public SolverClient {
public:
    explicit MockSolverClient(std::string name = "mock_solver")
        : SolverClient(std::move(name)) {}

    bool initialize() override {
        setState(ClientState::Initialized);
        return initializeResult_;
    }

    bool destroy() override {
        setState(ClientState::Uninitialized);
        return true;
    }

    bool connect(const std::string& target, int /*timeout*/, int /*maxRetry*/) override {
        if (connectResult_) {
            setState(ClientState::Connected);
            solverPath_ = target;
        }
        return connectResult_;
    }

    bool disconnect() override {
        setState(ClientState::Disconnected);
        solverPath_.clear();
        return true;
    }

    bool isConnected() const override {
        return getState() == ClientState::Connected;
    }

    std::vector<std::string> scan() override {
        return scanResults_;
    }

    PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight) override {

        lastImagePath_ = imageFilePath;
        lastCoordinates_ = initialCoordinates;
        lastFovW_ = fovW;
        lastFovH_ = fovH;
        lastImageWidth_ = imageWidth;
        lastImageHeight_ = imageHeight;

        solving_.store(true);

        // Simulate solve time
        if (solveDelayMs_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(solveDelayMs_));
        }

        if (abortRequested_.load()) {
            solving_.store(false);
            PlateSolveResult result;
            result.errorMessage = "Aborted";
            return result;
        }

        solving_.store(false);
        lastResult_ = solveResult_;
        return solveResult_;
    }

    // Test helpers
    void setInitializeResult(bool result) { initializeResult_ = result; }
    void setConnectResult(bool result) { connectResult_ = result; }
    void setScanResults(std::vector<std::string> results) { scanResults_ = std::move(results); }
    void setSolveResult(const PlateSolveResult& result) { solveResult_ = result; }
    void setSolveDelay(int ms) { solveDelayMs_ = ms; }

    std::string getLastImagePath() const { return lastImagePath_; }
    std::optional<Coordinates> getLastCoordinates() const { return lastCoordinates_; }
    double getLastFovW() const { return lastFovW_; }
    double getLastFovH() const { return lastFovH_; }
    int getLastImageWidth() const { return lastImageWidth_; }
    int getLastImageHeight() const { return lastImageHeight_; }

private:
    bool initializeResult_{true};
    bool connectResult_{true};
    std::vector<std::string> scanResults_;
    PlateSolveResult solveResult_;
    int solveDelayMs_{0};
    std::string solverPath_;

    std::string lastImagePath_;
    std::optional<Coordinates> lastCoordinates_;
    double lastFovW_{0};
    double lastFovH_{0};
    int lastImageWidth_{0};
    int lastImageHeight_{0};
};

// ==================== Coordinates Tests ====================

TEST(CoordinatesTest, DefaultConstruction) {
    Coordinates coords;
    EXPECT_EQ(coords.ra, 0.0);
    EXPECT_EQ(coords.dec, 0.0);
}

TEST(CoordinatesTest, IsValid) {
    Coordinates valid{180.0, 45.0};
    EXPECT_TRUE(valid.isValid());

    Coordinates invalidRA{-10.0, 45.0};
    EXPECT_FALSE(invalidRA.isValid());

    Coordinates invalidRA2{370.0, 45.0};
    EXPECT_FALSE(invalidRA2.isValid());

    Coordinates invalidDec{180.0, -100.0};
    EXPECT_FALSE(invalidDec.isValid());

    Coordinates invalidDec2{180.0, 100.0};
    EXPECT_FALSE(invalidDec2.isValid());

    Coordinates edgeCase{0.0, -90.0};
    EXPECT_TRUE(edgeCase.isValid());

    Coordinates edgeCase2{359.999, 90.0};
    EXPECT_TRUE(edgeCase2.isValid());
}

// ==================== PlateSolveResult Tests ====================

TEST(PlateSolveResultTest, DefaultConstruction) {
    PlateSolveResult result;
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.pixelScale, 0.0);
    EXPECT_EQ(result.positionAngle, 0.0);
    EXPECT_FALSE(result.flipped.has_value());
}

TEST(PlateSolveResultTest, Clear) {
    PlateSolveResult result;
    result.success = true;
    result.coordinates = {180.0, 45.0};
    result.pixelScale = 1.5;
    result.positionAngle = 90.0;
    result.flipped = true;
    result.errorMessage = "test error";

    result.clear();

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.coordinates.ra, 0.0);
    EXPECT_EQ(result.coordinates.dec, 0.0);
    EXPECT_EQ(result.pixelScale, 0.0);
    EXPECT_EQ(result.positionAngle, 0.0);
    EXPECT_FALSE(result.flipped.has_value());
    EXPECT_TRUE(result.errorMessage.empty());
}

// ==================== SolverOptions Tests ====================

TEST(SolverOptionsTest, DefaultValues) {
    SolverOptions options;
    EXPECT_FALSE(options.scaleLow.has_value());
    EXPECT_FALSE(options.scaleHigh.has_value());
    EXPECT_FALSE(options.searchCenter.has_value());
    EXPECT_FALSE(options.searchRadius.has_value());
    EXPECT_FALSE(options.downsample.has_value());
    EXPECT_FALSE(options.depth.has_value());
    EXPECT_EQ(options.timeout, 120);
    EXPECT_FALSE(options.generatePlots);
    EXPECT_TRUE(options.overwrite);
}

// ==================== SolverClient Tests ====================

class SolverClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        solver_ = std::make_shared<MockSolverClient>("test_solver");
    }

    std::shared_ptr<MockSolverClient> solver_;
};

TEST_F(SolverClientTest, Construction) {
    EXPECT_EQ(solver_->getName(), "test_solver");
    EXPECT_EQ(solver_->getType(), ClientType::Solver);
    EXPECT_FALSE(solver_->isSolving());
}

TEST_F(SolverClientTest, Capabilities) {
    EXPECT_TRUE(solver_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(solver_->hasCapability(ClientCapability::Scan));
    EXPECT_TRUE(solver_->hasCapability(ClientCapability::Configure));
    EXPECT_TRUE(solver_->hasCapability(ClientCapability::AsyncOperation));
    EXPECT_TRUE(solver_->hasCapability(ClientCapability::StatusQuery));
}

TEST_F(SolverClientTest, InitializeAndConnect) {
    EXPECT_TRUE(solver_->initialize());
    EXPECT_EQ(solver_->getState(), ClientState::Initialized);

    EXPECT_TRUE(solver_->connect("/usr/bin/solver"));
    EXPECT_TRUE(solver_->isConnected());
}

TEST_F(SolverClientTest, Solve) {
    solver_->initialize();
    solver_->connect("/usr/bin/solver");

    PlateSolveResult expectedResult;
    expectedResult.success = true;
    expectedResult.coordinates = {180.5, 45.25};
    expectedResult.pixelScale = 1.5;
    expectedResult.positionAngle = 90.0;
    solver_->setSolveResult(expectedResult);

    Coordinates hint{180.0, 45.0};
    auto result = solver_->solve("/path/to/image.fits", hint, 2.0, 1.5, 1920, 1080);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.coordinates.ra, 180.5);
    EXPECT_DOUBLE_EQ(result.coordinates.dec, 45.25);
    EXPECT_DOUBLE_EQ(result.pixelScale, 1.5);
    EXPECT_DOUBLE_EQ(result.positionAngle, 90.0);

    EXPECT_EQ(solver_->getLastImagePath(), "/path/to/image.fits");
    EXPECT_TRUE(solver_->getLastCoordinates().has_value());
    EXPECT_DOUBLE_EQ(solver_->getLastFovW(), 2.0);
    EXPECT_DOUBLE_EQ(solver_->getLastFovH(), 1.5);
    EXPECT_EQ(solver_->getLastImageWidth(), 1920);
    EXPECT_EQ(solver_->getLastImageHeight(), 1080);
}

TEST_F(SolverClientTest, SolveWithoutHint) {
    solver_->initialize();
    solver_->connect("/usr/bin/solver");

    PlateSolveResult expectedResult;
    expectedResult.success = true;
    solver_->setSolveResult(expectedResult);

    auto result = solver_->solve("/path/to/image.fits", std::nullopt, 2.0, 1.5, 1920, 1080);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(solver_->getLastCoordinates().has_value());
}

TEST_F(SolverClientTest, SolveAsync) {
    solver_->initialize();
    solver_->connect("/usr/bin/solver");

    PlateSolveResult expectedResult;
    expectedResult.success = true;
    expectedResult.coordinates = {100.0, 30.0};
    solver_->setSolveResult(expectedResult);
    solver_->setSolveDelay(50);  // 50ms delay

    auto future = solver_->solveAsync("/path/to/image.fits", std::nullopt, 2.0, 1.5, 1920, 1080);

    // Should be solving
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(solver_->isSolving());

    // Wait for result
    auto result = future.get();
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(solver_->isSolving());
}

TEST_F(SolverClientTest, Abort) {
    solver_->initialize();
    solver_->connect("/usr/bin/solver");
    solver_->setSolveDelay(500);  // Long delay

    auto future = solver_->solveAsync("/path/to/image.fits", std::nullopt, 2.0, 1.5, 1920, 1080);

    // Wait a bit then abort
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    solver_->abort();

    auto result = future.get();
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Aborted");
}

TEST_F(SolverClientTest, Options) {
    SolverOptions options;
    options.scaleLow = 0.5;
    options.scaleHigh = 2.0;
    options.timeout = 60;
    options.downsample = 2;

    solver_->setOptions(options);

    const auto& retrieved = solver_->getOptions();
    EXPECT_TRUE(retrieved.scaleLow.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleLow, 0.5);
    EXPECT_TRUE(retrieved.scaleHigh.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.scaleHigh, 2.0);
    EXPECT_EQ(retrieved.timeout, 60);
    EXPECT_TRUE(retrieved.downsample.has_value());
    EXPECT_EQ(*retrieved.downsample, 2);
}

TEST_F(SolverClientTest, LastResult) {
    solver_->initialize();
    solver_->connect("/usr/bin/solver");

    PlateSolveResult expectedResult;
    expectedResult.success = true;
    expectedResult.coordinates = {200.0, 50.0};
    solver_->setSolveResult(expectedResult);

    solver_->solve("/path/to/image.fits", std::nullopt, 2.0, 1.5, 1920, 1080);

    const auto& lastResult = solver_->getLastResult();
    EXPECT_TRUE(lastResult.success);
    EXPECT_DOUBLE_EQ(lastResult.coordinates.ra, 200.0);
    EXPECT_DOUBLE_EQ(lastResult.coordinates.dec, 50.0);
}

// ==================== Utility Function Tests ====================

TEST(SolverUtilityTest, ToRadians) {
    // Test via derived class static methods would require exposing them
    // For now, test the math directly
    double degrees = 180.0;
    double radians = degrees * M_PI / 180.0;
    EXPECT_DOUBLE_EQ(radians, M_PI);

    degrees = 90.0;
    radians = degrees * M_PI / 180.0;
    EXPECT_DOUBLE_EQ(radians, M_PI / 2.0);
}

TEST(SolverUtilityTest, ToDegrees) {
    double radians = M_PI;
    double degrees = radians * 180.0 / M_PI;
    EXPECT_DOUBLE_EQ(degrees, 180.0);
}

TEST(SolverUtilityTest, ArcsecToDegree) {
    double arcsec = 3600.0;
    double degrees = arcsec / 3600.0;
    EXPECT_DOUBLE_EQ(degrees, 1.0);

    arcsec = 1.0;
    degrees = arcsec / 3600.0;
    EXPECT_NEAR(degrees, 0.000277778, 0.000001);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
