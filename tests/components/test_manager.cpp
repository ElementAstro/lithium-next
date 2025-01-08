#ifndef LITHIUM_TEST_MANAGER_HPP
#define LITHIUM_TEST_MANAGER_HPP

#include <gtest/gtest.h>
#include <memory>
#include "components/manager.hpp"

using namespace lithium;

class ComponentManagerTest : public ::testing::Test {
protected:
    void SetUp() override { manager = std::make_unique<ComponentManager>(); }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<ComponentManager> manager;
};

TEST_F(ComponentManagerTest, Initialize) { EXPECT_TRUE(manager->initialize()); }

TEST_F(ComponentManagerTest, Destroy) { EXPECT_TRUE(manager->destroy()); }

TEST_F(ComponentManagerTest, CreateShared) {
    auto sharedManager = ComponentManager::createShared();
    EXPECT_NE(sharedManager, nullptr);
}

TEST_F(ComponentManagerTest, LoadComponent) {
    json params = {{"name", "test_component"}, {"version", "1.0"}};
    EXPECT_TRUE(manager->loadComponent(params));
}

TEST_F(ComponentManagerTest, UnloadComponent) {
    json params = {{"name", "test_component"}, {"version", "1.0"}};
    EXPECT_TRUE(manager->unloadComponent(params));
}

TEST_F(ComponentManagerTest, ScanComponents) {
    auto components = manager->scanComponents("/path/to/components");
    EXPECT_FALSE(components.empty());
}

TEST_F(ComponentManagerTest, GetComponent) {
    auto component = manager->getComponent("test_component");
    EXPECT_TRUE(component.has_value());
}

TEST_F(ComponentManagerTest, GetComponentInfo) {
    auto info = manager->getComponentInfo("test_component");
    EXPECT_TRUE(info.has_value());
}

TEST_F(ComponentManagerTest, GetComponentList) {
    auto components = manager->getComponentList();
    EXPECT_FALSE(components.empty());
}

TEST_F(ComponentManagerTest, GetComponentDoc) {
    auto doc = manager->getComponentDoc("test_component");
    EXPECT_FALSE(doc.empty());
}

TEST_F(ComponentManagerTest, HasComponent) {
    EXPECT_TRUE(manager->hasComponent("test_component"));
}

TEST_F(ComponentManagerTest, PrintDependencyTree) {
    manager->printDependencyTree();
    // No assertion, just ensure it doesn't crash
}

#endif  // LITHIUM_TEST_MANAGER_HPP