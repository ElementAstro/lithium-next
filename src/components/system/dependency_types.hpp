#ifndef LITHIUM_SYSTEM_DEPENDENCY_TYPES_HPP
#define LITHIUM_SYSTEM_DEPENDENCY_TYPES_HPP

#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace lithium::system {

/**
 * @struct VersionInfo
 * @brief Represents semantic version information for a dependency.
 *
 * This structure encapsulates the major, minor, and patch version numbers,
 * as well as an optional prerelease tag (such as "alpha", "beta", etc.).
 * It is used to describe the version of a software dependency in a
 * standardized way.
 */
struct VersionInfo {
    int major{0};  ///< Major version number (breaking changes).
    int minor{
        0};  ///< Minor version number (feature additions, backward compatible).
    int patch{0};  ///< Patch version number (bug fixes, backward compatible).
    std::string prerelease;  ///< Optional prerelease tag (e.g., "alpha",
                             ///< "beta", "rc.1").
};

/**
 * @brief Output stream operator for VersionInfo.
 * @param os The output stream.
 * @param vi The VersionInfo object to print.
 * @return Reference to the output stream.
 *
 * Prints the version in the format "major.minor.patch[-prerelease]".
 */
std::ostream& operator<<(std::ostream& os, const VersionInfo& vi);

/**
 * @brief Equality comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if all fields are equal, false otherwise.
 */
inline bool operator==(const VersionInfo& lhs, const VersionInfo& rhs) {
    return lhs.major == rhs.major && lhs.minor == rhs.minor &&
           lhs.patch == rhs.patch && lhs.prerelease == rhs.prerelease;
}

/**
 * @brief Less-than comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if lhs is less than rhs, false otherwise.
 *
 * Compares major, then minor, then patch, then prerelease lexicographically.
 */
inline bool operator<(const VersionInfo& lhs, const VersionInfo& rhs) {
    if (lhs.major != rhs.major)
        return lhs.major < rhs.major;
    if (lhs.minor != rhs.minor)
        return lhs.minor < rhs.minor;
    if (lhs.patch != rhs.patch)
        return lhs.patch < rhs.patch;
    return lhs.prerelease < rhs.prerelease;
}

/**
 * @brief Inequality comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if any field differs, false otherwise.
 */
inline bool operator!=(const VersionInfo& lhs, const VersionInfo& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Greater-than comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if lhs is greater than rhs, false otherwise.
 */
inline bool operator>(const VersionInfo& lhs, const VersionInfo& rhs) {
    return rhs < lhs;
}

/**
 * @brief Less-than-or-equal comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if lhs is less than or equal to rhs, false otherwise.
 */
inline bool operator<=(const VersionInfo& lhs, const VersionInfo& rhs) {
    return !(rhs < lhs);
}

/**
 * @brief Greater-than-or-equal comparison operator for VersionInfo.
 * @param lhs Left-hand side VersionInfo.
 * @param rhs Right-hand side VersionInfo.
 * @return True if lhs is greater than or equal to rhs, false otherwise.
 */
inline bool operator>=(const VersionInfo& lhs, const VersionInfo& rhs) {
    return !(lhs < rhs);
}

/**
 * @struct DependencyInfo
 * @brief Describes a software dependency and its metadata.
 *
 * This structure contains all relevant information about a dependency,
 * including its name, version, associated package manager, transitive
 * dependencies, and version constraints.
 */
struct DependencyInfo {
    std::string name;            ///< Name of the dependency (e.g., "openssl").
    VersionInfo version;         ///< Version information for the dependency.
    std::string packageManager;  ///< Name of the package manager to use (e.g.,
                                 ///< "apt", "brew").
    std::vector<std::string>
        dependencies;      ///< List of names of dependencies this depends on.
    bool optional{false};  ///< Whether this dependency is optional.
    std::string
        minVersion;  ///< Minimum required version (as string, e.g., "1.2.0").
    std::string
        maxVersion;  ///< Maximum allowed version (as string, e.g., "2.0.0").
};

/**
 * @struct PackageManagerInfo
 * @brief Contains information and command generators for a package manager.
 *
 * This structure provides the name of the package manager and function
 * objects to generate shell commands for checking, installing, uninstalling,
 * and searching for dependencies. These functions are used to interact with
 * the system's package management tools in a platform-agnostic way.
 */
struct PackageManagerInfo {
    std::string name;  ///< Name of the package manager (e.g., "apt", "brew").
    std::function<std::string(const DependencyInfo&)>
        getCheckCommand;  ///< Generates a command to check if a dependency is
                          ///< installed.
    std::function<std::string(const DependencyInfo&)>
        getInstallCommand;  ///< Generates a command to install a dependency.
    std::function<std::string(const DependencyInfo&)>
        getUninstallCommand;  ///< Generates a command to uninstall a
                              ///< dependency.
    std::function<std::string(const std::string&)>
        getSearchCommand;  ///< Generates a command to search for a dependency.
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_DEPENDENCY_TYPES_HPP
