#include "version.hpp"

#include <spdlog/spdlog.h>
#include <format>

#include "atom/error/exception.hpp"

namespace lithium {

auto Version::parse(std::string_view versionStr) -> Version {
    if (versionStr.empty()) {
        spdlog::error("Empty version string provided");
        THROW_INVALID_ARGUMENT("Empty version string");
    }

    size_t pos = 0;
    auto firstDot = versionStr.find('.', pos);
    if (firstDot == std::string_view::npos) {
        spdlog::error("Invalid version format: missing first dot in '{}'",
                      versionStr);
        THROW_INVALID_ARGUMENT("Invalid version format");
    }

    int major = parseInt(versionStr.substr(pos, firstDot - pos));
    pos = firstDot + 1;

    auto secondDot = versionStr.find('.', pos);
    if (secondDot == std::string_view::npos) {
        spdlog::error("Invalid version format: missing second dot in '{}'",
                      versionStr);
        THROW_INVALID_ARGUMENT("Invalid version format");
    }

    int minor = parseInt(versionStr.substr(pos, secondDot - pos));
    pos = secondDot + 1;

    auto dashPos = versionStr.find('-', pos);
    auto plusPos = versionStr.find('+', pos);
    size_t endPos = std::min(dashPos, plusPos);

    int patch = parseInt(versionStr.substr(pos, endPos - pos));

    std::string prerelease;
    std::string build;

    if (dashPos != std::string_view::npos) {
        size_t prereleaseEnd =
            (plusPos != std::string_view::npos && plusPos > dashPos)
                ? plusPos
                : versionStr.length();
        prerelease = std::string(
            versionStr.substr(dashPos + 1, prereleaseEnd - dashPos - 1));
    }

    if (plusPos != std::string_view::npos) {
        build = std::string(versionStr.substr(plusPos + 1));
    }

    spdlog::debug("Parsed version: {}.{}.{}-{}+{}", major, minor, patch,
                  prerelease, build);
    return {major, minor, patch, std::move(prerelease), std::move(build)};
}

auto Version::toString() const -> std::string {
    std::string result = std::format("{}.{}.{}", major, minor, patch);

    if (!prerelease.empty()) {
        result += "-" + prerelease;
    }
    if (!build.empty()) {
        result += "+" + build;
    }

    return result;
}

auto Version::toShortString() const -> std::string {
    return std::format("{}.{}", major, minor);
}

auto Version::isCompatibleWith(const Version& other) const noexcept -> bool {
    if (major != other.major) {
        return false;
    }

    if (minor < other.minor) {
        return true;
    }

    return minor == other.minor && patch <= other.patch;
}

auto Version::satisfiesRange(const Version& min,
                             const Version& max) const noexcept -> bool {
    return *this >= min && *this <= max;
}

auto operator<<(std::ostream& os, const Version& version) -> std::ostream& {
    os << version.major << '.' << version.minor << '.' << version.patch;
    if (!version.prerelease.empty()) {
        os << '-' << version.prerelease;
    }
    if (!version.build.empty()) {
        os << '+' << version.build;
    }
    return os;
}

auto DateVersion::parse(std::string_view dateStr) -> DateVersion {
    if (dateStr.empty()) {
        spdlog::error("Empty date string provided");
        THROW_INVALID_ARGUMENT("Empty date string");
    }

    size_t pos = 0;
    auto firstDash = dateStr.find('-', pos);
    if (firstDash == std::string_view::npos) {
        spdlog::error("Invalid date format: missing first dash in '{}'",
                      dateStr);
        THROW_INVALID_ARGUMENT("Invalid date format");
    }

    int year = parseInt(dateStr.substr(pos, firstDash - pos));
    pos = firstDash + 1;

    auto secondDash = dateStr.find('-', pos);
    if (secondDash == std::string_view::npos) {
        spdlog::error("Invalid date format: missing second dash in '{}'",
                      dateStr);
        THROW_INVALID_ARGUMENT("Invalid date format");
    }

    int month = parseInt(dateStr.substr(pos, secondDash - pos));
    int day = parseInt(dateStr.substr(secondDash + 1));

    constexpr int MIN_MONTH = 1, MAX_MONTH = 12;
    constexpr int MIN_DAY = 1, MAX_DAY = 31;

    if (month < MIN_MONTH || month > MAX_MONTH || day < MIN_DAY ||
        day > MAX_DAY) {
        spdlog::error("Invalid date values: year={}, month={}, day={}", year,
                      month, day);
        THROW_INVALID_ARGUMENT("Invalid date values");
    }

    spdlog::debug("Parsed date version: {}-{:02d}-{:02d}", year, month, day);
    return {year, month, day};
}

auto operator<<(std::ostream& os, const DateVersion& version) -> std::ostream& {
    os << std::format("{}-{:02d}-{:02d}", version.year, version.month,
                      version.day);
    return os;
}

auto checkVersion(const Version& actualVersion,
                  const std::string& requiredVersionStr,
                  VersionCompareStrategy strategy) -> bool {
    if (requiredVersionStr.empty()) {
        spdlog::warn("Empty required version string, assuming match");
        return true;
    }

    size_t opLength = 1;
    if (requiredVersionStr.size() > 1 &&
        (requiredVersionStr[1] == '=' || requiredVersionStr[1] == '>')) {
        opLength = 2;
    }

    std::string_view operation =
        std::string_view(requiredVersionStr).substr(0, opLength);
    std::string_view versionPart =
        std::string_view(requiredVersionStr).substr(opLength);

    Version requiredVersion;
    try {
        requiredVersion = Version::parse(versionPart);
    } catch (const std::invalid_argument& e) {
        spdlog::error("Failed to parse required version '{}': {}", versionPart,
                      e.what());
        THROW_INVALID_ARGUMENT("Invalid version format: ", e.what());
    }

    Version actual = actualVersion;
    Version required = requiredVersion;

    if (strategy == VersionCompareStrategy::IgnorePrerelease) {
        actual = Version{actualVersion.major, actualVersion.minor,
                         actualVersion.patch};
        required = Version{requiredVersion.major, requiredVersion.minor,
                           requiredVersion.patch};
    } else if (strategy == VersionCompareStrategy::OnlyMajorMinor) {
        actual = Version{actualVersion.major, actualVersion.minor, 0};
        required = Version{requiredVersion.major, requiredVersion.minor, 0};
    }

    bool result = false;
    if (operation == "^") {
        result = actual.major == required.major && actual >= required;
    } else if (operation == "~") {
        result = actual.major == required.major &&
                 actual.minor == required.minor && actual >= required;
    } else if (operation == ">") {
        result = actual > required;
    } else if (operation == "<") {
        result = actual < required;
    } else if (operation == ">=") {
        result = actual >= required;
    } else if (operation == "<=") {
        result = actual <= required;
    } else if (operation == "=") {
        result = actual == required;
    } else {
        result = actual == required;
    }

    spdlog::debug("Version check: {} {} {} = {}", actual.toString(), operation,
                  required.toString(), result);
    return result;
}

auto checkDateVersion(const DateVersion& actualVersion,
                      const std::string& requiredVersionStr) -> bool {
    if (requiredVersionStr.empty()) {
        spdlog::warn("Empty required date version string, assuming match");
        return true;
    }

    size_t opLength = 1;
    if (requiredVersionStr.size() > 1 && requiredVersionStr[1] == '=') {
        opLength = 2;
    }

    std::string_view operation =
        std::string_view(requiredVersionStr).substr(0, opLength);
    std::string_view versionPart =
        std::string_view(requiredVersionStr).substr(opLength);

    DateVersion requiredVersion;
    try {
        requiredVersion = DateVersion::parse(versionPart);
    } catch (const std::invalid_argument& e) {
        spdlog::error("Failed to parse required date version '{}': {}",
                      versionPart, e.what());
        throw;
    }

    bool result = false;
    if (operation == ">") {
        result = actualVersion > requiredVersion;
    } else if (operation == "<") {
        result = actualVersion < requiredVersion;
    } else if (operation == ">=") {
        result = actualVersion >= requiredVersion;
    } else if (operation == "<=") {
        result = actualVersion <= requiredVersion;
    } else if (operation == "=") {
        result = actualVersion == requiredVersion;
    } else {
        spdlog::error("Invalid comparison operator: {}", operation);
        THROW_INVALID_ARGUMENT("Invalid comparison operator");
    }

    spdlog::debug(
        "Date version check: {}-{:02d}-{:02d} {} {}-{:02d}-{:02d} = {}",
        actualVersion.year, actualVersion.month, actualVersion.day, operation,
        requiredVersion.year, requiredVersion.month, requiredVersion.day,
        result);
    return result;
}

auto VersionRange::contains(const Version& version) const noexcept -> bool {
    bool afterMin = includeMin ? (version >= min) : (version > min);
    bool beforeMax = includeMax ? (version <= max) : (version < max);
    return afterMin && beforeMax;
}

auto VersionRange::parse(std::string_view rangeStr) -> VersionRange {
    if (rangeStr.empty()) {
        spdlog::error("Empty version range string provided");
        THROW_INVALID_ARGUMENT("Empty version range string");
    }

    auto separatorPos = rangeStr.find(',');
    if (separatorPos == std::string_view::npos) {
        spdlog::error("Invalid version range format: missing comma in '{}'",
                      rangeStr);
        THROW_INVALID_ARGUMENT("Invalid version range format");
    }

    bool includeMin = rangeStr[0] == '[';
    bool includeMax = rangeStr[rangeStr.length() - 1] == ']';

    if (rangeStr.length() < 3) {
        spdlog::error("Version range string too short: '{}'", rangeStr);
        THROW_INVALID_ARGUMENT("Invalid version range format");
    }

    auto minStr = rangeStr.substr(1, separatorPos - 1);
    auto maxStr =
        rangeStr.substr(separatorPos + 1, rangeStr.length() - separatorPos - 2);

    spdlog::debug(
        "Parsing version range: min='{}' (include={}), max='{}' (include={})",
        minStr, includeMin, maxStr, includeMax);

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

auto VersionRange::overlaps(const VersionRange& other) const noexcept -> bool {
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
