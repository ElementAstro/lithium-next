#include "platform_detector.hpp"
#include "atom/sysinfo/os.hpp"

namespace lithium::system {

PlatformDetector::PlatformDetector() { detectPlatform(); }

PlatformDetector::PlatformDetector(
    const atom::system::OperatingSystemInfo& info) {
    detectPlatform(info);
}

auto PlatformDetector::getCurrentPlatform() const -> std::string {
    return platform_;
}

auto PlatformDetector::getDistroType() const -> DistroType {
    return distroType_;
}

auto PlatformDetector::getDefaultPackageManager() const -> std::string {
    switch (distroType_) {
        case DistroType::DEBIAN:
            return "apt";
        case DistroType::REDHAT:
            return "dnf";
        case DistroType::ARCH:
            return "pacman";
        case DistroType::MACOS:
            return "brew";
        case DistroType::WINDOWS:
            return "choco";
        default:
            return "apt";
    }
}

void PlatformDetector::detectPlatform() {
    using atom::system::getOperatingSystemInfo;
    auto osInfo = getOperatingSystemInfo();
    detectPlatform(osInfo);
}

void PlatformDetector::detectPlatform(
    const atom::system::OperatingSystemInfo& osInfo) {
    // Set platform string
    platform_ = osInfo.osName;

    // Basic mapping based on osName and kernelVersion
    if (osInfo.osName.find("Windows") != std::string::npos) {
        distroType_ = DistroType::WINDOWS;
    } else if (osInfo.osName.find("Darwin") != std::string::npos ||
               osInfo.osName.find("macOS") != std::string::npos) {
        distroType_ = DistroType::MACOS;
    } else if (osInfo.osName.find("Ubuntu") != std::string::npos ||
               osInfo.osName.find("Debian") != std::string::npos) {
        distroType_ = DistroType::DEBIAN;
    } else if (osInfo.osName.find("Fedora") != std::string::npos ||
               osInfo.osName.find("Red Hat") != std::string::npos ||
               osInfo.osName.find("CentOS") != std::string::npos) {
        distroType_ = DistroType::REDHAT;
    } else if (osInfo.osName.find("Arch") != std::string::npos) {
        distroType_ = DistroType::ARCH;
    } else if (osInfo.osName.find("openSUSE") != std::string::npos) {
        distroType_ = DistroType::OPENSUSE;
    } else if (osInfo.osName.find("Gentoo") != std::string::npos) {
        distroType_ = DistroType::GENTOO;
    } else if (osInfo.osName.find("Slackware") != std::string::npos) {
        distroType_ = DistroType::SLACKWARE;
    } else if (osInfo.osName.find("Void") != std::string::npos) {
        distroType_ = DistroType::VOID;
    } else if (osInfo.osName.find("Alpine") != std::string::npos) {
        distroType_ = DistroType::ALPINE;
    } else if (osInfo.osName.find("Clear Linux") != std::string::npos) {
        distroType_ = DistroType::CLEAR;
    } else if (osInfo.osName.find("Solus") != std::string::npos) {
        distroType_ = DistroType::SOLUS;
    } else if (osInfo.osName.find("Embedded") != std::string::npos) {
        distroType_ = DistroType::EMBEDDED;
    } else {
        distroType_ = DistroType::UNKNOWN;
    }
}

}  // namespace lithium::system
