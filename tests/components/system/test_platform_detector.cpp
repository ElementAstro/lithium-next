#include <gtest/gtest.h>
#include "components/system/platform_detector.hpp"
#include "atom/sysinfo/os.hpp"

using namespace lithium::system;

class PlatformDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<PlatformDetector>();
    }

    void TearDown() override {
        detector.reset();
    }

    std::unique_ptr<PlatformDetector> detector;
};

TEST_F(PlatformDetectorTest, GetCurrentPlatform) {
    auto platform = detector->getCurrentPlatform();
    EXPECT_FALSE(platform.empty());
    
    // Platform should be one of the known types
    bool isKnownPlatform = 
        platform.find("Windows") != std::string::npos ||
        platform.find("Linux") != std::string::npos ||
        platform.find("Darwin") != std::string::npos ||
        platform.find("macOS") != std::string::npos;
    EXPECT_TRUE(isKnownPlatform);
}

TEST_F(PlatformDetectorTest, GetDistroType) {
    auto distroType = detector->getDistroType();
    EXPECT_NE(distroType, DistroType::UNKNOWN);
}

TEST_F(PlatformDetectorTest, GetDefaultPackageManager) {
    auto pkgMgr = detector->getDefaultPackageManager();
    EXPECT_FALSE(pkgMgr.empty());
    
    // Should return a known package manager
    std::vector<std::string> knownPkgMgrs = {
        "apt", "dnf", "yum", "pacman", "zypper",
        "brew", "port", "choco", "scoop", "winget"
    };
    
    bool isKnown = std::find(knownPkgMgrs.begin(), knownPkgMgrs.end(), pkgMgr) 
                   != knownPkgMgrs.end();
    EXPECT_TRUE(isKnown);
}

// Test with mocked OS info
TEST(PlatformDetectorMockTest, DetectWindowsPlatform) {
    atom::system::OperatingSystemInfo mockInfo;
    mockInfo.osName = "Windows 11";
    mockInfo.osVersion = "10.0.22000";
    
    PlatformDetector detector(mockInfo);
    
    EXPECT_EQ(detector.getDistroType(), DistroType::WINDOWS);
    EXPECT_EQ(detector.getDefaultPackageManager(), "choco");
}

TEST(PlatformDetectorMockTest, DetectMacOSPlatform) {
    atom::system::OperatingSystemInfo mockInfo;
    mockInfo.osName = "macOS";
    mockInfo.osVersion = "14.0";
    
    PlatformDetector detector(mockInfo);
    
    EXPECT_EQ(detector.getDistroType(), DistroType::MACOS);
    EXPECT_EQ(detector.getDefaultPackageManager(), "brew");
}

TEST(PlatformDetectorMockTest, DetectDebianPlatform) {
    atom::system::OperatingSystemInfo mockInfo;
    mockInfo.osName = "Ubuntu 22.04";
    mockInfo.osVersion = "22.04";
    
    PlatformDetector detector(mockInfo);
    
    EXPECT_EQ(detector.getDistroType(), DistroType::DEBIAN);
    EXPECT_EQ(detector.getDefaultPackageManager(), "apt");
}

TEST(PlatformDetectorMockTest, DetectRedHatPlatform) {
    atom::system::OperatingSystemInfo mockInfo;
    mockInfo.osName = "Fedora 39";
    mockInfo.osVersion = "39";
    
    PlatformDetector detector(mockInfo);
    
    EXPECT_EQ(detector.getDistroType(), DistroType::REDHAT);
    EXPECT_EQ(detector.getDefaultPackageManager(), "dnf");
}

TEST(PlatformDetectorMockTest, DetectArchPlatform) {
    atom::system::OperatingSystemInfo mockInfo;
    mockInfo.osName = "Arch Linux";
    mockInfo.osVersion = "rolling";
    
    PlatformDetector detector(mockInfo);
    
    EXPECT_EQ(detector.getDistroType(), DistroType::ARCH);
    EXPECT_EQ(detector.getDefaultPackageManager(), "pacman");
}
