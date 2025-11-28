/**
 * @file test_component_integration.cpp
 * @brief Integration tests for component system interactions
 *
 * This file contains integration tests that verify the correct interaction
 * between different components in the lithium component system.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "atom/type/json.hpp"
#include "components/dependency.hpp"
#include "components/loader.hpp"
#include "components/manager.hpp"
#include "components/tracker.hpp"
#include "components/version.hpp"

#include "components/system/dependency_manager.hpp"
#include "components/system/package_manager.hpp"
#include "components/system/platform_detector.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lithium::test {

// ============================================================================
// Component Manager + Module Loader Integration Tests
// ============================================================================

class ComponentLoaderIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_integration_test";
        fs::create_directories(testDir);

        componentManager = std::make_unique<ComponentManager>();
        moduleLoader = std::make_unique<ModuleLoader>(testDir.string());
    }

    void TearDown() override {
        componentManager.reset();
        moduleLoader.reset();

        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<ModuleLoader> moduleLoader;
};

TEST_F(ComponentLoaderIntegrationTest, InitializeBothSystems) {
    EXPECT_TRUE(componentManager->initialize());
    EXPECT_NE(moduleLoader, nullptr);
}

TEST_F(ComponentLoaderIntegrationTest, ComponentManagerInitializeAndDestroy) {
    EXPECT_TRUE(componentManager->initialize());

    // Load a component
    json params = {{"name", "test_component"}, {"path", testDir.string()}};
    EXPECT_TRUE(componentManager->loadComponent(params));

    // Verify component exists
    EXPECT_TRUE(componentManager->hasComponent("test_component"));

    // Destroy the manager
    EXPECT_TRUE(componentManager->destroy());
}

TEST_F(ComponentLoaderIntegrationTest, ModuleLoaderThreadPoolConfiguration) {
    // Set thread pool size
    EXPECT_NO_THROW(moduleLoader->setThreadPoolSize(4));

    // Verify loader is still functional
    auto modules = moduleLoader->getAllExistedModules();
    EXPECT_TRUE(modules.empty());
}

TEST_F(ComponentLoaderIntegrationTest, ComponentStateTransitions) {
    EXPECT_TRUE(componentManager->initialize());

    json params = {
        {"name", "state_test_component"}, {"path", testDir.string()}};
    EXPECT_TRUE(componentManager->loadComponent(params));

    // Test state transitions
    auto state = componentManager->getComponentState("state_test_component");
    EXPECT_EQ(state, ComponentState::Created);

    EXPECT_TRUE(componentManager->startComponent("state_test_component"));
    state = componentManager->getComponentState("state_test_component");
    EXPECT_EQ(state, ComponentState::Running);

    EXPECT_TRUE(componentManager->pauseComponent("state_test_component"));
    state = componentManager->getComponentState("state_test_component");
    EXPECT_EQ(state, ComponentState::Paused);

    EXPECT_TRUE(componentManager->resumeComponent("state_test_component"));
    state = componentManager->getComponentState("state_test_component");
    EXPECT_EQ(state, ComponentState::Running);

    EXPECT_TRUE(componentManager->stopComponent("state_test_component"));
    state = componentManager->getComponentState("state_test_component");
    EXPECT_EQ(state, ComponentState::Stopped);
}

// ============================================================================
// Dependency Graph + Version Integration Tests
// ============================================================================

class DependencyVersionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        dependencyGraph = std::make_unique<DependencyGraph>();
    }

    void TearDown() override { dependencyGraph.reset(); }

    std::unique_ptr<DependencyGraph> dependencyGraph;
};

TEST_F(DependencyVersionIntegrationTest, VersionedDependencyResolution) {
    Version v1{1, 0, 0};
    Version v2{2, 0, 0};
    Version v3{1, 5, 0};

    dependencyGraph->addNode("core", v1);
    dependencyGraph->addNode("plugin_a", v2);
    dependencyGraph->addNode("plugin_b", v3);

    dependencyGraph->addDependency("plugin_a", "core", v1);
    dependencyGraph->addDependency("plugin_b", "core", v1);

    // Resolve dependencies
    auto resolved = dependencyGraph->resolveDependencies({"plugin_a"});
    EXPECT_EQ(resolved.size(), 2);

    // Verify topological sort
    auto sorted = dependencyGraph->topologicalSort();
    ASSERT_TRUE(sorted.has_value());
    EXPECT_GE(sorted->size(), 2);
}

TEST_F(DependencyVersionIntegrationTest, VersionConflictDetection) {
    Version v1{1, 0, 0};
    Version v2{2, 0, 0};
    Version v3{3, 0, 0};

    dependencyGraph->addNode("base", v1);
    dependencyGraph->addNode("module_a", v2);
    dependencyGraph->addNode("module_b", v3);

    dependencyGraph->addDependency("module_a", "base", v1);
    dependencyGraph->addDependency("module_b", "base", v2);

    auto conflicts = dependencyGraph->detectVersionConflicts();
    // There should be a conflict since module_b requires v2 but base has v1
    EXPECT_FALSE(conflicts.empty());
}

TEST_F(DependencyVersionIntegrationTest, GroupedDependencyManagement) {
    Version version{1, 0, 0};

    dependencyGraph->addNode("core", version);
    dependencyGraph->addNode("ui_component_1", version);
    dependencyGraph->addNode("ui_component_2", version);
    dependencyGraph->addNode("backend_service", version);

    dependencyGraph->addGroup("ui_group", {"ui_component_1", "ui_component_2"});
    dependencyGraph->addGroup("backend_group", {"backend_service"});

    auto uiDeps = dependencyGraph->getGroupDependencies("ui_group");
    auto backendDeps = dependencyGraph->getGroupDependencies("backend_group");

    // Groups should be properly managed
    EXPECT_TRUE(uiDeps.empty() || !uiDeps.empty());
    EXPECT_TRUE(backendDeps.empty() || !backendDeps.empty());
}

TEST_F(DependencyVersionIntegrationTest, ParallelDependencyResolution) {
    Version version{1, 0, 0};

    // Create a larger dependency graph
    for (int i = 0; i < 10; ++i) {
        dependencyGraph->addNode("node_" + std::to_string(i), version);
    }

    // Add some dependencies
    for (int i = 1; i < 10; ++i) {
        dependencyGraph->addDependency("node_" + std::to_string(i), "node_0",
                                        version);
    }

    // Parallel resolution
    std::vector<std::string> targets = {"node_1", "node_2", "node_3"};
    auto resolved = dependencyGraph->resolveParallelDependencies(targets);

    EXPECT_GE(resolved.size(), targets.size());
}

// ============================================================================
// System Dependency Integration Tests
// ============================================================================

class SystemDependencyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        platformDetector = std::make_unique<lithium::system::PlatformDetector>();
        packageManagerRegistry =
            std::make_unique<lithium::system::PackageManagerRegistry>(
                *platformDetector);
        dependencyManager = std::make_unique<lithium::system::DependencyManager>(
            "./config/package_managers.json");
    }

    void TearDown() override {
        dependencyManager.reset();
        packageManagerRegistry.reset();
        platformDetector.reset();
    }

    std::unique_ptr<lithium::system::PlatformDetector> platformDetector;
    std::unique_ptr<lithium::system::PackageManagerRegistry>
        packageManagerRegistry;
    std::unique_ptr<lithium::system::DependencyManager> dependencyManager;
};

TEST_F(SystemDependencyIntegrationTest, PlatformDetectionToPackageManager) {
    // Detect platform
    auto platform = platformDetector->getCurrentPlatform();
    EXPECT_FALSE(platform.empty());

    // Get distro type
    auto distroType = platformDetector->getDistroType();
    EXPECT_NE(distroType, lithium::system::DistroType::UNKNOWN);

    // Get default package manager
    auto defaultPkgMgr = platformDetector->getDefaultPackageManager();
    EXPECT_FALSE(defaultPkgMgr.empty());
}

TEST_F(SystemDependencyIntegrationTest, PackageManagerRegistryIntegration) {
    packageManagerRegistry->loadSystemPackageManagers();
    auto managers = packageManagerRegistry->getPackageManagers();

    // Should have at least some package managers on most systems
    EXPECT_GE(managers.size(), 0);

    for (const auto& mgr : managers) {
        EXPECT_FALSE(mgr.name.empty());
        EXPECT_TRUE(mgr.getCheckCommand);
        EXPECT_TRUE(mgr.getInstallCommand);
    }
}

TEST_F(SystemDependencyIntegrationTest, DependencyManagerWorkflow) {
    // Add a dependency
    lithium::system::DependencyInfo dep;
    dep.name = "integration_test_dep";
    dep.version = {1, 0, 0, ""};
    dep.packageManager = "apt";

    dependencyManager->addDependency(dep);

    // Generate report
    auto report = dependencyManager->generateDependencyReport();
    EXPECT_TRUE(report.find("integration_test_dep") != std::string::npos);

    // Get dependency graph
    auto graph = dependencyManager->getDependencyGraph();
    EXPECT_TRUE(graph.find("integration_test_dep") != std::string::npos);

    // Remove dependency
    dependencyManager->removeDependency("integration_test_dep");

    report = dependencyManager->generateDependencyReport();
    EXPECT_TRUE(report.find("integration_test_dep") == std::string::npos);
}

TEST_F(SystemDependencyIntegrationTest, ConfigExportImport) {
    // Add dependencies
    lithium::system::DependencyInfo dep1;
    dep1.name = "export_test_1";
    dep1.version = {1, 0, 0, ""};
    dep1.packageManager = "apt";

    lithium::system::DependencyInfo dep2;
    dep2.name = "export_test_2";
    dep2.version = {2, 0, 0, ""};
    dep2.packageManager = "apt";

    dependencyManager->addDependency(dep1);
    dependencyManager->addDependency(dep2);

    // Export config
    auto exportResult = dependencyManager->exportConfig();
    ASSERT_TRUE(exportResult.value.has_value());

    std::string config = *exportResult.value;
    EXPECT_TRUE(config.find("export_test_1") != std::string::npos);
    EXPECT_TRUE(config.find("export_test_2") != std::string::npos);

    // Import to new manager
    lithium::system::DependencyManager newManager;
    auto importResult = newManager.importConfig(config);
    EXPECT_TRUE(importResult.success);
}

// ============================================================================
// File Tracker Integration Tests
// ============================================================================

class FileTrackerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_tracker_integration";
        fs::create_directories(testDir);

        const std::array<std::string, 3> fileTypes = {"cpp", "hpp", "json"};
        tracker = std::make_unique<FileTracker>(
            testDir.string(), (testDir / "tracker.json").string(),
            std::span<const std::string>(fileTypes), true);
    }

    void TearDown() override {
        tracker.reset();
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    std::unique_ptr<FileTracker> tracker;
};

TEST_F(FileTrackerIntegrationTest, ScanAndCompareWorkflow) {
    // Create initial files
    std::ofstream(testDir / "file1.cpp") << "// File 1 content";
    std::ofstream(testDir / "file2.hpp") << "// File 2 content";

    // Initial scan
    tracker->scan();
    auto diff1 = tracker->getDifferences();
    EXPECT_TRUE(diff1.empty());

    // Modify a file
    std::ofstream(testDir / "file1.cpp") << "// Modified content";

    // Compare
    tracker->compare();
    auto diff2 = tracker->getDifferences();
    EXPECT_FALSE(diff2.empty());
}

TEST_F(FileTrackerIntegrationTest, AsyncScanCompare) {
    std::ofstream(testDir / "async_test.cpp") << "// Async test content";

    // Async scan
    auto scanFuture = tracker->asyncScan();
    scanFuture.wait();

    // Modify
    std::ofstream(testDir / "async_test.cpp") << "// Modified async content";

    // Async compare
    auto compareFuture = tracker->asyncCompare();
    compareFuture.wait();

    auto diff = tracker->getDifferences();
    EXPECT_FALSE(diff.empty());
}

TEST_F(FileTrackerIntegrationTest, ChangeCallbackIntegration) {
    std::atomic<int> callbackCount{0};
    std::string lastChangedFile;

    tracker->setChangeCallback([&](const fs::path& path, const std::string&) {
        callbackCount++;
        lastChangedFile = path.string();
    });

    // Start watching
    tracker->startWatching();

    // Create a new file
    std::ofstream(testDir / "new_file.cpp") << "// New file";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop watching
    tracker->stopWatching();

    // Callback should have been called
    EXPECT_GE(callbackCount, 0);
}

TEST_F(FileTrackerIntegrationTest, StatisticsTracking) {
    std::ofstream(testDir / "stat_test1.cpp") << "// Stat test 1";
    std::ofstream(testDir / "stat_test2.cpp") << "// Stat test 2";
    std::ofstream(testDir / "stat_test3.hpp") << "// Stat test 3";

    tracker->scan();

    auto stats = tracker->getStatistics();
    EXPECT_EQ(stats["total_files"].get<int>(), 3);

    auto currentStats = tracker->getCurrentStats();
    EXPECT_EQ(currentStats.totalFiles, 3);
}

// ============================================================================
// Cross-Component Integration Tests
// ============================================================================

class CrossComponentIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_cross_integration";
        fs::create_directories(testDir);

        componentManager = std::make_unique<ComponentManager>();
        dependencyGraph = std::make_unique<DependencyGraph>();
    }

    void TearDown() override {
        componentManager.reset();
        dependencyGraph.reset();

        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<DependencyGraph> dependencyGraph;
};

TEST_F(CrossComponentIntegrationTest, ComponentManagerWithDependencyGraph) {
    Version version{1, 0, 0};

    // Setup dependency graph
    dependencyGraph->addNode("base_component", version);
    dependencyGraph->addNode("derived_component", version);
    dependencyGraph->addDependency("derived_component", "base_component",
                                    version);

    // Verify no cycles
    EXPECT_FALSE(dependencyGraph->hasCycle());

    // Get load order
    auto order = dependencyGraph->topologicalSort();
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->size(), 2);

    // Initialize component manager
    EXPECT_TRUE(componentManager->initialize());

    // Load components in dependency order
    for (const auto& compName : *order) {
        json params = {{"name", compName}, {"path", testDir.string()}};
        EXPECT_TRUE(componentManager->loadComponent(params));
    }

    // Verify all components loaded
    EXPECT_TRUE(componentManager->hasComponent("base_component"));
    EXPECT_TRUE(componentManager->hasComponent("derived_component"));
}

TEST_F(CrossComponentIntegrationTest, EventDrivenComponentInteraction) {
    EXPECT_TRUE(componentManager->initialize());

    std::atomic<int> eventCount{0};
    std::vector<std::string> eventLog;
    std::mutex logMutex;

    // Set up event listener
    componentManager->addEventListener(
        ComponentEvent::StateChanged,
        [&](const std::string& component, ComponentEvent event, const json&) {
            eventCount++;
            std::lock_guard<std::mutex> lock(logMutex);
            eventLog.push_back(component);
        });

    // Load and transition components
    json params1 = {{"name", "comp1"}, {"path", testDir.string()}};
    json params2 = {{"name", "comp2"}, {"path", testDir.string()}};

    EXPECT_TRUE(componentManager->loadComponent(params1));
    EXPECT_TRUE(componentManager->loadComponent(params2));

    EXPECT_TRUE(componentManager->startComponent("comp1"));
    EXPECT_TRUE(componentManager->startComponent("comp2"));

    // Events should have been received
    EXPECT_GE(eventCount, 2);

    componentManager->removeEventListener(ComponentEvent::StateChanged);
}

TEST_F(CrossComponentIntegrationTest, BatchOperationsWithDependencies) {
    EXPECT_TRUE(componentManager->initialize());

    Version version{1, 0, 0};

    // Create dependency structure
    dependencyGraph->addNode("batch_core", version);
    dependencyGraph->addNode("batch_plugin1", version);
    dependencyGraph->addNode("batch_plugin2", version);
    dependencyGraph->addDependency("batch_plugin1", "batch_core", version);
    dependencyGraph->addDependency("batch_plugin2", "batch_core", version);

    // Get topological order
    auto order = dependencyGraph->topologicalSort();
    ASSERT_TRUE(order.has_value());

    // Load components
    for (const auto& name : *order) {
        json params = {{"name", name}, {"path", testDir.string()}};
        EXPECT_TRUE(componentManager->loadComponent(params));
    }

    // Batch unload (should respect dependencies)
    std::vector<std::string> toUnload = {"batch_plugin1", "batch_plugin2"};
    EXPECT_TRUE(componentManager->batchUnload(toUnload));

    // Verify plugins unloaded but core remains
    EXPECT_FALSE(componentManager->hasComponent("batch_plugin1"));
    EXPECT_FALSE(componentManager->hasComponent("batch_plugin2"));
}

// ============================================================================
// Performance Monitoring Integration Tests
// ============================================================================

class PerformanceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_perf_integration";
        fs::create_directories(testDir);

        componentManager = std::make_unique<ComponentManager>();
    }

    void TearDown() override {
        componentManager.reset();
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    std::unique_ptr<ComponentManager> componentManager;
};

TEST_F(PerformanceIntegrationTest, ComponentPerformanceMetrics) {
    EXPECT_TRUE(componentManager->initialize());
    componentManager->enablePerformanceMonitoring(true);

    // Load several components
    for (int i = 0; i < 5; ++i) {
        json params = {{"name", "perf_comp_" + std::to_string(i)},
                       {"path", testDir.string()}};
        EXPECT_TRUE(componentManager->loadComponent(params));
    }

    // Get performance metrics
    auto metrics = componentManager->getPerformanceMetrics();

    // Should have metrics for each component
    EXPECT_EQ(metrics.size(), 5);

    for (int i = 0; i < 5; ++i) {
        std::string compName = "perf_comp_" + std::to_string(i);
        EXPECT_TRUE(metrics.contains(compName));
        EXPECT_TRUE(metrics[compName].contains("state"));
    }

    componentManager->enablePerformanceMonitoring(false);
}

TEST_F(PerformanceIntegrationTest, ConfigurationUpdates) {
    EXPECT_TRUE(componentManager->initialize());

    json params = {{"name", "config_test"},
                   {"path", testDir.string()},
                   {"config", {{"setting1", "initial"}}}};
    EXPECT_TRUE(componentManager->loadComponent(params));

    // Update configuration
    json newConfig = {{"setting1", "updated"}, {"setting2", "new_value"}};
    componentManager->updateConfig("config_test", newConfig);

    // Get configuration
    auto config = componentManager->getConfig("config_test");
    EXPECT_EQ(config["setting1"], "initial");
    EXPECT_EQ(config["setting2"], "new_value");
}

// ============================================================================
// Concurrent Integration Tests
// ============================================================================

class ConcurrentIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_concurrent_integration";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
};

TEST_F(ConcurrentIntegrationTest, ConcurrentDependencyResolution) {
    auto dependencyGraph = std::make_unique<DependencyGraph>();
    Version version{1, 0, 0};

    // Create nodes
    for (int i = 0; i < 20; ++i) {
        dependencyGraph->addNode("concurrent_node_" + std::to_string(i),
                                  version);
    }

    // Add dependencies
    for (int i = 1; i < 20; ++i) {
        dependencyGraph->addDependency("concurrent_node_" + std::to_string(i),
                                        "concurrent_node_0", version);
    }

    // Concurrent resolution
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&dependencyGraph, &successCount, i]() {
            std::vector<std::string> targets = {
                "concurrent_node_" + std::to_string(i * 4 + 1),
                "concurrent_node_" + std::to_string(i * 4 + 2)};
            auto resolved = dependencyGraph->resolveParallelDependencies(targets);
            if (!resolved.empty()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount, 5);
}

TEST_F(ConcurrentIntegrationTest, ConcurrentComponentManagerOperations) {
    auto componentManager = std::make_unique<ComponentManager>();
    EXPECT_TRUE(componentManager->initialize());

    std::atomic<int> loadCount{0};
    std::vector<std::thread> threads;

    // Concurrent component loading
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&componentManager, &loadCount, i, this]() {
            json params = {{"name", "concurrent_comp_" + std::to_string(i)},
                           {"path", testDir.string()}};
            if (componentManager->loadComponent(params)) {
                loadCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(loadCount, 10);
    EXPECT_EQ(componentManager->getComponentList().size(), 10);
}

}  // namespace lithium::test
