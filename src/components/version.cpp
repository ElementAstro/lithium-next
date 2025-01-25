#include "version.hpp"

#include <format>

#include "atom/error/exception.hpp"

namespace lithium {

auto Version::toString() const -> std::string {
    auto result = std::format("{}.{}.{}", major, minor, patch);
    if (!prerelease.empty()) {
        result += "-" + prerelease;
    }
    if (!build.empty()) {
        result += "+" + build;
    }

    return result;
}

auto Version::isCompatibleWith(const Version& other) const -> bool {
    // 主版本号相同时认为兼容
    if (major != other.major) {
        return false;
    }

    // 如果次版本号小于目标版本，则兼容
    if (minor < other.minor) {
        return true;
    }

    // 次版本号相同时，补丁版本号小于等于目标版本则兼容
    return minor == other.minor && patch <= other.patch;
}

auto Version::satisfiesRange(const Version& min,
                             const Version& max) const -> bool {
    return *this >= min && *this <= max;
}

auto Version::toShortString() const -> std::string {
    return std::format("{}.{}", major, minor);
}

auto operator<<(std::ostream& outputStream,
                const Version& version) -> std::ostream& {
    outputStream << version.major << '.' << version.minor << '.'
                 << version.patch;
    if (!version.prerelease.empty()) {
        outputStream << '-' << version.prerelease;
    }
    if (!version.build.empty()) {
        outputStream << '+' << version.build;
    }
    return outputStream;
}

constexpr auto DateVersion::parse(std::string_view dateStr) -> DateVersion {
    size_t pos = 0;
    auto nextDash = dateStr.find('-', pos);
    if (nextDash == std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Invalid date format");
    }

    int year = parseInt(dateStr.substr(pos, nextDash - pos));
    pos = nextDash + 1;

    nextDash = dateStr.find('-', pos);
    if (nextDash == std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Invalid date format");
    }

    int month = parseInt(dateStr.substr(pos, nextDash - pos));
    int day = parseInt(dateStr.substr(nextDash + 1));

    constexpr int MAX_MONTH = 12;
    constexpr int MAX_DAY = 31;

    if (month < 1 || month > MAX_MONTH || day < 1 || day > MAX_DAY) {
        THROW_INVALID_ARGUMENT("Invalid date values");
    }

    return {year, month, day};
}

auto operator<<(std::ostream& outputStream,
                const DateVersion& version) -> std::ostream& {
    outputStream << version.year << '-' << version.month << '-' << version.day;
    return outputStream;
}

auto checkVersion(
    const Version& actualVersion, const std::string& requiredVersionStr,
    VersionCompareStrategy strategy) -> bool {
    size_t opLength = 1;
    if (requiredVersionStr.size() > 1 &&
        (requiredVersionStr[1] == '=' || requiredVersionStr[1] == '>')) {
        opLength = 2;
    }

    std::string operation = requiredVersionStr.substr(0, opLength);
    std::string versionPart = requiredVersionStr.substr(opLength);
    Version requiredVersion;

    try {
        requiredVersion = Version::parse(versionPart);
    } catch (const std::invalid_argument& e) {
        THROW_INVALID_ARGUMENT("Invalid version format: ", e.what());
        throw;
    }

    if (strategy == VersionCompareStrategy::IgnorePrerelease) {
        // 创建临时版本对象，忽略预发布信息
        Version actual{actualVersion.major, actualVersion.minor,
                       actualVersion.patch};
        Version required{requiredVersion.major, requiredVersion.minor,
                         requiredVersion.patch};
        if (operation == "^") {
            return actual.major == required.major && actual >= required;
        }
        if (operation == "~") {
            return actual.major == required.major &&
                   actual.minor == required.minor && actual >= required;
        }
        if (operation == ">") {
            return actual > required;
        }
        if (operation == "<") {
            return actual < required;
        }
        if (operation == ">=") {
            return actual >= required;
        }
        if (operation == "<=") {
            return actual <= required;
        }
        if (operation == "=") {
            return actual == required;
        }
    }

    if (operation == "^") {
        return actualVersion.major == requiredVersion.major &&
               actualVersion >= requiredVersion;
    }
    if (operation == "~") {
        return actualVersion.major == requiredVersion.major &&
               actualVersion.minor == requiredVersion.minor &&
               actualVersion >= requiredVersion;
    }
    if (operation == ">") {
        return actualVersion > requiredVersion;
    }
    if (operation == "<") {
        return actualVersion < requiredVersion;
    }
    if (operation == ">=") {
        return actualVersion >= requiredVersion;
    }
    if (operation == "<=") {
        return actualVersion <= requiredVersion;
    }
    if (operation == "=") {
        return actualVersion == requiredVersion;
    }

    return actualVersion == requiredVersion;
}

auto checkDateVersion(const DateVersion& actualVersion,
                      const std::string& requiredVersionStr) -> bool {
    size_t opLength = 1;
    if (requiredVersionStr.size() > 1 && requiredVersionStr[1] == '=') {
        opLength = 2;
    }

    std::string operation = requiredVersionStr.substr(0, opLength);
    DateVersion requiredVersion =
        DateVersion::parse(requiredVersionStr.substr(opLength));

    if (operation == ">") {
        return actualVersion > requiredVersion;
    }
    if (operation == "<") {
        return actualVersion < requiredVersion;
    }
    if (operation == ">=") {
        return actualVersion >= requiredVersion;
    }
    if (operation == "<=") {
        return actualVersion <= requiredVersion;
    }
    if (operation == "=") {
        return actualVersion == requiredVersion;
    }

    THROW_INVALID_ARGUMENT("Invalid comparison operator");
}

auto VersionRange::contains(const Version& version) const -> bool {
    bool afterMin = includeMin ? (version >= min) : (version > min);
    bool beforeMax = includeMax ? (version <= max) : (version < max);
    return afterMin && beforeMax;
}

auto VersionRange::parse(std::string_view rangeStr) -> VersionRange {
    auto separatorPos = rangeStr.find(',');
    if (separatorPos == std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Invalid version range format");
    }

    bool includeMin = rangeStr[0] == '[';
    bool includeMax = rangeStr[rangeStr.length() - 1] == ']';

    auto minStr = rangeStr.substr(1, separatorPos - 1);
    auto maxStr =
        rangeStr.substr(separatorPos + 1, rangeStr.length() - separatorPos - 2);

    return {Version::parse(minStr), Version::parse(maxStr), includeMin,
            includeMax};
}

auto VersionRange::from(Version minVer) -> VersionRange {
    return {std::move(minVer), Version{999, 999, 999}, true, false};
}

auto VersionRange::upTo(Version maxVer) -> VersionRange {
    return {Version{0, 0, 0}, std::move(maxVer), true, true};
}

auto VersionRange::toString() const -> std::string {
    return std::format("{}{}.{}.{}, {}.{}.{}{}", includeMin ? "[" : "(",
                       min.major, min.minor, min.patch, max.major, max.minor,
                       max.patch, includeMax ? "]" : ")");
}

auto VersionRange::overlaps(const VersionRange& other) const -> bool {
    if (max < other.min || other.max < min) {
        return false;
    }

    if (max == other.min) {
        return includeMax && other.includeMin;
    }

    if (min == other.max) {
        return includeMin && other.includeMax;
    }

    return true;
}

}  // namespace lithium
