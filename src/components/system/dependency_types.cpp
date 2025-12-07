#include "dependency_types.hpp"

#include <regex>
#include <sstream>

namespace lithium::system {

std::ostream& operator<<(std::ostream& os, const VersionInfo& vi) {
    os << vi.major << "." << vi.minor << "." << vi.patch;
    if (!vi.prerelease.empty()) {
        os << "-" << vi.prerelease;
    }
    return os;
}

VersionInfo parseVersion(const std::string& version) {
    // Regex supports: "1", "1.2", "1.2.3", "1.2.3-beta", "1.2.3-rc.1"
    static const std::regex versionRegex(
        R"((\d+)(?:\.(\d+))?(?:\.(\d+))?(?:-(.+))?)");
    std::smatch matches;
    VersionInfo info{};

    if (std::regex_match(version, matches, versionRegex)) {
        info.major = std::stoi(matches[1].str());
        if (matches[2].matched) {
            info.minor = std::stoi(matches[2].str());
        }
        if (matches[3].matched) {
            info.patch = std::stoi(matches[3].str());
        }
        if (matches[4].matched) {
            info.prerelease = matches[4].str();
        }
    }

    return info;
}

std::string versionToString(const VersionInfo& version) {
    std::ostringstream oss;
    oss << version;
    return oss.str();
}

bool isValidVersion(const std::string& version) {
    if (version.empty()) {
        return false;
    }
    static const std::regex versionRegex(
        R"((\d+)(?:\.(\d+))?(?:\.(\d+))?(?:-(.+))?)");
    return std::regex_match(version, versionRegex);
}

int compareVersions(const std::string& v1, const std::string& v2) {
    VersionInfo ver1 = parseVersion(v1);
    VersionInfo ver2 = parseVersion(v2);

    if (ver1 < ver2) {
        return -1;
    }
    if (ver1 == ver2) {
        return 0;
    }
    return 1;
}

bool satisfiesMinVersion(const VersionInfo& version,
                         const VersionInfo& minVersion) {
    return version >= minVersion;
}

bool satisfiesMaxVersion(const VersionInfo& version,
                         const VersionInfo& maxVersion) {
    return version <= maxVersion;
}

bool isVersionInRange(const VersionInfo& version, const std::string& minVersion,
                      const std::string& maxVersion) {
    if (!minVersion.empty()) {
        VersionInfo minVer = parseVersion(minVersion);
        if (!satisfiesMinVersion(version, minVer)) {
            return false;
        }
    }
    if (!maxVersion.empty()) {
        VersionInfo maxVer = parseVersion(maxVersion);
        if (!satisfiesMaxVersion(version, maxVer)) {
            return false;
        }
    }
    return true;
}

}  // namespace lithium::system
