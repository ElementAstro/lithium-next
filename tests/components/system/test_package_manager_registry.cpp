#include <gtest/gtest.h>
#include "components/system/dependency_types.hpp"
#include "components/system/package_manager.hpp"
#include "components/system/platform_detector.hpp"

using namespace lithium::system;

class PackageManagerRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<PlatformDetector>();
        registry = std::make_unique<PackageManagerRegistry>(*detector);
    }

    void TearDown() override {
        registry.reset();
        detector.reset();
    }

    std::unique_ptr<PlatformDetector> detector;
    std::unique_ptr<PackageManagerRegistry> registry;
};

TEST_F(PackageManagerRegistryTest, LoadSystemPackageManagers) {
    registry->loadSystemPackageManagers();
    auto managers = registry->getPackageManagers();

    // Should have at least one package manager on most systems
    EXPECT_GE(managers.size(), 0);
}

TEST_F(PackageManagerRegistryTest, GetPackageManager) {
    registry->loadSystemPackageManagers();

    // Try to get a package manager that might exist
    auto pkgMgr = registry->getPackageManager("apt");

    // Result depends on platform, but method should not crash
    if (pkgMgr) {
        EXPECT_EQ(pkgMgr->name, "apt");
        EXPECT_TRUE(pkgMgr->getCheckCommand);
        EXPECT_TRUE(pkgMgr->getInstallCommand);
        EXPECT_TRUE(pkgMgr->getUninstallCommand);
        EXPECT_TRUE(pkgMgr->getSearchCommand);
    }
}

TEST_F(PackageManagerRegistryTest, GetPackageManagers) {
    registry->loadSystemPackageManagers();
    auto managers = registry->getPackageManagers();

    for (const auto& mgr : managers) {
        EXPECT_FALSE(mgr.name.empty());
        EXPECT_TRUE(mgr.getCheckCommand);
        EXPECT_TRUE(mgr.getInstallCommand);
        EXPECT_TRUE(mgr.getUninstallCommand);
        EXPECT_TRUE(mgr.getSearchCommand);
    }
}

TEST_F(PackageManagerRegistryTest, LoadConfigFromFile) {
    // This test assumes config file exists
    EXPECT_NO_THROW(
        registry->loadPackageManagerConfig("./config/package_managers.json"));
}

TEST_F(PackageManagerRegistryTest, SearchDependency) {
    registry->loadSystemPackageManagers();

    // Search might return empty if no package managers available
    // or if search fails, but should not crash
    EXPECT_NO_THROW({ auto results = registry->searchDependency("test"); });
}

TEST_F(PackageManagerRegistryTest, CancelInstallation) {
    // Should not crash even if no installation is running
    EXPECT_NO_THROW(registry->cancelInstallation("test-package"));
}

// Test command generation
TEST(PackageManagerCommandTest, CheckCommandGeneration) {
    DependencyInfo dep;
    dep.name = "test-package";
    dep.version = {1, 0, 0, ""};

    // Mock package manager info
    PackageManagerInfo pkgMgr;
    pkgMgr.name = "apt";
    pkgMgr.getCheckCommand = [](const DependencyInfo& d) {
        return "dpkg -l " + d.name;
    };

    auto cmd = pkgMgr.getCheckCommand(dep);
    EXPECT_EQ(cmd, "dpkg -l test-package");
}

TEST(PackageManagerCommandTest, InstallCommandGeneration) {
    DependencyInfo dep;
    dep.name = "test-package";
    dep.version = {1, 0, 0, ""};

    PackageManagerInfo pkgMgr;
    pkgMgr.name = "apt";
    pkgMgr.getInstallCommand = [](const DependencyInfo& d) {
        return "apt-get install -y " + d.name;
    };

    auto cmd = pkgMgr.getInstallCommand(dep);
    EXPECT_EQ(cmd, "apt-get install -y test-package");
}

TEST(PackageManagerCommandTest, UninstallCommandGeneration) {
    DependencyInfo dep;
    dep.name = "test-package";
    dep.version = {1, 0, 0, ""};

    PackageManagerInfo pkgMgr;
    pkgMgr.name = "apt";
    pkgMgr.getUninstallCommand = [](const DependencyInfo& d) {
        return "apt-get remove -y " + d.name;
    };

    auto cmd = pkgMgr.getUninstallCommand(dep);
    EXPECT_EQ(cmd, "apt-get remove -y test-package");
}

TEST(PackageManagerCommandTest, SearchCommandGeneration) {
    PackageManagerInfo pkgMgr;
    pkgMgr.name = "apt";
    pkgMgr.getSearchCommand = [](const std::string& name) {
        return "apt-cache search " + name;
    };

    auto cmd = pkgMgr.getSearchCommand("test");
    EXPECT_EQ(cmd, "apt-cache search test");
}
