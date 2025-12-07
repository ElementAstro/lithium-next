/*
 * test_solver_type_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "client/solver/service/solver_type_registry.hpp"

namespace lithium::solver {
namespace {

class SolverTypeRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing registrations
        auto& registry = SolverTypeRegistry::getInstance();
        registry.clear();
    }

    void TearDown() override {
        auto& registry = SolverTypeRegistry::getInstance();
        registry.clear();
    }

    SolverTypeInfo createTestType(const std::string& name, int priority = 50) {
        SolverTypeInfo info;
        info.typeName = name;
        info.displayName = name + " Display";
        info.pluginName = "TestPlugin";
        info.version = "1.0.0";
        info.description = "Test solver";
        info.priority = priority;
        info.enabled = true;
        info.capabilities.supportsBlindSolve = true;
        info.capabilities.supportsHintedSolve = true;
        return info;
    }
};

TEST_F(SolverTypeRegistryTest, RegisterAndRetrieveType) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto testType = createTestType("TestSolver");
    EXPECT_TRUE(registry.registerType(testType));

    EXPECT_TRUE(registry.hasType("TestSolver"));

    auto retrieved = registry.getTypeInfo("TestSolver");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->typeName, "TestSolver");
    EXPECT_EQ(retrieved->displayName, "TestSolver Display");
}

TEST_F(SolverTypeRegistryTest, PreventDuplicateRegistration) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto testType = createTestType("DuplicateTest");
    EXPECT_TRUE(registry.registerType(testType));
    EXPECT_FALSE(registry.registerType(testType));  // Should fail
}

TEST_F(SolverTypeRegistryTest, UnregisterType) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto testType = createTestType("ToUnregister");
    registry.registerType(testType);

    EXPECT_TRUE(registry.hasType("ToUnregister"));
    EXPECT_TRUE(registry.unregisterType("ToUnregister"));
    EXPECT_FALSE(registry.hasType("ToUnregister"));
}

TEST_F(SolverTypeRegistryTest, GetEnabledTypes) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto type1 = createTestType("Enabled1");
    type1.enabled = true;

    auto type2 = createTestType("Disabled");
    type2.enabled = false;

    auto type3 = createTestType("Enabled2");
    type3.enabled = true;

    registry.registerType(type1);
    registry.registerType(type2);
    registry.registerType(type3);

    auto enabled = registry.getEnabledTypes();
    EXPECT_EQ(enabled.size(), 2);

    bool found1 = false, found2 = false;
    for (const auto& t : enabled) {
        if (t.typeName == "Enabled1") found1 = true;
        if (t.typeName == "Enabled2") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(SolverTypeRegistryTest, GetBestTypePriority) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto lowPriority = createTestType("LowPriority", 30);
    auto highPriority = createTestType("HighPriority", 90);
    auto medPriority = createTestType("MedPriority", 50);

    registry.registerType(lowPriority);
    registry.registerType(medPriority);
    registry.registerType(highPriority);

    auto best = registry.getBestType();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best->typeName, "HighPriority");
}

TEST_F(SolverTypeRegistryTest, SetTypeEnabled) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto testType = createTestType("EnableTest");
    testType.enabled = true;
    registry.registerType(testType);

    auto info = registry.getTypeInfo("EnableTest");
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->enabled);

    registry.setTypeEnabled("EnableTest", false);
    info = registry.getTypeInfo("EnableTest");
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE(info->enabled);
}

TEST_F(SolverTypeRegistryTest, GetAllTypeNames) {
    auto& registry = SolverTypeRegistry::getInstance();

    registry.registerType(createTestType("Type1"));
    registry.registerType(createTestType("Type2"));
    registry.registerType(createTestType("Type3"));

    auto names = registry.getAllTypeNames();
    EXPECT_EQ(names.size(), 3);
}

TEST_F(SolverTypeRegistryTest, EventSubscription) {
    auto& registry = SolverTypeRegistry::getInstance();

    bool eventFired = false;
    std::string eventTypeName;

    auto subId = registry.subscribe([&](const SolverPluginEvent& event) {
        eventFired = true;
        eventTypeName = event.typeName;
    });

    registry.registerType(createTestType("EventTest"));

    EXPECT_TRUE(eventFired);
    EXPECT_EQ(eventTypeName, "EventTest");

    registry.unsubscribe(subId);
}

TEST_F(SolverTypeRegistryTest, FilterByCapability) {
    auto& registry = SolverTypeRegistry::getInstance();

    auto blindSupport = createTestType("BlindSolver");
    blindSupport.capabilities.supportsBlindSolve = true;
    blindSupport.capabilities.supportsHintedSolve = false;

    auto hintedOnly = createTestType("HintedSolver");
    hintedOnly.capabilities.supportsBlindSolve = false;
    hintedOnly.capabilities.supportsHintedSolve = true;

    auto both = createTestType("FullSolver");
    both.capabilities.supportsBlindSolve = true;
    both.capabilities.supportsHintedSolve = true;

    registry.registerType(blindSupport);
    registry.registerType(hintedOnly);
    registry.registerType(both);

    auto allTypes = registry.getAllTypes();
    EXPECT_EQ(allTypes.size(), 3);

    int blindCount = 0;
    for (const auto& t : allTypes) {
        if (t.capabilities.supportsBlindSolve) {
            blindCount++;
        }
    }
    EXPECT_EQ(blindCount, 2);
}

TEST_F(SolverTypeRegistryTest, NonExistentType) {
    auto& registry = SolverTypeRegistry::getInstance();

    EXPECT_FALSE(registry.hasType("NonExistent"));

    auto info = registry.getTypeInfo("NonExistent");
    EXPECT_FALSE(info.has_value());
}

}  // namespace
}  // namespace lithium::solver
