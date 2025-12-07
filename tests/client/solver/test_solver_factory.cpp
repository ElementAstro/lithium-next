/*
 * test_solver_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "client/common/solver_client.hpp"
#include "client/solver/service/solver_factory.hpp"

namespace lithium::solver {
namespace {

// Mock SolverClient for testing
class MockSolverClient : public client::SolverClient {
public:
    explicit MockSolverClient(std::string name = "mock")
        : SolverClient(std::move(name)) {}

    bool initialize() override { return true; }
    bool destroy() override { return true; }
    bool connect(const std::string&, int, int) override {
        connected_ = true;
        return true;
    }
    bool disconnect() override {
        connected_ = false;
        return true;
    }
    bool isConnected() const override { return connected_; }
    std::vector<std::string> scan() override { return {}; }

    client::PlateSolveResult solve(
        const std::string&,
        const std::optional<client::Coordinates>&,
        double, double, int, int) override {
        client::PlateSolveResult result;
        result.success = true;
        result.coordinates.ra = 180.0;
        result.coordinates.dec = 45.0;
        return result;
    }

    void abort() override {}

protected:
    std::string getOutputPath(const std::string& path) const override {
        return path + ".wcs";
    }

private:
    bool connected_{false};
};

class SolverFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& factory = SolverFactory::getInstance();
        factory.clear();
    }

    void TearDown() override {
        auto& factory = SolverFactory::getInstance();
        factory.clear();
    }
};

TEST_F(SolverFactoryTest, RegisterAndCreateSolver) {
    auto& factory = SolverFactory::getInstance();

    factory.registerCreator("MockSolver",
        [](const std::string& id, const nlohmann::json&) {
            return std::make_shared<MockSolverClient>(id);
        });

    EXPECT_TRUE(factory.hasCreator("MockSolver"));

    auto solver = factory.createSolver("MockSolver", {});
    ASSERT_NE(solver, nullptr);
}

TEST_F(SolverFactoryTest, UnregisterCreator) {
    auto& factory = SolverFactory::getInstance();

    factory.registerCreator("TempSolver",
        [](const std::string& id, const nlohmann::json&) {
            return std::make_shared<MockSolverClient>(id);
        });

    EXPECT_TRUE(factory.hasCreator("TempSolver"));

    factory.unregisterCreator("TempSolver");

    EXPECT_FALSE(factory.hasCreator("TempSolver"));
}

TEST_F(SolverFactoryTest, CreateNonexistentSolver) {
    auto& factory = SolverFactory::getInstance();

    auto solver = factory.createSolver("NonExistent", {});
    EXPECT_EQ(solver, nullptr);
}

TEST_F(SolverFactoryTest, PassConfigToCreator) {
    auto& factory = SolverFactory::getInstance();

    nlohmann::json capturedConfig;

    factory.registerCreator("ConfigTest",
        [&capturedConfig](const std::string& id, const nlohmann::json& config) {
            capturedConfig = config;
            return std::make_shared<MockSolverClient>(id);
        });

    nlohmann::json testConfig = {{"timeout", 60}, {"downsample", 2}};
    factory.createSolver("ConfigTest", testConfig);

    EXPECT_EQ(capturedConfig["timeout"], 60);
    EXPECT_EQ(capturedConfig["downsample"], 2);
}

TEST_F(SolverFactoryTest, GetRegisteredTypes) {
    auto& factory = SolverFactory::getInstance();

    factory.registerCreator("Type1",
        [](const std::string& id, const nlohmann::json&) {
            return std::make_shared<MockSolverClient>(id);
        });

    factory.registerCreator("Type2",
        [](const std::string& id, const nlohmann::json&) {
            return std::make_shared<MockSolverClient>(id);
        });

    auto types = factory.getRegisteredTypes();
    EXPECT_EQ(types.size(), 2);

    bool foundType1 = false, foundType2 = false;
    for (const auto& t : types) {
        if (t == "Type1") foundType1 = true;
        if (t == "Type2") foundType2 = true;
    }
    EXPECT_TRUE(foundType1);
    EXPECT_TRUE(foundType2);
}

TEST_F(SolverFactoryTest, PreventDuplicateCreator) {
    auto& factory = SolverFactory::getInstance();

    auto creator = [](const std::string& id, const nlohmann::json&) {
        return std::make_shared<MockSolverClient>(id);
    };

    EXPECT_TRUE(factory.registerCreator("Duplicate", creator));
    EXPECT_FALSE(factory.registerCreator("Duplicate", creator));
}

TEST_F(SolverFactoryTest, SolverFunctionalityAfterCreation) {
    auto& factory = SolverFactory::getInstance();

    factory.registerCreator("FunctionalTest",
        [](const std::string& id, const nlohmann::json&) {
            auto solver = std::make_shared<MockSolverClient>(id);
            solver->initialize();
            return solver;
        });

    auto solver = factory.createSolver("FunctionalTest", {});
    ASSERT_NE(solver, nullptr);

    // Test solve
    auto result = solver->solve("test.fits", std::nullopt, 1.0, 1.0, 0, 0);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.coordinates.ra, 180.0);
    EXPECT_EQ(result.coordinates.dec, 45.0);
}

}  // namespace
}  // namespace lithium::solver
