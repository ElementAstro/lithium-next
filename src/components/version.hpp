#ifndef LITHIUM_ADDON_VERSION_HPP
#define LITHIUM_ADDON_VERSION_HPP

#include <charconv>
#include <string>
#include <string_view>
#include <utility>

#include "atom/error/exception.hpp"

#include "atom/macro.hpp"

namespace lithium {
/**
 * @brief Strategies for comparing versions.
 */
enum class VersionCompareStrategy {
    Strict,  ///< Compare full version including prerelease and build metadata
    IgnorePrerelease,  ///< Ignore prerelease information
    OnlyMajorMinor     ///< Compare only major and minor versions
};

/**
 * @brief Represents a semantic version.
 */
struct Version {
    int major;               ///< Major version number
    int minor;               ///< Minor version number
    int patch;               ///< Patch version number
    std::string prerelease;  ///< Prerelease information (e.g., alpha, beta, rc)
    std::string build;       ///< Build metadata

    /**
     * @brief Default constructor initializing version to 0.0.0.
     */
    constexpr Version() : major(0), minor(0), patch(0) {}

    /**
     * @brief Constructs a Version object with specified values.
     * @param maj Major version number
     * @param min Minor version number
     * @param pat Patch version number
     * @param pre Prerelease information
     * @param bld Build metadata
     */
    constexpr Version(int maj, int min, int pat, std::string pre = "",
                      std::string bld = "")
        : major(maj),
          minor(min),
          patch(pat),
          prerelease(std::move(pre)),
          build(std::move(bld)) {}

    /**
     * @brief Parses a version string into a Version object.
     * @param versionStr The version string to parse
     * @return Parsed Version object
     * @throws std::invalid_argument if the version string is invalid
     */
    static constexpr auto parse(std::string_view versionStr) -> Version;

    /**
     * @brief Converts the Version object to a string.
     * @return Version string
     */
    [[nodiscard]] auto toString() const -> std::string;

    constexpr auto operator<(const Version& other) const -> bool;
    constexpr auto operator>(const Version& other) const -> bool;
    constexpr auto operator==(const Version& other) const -> bool;
    constexpr auto operator<=(const Version& other) const -> bool;
    constexpr auto operator>=(const Version& other) const -> bool;

    /**
     * @brief Checks if the current version is compatible with another version.
     * @param other The other version to compare with
     * @return True if compatible, false otherwise
     */
    [[nodiscard]] bool isCompatibleWith(const Version& other) const;

    /**
     * @brief Checks if the current version satisfies a given version range.
     * @param min The minimum version in the range
     * @param max The maximum version in the range
     * @return True if the version is within the range, false otherwise
     */
    [[nodiscard]] bool satisfiesRange(const Version& min,
                                      const Version& max) const;

    /**
     * @brief Converts the Version object to a short string (major.minor).
     * @return Short version string
     */
    [[nodiscard]] std::string toShortString() const;

    /**
     * @brief Checks if the version is a prerelease.
     * @return True if it is a prerelease, false otherwise
     */
    [[nodiscard]] bool isPrerelease() const { return !prerelease.empty(); }

    /**
     * @brief Checks if the version has build metadata.
     * @return True if it has build metadata, false otherwise
     */
    [[nodiscard]] bool hasBuildMetadata() const { return !build.empty(); }
} ATOM_ALIGNAS(128);

/**
 * @brief Outputs the Version object to an output stream.
 * @param os The output stream
 * @param version The Version object
 * @return The output stream
 */
auto operator<<(std::ostream& os, const Version& version) -> std::ostream&;

/**
 * @brief Represents a date version.
 */
struct DateVersion {
    int year;   ///< Year
    int month;  ///< Month
    int day;    ///< Day

    /**
     * @brief Constructs a DateVersion object with specified values.
     * @param y Year
     * @param m Month
     * @param d Day
     */
    constexpr DateVersion(int y, int m, int d) : year(y), month(m), day(d) {}

    /**
     * @brief Parses a date string into a DateVersion object.
     * @param dateStr The date string to parse
     * @return Parsed DateVersion object
     * @throws std::invalid_argument if the date string is invalid
     */
    static constexpr auto parse(std::string_view dateStr) -> DateVersion;

    constexpr auto operator<(const DateVersion& other) const -> bool;
    constexpr auto operator>(const DateVersion& other) const -> bool;
    constexpr auto operator==(const DateVersion& other) const -> bool;
    constexpr auto operator<=(const DateVersion& other) const -> bool;
    constexpr auto operator>=(const DateVersion& other) const -> bool;
} ATOM_ALIGNAS(16);

/**
 * @brief Outputs the DateVersion object to an output stream.
 * @param outputStream The output stream
 * @param version The DateVersion object
 * @return The output stream
 */
auto operator<<(std::ostream& outputStream,
                const DateVersion& version) -> std::ostream&;

/**
 * @brief Checks if the actual version satisfies the required version string.
 * @param actualVersion The actual version
 * @param requiredVersionStr The required version string
 * @return True if the actual version satisfies the required version, false
 * otherwise
 */
auto checkVersion(
    const Version& actualVersion, const std::string& requiredVersionStr,
    VersionCompareStrategy strategy = VersionCompareStrategy::Strict) -> bool;

/**
 * @brief Checks if the actual date version satisfies the required date version
 * string.
 * @param actualVersion The actual date version
 * @param requiredVersionStr The required date version string
 * @return True if the actual date version satisfies the required date version,
 * false otherwise
 */
auto checkDateVersion(const DateVersion& actualVersion,
                      const std::string& requiredVersionStr) -> bool;

/**
 * @brief Parses a string into an integer.
 * @param str The string to parse
 * @return Parsed integer
 * @throws std::invalid_argument if the string is not a valid integer
 */
constexpr auto parseInt(std::string_view str) -> int {
    int result = 0;
    auto [ptr, ec] =
        std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec != std::errc()) {
        THROW_INVALID_ARGUMENT("Invalid integer format");
    }
    return result;
}

/**
 * @brief Parses a version string into a Version object.
 * @param versionStr The version string to parse
 * @return Parsed Version object
 * @throws std::invalid_argument if the version string is invalid
 */
constexpr auto Version::parse(std::string_view versionStr) -> Version {
    size_t pos = 0;
    auto nextDot = versionStr.find('.', pos);
    if (nextDot == std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Invalid version format");
    }

    int major = parseInt(versionStr.substr(pos, nextDot - pos));
    pos = nextDot + 1;

    nextDot = versionStr.find('.', pos);
    if (nextDot == std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Invalid version format");
    }

    int minor = parseInt(versionStr.substr(pos, nextDot - pos));
    pos = nextDot + 1;

    auto nextDash = versionStr.find('-', pos);
    auto nextPlus = versionStr.find('+', pos);
    size_t endPos = std::min(nextDash, nextPlus);

    int patch = parseInt(versionStr.substr(pos, endPos - pos));

    std::string prerelease = (nextDash != std::string_view::npos)
                                 ? std::string(versionStr.substr(
                                       nextDash + 1, nextPlus - nextDash - 1))
                                 : "";

    std::string build = (nextPlus != std::string_view::npos)
                            ? std::string(versionStr.substr(nextPlus + 1))
                            : "";

    return {major, minor, patch, prerelease, build};
}

constexpr auto Version::operator<(const Version& other) const -> bool {
    if (major != other.major) {
        return major < other.major;
    }
    if (minor != other.minor) {
        return minor < other.minor;
    }
    if (patch != other.patch) {
        return patch < other.patch;
    }
    if (prerelease.empty() && other.prerelease.empty()) {
        return false;
    }
    if (prerelease.empty()) {
        return false;
    }
    if (other.prerelease.empty()) {
        return true;
    }
    return prerelease < other.prerelease;
}

constexpr auto Version::operator>(const Version& other) const -> bool {
    return other < *this;
}
constexpr auto Version::operator==(const Version& other) const -> bool {
    return major == other.major && minor == other.minor &&
           patch == other.patch && prerelease == other.prerelease;
}
constexpr auto Version::operator<=(const Version& other) const -> bool {
    return *this < other || *this == other;
}
constexpr auto Version::operator>=(const Version& other) const -> bool {
    return *this > other || *this == other;
}

constexpr auto DateVersion::operator<(const DateVersion& other) const -> bool {
    if (year != other.year) {
        return year < other.year;
    }

    if (month != other.month) {
        return month < other.month;
    }
    return day < other.day;
}

constexpr auto DateVersion::operator>(const DateVersion& other) const -> bool {
    return other < *this;
}
constexpr auto DateVersion::operator==(const DateVersion& other) const -> bool {
    return year == other.year && month == other.month && day == other.day;
}
constexpr auto DateVersion::operator<=(const DateVersion& other) const -> bool {
    return *this < other || *this == other;
}
constexpr auto DateVersion::operator>=(const DateVersion& other) const -> bool {
    return *this > other || *this == other;
}

/**
 * @brief Represents a range of versions.
 */
struct VersionRange {
    Version min;      ///< Minimum version in the range
    Version max;      ///< Maximum version in the range
    bool includeMin;  ///< Whether to include the minimum version in the range
    bool includeMax;  ///< Whether to include the maximum version in the range

    /**
     * @brief Constructs a VersionRange object with specified values.
     * @param minVer Minimum version
     * @param maxVer Maximum version
     * @param incMin Whether to include the minimum version
     * @param incMax Whether to include the maximum version
     */
    constexpr VersionRange(Version minVer, Version maxVer, bool incMin = true,
                           bool incMax = true)
        : min(std::move(minVer)),
          max(std::move(maxVer)),
          includeMin(incMin),
          includeMax(incMax) {}

    /**
     * @brief Checks if a version is within the range.
     * @param version The version to check
     * @return True if the version is within the range, false otherwise
     */
    [[nodiscard]] bool contains(const Version& version) const;

    /**
     * @brief Parses a version range string into a VersionRange object.
     * @param rangeStr The version range string to parse
     * @return Parsed VersionRange object
     * @throws std::invalid_argument if the version range string is invalid
     */
    static VersionRange parse(std::string_view rangeStr);

    /**
     * @brief 创建一个从指定版本开始的开放范围
     * @param minVer 最小版本
     * @return 版本范围对象
     */
    static VersionRange from(Version minVer);

    /**
     * @brief 创建一个到指定版本为止的开放范围
     * @param maxVer 最大版本
     * @return 版本范围对象
     */
    static VersionRange upTo(Version maxVer);

    /**
     * @brief 将范围转换为字符串表示
     * @return 范围的字符串表示
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief 检查两个版本范围是否有交集
     * @param other 另一个版本范围
     * @return 如果有交集返回true，否则返回false
     */
    [[nodiscard]] bool overlaps(const VersionRange& other) const;
};

}  // namespace lithium

#endif