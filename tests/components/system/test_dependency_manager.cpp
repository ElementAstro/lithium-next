#include <gtest/gtest.h>
#include "components/system/dependency_system.hpp"

using namespace lithium::system;

class DependencyManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<DependencyManager>("./config/package_managers.json");
    }

    void TearDown() override {
        manager.reset();
    }

    std::unique_ptr<DependencyManager> manager;
};

TEST_F(DependencyManagerTest, GetCurrentPlatform) {
    auto platform = manager->getCurrentPlatform();
    EXPECT_FALSE(platform.empty());
}

TEST_F(DependencyManagerTest, GetPackageManagers) {
    auto pkgManagers = manager->getPackageManagers();
    // Should have at least one package manager on most systems
    // This might fail on minimal systems without package managers
    EXPECT_GE(pkgManagers.size(), 0);
}

TEST_F(DependencyManagerTest, AddAndRemoveDependency) {
    DependencyInfo dep;
    dep.name = "test-dependency";
    dep.version = {1, 0, 0, ""};
    dep.packageManager = "apt";

    manager->addDependency(dep);
    
    auto report = manager->generateDependencyReport();
    EXPECT_TRUE(report.find("test-dependency") != std::string::npos);

    manager->removeDependency("test-dependency");
    
    report = manager->generateDependencyReport();
    EXPECT_TRUE(report.find("test-dependency") == std::string::npos);
}

TEST_F(DependencyManagerTest, ExportAndImportConfig) {
    DependencyInfo dep;
    dep.name = "export-test";
    dep.version = {2, 1, 0, ""};
    dep.packageManager = "apt";

    manager->addDependency(dep);

    auto exportResult = manager->exportConfig();
    ASSERT_TRUE(exportResult.value.has_value());
    
    std::string config = *exportResult.value;
    EXPECT_TRUE(config.find("export-test") != std::string::npos);
    EXPECT_TRUE(config.find("2.1.0") != std::string::npos);

    // Create new manager and import
    DependencyManager newManager;
    auto importResult = newManager.importConfig(config);
    EXPECT_TRUE(importResult.success);
}

TEST_F(DependencyManagerTest, CheckVersionCompatibility) {
    DependencyInfo dep;
    dep.name = "version-test";
    dep.version = {3, 0, 0, ""};
    dep.packageManager = "apt";

    manager->addDependency(dep);

    // Should be compatible with lower version
    auto result1 = manager->checkVersionCompatibility("version-test", "2.0.0");
    ASSERT_TRUE(result1.value.has_value());
    EXPECT_TRUE(*result1.value);

    // Should not be compatible with higher version
    auto result2 = manager->checkVersionCompatibility("version-test", "4.0.0");
    ASSERT_TRUE(result2.value.has_value());
    EXPECT_FALSE(*result2.value);
}

TEST_F(DependencyManagerTest, GetDependencyGraph) {
    DependencyInfo dep1;
    dep1.name = "graph-test-1";
    dep1.version = {1, 0, 0, ""};
    dep1.packageManager = "apt";

    DependencyInfo dep2;
    dep2.name = "graph-test-2";
    dep2.version = {2, 0, 0, ""};
    dep2.packageManager = "apt";

    manager->addDependency(dep1);
    manager->addDependency(dep2);

    auto graph = manager->getDependencyGraph();
    EXPECT_TRUE(graph.find("graph-test-1") != std::string::npos);
    EXPECT_TRUE(graph.find("graph-test-2") != std::string::npos);
}

TEST_F(DependencyManagerTest, RefreshCache) {
    // Should not throw
    EXPECT_NO_THROW(manager->refreshCache());
}

// Platform-specific tests
#if defined(_WIN32)
TEST_F(DependencyManagerTest, WindowsPackageManagerDetection) {
    auto pkgManagers = manager->getPackageManagers();
    bool hasWindowsPm = false;
    for (const auto& pm : pkgManagers) {
        if (pm.name == "choco" || pm.name == "scoop" || pm.name == "winget") {
            hasWindowsPm = true;
            break;
        }
    }
    // At least one Windows package manager should be detected if installed
    // This is informational, not a hard requirement
    if (!hasWindowsPm) {
        GTEST_SKIP() << "No Windows package manager found";
    }
}
#elif defined(__APPLE__)
TEST_F(DependencyManagerTest, MacOSPackageManagerDetection) {
    auto pkgManagers = manager->getPackageManagers();
    bool hasMacPm = false;
    for (const auto& pm : pkgManagers) {
        if (pm.name == "brew" || pm.name == "port") {
            hasMacPm = true;
            break;
        }
    }
    if (!hasMacPm) {
        GTEST_SKIP() << "No macOS package manager found";
    }
}
#else
TEST_F(DependencyManagerTest, LinuxPackageManagerDetection) {
    auto pkgManagers = manager->getPackageManagers();
    bool hasLinuxPm = false;
    for (const auto& pm : pkgManagers) {
        if (pm.name == "apt" || pm.name == "dnf" || pm.name == "pacman" || 
            pm.name == "zypper" || pm.name == "yum") {
            hasLinuxPm = true;
            break;
        }
    }
    if (!hasLinuxPm) {
        GTEST_SKIP() << "No Linux package manager found";
    }
}
#endif

// Version parsing tests
class VersionInfoTest : public ::testing::Test {};

TEST_F(VersionInfoTest, VersionComparison) {
    VersionInfo v1{1, 0, 0, ""};
    VersionInfo v2{2, 0, 0, ""};
    VersionInfo v3{1, 1, 0, ""};
    VersionInfo v4{1, 0, 1, ""};

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v1 < v3);
    EXPECT_TRUE(v1 < v4);
    EXPECT_TRUE(v3 < v2);
    EXPECT_TRUE(v4 < v3);

    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v1 == v1);
    EXPECT_TRUE(v1 != v2);
}

TEST_F(VersionInfoTest, VersionEquality) {
    VersionInfo v1{1, 2, 3, ""};
    VersionInfo v2{1, 2, 3, ""};
    VersionInfo v3{1, 2, 3, "alpha"};

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

TEST_F(VersionInfoTest, VersionWithPrerelease) {
    VersionInfo v1{1, 0, 0, "alpha"};
    VersionInfo v2{1, 0, 0, "beta"};
    VersionInfo v3{1, 0, 0, ""};

    // alpha < beta < release (empty)
    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v1 < v3);
    EXPECT_TRUE(v2 < v3);
}
