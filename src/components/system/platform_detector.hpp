#ifndef LITHIUM_SYSTEM_PLATFORM_DETECTOR_HPP
#define LITHIUM_SYSTEM_PLATFORM_DETECTOR_HPP

#include <string>

#include "atom/sysinfo/os.hpp"

namespace lithium::system {

/**
 * @enum DistroType
 * @brief Enumerates supported Linux distributions and platforms.
 */
enum class DistroType {
    UNKNOWN,    ///< Unknown or unsupported distribution.
    DEBIAN,     ///< Debian-based distributions (e.g., Ubuntu).
    REDHAT,     ///< Red Hat-based distributions (e.g., Fedora, CentOS).
    ARCH,       ///< Arch Linux and derivatives.
    OPENSUSE,   ///< openSUSE distribution.
    GENTOO,     ///< Gentoo Linux.
    SLACKWARE,  ///< Slackware Linux.
    VOID,       ///< Void Linux.
    ALPINE,     ///< Alpine Linux.
    CLEAR,      ///< Clear Linux.
    SOLUS,      ///< Solus Linux.
    EMBEDDED,   ///< Embedded Linux systems.
    MACOS,      ///< Apple macOS.
    WINDOWS     ///< Microsoft Windows.
};

/**
 * @class PlatformDetector
 * @brief Detects the current operating system and distribution type.
 *
 * Provides methods to query the current platform, distribution type,
 * and the default package manager for the detected platform.
 */
class PlatformDetector {
public:
    /**
     * @brief Construct a PlatformDetector and perform platform detection.
     */
    PlatformDetector();

    /**
     * @brief Construct detector with explicit OS info (testing support).
     */
    explicit PlatformDetector(const atom::system::OperatingSystemInfo& info);

    /**
     * @brief Get a string identifier for the current platform.
     * @return The platform name (e.g., "linux", "macos", "windows").
     */
    auto getCurrentPlatform() const -> std::string;

    /**
     * @brief Get the detected Linux distribution or platform type.
     * @return The DistroType enumeration value.
     */
    auto getDistroType() const -> DistroType;

    /**
     * @brief Get the default package manager for the detected platform.
     * @return The name of the default package manager (e.g., "apt", "brew").
     */
    auto getDefaultPackageManager() const -> std::string;

private:
    /**
     * @brief Detect the current platform and distribution type.
     *
     * Populates the distroType_ and platform_ members.
     */
    void detectPlatform();
    void detectPlatform(const atom::system::OperatingSystemInfo& info);

    DistroType distroType_ =
        DistroType::UNKNOWN;  ///< Detected distribution type.
    std::string platform_;    ///< Platform identifier string.
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_PLATFORM_DETECTOR_HPP
