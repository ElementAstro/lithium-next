#include "platform_detector.hpp"

#include <algorithm>

#include "atom/sysinfo/os.hpp"

namespace lithium::system {

// ============================================================================
// Free Functions
// ============================================================================

std::string distroTypeToString(DistroType distro) {
    switch (distro) {
        case DistroType::UNKNOWN:
            return "unknown";
        case DistroType::DEBIAN:
            return "debian";
        case DistroType::REDHAT:
            return "redhat";
        case DistroType::ARCH:
            return "arch";
        case DistroType::OPENSUSE:
            return "opensuse";
        case DistroType::GENTOO:
            return "gentoo";
        case DistroType::SLACKWARE:
            return "slackware";
        case DistroType::VOID:
            return "void";
        case DistroType::ALPINE:
            return "alpine";
        case DistroType::CLEAR:
            return "clear";
        case DistroType::SOLUS:
            return "solus";
        case DistroType::EMBEDDED:
            return "embedded";
        case DistroType::MACOS:
            return "macos";
        case DistroType::WINDOWS:
            return "windows";
    }
    return "unknown";
}

std::string getDefaultPackageManagerForDistro(DistroType distro) {
    switch (distro) {
        case DistroType::DEBIAN:
            return "apt";
        case DistroType::REDHAT:
            return "dnf";
        case DistroType::ARCH:
            return "pacman";
        case DistroType::OPENSUSE:
            return "zypper";
        case DistroType::GENTOO:
            return "emerge";
        case DistroType::SLACKWARE:
            return "slackpkg";
        case DistroType::VOID:
            return "xbps";
        case DistroType::ALPINE:
            return "apk";
        case DistroType::CLEAR:
            return "swupd";
        case DistroType::SOLUS:
            return "eopkg";
        case DistroType::EMBEDDED:
            return "opkg";
        case DistroType::MACOS:
            return "brew";
        case DistroType::WINDOWS:
            return "choco";
        case DistroType::UNKNOWN:
        default:
            return "apt";  // Fallback to apt for unknown Linux
    }
}

std::vector<std::string> getSupportedPackageManagers(DistroType distro) {
    switch (distro) {
        case DistroType::DEBIAN:
            return {"apt", "apt-get", "dpkg", "snap", "flatpak"};
        case DistroType::REDHAT:
            return {"dnf", "yum", "rpm", "flatpak"};
        case DistroType::ARCH:
            return {"pacman", "yay", "paru", "flatpak"};
        case DistroType::OPENSUSE:
            return {"zypper", "rpm", "flatpak"};
        case DistroType::GENTOO:
            return {"emerge", "portage"};
        case DistroType::SLACKWARE:
            return {"slackpkg", "sbopkg"};
        case DistroType::VOID:
            return {"xbps", "xbps-install"};
        case DistroType::ALPINE:
            return {"apk"};
        case DistroType::CLEAR:
            return {"swupd"};
        case DistroType::SOLUS:
            return {"eopkg"};
        case DistroType::EMBEDDED:
            return {"opkg"};
        case DistroType::MACOS:
            return {"brew", "port", "mas"};
        case DistroType::WINDOWS:
            return {"choco", "scoop", "winget"};
        case DistroType::UNKNOWN:
        default:
            return {"apt"};
    }
}

// ============================================================================
// PlatformDetector Implementation
// ============================================================================

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
    return getDefaultPackageManagerForDistro(distroType_);
}

auto PlatformDetector::getSupportedPackageManagers() const
    -> std::vector<std::string> {
    return lithium::system::getSupportedPackageManagers(distroType_);
}

auto PlatformDetector::isPackageManagerSupported(
    const std::string& packageManager) const -> bool {
    auto supported = getSupportedPackageManagers();
    return std::find(supported.begin(), supported.end(), packageManager) !=
           supported.end();
}

auto PlatformDetector::getNormalizedPlatform() const -> std::string {
    switch (distroType_) {
        case DistroType::MACOS:
            return "macos";
        case DistroType::WINDOWS:
            return "windows";
        default:
            return "linux";
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
