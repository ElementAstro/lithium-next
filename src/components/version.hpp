#ifndef LITHIUM_ADDON_VERSION_HPP
#define LITHIUM_ADDON_VERSION_HPP

#include <charconv>
#include <iostream>
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
 * @brief Represents a semantic version following SemVer specification.
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
    constexpr Version() noexcept : major(0), minor(0), patch(0) {}

    /**
     * @brief Constructs a Version object with specified values.
     * @param maj Major version number
     * @param min Minor version number
     * @param pat Patch version number
     * @param pre Prerelease information
     * @param bld Build metadata
     */
    constexpr Version(int maj, int min, int pat, std::string pre = "",
                      std::string bld = "") noexcept
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
    static auto parse(std::string_view versionStr) -> Version;

    /**
     * @brief Converts the Version object to a string.
     * @return Version string
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief Converts the Version object to a short string (major.minor).
     * @return Short version string
     */
    [[nodiscard]] auto toShortString() const -> std::string;

    /**
     * @brief Checks if the current version is compatible with another version.
     * @param other The other version to compare with
     * @return True if compatible, false otherwise
     */
    [[nodiscard]] auto isCompatibleWith(const Version& other) const noexcept
        -> bool;

    /**
     * @brief Checks if the current version satisfies a given version range.
     * @param min The minimum version in the range
     * @param max The maximum version in the range
     * @return True if the version is within the range, false otherwise
     */
    [[nodiscard]] auto satisfiesRange(const Version& min,
                                      const Version& max) const noexcept
        -> bool;

    /**
     * @brief Checks if the version is a prerelease.
     * @return True if it is a prerelease, false otherwise
     */
    [[nodiscard]] constexpr auto isPrerelease() const noexcept -> bool {
        return !prerelease.empty();
    }

    /**
     * @brief Checks if the version has build metadata.
     * @return True if it has build metadata, false otherwise
     */
    [[nodiscard]] constexpr auto hasBuildMetadata() const noexcept -> bool {
        return !build.empty();
    }

    constexpr auto operator<(const Version& other) const noexcept -> bool;
    constexpr auto operator>(const Version& other) const noexcept -> bool;
    constexpr auto operator==(const Version& other) const noexcept -> bool;
    constexpr auto operator<=(const Version& other) const noexcept -> bool;
    constexpr auto operator>=(const Version& other) const noexcept -> bool;
} ATOM_ALIGNAS(128);

/**
 * @brief Outputs the Version object to an output stream.
 * @param os The output stream
 * @param version The Version object
 * @return The output stream
 */
auto operator<<(std::ostream& os, const Version& version) -> std::ostream&;

/**
 * @brief Represents a date-based version.
 */
struct DateVersion {
    int year;   ///< Year
    int month;  ///< Month (1-12)
    int day;    ///< Day (1-31)

    /**
     * @brief Constructs a DateVersion object with specified values.
     * @param y Year
     * @param m Month
     * @param d Day
     */
    constexpr DateVersion(int y, int m, int d) noexcept
        : year(y), month(m), day(d) {}

    constexpr DateVersion() noexcept : year(0), month(0), day(0) {}

    /**
     * @brief Parses a date string into a DateVersion object.
     * @param dateStr The date string to parse (YYYY-MM-DD format)
     * @return Parsed DateVersion object
     * @throws std::invalid_argument if the date string is invalid
     */
    static auto parse(std::string_view dateStr) -> DateVersion;

    constexpr auto operator<(const DateVersion& other) const noexcept -> bool;
    constexpr auto operator>(const DateVersion& other) const noexcept -> bool;
    constexpr auto operator==(const DateVersion& other) const noexcept -> bool;
    constexpr auto operator<=(const DateVersion& other) const noexcept -> bool;
    constexpr auto operator>=(const DateVersion& other) const noexcept -> bool;
} ATOM_ALIGNAS(16);

/**
 * @brief Outputs the DateVersion object to an output stream.
 * @param os The output stream
 * @param version The DateVersion object
 * @return The output stream
 */
auto operator<<(std::ostream& os, const DateVersion& version) -> std::ostream&;

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
                           bool incMax = true) noexcept
        : min(std::move(minVer)),
          max(std::move(maxVer)),
          includeMin(incMin),
          includeMax(incMax) {}

    /**
     * @brief Checks if a version is within the range.
     * @param version The version to check
     * @return True if the version is within the range, false otherwise
     */
    [[nodiscard]] auto contains(const Version& version) const noexcept -> bool;

    /**
     * @brief Parses a version range string into a VersionRange object.
     * @param rangeStr The version range string to parse
     * @return Parsed VersionRange object
     * @throws std::invalid_argument if the version range string is invalid
     */
    static auto parse(std::string_view rangeStr) -> VersionRange;

    /**
     * @brief Creates an open range starting from the specified version.
     * @param minVer Minimum version
     * @return Version range object
     */
    static auto from(Version minVer) -> VersionRange;

    /**
     * @brief Creates an open range up to the specified version.
     * @param maxVer Maximum version
     * @return Version range object
     */
    static auto upTo(Version maxVer) -> VersionRange;

    /**
     * @brief Converts the range to string representation.
     * @return String representation of the range
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief Checks if two version ranges overlap.
     * @param other Another version range
     * @return True if ranges overlap, false otherwise
     */
    [[nodiscard]] auto overlaps(const VersionRange& other) const noexcept
        -> bool;
};

/**
 * @brief Checks if the actual version satisfies the required version string.
 * @param actualVersion The actual version
 * @param requiredVersionStr The required version string
 * @param strategy Comparison strategy
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
    if (ec != std::errc{}) {
        THROW_INVALID_ARGUMENT("Invalid integer format");
    }
    return result;
}

constexpr auto Version::operator<(const Version& other) const noexcept -> bool {
    if (major != other.major)
        return major < other.major;
    if (minor != other.minor)
        return minor < other.minor;
    if (patch != other.patch)
        return patch < other.patch;

    if (prerelease.empty() && other.prerelease.empty())
        return false;
    if (prerelease.empty())
        return false;
    if (other.prerelease.empty())
        return true;

    return prerelease < other.prerelease;
}

constexpr auto Version::operator>(const Version& other) const noexcept -> bool {
    return other < *this;
}

constexpr auto Version::operator==(const Version& other) const noexcept
    -> bool {
    return major == other.major && minor == other.minor &&
           patch == other.patch && prerelease == other.prerelease;
}

constexpr auto Version::operator<=(const Version& other) const noexcept
    -> bool {
    return !(other < *this);
}

constexpr auto Version::operator>=(const Version& other) const noexcept
    -> bool {
    return !(*this < other);
}

constexpr auto DateVersion::operator<(const DateVersion& other) const noexcept
    -> bool {
    if (year != other.year)
        return year < other.year;
    if (month != other.month)
        return month < other.month;
    return day < other.day;
}

constexpr auto DateVersion::operator>(const DateVersion& other) const noexcept
    -> bool {
    return other < *this;
}

constexpr auto DateVersion::operator==(const DateVersion& other) const noexcept
    -> bool {
    return year == other.year && month == other.month && day == other.day;
}

constexpr auto DateVersion::operator<=(const DateVersion& other) const noexcept
    -> bool {
    return !(other < *this);
}

constexpr auto DateVersion::operator>=(const DateVersion& other) const noexcept
    -> bool {
    return !(*this < other);
}

}  // namespace lithium

#endif
