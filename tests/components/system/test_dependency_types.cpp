/**
 * @file test_dependency_types.cpp
 * @brief Unit tests for dependency types defined in dependency_types.hpp
 *
 * Tests VersionInfo, DependencyInfo, and PackageManagerInfo structures
 */

#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "components/system/dependency_types.hpp"

using namespace lithium::system;

// ============================================================================
// VersionInfo Tests
// ============================================================================

class VersionInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(VersionInfoTest, DefaultValues) {
    VersionInfo version;
    EXPECT_EQ(version.major, 0);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 0);
    EXPECT_TRUE(version.prerelease.empty());
}

TEST_F(VersionInfoTest, SetVersionNumbers) {
    VersionInfo version;
    version.major = 1;
    version.minor = 2;
    version.patch = 3;

    EXPECT_EQ(version.major, 1);
    EXPECT_EQ(version.minor, 2);
    EXPECT_EQ(version.patch, 3);
}

TEST_F(VersionInfoTest, SetPrerelease) {
    VersionInfo version;
    version.major = 1;
    version.minor = 0;
    version.patch = 0;
    version.prerelease = "alpha";

    EXPECT_EQ(version.prerelease, "alpha");
}

TEST_F(VersionInfoTest, EqualityOperator) {
    VersionInfo v1{1, 2, 3, ""};
    VersionInfo v2{1, 2, 3, ""};
    VersionInfo v3{1, 2, 4, ""};
    VersionInfo v4{1, 2, 3, "alpha"};

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_FALSE(v1 == v4);
}

TEST_F(VersionInfoTest, InequalityOperator) {
    VersionInfo v1{1, 2, 3, ""};
    VersionInfo v2{1, 2, 4, ""};

    EXPECT_TRUE(v1 != v2);
    EXPECT_FALSE(v1 != v1);
}

TEST_F(VersionInfoTest, LessThanOperator) {
    VersionInfo v1{1, 0, 0, ""};
    VersionInfo v2{2, 0, 0, ""};
    VersionInfo v3{1, 1, 0, ""};
    VersionInfo v4{1, 0, 1, ""};
    VersionInfo v5{1, 0, 0, "alpha"};
    VersionInfo v6{1, 0, 0, "beta"};

    // Major version comparison
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v2 < v1);

    // Minor version comparison
    EXPECT_TRUE(v1 < v3);
    EXPECT_FALSE(v3 < v1);

    // Patch version comparison
    EXPECT_TRUE(v1 < v4);
    EXPECT_FALSE(v4 < v1);

    // Prerelease comparison
    EXPECT_TRUE(v5 < v6);
    EXPECT_FALSE(v6 < v5);
}

TEST_F(VersionInfoTest, GreaterThanOperator) {
    VersionInfo v1{2, 0, 0, ""};
    VersionInfo v2{1, 0, 0, ""};

    EXPECT_TRUE(v1 > v2);
    EXPECT_FALSE(v2 > v1);
}

TEST_F(VersionInfoTest, LessThanOrEqualOperator) {
    VersionInfo v1{1, 0, 0, ""};
    VersionInfo v2{1, 0, 0, ""};
    VersionInfo v3{2, 0, 0, ""};

    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v3);
    EXPECT_FALSE(v3 <= v1);
}

TEST_F(VersionInfoTest, GreaterThanOrEqualOperator) {
    VersionInfo v1{2, 0, 0, ""};
    VersionInfo v2{2, 0, 0, ""};
    VersionInfo v3{1, 0, 0, ""};

    EXPECT_TRUE(v1 >= v2);
    EXPECT_TRUE(v1 >= v3);
    EXPECT_FALSE(v3 >= v1);
}

TEST_F(VersionInfoTest, OutputStreamOperator) {
    VersionInfo version{1, 2, 3, ""};
    std::ostringstream oss;
    oss << version;

    std::string output = oss.str();
    // Should contain version numbers
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

TEST_F(VersionInfoTest, OutputStreamWithPrerelease) {
    VersionInfo version{1, 0, 0, "beta"};
    std::ostringstream oss;
    oss << version;

    std::string output = oss.str();
    EXPECT_FALSE(output.empty());
}

TEST_F(VersionInfoTest, VersionSorting) {
    std::vector<VersionInfo> versions = {
        {2, 0, 0, ""},
        {1, 0, 0, ""},
        {1, 5, 0, ""},
        {1, 0, 1, ""},
        {3, 0, 0, "alpha"}
    };

    std::sort(versions.begin(), versions.end());

    EXPECT_EQ(versions[0].major, 1);
    EXPECT_EQ(versions[0].minor, 0);
    EXPECT_EQ(versions[0].patch, 0);

    EXPECT_EQ(versions[4].major, 3);
}

// ============================================================================
// DependencyInfo Tests
// ============================================================================

class DependencyInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyInfoTest, DefaultValues) {
    DependencyInfo dep;
    EXPECT_TRUE(dep.name.empty());
    EXPECT_EQ(dep.version.major, 0);
    EXPECT_TRUE(dep.packageManager.empty());
    EXPECT_TRUE(dep.dependencies.empty());
    EXPECT_FALSE(dep.optional);
    EXPECT_TRUE(dep.minVersion.empty());
    EXPECT_TRUE(dep.maxVersion.empty());
}

TEST_F(DependencyInfoTest, SetBasicInfo) {
    DependencyInfo dep;
    dep.name = "openssl";
    dep.version = {1, 1, 1, ""};
    dep.packageManager = "apt";

    EXPECT_EQ(dep.name, "openssl");
    EXPECT_EQ(dep.version.major, 1);
    EXPECT_EQ(dep.version.minor, 1);
    EXPECT_EQ(dep.version.patch, 1);
    EXPECT_EQ(dep.packageManager, "apt");
}

TEST_F(DependencyInfoTest, SetDependencies) {
    DependencyInfo dep;
    dep.name = "mylib";
    dep.dependencies = {"zlib", "openssl", "curl"};

    EXPECT_EQ(dep.dependencies.size(), 3);
    EXPECT_EQ(dep.dependencies[0], "zlib");
    EXPECT_EQ(dep.dependencies[1], "openssl");
    EXPECT_EQ(dep.dependencies[2], "curl");
}

TEST_F(DependencyInfoTest, OptionalDependency) {
    DependencyInfo dep;
    dep.name = "optional-lib";
    dep.optional = true;

    EXPECT_TRUE(dep.optional);
}

TEST_F(DependencyInfoTest, VersionConstraints) {
    DependencyInfo dep;
    dep.name = "constrained-lib";
    dep.minVersion = "1.0.0";
    dep.maxVersion = "2.0.0";

    EXPECT_EQ(dep.minVersion, "1.0.0");
    EXPECT_EQ(dep.maxVersion, "2.0.0");
}

TEST_F(DependencyInfoTest, CompleteDependencyInfo) {
    DependencyInfo dep;
    dep.name = "complete-lib";
    dep.version = {2, 5, 0, "rc1"};
    dep.packageManager = "brew";
    dep.dependencies = {"dep1", "dep2"};
    dep.optional = false;
    dep.minVersion = "2.0.0";
    dep.maxVersion = "3.0.0";

    EXPECT_EQ(dep.name, "complete-lib");
    EXPECT_EQ(dep.version.major, 2);
    EXPECT_EQ(dep.version.minor, 5);
    EXPECT_EQ(dep.version.patch, 0);
    EXPECT_EQ(dep.version.prerelease, "rc1");
    EXPECT_EQ(dep.packageManager, "brew");
    EXPECT_EQ(dep.dependencies.size(), 2);
    EXPECT_FALSE(dep.optional);
    EXPECT_EQ(dep.minVersion, "2.0.0");
    EXPECT_EQ(dep.maxVersion, "3.0.0");
}

TEST_F(DependencyInfoTest, EmptyDependencies) {
    DependencyInfo dep;
    dep.name = "standalone-lib";
    dep.dependencies.clear();

    EXPECT_TRUE(dep.dependencies.empty());
}

TEST_F(DependencyInfoTest, ManyDependencies) {
    DependencyInfo dep;
    dep.name = "many-deps-lib";

    for (int i = 0; i < 50; ++i) {
        dep.dependencies.push_back("dep_" + std::to_string(i));
    }

    EXPECT_EQ(dep.dependencies.size(), 50);
}

// ============================================================================
// PackageManagerInfo Tests
// ============================================================================

class PackageManagerInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PackageManagerInfoTest, DefaultValues) {
    PackageManagerInfo pmInfo;
    EXPECT_TRUE(pmInfo.name.empty());
    EXPECT_FALSE(pmInfo.getCheckCommand);
    EXPECT_FALSE(pmInfo.getInstallCommand);
    EXPECT_FALSE(pmInfo.getUninstallCommand);
    EXPECT_FALSE(pmInfo.getSearchCommand);
}

TEST_F(PackageManagerInfoTest, SetName) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";

    EXPECT_EQ(pmInfo.name, "apt");
}

TEST_F(PackageManagerInfoTest, SetCheckCommand) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";
    pmInfo.getCheckCommand = [](const DependencyInfo& dep) {
        return "dpkg -l " + dep.name;
    };

    DependencyInfo dep;
    dep.name = "openssl";

    EXPECT_TRUE(pmInfo.getCheckCommand);
    EXPECT_EQ(pmInfo.getCheckCommand(dep), "dpkg -l openssl");
}

TEST_F(PackageManagerInfoTest, SetInstallCommand) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";
    pmInfo.getInstallCommand = [](const DependencyInfo& dep) {
        return "apt-get install -y " + dep.name;
    };

    DependencyInfo dep;
    dep.name = "curl";

    EXPECT_TRUE(pmInfo.getInstallCommand);
    EXPECT_EQ(pmInfo.getInstallCommand(dep), "apt-get install -y curl");
}

TEST_F(PackageManagerInfoTest, SetUninstallCommand) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";
    pmInfo.getUninstallCommand = [](const DependencyInfo& dep) {
        return "apt-get remove -y " + dep.name;
    };

    DependencyInfo dep;
    dep.name = "vim";

    EXPECT_TRUE(pmInfo.getUninstallCommand);
    EXPECT_EQ(pmInfo.getUninstallCommand(dep), "apt-get remove -y vim");
}

TEST_F(PackageManagerInfoTest, SetSearchCommand) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";
    pmInfo.getSearchCommand = [](const std::string& name) {
        return "apt-cache search " + name;
    };

    EXPECT_TRUE(pmInfo.getSearchCommand);
    EXPECT_EQ(pmInfo.getSearchCommand("python"), "apt-cache search python");
}

TEST_F(PackageManagerInfoTest, CompletePackageManager) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "brew";
    pmInfo.getCheckCommand = [](const DependencyInfo& dep) {
        return "brew list " + dep.name;
    };
    pmInfo.getInstallCommand = [](const DependencyInfo& dep) {
        return "brew install " + dep.name;
    };
    pmInfo.getUninstallCommand = [](const DependencyInfo& dep) {
        return "brew uninstall " + dep.name;
    };
    pmInfo.getSearchCommand = [](const std::string& name) {
        return "brew search " + name;
    };

    DependencyInfo dep;
    dep.name = "wget";

    EXPECT_EQ(pmInfo.name, "brew");
    EXPECT_EQ(pmInfo.getCheckCommand(dep), "brew list wget");
    EXPECT_EQ(pmInfo.getInstallCommand(dep), "brew install wget");
    EXPECT_EQ(pmInfo.getUninstallCommand(dep), "brew uninstall wget");
    EXPECT_EQ(pmInfo.getSearchCommand("wget"), "brew search wget");
}

TEST_F(PackageManagerInfoTest, CommandWithVersion) {
    PackageManagerInfo pmInfo;
    pmInfo.name = "apt";
    pmInfo.getInstallCommand = [](const DependencyInfo& dep) {
        std::string cmd = "apt-get install -y " + dep.name;
        if (dep.version.major > 0) {
            cmd += "=" + std::to_string(dep.version.major) + "." +
                   std::to_string(dep.version.minor) + "." +
                   std::to_string(dep.version.patch);
        }
        return cmd;
    };

    DependencyInfo dep;
    dep.name = "nginx";
    dep.version = {1, 18, 0, ""};

    EXPECT_EQ(pmInfo.getInstallCommand(dep), "apt-get install -y nginx=1.18.0");
}

TEST_F(PackageManagerInfoTest, DifferentPackageManagers) {
    // Test apt
    PackageManagerInfo apt;
    apt.name = "apt";
    apt.getInstallCommand = [](const DependencyInfo& dep) {
        return "apt-get install " + dep.name;
    };

    // Test dnf
    PackageManagerInfo dnf;
    dnf.name = "dnf";
    dnf.getInstallCommand = [](const DependencyInfo& dep) {
        return "dnf install " + dep.name;
    };

    // Test pacman
    PackageManagerInfo pacman;
    pacman.name = "pacman";
    pacman.getInstallCommand = [](const DependencyInfo& dep) {
        return "pacman -S " + dep.name;
    };

    DependencyInfo dep;
    dep.name = "git";

    EXPECT_EQ(apt.getInstallCommand(dep), "apt-get install git");
    EXPECT_EQ(dnf.getInstallCommand(dep), "dnf install git");
    EXPECT_EQ(pacman.getInstallCommand(dep), "pacman -S git");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(VersionInfoTest, ZeroVersion) {
    VersionInfo v{0, 0, 0, ""};
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST_F(VersionInfoTest, LargeVersionNumbers) {
    VersionInfo v{999, 999, 999, ""};
    EXPECT_EQ(v.major, 999);
    EXPECT_EQ(v.minor, 999);
    EXPECT_EQ(v.patch, 999);
}

TEST_F(DependencyInfoTest, SpecialCharactersInName) {
    DependencyInfo dep;
    dep.name = "lib-with_special.chars@1.0";

    EXPECT_EQ(dep.name, "lib-with_special.chars@1.0");
}

TEST_F(DependencyInfoTest, LongDependencyName) {
    DependencyInfo dep;
    std::string longName(1000, 'a');
    dep.name = longName;

    EXPECT_EQ(dep.name.length(), 1000);
}

}  // namespace lithium::test
