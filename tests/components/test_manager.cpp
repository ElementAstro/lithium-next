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

// Component State Management Tests
TEST_F(ComponentManagerTest, ComponentStateManagement) {
    json params = {{"name", "test_component"},
                   {"path", "/test/path"},
                   {"version", "1.0.0"}};

    EXPECT_TRUE(manager->loadComponent(params));
    EXPECT_EQ(manager->getComponentState("test_component"),
              ComponentState::Created);

    EXPECT_TRUE(manager->startComponent("test_component"));
    EXPECT_EQ(manager->getComponentState("test_component"),
              ComponentState::Running);

    EXPECT_TRUE(manager->pauseComponent("test_component"));
    EXPECT_EQ(manager->getComponentState("test_component"),
              ComponentState::Paused);

    EXPECT_TRUE(manager->resumeComponent("test_component"));
    EXPECT_EQ(manager->getComponentState("test_component"),
              ComponentState::Running);

    EXPECT_TRUE(manager->stopComponent("test_component"));
    EXPECT_EQ(manager->getComponentState("test_component"),
              ComponentState::Stopped);
}

// Event Handling Tests
TEST_F(ComponentManagerTest, EventHandling) {
    bool eventReceived = false;
    ComponentEvent receivedEvent;
    std::string receivedComponent;

    manager->addEventListener(
        ComponentEvent::StateChanged,
        [&](const std::string& component, ComponentEvent event, const json&) {
            eventReceived = true;
            receivedEvent = event;
            receivedComponent = component;
        });

    json params = {{"name", "test_component"}, {"path", "/test/path"}};

    EXPECT_TRUE(manager->loadComponent(params));
    EXPECT_TRUE(manager->startComponent("test_component"));

    EXPECT_TRUE(eventReceived);
    EXPECT_EQ(receivedEvent, ComponentEvent::StateChanged);
    EXPECT_EQ(receivedComponent, "test_component");

    manager->removeEventListener(ComponentEvent::StateChanged);
}

// Configuration Management Tests
TEST_F(ComponentManagerTest, ConfigurationManagement) {
    json params = {{"name", "test_component"},
                   {"path", "/test/path"},
                   {"config", {{"setting1", "value1"}}}};

    EXPECT_TRUE(manager->loadComponent(params));

    json newConfig = {{"setting2", "value2"}};
    manager->updateConfig("test_component", newConfig);

    auto config = manager->getConfig("test_component");
    EXPECT_EQ(config["setting1"], "value1");
    EXPECT_EQ(config["setting2"], "value2");
}

// Component Group Management Tests
TEST_F(ComponentManagerTest, GroupManagement) {
    json params1 = {{"name", "component1"}, {"path", "/test/path1"}};
    json params2 = {{"name", "component2"}, {"path", "/test/path2"}};

    EXPECT_TRUE(manager->loadComponent(params1));
    EXPECT_TRUE(manager->loadComponent(params2));

    manager->addToGroup("component1", "group1");
    manager->addToGroup("component2", "group1");

    auto groupComponents = manager->getGroupComponents("group1");
    EXPECT_EQ(groupComponents.size(), 2);
    EXPECT_TRUE(std::find(groupComponents.begin(), groupComponents.end(),
                          "component1") != groupComponents.end());
    EXPECT_TRUE(std::find(groupComponents.begin(), groupComponents.end(),
                          "component2") != groupComponents.end());
}

// Batch Operation Tests
TEST_F(ComponentManagerTest, BatchOperations) {
    std::vector<std::string> components = {"comp1", "comp2", "comp3"};

    for (const auto& comp : components) {
        json params = {{"name", comp}, {"path", "/test/path/" + comp}};
        EXPECT_TRUE(manager->loadComponent(params));
    }

    EXPECT_TRUE(manager->batchLoad(components));
    EXPECT_TRUE(manager->batchUnload(components));

    for (const auto& comp : components) {
        EXPECT_FALSE(manager->hasComponent(comp));
    }
}

// Performance Monitoring Tests
TEST_F(ComponentManagerTest, PerformanceMonitoring) {
    manager->enablePerformanceMonitoring(true);

    json params = {{"name", "test_component"}, {"path", "/test/path"}};
    EXPECT_TRUE(manager->loadComponent(params));

    auto metrics = manager->getPerformanceMetrics();
    EXPECT_TRUE(metrics.contains("test_component"));
    EXPECT_TRUE(metrics["test_component"].contains("state"));
    EXPECT_TRUE(metrics["test_component"].contains("load_time"));

    manager->enablePerformanceMonitoring(false);
    EXPECT_TRUE(manager->getPerformanceMetrics().empty());
}

// Error Handling Tests
TEST_F(ComponentManagerTest, ErrorHandling) {
    json invalidParams = {
        {"name", "invalid_component"}
        // Missing required 'path' field
    };

    EXPECT_FALSE(manager->loadComponent(invalidParams));
    EXPECT_FALSE(manager->getLastError().empty());

    manager->clearErrors();
    EXPECT_TRUE(manager->getLastError().empty());
}

// Thread Safety Tests
TEST_F(ComponentManagerTest, ThreadSafety) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            json params = {{"name", "component_" + std::to_string(i)},
                           {"path", "/test/path_" + std::to_string(i)}};
            if (manager->loadComponent(params)) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount, numThreads);
    EXPECT_EQ(manager->getComponentList().size(), numThreads);
}

// Component Dependency Tests
TEST_F(ComponentManagerTest, DependencyHandling) {
    json params1 = {{"name", "base_component"},
                    {"path", "/test/path1"},
                    {"version", "1.0.0"}};

    json params2 = {
        {"name", "dependent_component"},
        {"path", "/test/path2"},
        {"version", "1.0.0"},
        {"dependencies",
         json::array({{{"name", "base_component"}, {"version", "1.0.0"}}})}};

    EXPECT_TRUE(manager->loadComponent(params1));
    EXPECT_TRUE(manager->loadComponent(params2));

    // Try to unload base component (should fail due to dependency)
    EXPECT_FALSE(manager->unloadComponent(params1));

    // Unload dependent component first
    EXPECT_TRUE(manager->unloadComponent(params2));
    // Now base component can be unloaded
    EXPECT_TRUE(manager->unloadComponent(params1));
}

#endif  // LITHIUM_TEST_MANAGER_HPP
